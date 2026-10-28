$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$config=Join-Path $PSScriptRoot 'sdkconfig'
$from=@(
  (Join-Path (Split-Path $PSScriptRoot -Parent) 'OpenTail-CORE-Alpha-v0.1.23-BLE-Outputs\sdkconfig'),
  (Join-Path (Split-Path $PSScriptRoot -Parent) 'OpenTail-CORE-Alpha-v0.1.22-Preview-Triggers\sdkconfig'),
  (Join-Path (Split-Path $PSScriptRoot -Parent) 'OpenTail-CORE-Alpha-v0.1.21-Studio-Zones\sdkconfig'),
  (Join-Path (Split-Path $PSScriptRoot -Parent) 'OpenTail-CORE-Alpha-v0.1.20-BLE-Control\sdkconfig'),
  (Join-Path (Split-Path $PSScriptRoot -Parent) 'OpenTail-CORE-Alpha-v0.1.18-Universal-Triggers\sdkconfig'),
  (Join-Path (Split-Path $PSScriptRoot -Parent) 'OpenTail-CORE-Alpha-v0.1.17-Universal-Bench\sdkconfig')
)
$existing=$null
if(Test-Path $config){$existing=(Get-Content $config | Where-Object {$_ -match '^CONFIG_OPENTAIL_AP_PASSWORD='} | Select-Object -Last 1)}
if(-not $existing -or $existing -match 'OpenTailLab-ChangeMe'){
  foreach($candidate in $from){
    if(Test-Path $candidate){
      $entry=(Get-Content $candidate | Where-Object {$_ -match '^CONFIG_OPENTAIL_AP_PASSWORD='} | Select-Object -Last 1)
      if($entry -and $entry -notmatch 'OpenTailLab-ChangeMe'){$existing=$entry;break}
    }
  }
}
if(-not $existing -or $existing -match 'OpenTailLab-ChangeMe'){
  throw 'Custom AP password was not found. Run idf.py menuconfig to configure OpenTail AP password. Do not flash default.'
}
$lines=@();if(Test-Path $config){$lines=@(Get-Content $config | Where-Object {$_ -notmatch '^CONFIG_OPENTAIL_AP_PASSWORD='})}
$lines+= $existing
[System.IO.File]::WriteAllLines($config,[string[]]$lines,[System.Text.UTF8Encoding]::new($false))
# Keep CORE's embedded /studio page synchronized with the reviewed web-studio source.
# This changes the build artifact only; it does not flash the device or erase NVS.
& python (Join-Path $PSScriptRoot 'tools/sync_studio_asset.py')
if($LASTEXITCODE -ne 0){throw 'Studio asset synchronization failed. Do not flash.'}
Write-Host 'Existing custom AP password preserved (not displayed). Configuring v0.1.24...'
& idf.py reconfigure
if($LASTEXITCODE -ne 0){throw 'ESP-IDF configure failed. Do not flash.'}
# Two Ninja workers avoid the concurrent compiler crash seen on this Windows machine.
& ninja -C build -j 2
if($LASTEXITCODE -ne 0){throw 'ESP-IDF build failed. Do not flash.'}
Write-Host 'Build command completed. Verify Ninja succeeded with exit code 0 before flashing.'
