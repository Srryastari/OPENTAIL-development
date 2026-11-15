# Pure, read-only OpenTail bench diagnostic classifiers. Windows PowerShell 5.1.
# Never makes BLE, serial, firmware or NVS writes.
function HasBenchEvidence([string]$body,[string]$pattern) {
 return [regex]::IsMatch($body,$pattern,[System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
}
function Get-BenchFailure([string]$android,[string]$esp) {
 $cases=@(
  @('OWNER_ENROLLMENT','OpenTailOwner:\s*Owner enrollment refused\s*\(GATT\s*2\)|Owner enrollment refused[^\r\n]*GATT\s*2'),
  @('OWNER_CHALLENGE','OpenTailOwner:\s*Owner challenge unavailable|BLE challenge unavailable'),
  @('OWNER_AUTHORIZATION','OpenTailOwner:\s*Owner HMAC rejected|Owner authorization rejected|BLE\s*[^A-Za-z0-9\\r\\n]{1,4}\s*ERR AUTH'),
  @('GATT_CONNECTION','(?:OpenTailOwner:|BluetoothGatt:)[^\r\n]*(?:GATT 133|status=133|status: 133)|BLE connection failed[^\r\n]*GATT'),
  @('SAVE','SAVE_REJECT|Panel Save[^\r\n]*(?:reject|failed)|Save[^\r\n]*code 10'),
  @('RENDERER','Renderer internal stack/TCB allocation failed|Pixel task intentionally disabled|render task could not start|render(?:er)?[^\r\n]*FAULT'),
  @('RMT','PIXEL_PROBE_RMT_FAIL|RMT[^\r\n]*(?:failed|error|timeout)'),
  @('COMMAND','BLE write failed[^\r\n]*GATT|BLE\s*[^A-Za-z0-9\\r\\n]{1,4}\s*ERR (?:FORMAT|BUSY|RANGE)')
 )
 foreach($case in $cases) {
  if(HasBenchEvidence ($android+"\n"+$esp) $case[1]) {return [string]$case[0]}
 }
 return ""
}
function Get-BenchStages([string]$android,[string]$esp,[string]$mode,[bool]$started) {
 $stages=[ordered]@{}
 foreach($name in @('gatt_connection','os_bond','owner_enrollment','owner_challenge',
                    'owner_authorization','command','layout','save','renderer','rmt','pixel_probe')) {
  $stages[$name]=[ordered]@{status='NOT RUN';evidence='No matching physical event observed'}
 }
 if(-not $started){return $stages}
 $both=$android+"\n"+$esp
 $rules=@(
  @('gatt_connection','BluetoothGatt:[^\r\n]*(?:onSearchComplete\(\)[^\r\n]*Status=0|onClientConnectionState\(\)[^\r\n]*status=0[^\r\n]*newState=2)|OpenTailOwner:[^\r\n]*GATT owner chars','BluetoothGatt:[^\r\n]*(?:status=133|GATT 133)|OpenTailOwner:[^\r\n]*GATT 133'),
  @('os_bond','BluetoothBondStateMachine:[^\r\n]*newState:\s*2\b|bond state change to 11 -> 12|BLUETOOTH_BOND_STATE_BONDED|OpenTailOwner:[^\r\n]*Bluetooth bond complete','Bluetooth pairing (?:failed|not completed)|BLE bond failed'),
  @('owner_enrollment','OpenTailOwner:[^\r\n]*Owner enrollment accepted','Owner enrollment refused|OpenTailOwner:[^\r\n]*Owner enrollment refused'),
  @('owner_challenge','OpenTailOwner:[^\r\n]*Owner HMAC proof queued','OpenTailOwner:[^\r\n]*Owner challenge unavailable|BLE challenge unavailable'),
  @('owner_authorization','OpenTailOwner:[^\r\n]*Controller accepted owner HMAC|BLE\s*[^A-Za-z0-9\\r\\n]{1,4}\s*OK AUTH','OpenTailOwner:[^\r\n]*Owner HMAC rejected|Owner authorization rejected|BLE\s*[^A-Za-z0-9\\r\\n]{1,4}\s*ERR AUTH'),
  @('command','BLE\s*[^A-Za-z0-9\\r\\n]{1,4}\s*OK (?:SHOW|STOP|FX)|PIXEL_PROBE_ACCEPT','BLE write failed[^\r\n]*GATT|BLE\s*[^A-Za-z0-9\\r\\n]{1,4}\s*ERR (?:FORMAT|RANGE|BUSY)'),
  @('layout','Panel v4[^\r\n]*(?:readback[^\r\n]*verified|snapshot[^\r\n]*accepted)|PANEL_PREVIEW_UPDATE_ACCEPT','Panel v4[^\r\n]*(?:rejected|failed)|PANEL_PREVIEW_UPDATE_REJECT'),
  @('save','Panel Save[^\r\n]*(?:confirmed|complete|saved)[^\r\n]*readback|Save[^\r\n]*authoritative readback verified','SAVE_REJECT|Panel Save[^\r\n]*(?:reject|failed)|Save[^\r\n]*code 10'),
  @('renderer','Renderer task READY|Renderer task[^\r\n]*READY','Renderer internal stack/TCB allocation failed|Pixel task intentionally disabled|render task could not start'),
  @('rmt','PIXEL_PROBE_RMT_DONE','PIXEL_PROBE_RMT_FAIL|RMT[^\r\n]*(?:failed|error|timeout)'),
  @('pixel_probe','PIXEL_PROBE_ACCEPT','PIXEL_PROBE_REJECT|Pixel 0 probe rejected')
 )
 foreach($rule in $rules) {
  $name=$rule[0];$pass=$rule[1];$fail=$rule[2]
  if(HasBenchEvidence $both $fail){$stages[$name]=[ordered]@{status='FAIL';evidence='Explicit failure event matched'}}
  elseif(HasBenchEvidence $both $pass){$stages[$name]=[ordered]@{status='PASS';evidence='Explicit software/protocol event matched; optical result not inferred'}}
 }
 if($stages['gatt_connection'].status -eq 'FAIL') {
  foreach($n in @('os_bond','owner_enrollment','owner_challenge','owner_authorization','command')) {
   if($stages[$n].status -eq 'NOT RUN'){$stages[$n]=[ordered]@{status='BLOCKED';evidence='GATT connection failed'}}
  }
 }
 if($stages['owner_enrollment'].status -eq 'FAIL') {
  foreach($n in @('owner_challenge','owner_authorization','command')) {
   if($stages[$n].status -eq 'NOT RUN'){$stages[$n]=[ordered]@{status='BLOCKED';evidence='Owner enrollment refused'}}
  }
 }
 if($stages['owner_challenge'].status -eq 'FAIL' -and $stages['owner_authorization'].status -eq 'NOT RUN') {
  $stages['owner_authorization']= [ordered]@{status='BLOCKED';evidence='Owner challenge unavailable'}
 }
 if($mode -eq 'bluetooth'){
  foreach($n in @('layout','save','rmt','pixel_probe')){if($stages[$n].status -eq 'NOT RUN'){$stages[$n].evidence='Not requested in Bluetooth-only session'}}
 }
 if($mode -eq 'layout' -and $stages['save'].status -eq 'NOT RUN'){$stages['save'].evidence='Save not requested in Layout session'}
 if($mode -eq 'playback' -and $stages['save'].status -eq 'NOT RUN'){$stages['save'].evidence='Save not requested in Playback session'}
 return $stages
}
function Get-InstalledFirmwareIdentity([string]$serialText,[string]$androidText) {
 # Firmware startup prints version and boot ID; these do NOT identify a Git SHA.
 $version=$null;$boot=$null;$source='unavailable';$gitSha=$null
 $v=[regex]::Match($serialText,'(?i)CORE\s+(0\.\d+\.\d+(?:-[\w.]+)?)\s+BENCH ONLY')
 if($v.Success){$version=$v.Groups[1].Value;$source='physical ESP32 USB boot banner'}
 $b=[regex]::Match($serialText,'(?i)CORE boot identity:\s*([0-9a-f]{8})')
 if($b.Success){$boot=$b.Groups[1].Value}
 if(-not $version){
  $v=[regex]::Match($androidText,'(?i)OpenTail CORE\s+(0\.\d+\.\d+(?:-[\w.]+)?)\s*\|')
  if($v.Success){$version=$v.Groups[1].Value;$source='Android-observed BLE INFO (unverified against USB boot)'}
 }
 $sha=[regex]::Match($serialText,'(?im)\b(?:installed_git_sha|firmware_git_sha)\s*[=:]\s*([0-9a-f]{40})\b')
 if($sha.Success){$gitSha=$sha.Groups[1].Value}
 return [ordered]@{version=$version;installed_git_sha=$gitSha;boot_id=$boot;source=$source;
                    identity_verified=($null -ne $version);downloaded_sha_is_not_installed_sha=$true}
}
function Get-ClockCorrelation([string]$phoneClock,[datetime]$beforeUtc,[datetime]$afterUtc) {
 $m=[regex]::Match($phoneClock,'(?m)^\s*(\d{10,11})\s+([+-]\d{4})\s*$')
 if(-not $m.Success){return [ordered]@{available=$false;phone_skew_seconds=$null;phone_utc_offset=$null;uncertainty_ms=$null}}
 $phoneEpoch=[long]$m.Groups[1].Value
 $sec=$m.Groups[2].Value
 $offsetMin=([int]::Parse($sec.Substring(1,2))*60+[int]::Parse($sec.Substring(3,2)))
 if($sec.StartsWith('-')){$offsetMin=-$offsetMin}
 $mid=[datetime]::SpecifyKind($beforeUtc.AddTicks([long](($afterUtc.Ticks-$beforeUtc.Ticks)/2)),[DateTimeKind]::Utc)
 $pcSeconds=([datetimeoffset]$mid).ToUnixTimeSeconds()
 return [ordered]@{available=$true;phone_skew_seconds=($phoneEpoch-$pcSeconds);
                   phone_utc_offset=$sec;uncertainty_ms=([int](($afterUtc-$beforeUtc).TotalMilliseconds/2+1000))}
}
function Get-CorrelatedEvents([string]$android,[string]$timeline,$clock,[int]$maxEvents=40) {
 $events=New-Object System.Collections.Generic.List[object]
 $year=(Get-Date).Year
 $offset=[TimeSpan]::Zero
 if($clock.available) {
  $zone=[string]$clock.phone_utc_offset
  $mins=[int]::Parse($zone.Substring(1,2))*60+[int]::Parse($zone.Substring(3,2))
  if($zone.StartsWith('-')){$mins=-$mins}
  $offset=[TimeSpan]::FromMinutes($mins)
 }
 foreach($line in ($android -split '\r?\n')) {
  if($line -notmatch 'OpenTailOwner:|BluetoothGatt:.*(?:onSearchComplete|onClientConnectionState|GATT 133)|BluetoothBondStateMachine:.*bondStateChange|SAVE_REJECT|BLE.*(?:ERR AUTH|OK AUTH)'){continue}
  $utc=$null
  $m=[regex]::Match($line,'^(\d{2})-(\d{2})\s+(\d{2}):(\d{2}):(\d{2})\.(\d{3})')
  if($clock.available -and $m.Success) {
   try {
    $d=[datetimeoffset]::new($year,[int]$m.Groups[1].Value,[int]$m.Groups[2].Value,
     [int]$m.Groups[3].Value,[int]$m.Groups[4].Value,[int]$m.Groups[5].Value,$offset)
    $utc=$d.AddMilliseconds([int]$m.Groups[6].Value).AddSeconds(-[double]$clock.phone_skew_seconds).UtcDateTime.ToString('o')
   } catch {}
  }
  $msg=$line.Trim()
  $msg=[regex]::Replace($msg,'(?i)(?:[0-9A-F]{2}:){5}[0-9A-F]{2}','[device]')
  if($msg.Length -gt 190){$msg=$msg.Substring(0,190)}
  $events.Add([ordered]@{utc=$utc;source='android';message=$msg})
 }
 foreach($line in ($timeline -split '\r?\n')){
  if($line -notmatch 'OT_BLE|OT_CORE|PIXEL_PROBE|RMT|render|Renderer|SAVE_REJECT'){continue}
  $m=[regex]::Match($line,'^([^|]+)\|\s*(.*)$')
  if(-not $m.Success){continue}
  $msg=$m.Groups[2].Value.Trim()
  if($msg.Length -gt 190){$msg=$msg.Substring(0,190)}
  $events.Add([ordered]@{utc=$m.Groups[1].Value.Trim();source='esp32';message=$msg})
 }
 $sorted=@($events | Sort-Object {if($_.utc){$_.utc}else{'9999'}})
 if($sorted.Count -gt $maxEvents){$sorted=@($sorted | Select-Object -Last $maxEvents)}
 return @($sorted)
}
function AssertBenchDiagnosticsSelfTest {
 $a="11-14 19:18:19.091 BluetoothGatt: onSearchComplete() = Device=X Status=0`n11-14 19:18:23.191 BluetoothBondStateMachine: newState: 2`n11-14 19:18:23.300 W OpenTailOwner: Owner enrollment refused (GATT 2)"
 Need ((Get-BenchFailure $a '') -eq 'OWNER_ENROLLMENT') 'GATT2 classification failed'
 $s=Get-BenchStages $a '' bluetooth $true
 Need ($s.gatt_connection.status -eq 'PASS' -and $s.os_bond.status -eq 'PASS' -and
       $s.owner_enrollment.status -eq 'FAIL' -and $s.owner_authorization.status -eq 'BLOCKED') 'BLE stage classification failed'
 Need ((Get-BenchFailure 'I OpenTailOwner: Controller accepted owner HMAC' '') -eq '') 'Successful auth misclassified'
 Need ((Get-BenchFailure 'W OpenTailOwner: Owner challenge unavailable (GATT 2)' '') -eq 'OWNER_CHALLENGE') 'Challenge classification failed'
 Need ((Get-BenchFailure '' 'E (4) OT_CORE: Renderer internal stack/TCB allocation failed') -eq 'RENDERER') 'Renderer classification failed'
 Need ((Get-BenchFailure '' 'E PIXEL_PROBE_RMT_FAIL') -eq 'RMT') 'RMT classification failed'
 Need ((Get-BenchFailure 'BLE Save rejected code 10' '') -eq 'SAVE') 'Save classification failed'
 $id=Get-InstalledFirmwareIdentity 'I (500) OT_CORE: CORE 0.1.32-alpha BENCH ONLY: one ESP watchdog' ''
 Need ($id.version -eq '0.1.32-alpha' -and $null -eq $id.installed_git_sha) 'Installed firmware must not inherit downloaded source SHA'
 $clock=Get-ClockCorrelation "1794700000 -0600" ([datetime]'2026-11-15T01:20:00Z') ([datetime]'2026-11-15T01:20:01Z')
 Need ($clock.available -and $clock.phone_utc_offset -eq '-0600') 'Clock parser failed'
 $ev=@(Get-CorrelatedEvents "11-14 19:18:23.300 W OpenTailOwner: Owner enrollment refused (GATT 2)" "2026-11-15T01:18:23Z | I (123) OT_BLE: hello" $clock 5)
 Need ($ev.Count -eq 2 -and @($ev | Where-Object {$_.source -eq 'esp32'}).Count -eq 1) 'Correlated event parser failed'
 Need ((Get-BenchStages '' '' save $false).save.status -eq 'NOT RUN') 'Unexecuted Save must remain NOT RUN'
}
