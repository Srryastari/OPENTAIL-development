# OpenTail: Windows USB log capture + authorized Android BLE bench session.
# NEVER talks to the controller over HTTP/Wi-Fi. No automatic BLE writes or NVS writes.
[CmdletBinding()]
param(
 [switch]$FlashFirmware,
 [switch]$DirectLedProbe,
 [switch]$SelfTest,
 [ValidateRange(30,1800)][int]$CaptureSeconds=120,
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
$adbProcess=$null;$serialProcess=$null
$sessionStarted=$false
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
 $status=@(& $script:ghExe auth status --hostname github.com 2>&1)
 if($LASTEXITCODE -ne 0){
  Write-Host "A one-time GitHub browser sign-in is required. No account tokens will be saved in bench logs."
  $choice=Read-Host "Open GitHub CLI authorization now? Type LOGIN"
  if($choice -cne "LOGIN"){
   throw "SETUP DECLINED: GitHub CLI sign-in not authorized. Run gh auth login or rerun the launcher."
  }
  & $script:ghExe auth login --hostname github.com --web --git-protocol https
  if($LASTEXITCODE -ne 0){throw "GitHub browser sign-in did not finish. Rerun the launcher after signing in."}
  $status=@(& $script:ghExe auth status --hostname github.com 2>&1)
  if($LASTEXITCODE -ne 0){throw "GitHub CLI is still not authorized for github.com."}
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
function Artifact($run,[string]$pattern,[string]$folder) {
 $manifest=Api "repos/$repo/actions/runs/$($run.id)/artifacts?per_page=100"
 $matches=@($manifest.artifacts | Where-Object {$_.expired -eq $false -and $_.name -match $pattern})
 Need ($matches.Count -eq 1) "Exactly one unexpired matching artifact required in run $($run.id): $pattern"
 New-Item -ItemType Directory -Path $folder -Force | Out-Null
 $msg=@(& $script:ghExe run download ([string]$run.id) -R $repo -n ([string]$matches[0].name) -D $folder 2>&1)
 Need ($LASTEXITCODE -eq 0) "gh run download failed: $($msg -join ' ')"
 return [pscustomobject]@{id=$matches[0].id;name=$matches[0].name;run=$run.id;url="https://github.com/$repo/actions/runs/$($run.id)/artifacts/$($matches[0].id)"}
}
function LocalRegressions([string]$expectedSha) {
 if($null -eq (Get-Command git -ErrorAction SilentlyContinue) -or $null -eq (Get-Command node -ErrorAction SilentlyContinue)) {
  Step "Local source regressions" "SKIPPED" "Git or Node missing; exact-head GitHub CI remains authoritative"
  return
 }
 $diskHead=@(& git -C $root rev-parse HEAD 2>$null)
 if($LASTEXITCODE -ne 0 -or (($diskHead -join "").Trim() -ne $expectedSha)) {
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
function Phone([string]$specified) {
 $out=@(& adb devices -l)
 Need ($LASTEXITCODE -eq 0) "ADB devices failed"
 $devices=@($out | Where-Object {$_ -match '^(\S+)\s+device(\s|$)'} | ForEach-Object {([regex]::Match($_,'^(\S+)')).Groups[1].Value})
 if($specified){Need ($devices -contains $specified) "Phone $specified not authorized/connected";return $specified}
 Need ($devices.Count -eq 1) "Connect and authorize exactly one USB Android phone or use -AndroidSerial"
 return $devices[0]
}
function EspCom([string]$specified) {
 if($specified){Need ($specified -match '^COM[0-9]{1,3}$') "ESP port must be COM-number";return $specified.ToUpperInvariant()}
 $found=@(Get-CimInstance Win32_PnPEntity | Where-Object {[string]$_.Name -match '\(COM[0-9]+\)'} | ForEach-Object {
   [pscustomobject]@{port=([regex]::Match([string]$_.Name,'\((COM[0-9]+)\)')).Groups[1].Value;label=[string]$_.Name;id=[string]$_.PNPDeviceID}
 })
 $likely=@($found | Where-Object {($_.id+" "+$_.label) -match 'VID_303A|ESP32|USB.JTAG|CP210|CH340|FTDI|USB.Serial'})
 $espSpecific=@($likely | Where-Object {($_.id+" "+$_.label) -match 'VID_303A|ESP32|USB.JTAG'})
 $pick=if($espSpecific.Count -eq 1){$espSpecific}else{$likely}
 if($pick.Count -eq 0 -and $found.Count -eq 1){$pick=$found}
 Need ($pick.Count -eq 1) ("Ambiguous ESP serial port: "+(($found | ForEach-Object {$_.port+" "+$_.label}) -join "; ")+"; use -EspPort COM9")
 return $pick[0].port
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
function Evidence {
 if(-not $script:sessionStarted) {
  Step "BLE runtime" "NOT RUN" "Session stopped during setup; no BLE log was captured"
  Step "Pixel Probe" "NOT RUN" "No physical diagnostic was executed"
  Step "Save runtime" "NOT RUN" "No Save test was executed"
  Step "Playback runtime" "NOT RUN" "No playback test was executed"
  Step "Independent RMT test" "UNAVAILABLE" "Firmware has no verified USB LED command"
  return
 }
 $a=Raw $logcat;$b=Raw $serial
 $errors=[regex]::Matches($a,'(?i)gatt.{0,30}133|status[=: ]+133').Count
 $busy=[regex]::Matches($a,'(?i)pixel probe busy|pixel probe queued behind').Count
 $ok=[regex]::Matches($b,'PIXEL_PROBE_ACCEPT').Count
 $rmt=[regex]::Matches($b,'PIXEL_PROBE_RMT_DONE').Count
 $frame=[regex]::Matches($b,'PIXEL_PROBE_NONBLACK_FRAME').Count
 $save=[regex]::Matches(($a+[Environment]::NewLine+$b),'(?i)save.{0,50}(reject|code 10)|SAVE_REJECT').Count
 Step "BLE runtime" $(if($errors -gt 0){"FAIL"}else{"NOT VERIFIED"}) "GATT133=$errors; queued/busy=$busy"
 Step "Pixel Probe" "NOT OPTICALLY VERIFIED" "Controller ACCEPT=$ok; RMT_DONE=$rmt; NONBLACK_FRAME=$frame"
 Step "Save runtime" $(if($save -gt 0){"FAIL"}else{"NOT VERIFIED"}) "save rejection matches=$save; no automatic NVS write"
 Step "Playback runtime" "NOT OPTICALLY VERIFIED" "Human BLE app action required; no command issued by runner"
 Step "Independent RMT test" "UNAVAILABLE" "Existing firmware has no verified USB serial LED command; BLE Pixel Probe plus serial logs only"
}
function Report {
 Evidence
 $summary=[ordered]@{date=$stamp;android_sha=$androidSha;firmware_sha=$firmwareSha;phone_adb=$phone;esp_usb=$esp;
  result=$(if($failed -and -not $sessionStarted){"SETUP FAILURE / NO HARDWARE TESTS RUN"}elseif($failed){"FAIL / INCOMPLETE"}elseif($SelfTest){"OFFLINE SELFTEST PASS; NO HARDWARE TESTED"}else{"EXACT-HEAD SOFTWARE VERIFIED; PHYSICAL NOT VERIFIED"});
  checks=$steps;ci=$ci;artifacts=$artifacts;notes=@($notes)}
 $summary | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $session "report.json") -Encoding UTF8
 $lines=New-Object System.Collections.Generic.List[string]
 $lines.Add("# OpenTail Windows USB + BLE bench - $stamp")
 $lines.Add("Overall: "+$summary.result)
 $lines.Add("Android PR #111: "+$androidSha)
 $lines.Add("Firmware artifact PR #127 (installed firmware version not independently measured): "+$firmwareSha)
 $lines.Add("ADB: "+$phone+"; ESP serial: "+$esp)
 $lines.Add("")
 $lines.Add("| Check | Result | Evidence |")
 $lines.Add("|---|---|---|")
 foreach($key in $steps.Keys) {
  $v=$steps[$key]
  $lines.Add("| "+$key+" | "+$v.state+" | "+([string]$v.detail).Replace("|","/")+" |")
 }
 $lines.Add("")
 $lines.Add("## Executed GitHub Actions regression evidence")
 foreach($name in $ci.Keys){$lines.Add("- "+$name+": PASS, Actions run "+$ci[$name])}
 $lines.Add("")
 $lines.Add("## Exact-head artifact evidence")
 foreach($key in $artifacts.Keys){$lines.Add("- "+$key+": "+$artifacts[$key])}
 $lines.Add("")
 $lines.Add("## Logs: android-logcat.txt and esp32-serial.txt")
 $lines.Add("Raw logs stay on the PC. No BLE pairing secrets were requested.")
 $lines.Add("NEVER equate software/RMT_DONE with physical optical acceptance.")
 $lines.Add("No automatic firmware flashing, NVS erase, Save, reboot or hardcoded light counts.")
 foreach($n in $notes){$lines.Add("- "+$n)}
 $path=Join-Path $session "REPORT.md"
 $lines | Set-Content $path -Encoding UTF8
 Write-Host ("Report: "+$path)
}
$stamp=Get-Date -Format "yyyy-MM-dd HH:mm:ss"
try {
 if($SelfTest) {
  Need (-not $FlashFirmware -and -not $DirectLedProbe) "SelfTest cannot send hardware actions"
  Need ([string]$repo -eq "Srryastari/OPENTAIL-development") "Wrong repo"
  Step "Runner self-test" "PASS" "No USB or Bluetooth commands, firmware write or NVS reset"
 } else {
  EnsureGh
  Tool "adb"
  $phone=Phone $AndroidSerial;$esp=EspCom $EspPort
  Step "USB" "PASS" "Authorized phone $phone; ESP logging $esp"
  $androidSha=Head 111;$firmwareSha=Head 127
  $androidRuns=Api "repos/$repo/actions/runs?head_sha=$androidSha&per_page=100"
  $fwRuns=Api "repos/$repo/actions/runs?head_sha=$firmwareSha&per_page=100"
  $ai=GoodRun $androidRuns "Android exact-head software integration"
  $jni=GoodRun $androidRuns "Issue103 reconciled Effects/Firmware/Android exact SHA (software only)"
  $fi=GoodRun $fwRuns "Issue126 Panel/Zone exact-head CORE verification"
  $ci["Android regression"]=$ai.id;$ci["JNI 140 effects"]=$jni.id;$ci["Firmware regression"]=$fi.id
  Step "CI software" "PASS" "Exact-head Android BLE/geometry/Save, JNI playback/140 effects and Firmware workflows passed"
  LocalRegressions $androidSha
  $runs=@($androidRuns.workflow_runs | Where-Object {$_.name -eq "Verify OpenTail active sources" -and $_.status -eq "completed"} | Sort-Object created_at -Descending)
  Need ($runs.Count -ge 1) "No exact-head Android APK workflow"
  $apkRun=$runs[0]
  $jobs=Api "repos/$repo/actions/runs/$($apkRun.id)/jobs?per_page=100"
  Need (@($jobs.jobs | Where-Object {$_.name -eq "android-debug-apk" -and $_.conclusion -eq "success"}).Count -eq 1) "APK build job failed"
  $aa=Artifact $apkRun '^opentail-native-android-.+-debug-apk$' (Join-Path $session "apk")
  $fa=Artifact $fi ('^opentail-easyflash-.+'+[regex]::Escape($firmwareSha)+'$') (Join-Path $session "firmware")
  $apks=@(Get-ChildItem (Join-Path $session "apk") -Recurse -Filter "*.apk" -File)
  Need ($apks.Count -eq 1) "Downloaded APK count not 1"
  $apk=$apks[0].FullName
  $firmware=FirmwarePackage (Join-Path $session "firmware") $firmwareSha
  # The verified artifact includes an OPTIONAL destructive factory-reset
  # utility. Remove it from this session so the owner cannot accidentally
  # launch it while reviewing the downloaded images.
  Get-ChildItem (Join-Path $session "firmware") -Recurse -File |
    Where-Object {$_.Name -in @("03_FACTORY_RESET_COM9.bat","factory_reset.py")} |
    Remove-Item -Force
  $artifacts["Android APK"]=$aa.url+" SHA256="+(Get-FileHash -Algorithm SHA256 $apk).Hash
  $artifacts["Firmware app"]=$fa.url+" SHA256="+$firmware.sha
  Step "Verified artifacts" "PASS" "GitHub exact-head job, firmware manifest and SHA256SUMS verified"
  $result=@(& adb -s $phone install -r $apk 2>&1)
  Need ($LASTEXITCODE -eq 0 -and (($result -join " ") -match "Success")) ("ADB install failed; app data untouched: "+($result -join " "))
  Step "Android install" "PASS" "adb install -r preserves app data; never uninstalls"
  if($FlashFirmware) {
   ConfirmedFlash $firmware.image $esp $firmwareSha
   Step "Flash" "PASS" "Explicit app-only flash at 0x20000; no NVS erase"
  } else {Step "Flash" "SKIPPED" "Default: never flash"}
  $adbProcess=Start-Process -FilePath (Get-Command adb).Source -ArgumentList @("-s",$phone,"logcat","-v","threadtime","-T","1") -PassThru -NoNewWindow -RedirectStandardOutput $logcat -RedirectStandardError (Join-Path $session "logcat.stderr.txt")
  $capture=Join-Path $PSScriptRoot "Serial-Capture.ps1"
  Need (Test-Path $capture) "Serial-Capture.ps1 missing"
  $captureArguments=@("-NoProfile","-ExecutionPolicy","Bypass","-File",('"'+$capture+'"'),"-Port",$esp,"-OutFile",('"'+$serial+'"'),"-Seconds","$CaptureSeconds")
  $serialProcess=Start-Process -FilePath "powershell.exe" -ArgumentList $captureArguments -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $session "serial.stderr.txt")
  Start-Sleep -Seconds 2
  Need (-not $serialProcess.HasExited) "Serial capture exited early; inspect serial.stderr.txt"
  & adb -s $phone shell am start -n "com.opentail.android/.ComposeMainActivity" | Out-Null
  $script:sessionStarted=$true
  Step "Log capture" "CAPTURING" "$CaptureSeconds-second Android logcat + ESP USB serial session"
  Write-Host "On PHONE: connect and authorize OpenTail using Bluetooth."
  Write-Host "In Layout, use Check first LED (O1), then optionally O2, Show Modes and an unsaved edit."
  Write-Host "The runner NEVER taps Save or sends a BLE effect automatically."
  Write-Host "STOP priority remains in the existing BLE firmware/app protocol; this runner never sends LED commands."
  if($DirectLedProbe){Write-Host "Independent RMT command unavailable in current firmware; BLE Pixel Probe + serial evidence only."}
  Start-Sleep -Seconds $CaptureSeconds
  if($serialProcess.HasExited -and $serialProcess.ExitCode -ne 0){throw "Serial capture failed during session; inspect serial.stderr.txt"}
  if($adbProcess.HasExited -and $adbProcess.ExitCode -ne 0){throw "Android logcat capture failed during session; inspect logcat.stderr.txt"}
  Step "Log capture" "PASS" "Android/ESP captures completed"
  if((Head 111) -ne $androidSha -or (Head 127) -ne $firmwareSha) {
   $notes.Add("PR head changed during session; pinned artifacts may no longer be latest")
   Step "Head consistency" "WARN" "PR updated after artifact selection"
  } else {Step "Head consistency" "PASS" "Exact same PR heads after session"}
 }
} catch {
 $failed=$true
 Step "Runner error" "FAIL" $_.Exception.Message
} finally {
 foreach($proc in @($adbProcess,$serialProcess)){
  if($null -ne $proc){try {if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue}}catch{}}
 }
 Start-Sleep -Milliseconds 400
 Report
}
if($failed){exit 1}
exit 0
