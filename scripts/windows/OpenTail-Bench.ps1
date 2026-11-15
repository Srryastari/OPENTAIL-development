# OpenTail: Windows USB log capture + authorized Android BLE bench session.
# NEVER talks to the controller over HTTP/Wi-Fi. No automatic BLE writes or NVS writes.
[CmdletBinding()]
param(
 [switch]$FlashFirmware,
 [switch]$DirectLedProbe,
 [switch]$SelfTest,
 [ValidateRange(30,1800)][int]$CaptureSeconds=30,
 [ValidateSet('bluetooth','layout','save','playback')][string]$Mode='bluetooth',
 [string]$AndroidSerial="",
 [string]$EspPort=""
)
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
$repo="Srryastari/OPENTAIL-development"
$root=(Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$session=Join-Path $root ("bench-reports\bench-"+(Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -Path $session -ItemType Directory -Force | Out-Null
$steps=[ordered]@{}
$notes=New-Object System.Collections.Generic.List[string]
$androidSha="";$firmwareSha="";$phone="";$esp="";$failed=$false
$logcat=Join-Path $session "android-logcat.txt"
$serial=Join-Path $session "esp32-serial.txt"
$timeline=Join-Path $session "esp32-timeline.txt"
$stopFile=Join-Path $session "capture.stop"
$adbProcess=$null;$serialProcess=$null
$sessionStarted=$false
$installBlocked=$false
$earlyFailure=""
$clockInfo=[ordered]@{available=$false;phone_skew_seconds=$null;phone_utc_offset=$null;uncertainty_ms=$null}
$captureElapsed=0
. (Join-Path $PSScriptRoot "Bench-Diagnostics.ps1")
$ghExe=""
$artifacts=[ordered]@{}
$ci=[ordered]@{}
function Step([string]$name,[string]$state,[string]$detail) {
 $script:steps[$name]=[ordered]@{state=$state;detail=$detail}
 Write-Host ("[{0}] {1}: {2}" -f $state,$name,$detail)
}
function Need([bool]$condition,[string]$message) {if(-not $condition){throw $message}}
function Tool([string]$name) {Need ($null -ne (Get-Command $name -ErrorAction SilentlyContinue)) "$name not installed or not on PATH"}
# User-approved GitHub CLI bootstrap. Never silently install software or connect accounts.
function FindGh {
 $cmd=Get-Command gh.exe -ErrorAction SilentlyContinue
 if($null -ne $cmd){return [string]$cmd.Source}
 $candidates=@(
  (Join-Path $env:ProgramFiles "GitHub CLI\gh.exe"),
  (Join-Path ([Environment]::GetFolderPath("ProgramFilesX86")) "GitHub CLI\gh.exe"),
  (Join-Path $env:LOCALAPPDATA "Programs\GitHub CLI\gh.exe"),
  (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\gh.exe")
 )
 foreach($candidate in $candidates){
  if(Test-Path $candidate -PathType Leaf){return $candidate}
 }
 return ""
}
function EnsureGh {
 $found=FindGh
 if(-not $found) {
  Write-Host "GitHub CLI is missing. It downloads VERIFIED GitHub Actions builds; controller Bluetooth is unaffected."
  if($null -eq (Get-Command winget.exe -ErrorAction SilentlyContinue)){
   throw "SETUP REQUIRED: WinGet is missing. Install GitHub CLI from https://cli.github.com/ then rerun OpenTail-Bench.bat."
  }
  $choice=Read-Host "Install official GitHub CLI using WinGet now? Type INSTALL to approve"
  if($choice -cne "INSTALL") {
   throw "SETUP DECLINED: GitHub CLI was not installed. Install it with winget install --id GitHub.cli --source winget."
  }
  & winget.exe install --exact --id GitHub.cli --source winget --accept-package-agreements --accept-source-agreements
  if($LASTEXITCODE -ne 0){
   throw "WinGet could not install GitHub CLI. Check installer output; retry from a fresh console."
  }
  # WinGet's MSI may update only the registry PATH; make it visible to THIS
  # already-running PowerShell process so the owner needn't restart manually.
  foreach($scope in @("Machine","User")){
   $newPath=[Environment]::GetEnvironmentVariable("Path",$scope)
   if($newPath){$env:Path=$env:Path+";"+$newPath}
  }
  $found=FindGh
  if(-not $found){
   throw "GitHub CLI installed but executable is not yet visible. Close this runner, open a new window and rerun OpenTail-Bench.bat."
  }
 }
 $script:ghExe=$found
 $version=@(& $script:ghExe --version 2>&1)
 if($LASTEXITCODE -ne 0){throw "GitHub CLI is present but could not start. Repair GitHub CLI installation."}
 $authorized=$false
 try {
  & $script:ghExe auth status --hostname github.com 1>$null 2>$null
  $authorized=($LASTEXITCODE -eq 0)
 } catch {
  # gh returns a nonzero status while signed out; this is a setup prompt,
  # not a fatal PowerShell NativeCommandError.
  $authorized=$false
 }
 if(-not $authorized){
  Write-Host "A one-time GitHub browser sign-in is required. No account tokens will be saved in bench logs."
  $choice=Read-Host "Open GitHub CLI authorization now? Type LOGIN"
  if($choice -cne "LOGIN"){
   throw "SETUP DECLINED: GitHub CLI sign-in not authorized. Run gh auth login or rerun the launcher."
  }
  & $script:ghExe auth login --hostname github.com --web --git-protocol https
  if($LASTEXITCODE -ne 0){throw "GitHub browser sign-in did not finish. Rerun the launcher after signing in."}
  $authorized=$false
  try {
   & $script:ghExe auth status --hostname github.com 1>$null 2>$null
   $authorized=($LASTEXITCODE -eq 0)
  } catch {$authorized=$false}
  if(-not $authorized){throw "GitHub CLI is still not authorized for github.com."}
 }
 Step "GitHub setup" "PASS" "Official GitHub CLI found and user authorization verified"
}
function Api([string]$path) {
 $data=@(& $script:ghExe api $path 2>&1)
 if($LASTEXITCODE -ne 0){throw "GitHub API failed for $path : $($data -join ' ')"}
 return (($data -join [Environment]::NewLine) | ConvertFrom-Json)
}
function Head([int]$number) {
 $p=Api "repos/$repo/pulls/$number"
 Need ($p.draft -eq $true -and $p.merged -eq $false) "PR #$number is not draft/unmerged"
 return [string]$p.head.sha
}
function GoodRun($runs,[string]$name) {
 $items=@($runs.workflow_runs | Where-Object {$_.name -eq $name -and $_.status -eq "completed" -and $_.conclusion -eq "success"} | Sort-Object created_at -Descending)
 Need ($items.Count -ge 1) "Missing successful exact-head workflow: $name"
 return $items[0]
}
function Artifact($run,[string]$pattern,[string]$kind,[string]$sha) {
 # CI must have built this *exact* PR head; cache is keyed to artifact ID too.
 Need ([string]$run.head_sha -eq $sha) "Artifact workflow does not match expected immutable source SHA"
 $manifest=Api "repos/$repo/actions/runs/$($run.id)/artifacts?per_page=100"
 $matches=@($manifest.artifacts | Where-Object {$_.expired -eq $false -and $_.name -match $pattern})
 Need ($matches.Count -eq 1) "Exactly one unexpired matching artifact required in run $($run.id): $pattern"
 $entry=$matches[0]
 $directory=Join-Path $root ("bench-cache\"+$kind+"\"+$sha+"-"+$entry.id)
 $marker=Join-Path $directory "cache.json"
 $valid=$false
 if(Test-Path $marker -PathType Leaf) {
  try {
   $saved=Get-Content $marker -Raw | ConvertFrom-Json
   $valid=([string]$saved.sha -ceq $sha -and [string]$saved.artifact_id -ceq [string]$entry.id -and @($saved.files).Count -gt 0)
   if($valid) {
    foreach($file in @($saved.files)) {
     $local=Join-Path $directory ([string]$file.relative)
     if(-not (Test-Path -LiteralPath $local -PathType Leaf) -or
        (Get-FileHash -LiteralPath $local -Algorithm SHA256).Hash -ne [string]$file.sha256){$valid=$false;break}
    }
   }
  } catch {$valid=$false}
 }
 if(-not $valid) {
  if(Test-Path $directory){Remove-Item -LiteralPath $directory -Recurse -Force}
  New-Item -Path $directory -ItemType Directory -Force | Out-Null
  $msg=@(& $script:ghExe run download ([string]$run.id) -R $repo -n ([string]$entry.name) -D $directory 2>&1)
  Need ($LASTEXITCODE -eq 0) "gh run download failed: $($msg -join ' ')"
  # Never leave an executable factory-reset utility in the reusable cache.
  if($kind -eq 'firmware'){
   Get-ChildItem $directory -Recurse -File |
     Where-Object {$_.Name -in @('03_FACTORY_RESET_COM9.bat','factory_reset.py')} |
     Remove-Item -Force
  }
  $checks=@(Get-ChildItem -LiteralPath $directory -Recurse -File |
    ForEach-Object {
     [pscustomobject]@{relative=$_.FullName.Substring($directory.Length+1);
                       sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
  Need ($checks.Count -gt 0) "Downloaded artifact was empty"
  [ordered]@{sha=$sha;artifact_id=[string]$entry.id;files=$checks} |
     ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $marker -Encoding UTF8
  Step ("Artifact "+$kind) "PASS" "Downloaded exact-SHA artifact and cached verified file checksums"
 } else {
  Step ("Artifact "+$kind) "PASS" "Reused checksum-verified exact-SHA cache; no download"
 }
 return [pscustomobject]@{id=$entry.id;name=$entry.name;run=$run.id;folder=$directory;
    url="https://github.com/$repo/actions/runs/$($run.id)/artifacts/$($entry.id)"}
}
function HasGitMetadata([string]$folder) {
 return [bool](Test-Path -LiteralPath (Join-Path $folder ".git"))
}
function AssertZipCheckoutSelfTest {
 $folder=Join-Path $session "temporary-zip-check"
 New-Item -ItemType Directory -Path $folder -Force | Out-Null
 try {
  Need (-not (HasGitMetadata $folder)) "ZIP checkout must not be treated as a Git checkout"
  New-Item -ItemType Directory -Path (Join-Path $folder ".git") -Force | Out-Null
  Need (HasGitMetadata $folder) "Git metadata detection regression"
 } finally {
  Remove-Item -LiteralPath $folder -Recurse -Force -ErrorAction SilentlyContinue
 }
}
function LocalRegressions([string]$expectedSha) {
 # Source ZIPs do not contain .git. This is an optional check; do not invoke
 # git and fail the hardware session after successful authoritative CI.
 if(-not (HasGitMetadata $root)) {
  Step "Local source regressions" "SKIPPED" "Downloaded ZIP has no .git metadata; exact-head GitHub CI remains authoritative"
  return
 }
 if($null -eq (Get-Command git -ErrorAction SilentlyContinue) -or $null -eq (Get-Command node -ErrorAction SilentlyContinue)) {
  Step "Local source regressions" "SKIPPED" "Git or Node missing; exact-head GitHub CI remains authoritative"
  return
 }
 $diskHead=""
 try {
  $diskHeadLines=@(& git -C $root rev-parse HEAD 2>$null)
  if($LASTEXITCODE -eq 0){$diskHead=($diskHeadLines -join "").Trim()}
 } catch {
  # PowerShell 5.1 with ErrorActionPreference=Stop can throw on git stderr.
  Step "Local source regressions" "SKIPPED" "Local Git HEAD could not be read; exact-head GitHub CI remains authoritative"
  return
 }
 if($diskHead -ne $expectedSha) {
  Step "Local source regressions" "SKIPPED" "Current checkout is not Android PR #111 exact HEAD; no branch switched"
  return
 }
 foreach($file in @("android/qa/test_native_source.cjs","android/qa/test_live_edit.cjs")) {
  $full=Join-Path $root $file
  Need (Test-Path $full) "Missing source regression $file"
  $output=@(& node $full 2>&1)
  $output | Set-Content (Join-Path $session ("node-"+[System.IO.Path]::GetFileNameWithoutExtension($file)+".log")) -Encoding UTF8
  Need ($LASTEXITCODE -eq 0) "Local regression failed: $file; inspect node log"
 }
 Step "Local source regressions" "PASS" "Exact checked-out Android SHA: native-source BLE, live geometry and Save/static regressions"
}
# Android Studio often installs adb under the user SDK without updating PATH.
function EnsureAdb {
 $adb=Get-Command adb.exe -ErrorAction SilentlyContinue
 if($null -eq $adb) {
  $roots=@(
   [Environment]::GetEnvironmentVariable("ANDROID_HOME"),
   [Environment]::GetEnvironmentVariable("ANDROID_SDK_ROOT"),
   (Join-Path $env:LOCALAPPDATA "Android\Sdk")
  )
  foreach($sdk in $roots){
   if([string]::IsNullOrWhiteSpace($sdk)){continue}
   $exe=Join-Path $sdk "platform-tools\adb.exe"
   if(Test-Path $exe -PathType Leaf){
    $env:Path=(Split-Path $exe -Parent)+";"+$env:Path
    $adb=Get-Command adb.exe -ErrorAction SilentlyContinue
    if($null -ne $adb){break}
   }
  }
 }
 if($null -eq $adb){
  throw "SETUP REQUIRED: Android ADB not found. Install Android SDK Platform Tools from https://developer.android.com/tools/releases/platform-tools, then rerun the launcher."
 }
 Step "ADB setup" "PASS" "Android Platform Tools found"
}
# Pure parser used by the Windows offline self-test; never runs ADB by itself.
function ParseAdbList([string[]]$lines) {
 $result=@()
 foreach($line in $lines) {
  $match=[regex]::Match([string]$line,'^\s*(\S+)\s+(device|unauthorized|offline|authorizing|recovery|sideload)(?:\s|$)')
  if(-not $match.Success){continue}
  $serial=[string]$match.Groups[1].Value
  $status=[string]$match.Groups[2].Value
  # An emulator and an ADB-over-TCP session are not the requested USB phone.
  $usb=($serial -notmatch '^(emulator-\d+|(?:[0-9]{1,3}\.){3}[0-9]{1,3}:\d+|localhost:\d+|127\.0\.0\.1:\d+)$') -and
       ($serial -notmatch '^.+:\d+$')
  $result+=,[pscustomobject]@{serial=$serial;status=$status;usb=$usb}
 }
 return @($result)
}
# ADB daemon startup may return a null ExitCode from Start-Process on PS5.1.
# Require the canonical ADB header; explicitly reject any nonzero code.
function AssertAdbDiscovery([object]$exitCode,[string[]]$lines) {
 $validHeader=@($lines | Where-Object {$_ -match '^\s*List of devices attached\s*$'}).Count -ge 1
 Need $validHeader "ADB device list missing its expected header"
 if($null -ne $exitCode) {
  Need ([int]$exitCode -eq 0) ("ADB device discovery exit code: "+$exitCode)
 }
}
function AssertAdbParserSelfTest {
 $none=@(ParseAdbList @("List of devices attached","* daemon started successfully"))
 $unauthorized=@(ParseAdbList @("List of devices attached","R5TEST001 unauthorized usb:1-1 transport_id:1"))
 $authorized=@(ParseAdbList @("List of devices attached","R5TEST001 device product:test model:Phone transport_id:2"))
 $mixed=@(ParseAdbList @("List of devices attached","emulator-5554 device product:sdk","192.168.1.5:5555 device product:wireless","R5TEST001 device usb:1-1"))
 $multiple=@(ParseAdbList @("List of devices attached","A1 device usb:1","B2 device usb:2"))
 # Real ADB output with null ExitCode must be accepted, but not other errors.
 AssertAdbDiscovery -exitCode $null -lines @("List of devices attached","R5TEST001 device product:test")
 AssertAdbDiscovery -exitCode 0 -lines @("List of devices attached")
 $bad=$false
 try {AssertAdbDiscovery -exitCode $null -lines @("unexpected output")} catch {$bad=$true}
 Need $bad "ADB null exit code without header must fail"
 $bad=$false
 try {AssertAdbDiscovery -exitCode 1 -lines @("List of devices attached")} catch {$bad=$true}
 Need $bad "ADB nonzero exit code must fail"
 Need ($none.Count -eq 0) "ADB parser no-device regression"
 Need ($unauthorized.Count -eq 1 -and $unauthorized[0].status -eq "unauthorized") "ADB parser authorization regression"
 Need ($authorized.Count -eq 1 -and $authorized[0].usb -and $authorized[0].status -eq "device") "ADB parser single USB phone regression"
 Need (@($mixed | Where-Object {$_.usb -and $_.status -eq "device"}).Count -eq 1) "ADB parser must ignore emulators and wireless transports"
 Need (@($multiple | Where-Object {$_.usb -and $_.status -eq "device"}).Count -eq 2) "ADB parser multiple phones regression"
}
function Phone([string]$specified) {
 # ADB may start its daemon on the first query. Allow the owner to unlock,
 # enable debugging and approve the RSA prompt without restarting the bench.
 for($attempt=1;$attempt -le 3;$attempt++) {
  # Windows PowerShell 5.1 may promote ADB's harmless daemon-start
  # stderr banner to a terminating NativeCommandError under Stop.
  # Capture stdout/stderr separately and check the actual process exit code.
  $adbStdout=Join-Path $session "adb-devices.stdout.txt"
  $adbStderr=Join-Path $session "adb-devices.stderr.txt"
  $adbExe=(Get-Command adb.exe -ErrorAction Stop).Source
  Write-Host "Checking connected Android USB devices..."
  # Do NOT use Start-Process -Wait: Windows may wait for ADB's daemon
  # child process as well as the short-lived 'adb devices' client.
  # Wait only for the client PID, with a bounded timeout.
  $adbRun=Start-Process -FilePath $adbExe -ArgumentList @("devices","-l") -PassThru -NoNewWindow -RedirectStandardOutput $adbStdout -RedirectStandardError $adbStderr
  if(-not $adbRun.WaitForExit(15000)) {
   try {$adbRun.Kill()} catch {}
   throw "ADB device detection timed out after 15 seconds. Close other ADB sessions or run 'adb kill-server' in a separate PowerShell window, then retry."
  }
  $adbRun.Refresh()
  $out=@(Get-Content $adbStdout -ErrorAction SilentlyContinue)
  $err=@(Get-Content $adbStderr -ErrorAction SilentlyContinue)
  try {
   AssertAdbDiscovery -exitCode $adbRun.ExitCode -lines $out
  } catch {
   throw ("ADB device discovery failed: "+$_.Exception.Message+"; stderr: "+($err -join " ")+"; stdout: "+($out -join " "))
  }
  $found=@(ParseAdbList $out)
  $ready=@($found | Where-Object {$_.usb -and $_.status -eq "device"})
  $waiting=@($found | Where-Object {$_.usb -and $_.status -ne "device"})
  if($specified) {
   $chosen=@($ready | Where-Object {$_.serial -eq $specified})
   if($chosen.Count -eq 1){return $specified}
   $diagnosis=if(@($waiting | Where-Object {$_.serial -eq $specified}).Count -gt 0){"Device $specified is connected but USB debugging is not authorized/online"}else{"USB Android device $specified is not detected"}
  } elseif($ready.Count -eq 1) {
   Write-Host ("Authorized USB Android phone detected: "+$ready[0].serial)
   return [string]$ready[0].serial
  } elseif($ready.Count -gt 1) {
   $diagnosis="Multiple authorized USB Android devices: "+(($ready | ForEach-Object {$_.serial}) -join ", ")
  } elseif($waiting.Count -gt 0) {
   $diagnosis="USB Android detected but not authorized/online: "+(($waiting | ForEach-Object {$_.serial+" ("+$_.status+")"}) -join ", ")
  } else {
   $diagnosis="No USB Android phone detected by ADB"
  }
  Write-Warning $diagnosis
  Write-Host "On your PHONE: unlock it, enable Developer options > USB debugging, and approve 'Allow USB debugging?' (RSA fingerprint)."
  Write-Host "Check that the USB-C cable supports DATA and try File transfer / Android Auto in USB preferences if the PC doesn't see it."
  Write-Host "Do not change Bluetooth pairing, firmware, or NVS; these steps only enable USB diagnostics."
  if($ready.Count -gt 1){Write-Host "Unplug extra phones or run PowerShell with -AndroidSerial SERIAL to select one."}
  if($attempt -lt 3) {
   $answer=Read-Host "After checking the phone, press ENTER to retry ADB (or Q to stop)"
   if($answer -match '^(?i:q|quit|exit)$'){throw "ADB USB setup stopped by owner: $diagnosis"}
  } else {
   throw "ADB USB setup incomplete: $diagnosis. Run 'adb devices -l' in PowerShell to inspect the phone state."
  }
 }
 throw "ADB USB device detection failed"
}
# Keep COM candidates in an array even when exactly one PnP device matches.
# PowerShell 5.1 scalarizes "if" output; a PSCustomObject has no .Count with
# Set-StrictMode -Version Latest. Selection is pure for no-hardware tests.
function SelectEspPort([object[]]$candidates) {
 $found=@($candidates | Where-Object {$null -ne $_})
 $likely=@($found | Where-Object {($_.id+" "+$_.label) -match 'VID_303A|ESP32|USB.JTAG|CP210|CH340|FTDI|USB.Serial'})
 $espSpecific=@($likely | Where-Object {($_.id+" "+$_.label) -match 'VID_303A|ESP32|USB.JTAG'})
 $pick=@(if($espSpecific.Count -eq 1){$espSpecific}else{$likely})
 if($pick.Count -eq 0 -and $found.Count -eq 1){$pick=@($found)}
 Need ($pick.Count -eq 1) ("Ambiguous ESP serial port: "+(($found | ForEach-Object {$_.port+" "+$_.label}) -join "; ")+"; use -EspPort COM9")
 return [string]$pick[0].port
}
function AssertEspSelectorSelfTest {
 $native=[pscustomobject]@{port="COM9";label="USB JTAG Serial (COM9)";id="USB\\VID_303A&PID_1001"}
 $other=[pscustomobject]@{port="COM3";label="Communications Port (COM3)";id="ACPI\\PNP0501"}
 $generic=[pscustomobject]@{port="COM7";label="USB-SERIAL CH340 (COM7)";id="USB\\VID_1A86"}
 Need ((SelectEspPort -candidates @($native)) -eq "COM9") "Single ESP port must not scalarize"
 Need ((SelectEspPort -candidates @($native,$generic)) -eq "COM9") "Prefer the single native ESP32 port"
 Need ((SelectEspPort -candidates @($other)) -eq "COM3") "Single COM fallback regression"
 Need ((SelectEspPort -candidates @($generic)) -eq "COM7") "Single adapter COM regression"
 $ambiguous=$false
 try { $null=SelectEspPort -candidates @($native,[pscustomobject]@{port="COM10";label="ESP32 USB JTAG (COM10)";id="USB\\VID_303A"}) }
 catch { $ambiguous=$_.Exception.Message -match 'Ambiguous ESP serial port' }
 Need $ambiguous "Multiple ESP ports must fail closed"
 $missing=$false
 try { $null=SelectEspPort -candidates @() }
 catch { $missing=$_.Exception.Message -match 'Ambiguous ESP serial port' }
 Need $missing "No COM ports must fail closed"
}
function EspCom([string]$specified) {
 if($specified){
  Need ($specified -match '^COM[0-9]{1,3}$') "ESP port must be COM-number"
  return $specified.ToUpperInvariant()
 }
 $found=@(Get-CimInstance Win32_PnPEntity | Where-Object {[string]$_.Name -match '\(COM[0-9]+\)'} | ForEach-Object {
  [pscustomobject]@{port=([regex]::Match([string]$_.Name,'\((COM[0-9]+)\)')).Groups[1].Value;
    label=[string]$_.Name;id=[string]$_.PNPDeviceID}
 })
 return SelectEspPort -candidates $found
}
function FirmwarePackage([string]$folder,[string]$sha) {
 $mf=Get-ChildItem -Path $folder -Filter release_manifest.json -File -Recurse | Select-Object -First 1
 Need ($null -ne $mf) "Firmware artifact missing manifest"
 $m=Get-Content -Raw $mf.FullName | ConvertFrom-Json
 Need ($m.git_sha -eq $sha -and $m.chip -eq "ESP32-S3 N16R8") "Firmware SHA/board mismatch"
 Need ($m.erase_nvs -eq $false -and $m.nvs_partition -eq "0x9000") "Unsafe NVS manifest"
 $base=$mf.DirectoryName;$image=Join-Path $base "opentail_core_alpha.bin"
 Need (Test-Path $image) "Firmware app binary absent"
 $flashArgs=Get-Content -Raw (Join-Path $base "flasher_args.json") | ConvertFrom-Json
 Need ($flashArgs.flash_files.'0x20000' -eq "opentail_core_alpha.bin") "Unsafe app partition offset"
 $sums=Get-Content (Join-Path $base "SHA256SUMS.txt")
 foreach($file in @("release_manifest.json","flasher_args.json","opentail_core_alpha.bin")) {
   $hits=@($sums | Where-Object {$_ -match ('\s+'+[regex]::Escape($file)+'$')})
   Need ($hits.Count -eq 1) "Missing SHA256SUMS entry for $file"
   $expected=([regex]::Match($hits[0],'^[a-fA-F0-9]{64}')).Value
   $actual=(Get-FileHash -Algorithm SHA256 (Join-Path $base $file)).Hash
   Need ($expected -eq $actual) "SHA256 mismatch for $file"
 }
 return [pscustomobject]@{image=$image;sha=(Get-FileHash -Algorithm SHA256 $image).Hash}
}
function ConfirmedFlash([string]$image,[string]$port,[string]$sha) {
 Write-Warning "Optional flash changes app partition 0x20000 ONLY; NVS never erased."
 Write-Host "Firmware $sha to ESP32-S3 $port"
 $answer=Read-Host "To confirm type exactly FLASH $port"
 Need ($answer -ceq "FLASH $port") "Flash cancelled without writing"
 Tool "py"
 $version=@(& py -3 -m esptool version 2>&1)
 Need ($LASTEXITCODE -eq 0) "Install esptool: py -3 -m pip install esptool"
 $esptoolArguments=@("-3","-m","esptool","--chip","esp32s3","--port",$port,"--baud","115200","write_flash","--flash_mode","dio","--flash_freq","80m","--flash_size","16MB","0x20000",$image)
 & py @esptoolArguments 2>&1 | Tee-Object -FilePath (Join-Path $session "flash.log")
 Need ($LASTEXITCODE -eq 0) "Flashing failed; no erase command was used"
}
function Raw([string]$name) {if(Test-Path $name){return [string](Get-Content $name -Raw -ErrorAction SilentlyContinue)};return ""}
function Invoke-AdbCapture([string[]]$arguments,[string]$label,[int]$timeoutMs=20000) {
 $stdout=Join-Path $session ($label+".stdout.txt")
 $stderr=Join-Path $session ($label+".stderr.txt")
 $adbExe=[string](Get-Command adb.exe -ErrorAction Stop).Source
 $p=Start-Process -FilePath $adbExe -ArgumentList $arguments -NoNewWindow -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 if(-not $p.WaitForExit($timeoutMs)){
  try{$p.Kill()}catch{}
  throw "ADB command timed out ($label); inspect its capture files"
 }
 $p.Refresh()
 return [pscustomobject]@{exit=$p.ExitCode;
   output=((Raw $stdout)+[Environment]::NewLine+(Raw $stderr)).Trim()}
}
function Get-AndroidPackageState([string]$phone) {
 $r=Invoke-AdbCapture @("-s",$phone,"shell","dumpsys","package","com.opentail.android") "adb-package"
 if($r.exit -ne 0){return [pscustomobject]@{exists=$false;lastUpdate=""}}
 $present=([regex]::IsMatch($r.output,'(?m)^\s*Package \[com\.opentail\.android\]'))
 $m=[regex]::Match($r.output,'(?m)^\s*lastUpdateTime=([^\r\n]+)')
 return [pscustomobject]@{exists=$present;lastUpdate=$(if($m.Success){$m.Groups[1].Value.Trim()}else{""})}
}
function Install-VerifiedAndroid([string]$phone,[string]$apk,[string]$sha) {
 $package=Get-AndroidPackageState $phone
 $hash=(Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash
 $statePath=Join-Path $root "bench-cache\android-install.json"
 if((Test-Path $statePath) -and $package.exists -and $package.lastUpdate) {
  try {
   $previous=Get-Content $statePath -Raw | ConvertFrom-Json
   if($previous.phone -ceq $phone -and $previous.apk_sha256 -ceq $hash -and
      $previous.source_sha -ceq $sha -and $previous.last_update -ceq $package.lastUpdate) {
    Step "Android install" "PASS" "Verified same APK remains installed on same phone; skipped reinstall"
    return
   }
  }catch{}
 }
 $result=Invoke-AdbCapture @("-s",$phone,"install","-r",('"'+$apk+'"')) "adb-install" 120000
 if($result.output -match '(?i)INSTALL_FAILED_UPDATE_INCOMPATIBLE') {
  # Previous runner silently erased owner credentials. Never repeat that.
  Write-Warning "Existing OpenTail Android signing key differs. Uninstall deletes phone app data, saved owner key, layouts/projects stored only on phone."
  $answer=Read-Host "Type exactly ERASE APP DATA to permit uninstall/reinstall, or press ENTER to keep everything"
  if($answer -cne "ERASE APP DATA") {
   $script:installBlocked=$true
   Step "Android install" "BLOCKED" "Signing mismatch; owner did not authorize deleting app data; existing app untouched"
   throw "Android APK update blocked by signing mismatch; no uninstall attempted"
  }
  $gone=Invoke-AdbCapture @("-s",$phone,"uninstall","com.opentail.android") "adb-uninstall" 120000
  Need ($gone.exit -eq 0 -and $gone.output -match '(?m)^Success\s*$') ("Approved Android uninstall failed: "+$gone.output)
  $result=Invoke-AdbCapture @("-s",$phone,"install",('"'+$apk+'"')) "adb-install-replacement" 120000
  Need ($result.exit -eq 0 -and $result.output -match '(?m)^Success\s*$') ("Approved app uninstall succeeded but reinstall FAILED: "+$result.output)
  Step "Android install" "PASS" "Owner explicitly approved reinstall; phone app data and owner key erased, controller NVS unchanged"
 } else {
  Need ($result.exit -eq 0 -and $result.output -match '(?m)^Success\s*$') ("Android install failed without signing mismatch; existing app untouched: "+$result.output)
  Step "Android install" "PASS" "APK installed/updated, app data preserved"
 }
 $new=Get-AndroidPackageState $phone
 if($new.exists -and $new.lastUpdate) {
  $parent=Split-Path $statePath -Parent
  New-Item -Path $parent -ItemType Directory -Force | Out-Null
  [ordered]@{phone=$phone;apk_sha256=$hash;source_sha=$sha;last_update=$new.lastUpdate} |
      ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $statePath -Encoding UTF8
 }else{$notes.Add("Installed package fingerprint unavailable; next session will verify again")}
}
function Read-PhoneClock([string]$phone) {
 $before=(Get-Date).ToUniversalTime()
 try {
  $epoch=Invoke-AdbCapture @("-s",$phone,"shell","date","+%s") "adb-phone-epoch"
  $zone=Invoke-AdbCapture @("-s",$phone,"shell","date","+%z") "adb-phone-zone"
  $after=(Get-Date).ToUniversalTime()
  if($epoch.exit -eq 0 -and $zone.exit -eq 0) {
   return Get-ClockCorrelation (($epoch.output.Trim())+" "+($zone.output.Trim())) $before $after
  }
 }catch{}
 return [ordered]@{available=$false;phone_skew_seconds=$null;phone_utc_offset=$null;uncertainty_ms=$null}
}
function OwnerLogStage([string]$body) {
 # Non-secret, app-owned BLE diagnostic events; never evidence of optical output.
 $found=@([regex]::Matches($body,'(?m)^[^\r\n]*OpenTailOwner:\s*([^\r\n]+)') |
   ForEach-Object {$_.Groups[1].Value.Trim()})
 if($found.Count -eq 0){return "No OpenTailOwner handshake stage captured"}
 $last=[string]$found[$found.Count-1]
 return $last.Substring(0,[Math]::Min($last.Length,180))
}
function Evidence {
 $a=Raw $logcat;$b=Raw $serial
 $script:benchStages=Get-BenchStages $a $b $Mode $script:sessionStarted
 $script:installedIdentity=Get-InstalledFirmwareIdentity $b $a
 $script:correlated=@(Get-CorrelatedEvents $a (Raw $timeline) $clockInfo 40)
 if(-not $script:sessionStarted) {
  Step "BLE runtime" "NOT RUN" "Session stopped before physical capture"
  Step "Pixel Probe" "NOT RUN" "Not exercised"
  Step "Save runtime" "NOT RUN" "Not exercised"
  Step "Playback runtime" "NOT RUN" "Not exercised"
  Step "Independent RMT test" "UNAVAILABLE" "No verified read-only serial RMT diagnostic; BLE Pixel Probe requires owner action"
  return
 }
 Step "BLE runtime" $benchStages.gatt_connection.status $benchStages.gatt_connection.evidence
 Step "Owner authorization" $benchStages.owner_authorization.status $benchStages.owner_authorization.evidence
 Step "Pixel Probe" $benchStages.pixel_probe.status "Firmware receipt only; physical illumination requires human verification"
 Step "Save runtime" $benchStages.save.status $benchStages.save.evidence
 Step "Playback runtime" $benchStages.command.status "BLE command acknowledgment only; physical output not certified"
 Step "Independent RMT test" "UNAVAILABLE" "No verified independent USB RMT command"
}
function Report {
 Evidence
 $stages=$script:benchStages
 $observedFailures=@($stages.GetEnumerator() | Where-Object {$_.Value.status -eq 'FAIL'})
 $overall='BLOCKED'
 if($SelfTest){$overall='PASS'}
 elseif($failed -and -not $installBlocked){$overall='FAIL'}
 elseif($observedFailures.Count -gt 0){$overall='FAIL'}
 elseif($installBlocked){$overall='BLOCKED'}
 elseif($sessionStarted) {
  $goal=$(switch($Mode){'bluetooth'{'owner_authorization'};'layout'{'layout'};'save'{'save'};'playback'{'command'}})
  if($stages[$goal].status -eq 'PASS'){$overall='PASS'}
 }
 $summary=[ordered]@{schema_version=1;date=$stamp;mode=$Mode;status=$overall;
   capture_requested_seconds=$CaptureSeconds;capture_elapsed_seconds=$captureElapsed;
   early_exit_cause=$(if($earlyFailure){$earlyFailure}else{$null});
   android_sha=$androidSha;downloaded_firmware_sha=$firmwareSha;
   installed_firmware=$installedIdentity;phone_adb=$phone;esp_usb=$esp;
   clock_correlation=$clockInfo;stages=$stages;checks=$steps;ci=$ci;
   artifacts=$artifacts;events=$correlated;notes=@($notes)}
 $summary | ConvertTo-Json -Depth 15 | Set-Content (Join-Path $session "RESULTS.json") -Encoding UTF8
 # Retain the old filename for existing no-hardware smoke tests and consumers.
 $summary | ConvertTo-Json -Depth 15 | Set-Content (Join-Path $session "report.json") -Encoding UTF8
 $lines=New-Object System.Collections.Generic.List[string]
 $lines.Add("# OpenTail bench - $Mode - $overall")
 $lines.Add("Requested: $CaptureSeconds s; captured: $captureElapsed s; stop reason: "+$(if($earlyFailure){$earlyFailure}else{'normal/none'}))
 $lines.Add("Android source: "+$androidSha)
 $lines.Add("Downloaded firmware source (NOT confirmed installed): "+$firmwareSha)
 $installedVersion=$(if($installedIdentity.version){$installedIdentity.version}else{'UNAVAILABLE (no physical version banner)'})
 $lines.Add("Installed firmware version: "+$installedVersion+"; source: "+$installedIdentity.source)
 $lines.Add("Installed firmware Git SHA: "+$(if($installedIdentity.installed_git_sha){$installedIdentity.installed_git_sha}else{'NOT OBSERVED'}))
 $lines.Add("")
 $lines.Add("| Stage | Result | Evidence |")
 $lines.Add("|---|---|---|")
 foreach($name in $stages.Keys) {
  $value=$stages[$name]
  $lines.Add("| "+$name+" | "+$value.status+" | "+([string]$value.evidence).Replace('|','/')+" |")
 }
 $lines.Add("")
 $lines.Add("## Capture and integrity")
 foreach($name in $steps.Keys) {
  $v=$steps[$name]
  $lines.Add("- "+$name+": "+$v.state+" - "+$v.detail)
 }
 $lines.Add("- Android/COM9 alignment: "+$(if($clockInfo.available){"Phone clock skew $($clockInfo.phone_skew_seconds) seconds; phone offset $($clockInfo.phone_utc_offset), uncertainty at least $($clockInfo.uncertainty_ms) ms; ESP UTC from receive-time stamps"}else{"Phone clock unavailable; do not claim aligned Android UTC"}))
 $lines.Add("- Installed firmware SHA is never inferred from the downloaded artifact.")
 $lines.Add("- Physical LED illumination and saved-scene behavior require owner observation.")
 $lines.Add("- Files: RESULTS.json, android-logcat.txt, esp32-serial.txt, esp32-timeline.txt (when capture ran)")
 foreach($n in $notes){$lines.Add("- "+$n)}
 $lines | Set-Content (Join-Path $session "REPORT.md") -Encoding UTF8
 Write-Host ("Report: "+(Join-Path $session "REPORT.md"))
 Write-Host ("Structured: "+(Join-Path $session "RESULTS.json"))
}
$stamp=Get-Date -Format "yyyy-MM-dd HH:mm:ss"
try {
 if($SelfTest) {
  Need (-not $FlashFirmware -and -not $DirectLedProbe) "SelfTest cannot send hardware actions"
  Need ([string]$repo -eq "Srryastari/OPENTAIL-development") "Wrong repo"
  AssertZipCheckoutSelfTest
  Step "ZIP checkout self-test" "PASS" "Missing Git metadata safely bypasses optional local source checks"
  Need ((OwnerLogStage ("I OpenTailOwner: bonding`nI OpenTailOwner: Controller accepted owner HMAC")) -eq "Controller accepted owner HMAC") "Owner log stage selection failed"
  Need ((OwnerLogStage "I unrelated: no owner") -eq "No OpenTailOwner handshake stage captured") "Owner absent stage selection failed"
  Step "Owner log parser self-test" "PASS" "Latest sanitized app owner stage; not physical acceptance"
  AssertBenchDiagnosticsSelfTest
  Step "Stage classifier self-test" "PASS" "GATT2, bond, auth, Save, renderer, RMT, installed firmware and UTC timeline verified without hardware"
  Need ($CaptureSeconds -ge 30 -and $Mode -in @('bluetooth','layout','save','playback')) "FAST and targeted session parameter regression"
  AssertAdbParserSelfTest
  Step "ADB device parser self-test" "PASS" "No USB device / unauthorized / authorized / emulator / multiple device cases"
  AssertEspSelectorSelfTest
  Step "ESP COM selector self-test" "PASS" "Zero, one, multiple native/generic COM ports; strict-mode arrays"
  # Prevent reintroduction of the old unconditional Firmware #127 gate.
  $contract=Get-Content -LiteralPath $PSCommandPath -Raw
  Need ($contract.Contains('if($FlashFirmware){$firmwareSha=Head 127}')) "Firmware PR lookup must be flash-only"
  Need ($contract.Contains('if($FlashFirmware){'+"\n"+'   $fwRuns=') -or
        $contract.Contains('if($FlashFirmware){'+"\r\n"+'   $fwRuns=')) "Firmware CI must be flash-only"
  Need ($contract.Contains('($FlashFirmware -and (Head 127) -ne $firmwareSha)')) "Playback must not re-query Firmware #127"
  Step "Firmware-gate self-test" "PASS" "Playback skips firmware PR, CI and artifact lookup; no flash"
  Step "Runner self-test" "PASS" "No USB or Bluetooth commands, firmware write or NVS reset"
 } else {
  EnsureGh
  EnsureAdb
  $phone=Phone $AndroidSerial;$esp=EspCom $EspPort
  Step "USB" "PASS" "Authorized phone $phone; ESP logging $esp"
  # Explicit flash is the ONLY mode that depends on firmware PR #127 CI.
  # A playback session uses the real installed controller, with no firmware
  # PR query, release lookup, download, erase, or write.
  if($FlashFirmware -and $Mode -eq 'playback'){
   throw "Playback is read-only. Use OpenTail-Bench.bat flash separately if you intentionally want to flash firmware."
  }
  $androidSha=Head 111
  $firmwareSha=""
  if($FlashFirmware){$firmwareSha=Head 127}
  $androidRuns=Api "repos/$repo/actions/runs?head_sha=$androidSha&per_page=100"
  $ai=GoodRun $androidRuns "Android exact-head software integration"
  $jni=GoodRun $androidRuns "Issue103 reconciled Effects/Firmware/Android exact SHA (software only)"
  $ci["Android regression"]=$ai.id;$ci["JNI 140 effects"]=$jni.id
  $fi=$null
  if($FlashFirmware){
   $fwRuns=Api "repos/$repo/actions/runs?head_sha=$firmwareSha&per_page=100"
   $fi=GoodRun $fwRuns "Issue126 Panel/Zone exact-head CORE verification"
   $ci["Firmware regression"]=$fi.id
  }else{
   $notes.Add("Read-only playback uses installed firmware; no Firmware #127 PR query, CI gate, firmware download or flash")
   Step "Firmware CI" "SKIPPED" "Read-only $Mode: installed controller firmware only; Firmware #127 not required"
  }
  Step "CI software" "PASS" "Exact-head Android and JNI workflow passed; firmware CI only for explicit flash"
  LocalRegressions $androidSha
  # The exact-head Android workflow already gates native source, JNI,
  # JUnit and APK packaging. Use its own artifact rather than waiting for
  # unrelated firmware tasks in the broader source verification workflow.
  $jobs=Api "repos/$repo/actions/runs/$($ai.id)/jobs?per_page=100"
  Need (@($jobs.jobs | Where-Object {$_.name -eq "android-exact-head" -and $_.conclusion -eq "success"}).Count -eq 1) "Exact-head Android build job failed"
  $aa=Artifact $ai '^opentail-android-exact-head-debug-apk$' 'android' $androidSha
  $apks=@(Get-ChildItem $aa.folder -Recurse -Filter "*.apk" -File)
  Need ($apks.Count -eq 1) "Verified Android artifact must contain exactly one APK"
  $apk=$apks[0].FullName
  $artifacts["Android APK"]=$aa.url+" SHA256="+(Get-FileHash -Algorithm SHA256 $apk).Hash
  $firmware=$null
  if($FlashFirmware){
   $fa=Artifact $fi ('^opentail-easyflash-.+'+[regex]::Escape($firmwareSha)+'$') 'firmware' $firmwareSha
   $firmware=FirmwarePackage $fa.folder $firmwareSha
   $artifacts["Firmware app (optional flash only)"]=$fa.url+" SHA256="+$firmware.sha
  }
  Step "Verified artifacts" "PASS" "Exact Android SHA and cached file hashes verified; firmware binary downloaded only for explicit flash"
  Install-VerifiedAndroid $phone $apk $androidSha
  if($FlashFirmware) {
   ConfirmedFlash $firmware.image $esp $firmwareSha
   Step "Flash" "PASS" "Explicit app-only flash at 0x20000; no NVS erase"
  } else {Step "Flash" "SKIPPED" "Default: never flash"}
  $script:clockInfo=Read-PhoneClock $phone
  $adbProcess=Start-Process -FilePath (Get-Command adb).Source -ArgumentList @("-s",$phone,"logcat","-v","threadtime","-T","1") -PassThru -NoNewWindow -RedirectStandardOutput $logcat -RedirectStandardError (Join-Path $session "logcat.stderr.txt")
  $capture=Join-Path $PSScriptRoot "Serial-Capture.ps1"
  Need (Test-Path $capture) "Serial-Capture.ps1 missing"
  $captureArguments=@("-NoProfile","-ExecutionPolicy","Bypass","-File",('"'+$capture+'"'),
    "-Port",$esp,"-OutFile",('"'+$serial+'"'),"-Seconds","$CaptureSeconds",
    "-TimelineFile",('"'+$timeline+'"'),"-StopFile",('"'+$stopFile+'"'))
  $serialProcess=Start-Process -FilePath "powershell.exe" -ArgumentList $captureArguments -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $session "serial.stderr.txt")
  Start-Sleep -Seconds 2
  Need (-not $serialProcess.HasExited) "Serial capture exited early; inspect serial.stderr.txt"
  & adb -s $phone shell am start -n "com.opentail.android/.ComposeMainActivity" | Out-Null
  $script:sessionStarted=$true
  Step "Log capture" "CAPTURING" "$CaptureSeconds-second $Mode Android/COM9 read-only session (early exit on confirmed failures)"
  switch($Mode){
   'bluetooth' {Write-Host "PHONE: Connect, then check Controller access. Do not test lights yet."}
   'layout' {Write-Host "PHONE: Open Layout and inspect live mapping; Pixel Probe only if you explicitly choose."}
   'save' {Write-Host "PHONE: Test Save/Reload only when intentionally ready. No automatic writes."}
   'playback' {Write-Host "PHONE: Select Show Modes and test playback manually; STOP remains available."}
  }
  Write-Host "Runner never sends BLE commands, invokes Save or activates LEDs."
  if($DirectLedProbe){Write-Host "Independent RMT command unavailable; user-controlled BLE Pixel Probe only."}
  $watch=[System.Diagnostics.Stopwatch]::StartNew()
  while($watch.Elapsed.TotalSeconds -lt $CaptureSeconds){
   Start-Sleep -Milliseconds 750
   $at=@(Get-Content -LiteralPath $logcat -Tail 250 -ErrorAction SilentlyContinue) -join "`n"
   $bt=@(Get-Content -LiteralPath $serial -Tail 250 -ErrorAction SilentlyContinue) -join "`n"
   $reason=Get-BenchFailure $at $bt
   if($reason){
    $script:earlyFailure=$reason
    New-Item -Path $stopFile -ItemType File -Force | Out-Null
    Write-Host ("[FAIL] Early conclusive failure: "+$reason+"; finalizing logs now")
    break
   }
  }
  $script:captureElapsed=[int][Math]::Ceiling($watch.Elapsed.TotalSeconds)
  # Signal a clean serial END marker; no process kill, DTR, RTS or device command.
  if(-not (Test-Path $stopFile)){New-Item -Path $stopFile -ItemType File -Force | Out-Null}
  for($wait=0;$wait -lt 35;$wait++){
   if((Raw $serial).Contains("[OpenTail] USB SERIAL LOG END")){break}
   Start-Sleep -Milliseconds 200
  }
  # Start-Process -PassThru can expose a null ExitCode on Windows PS5.1.
  # Verify the receive-only serial logger's own START/END evidence instead of
  # treating a missing numeric exit code as a hardware/test failure.
  $serialBody=Raw $serial
  Need ($serialBody.Contains("[OpenTail] USB SERIAL LOG START")) "Serial logger did not write START marker; inspect serial.stderr.txt"
  Need (-not $serialBody.Contains("[OpenTail] USB SERIAL CAPTURE ERROR:")) "Serial logger recorded an error; inspect esp32-serial.txt and serial.stderr.txt"
  Need ($serialBody.Contains("[OpenTail] USB SERIAL LOG END")) "Serial logger did not complete; inspect esp32-serial.txt and serial.stderr.txt"
  if($serialProcess.HasExited -and $null -ne $serialProcess.ExitCode -and $serialProcess.ExitCode -ne 0) {
   throw "Serial capture exited with code $($serialProcess.ExitCode); inspect serial.stderr.txt"
  }
  Need ((Test-Path $logcat) -and ((Get-Item $logcat).Length -gt 0)) "Android logcat is missing or empty; inspect logcat.stderr.txt"
  if($adbProcess.HasExited -and $null -ne $adbProcess.ExitCode -and $adbProcess.ExitCode -ne 0) {
   throw "Android logcat exited with code $($adbProcess.ExitCode); inspect logcat.stderr.txt"
  }
  Step "Log capture" "PASS" "Android logcat and ESP serial START/END verified; captured $captureElapsed s"
  if((Head 111) -ne $androidSha -or ($FlashFirmware -and (Head 127) -ne $firmwareSha)) {
   $notes.Add("PR head changed during session; pinned artifacts may no longer be latest")
   Step "Head consistency" "WARN" "PR updated after artifact selection"
  } else {Step "Head consistency" "PASS" "Exact same PR heads after session"}
 }
} catch {
 $failed=$true
 Step "Runner error" $(if($installBlocked){"BLOCKED"}else{"FAIL"}) $_.Exception.Message
} finally {
 foreach($proc in @($adbProcess,$serialProcess)){
  if($null -ne $proc){try {if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue}}catch{}}
 }
 Start-Sleep -Milliseconds 400
 Report
}
if($failed){exit 1}
exit 0
