[CmdletBinding()]
param(
    [string]$Port = 'COM10',
    [string]$ExpectedMac = '84:c7:bb:5a:66:48'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$Firmware = $PSScriptRoot
$Version = '0.1.27-alpha'
$StudioVersion = '0.19.118'

Write-Host "OpenTail CORE v$Version / Studio v$StudioVersion — build, verify, back up, then flash app only."
Write-Host "Target: $Port · expected ESP32-S3 MAC: $ExpectedMac"
& (Join-Path $Firmware 'PREPARE-AND-BUILD.ps1')
if($LASTEXITCODE -ne 0){throw 'Build failed. No flash was attempted.'}

$build = Join-Path $Firmware 'build'
$app = Join-Path $Firmware 'releases\opentail-core-v0.1.27-alpha-studio-v0.19.118-app.bin'
$part = Join-Path $build 'partition_table\partition-table.bin'
$flashArgs = Join-Path $build 'flash_args'
if(-not (Test-Path -LiteralPath $app)){throw "Versioned app binary missing: $app"}
if(-not (Test-Path -LiteralPath $part)){throw 'Built partition table missing; no flash attempted.'}
if(-not (Test-Path -LiteralPath $flashArgs)){throw 'ESP-IDF flash_args missing; no flash attempted.'}

$config = Join-Path $Firmware 'sdkconfig'
$offsetMatch = [regex]::Match([System.IO.File]::ReadAllText($config),'(?m)^CONFIG_PARTITION_TABLE_OFFSET=(0x[0-9A-Fa-f]+|[0-9]+)$')
if(-not $offsetMatch.Success){
    $header = Join-Path $build 'config\sdkconfig.h'
    if(Test-Path -LiteralPath $header){
        $offsetMatch = [regex]::Match([System.IO.File]::ReadAllText($header),'(?m)^#define\s+CONFIG_PARTITION_TABLE_OFFSET\s+(0x[0-9A-Fa-f]+|[0-9]+)')
    }
}
if(-not $offsetMatch.Success){throw 'Partition-table offset could not be verified; no flash attempted.'}
$offsetText = $offsetMatch.Groups[1].Value
$partitionOffset = if($offsetText.StartsWith('0x',[System.StringComparison]::OrdinalIgnoreCase)){[Convert]::ToInt32($offsetText.Substring(2),16)}else{[int]$offsetText}
if($partitionOffset -ne 0x8000){throw "Expected partition-table offset 0x8000, found $offsetText; no flash attempted."}

$argsText = [System.IO.File]::ReadAllText($flashArgs)
if($argsText -notmatch '(?i)0x10000\s+(?:build[/\\])?opentail_core_alpha\.bin'){
    throw 'Application offset 0x10000 was not confirmed in flash_args; no flash attempted.'
}
if($argsText -notmatch '(?i)0x8000\s+(?:build[/\\])?partition_table[/\\]partition-table\.bin'){
    throw 'Partition-table offset 0x8000 was not confirmed in flash_args; no flash attempted.'
}

$chipOutput = & python -m esptool --port $Port chip_id 2>&1
$chipExit = $LASTEXITCODE
$chipText = ($chipOutput | Out-String)
Write-Host $chipText
if($chipExit -ne 0 -or $chipText -notmatch 'Chip is ESP32-S3'){
    throw "ESP32-S3 detection failed on $Port. Nothing was flashed."
}
if($chipText -notmatch [regex]::Escape($ExpectedMac)){
    throw "Connected board MAC does not match $ExpectedMac. Nothing was flashed."
}

$workspace = Join-Path $env:USERPROFILE 'Desktop\OpenTail'
$backupDir = Join-Path $workspace 'Backups'
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $backupDir "opentail-core-before-v$Version-$Port-$stamp-fullflash.bin"
Write-Host "Backing up full 16 MB flash to: $backup"
& python -m esptool --chip esp32s3 --port $Port -b 460800 --before default_reset --after hard_reset read_flash 0x0 0x1000000 $backup
if($LASTEXITCODE -ne 0){throw 'Full-flash backup failed. No flash was attempted.'}
if((Get-Item -LiteralPath $backup).Length -ne 16777216){throw 'Full-flash backup is not 16 MB. No flash was attempted.'}

$newPartition = [System.IO.File]::ReadAllBytes($part)
if($newPartition.Length -gt 0x1000){throw 'Built partition table exceeds the expected sector. No flash was attempted.'}
$backupBytes = [System.IO.File]::ReadAllBytes($backup)
$oldPartition = New-Object byte[] ($newPartition.Length)
[Array]::Copy($backupBytes,0x8000,$oldPartition,0,$newPartition.Length)
if([Convert]::ToBase64String($oldPartition) -ne [Convert]::ToBase64String($newPartition)){
    throw "Existing partition table differs from the new build. Backup retained; no flash attempted."
}
$appSize = 0
for($i=0;$i+32 -le $newPartition.Length;$i+=32){
    if($newPartition[$i] -eq 0xAA -and $newPartition[$i+1] -eq 0x50){
        $type = $newPartition[$i+2]
        $offset = [BitConverter]::ToUInt32($newPartition,$i+4)
        $size = [BitConverter]::ToUInt32($newPartition,$i+8)
        if($type -eq 0 -and $offset -eq 0x10000){$appSize=$size;break}
    }
}
if($appSize -le 0){throw 'Could not confirm an app partition at 0x10000. Backup retained; no flash attempted.'}
if((Get-Item -LiteralPath $app).Length -gt $appSize){throw 'App image exceeds the confirmed application partition. No flash attempted.'}

Write-Host ''
Write-Host 'READY TO FLASH'
Write-Host "  Firmware: CORE v$Version / Studio v$StudioVersion"
Write-Host "  Board: ESP32-S3 $ExpectedMac on $Port"
Write-Host "  Backup: $backup"
Write-Host '  Partition table: identical to current device'
Write-Host '  Write scope: application partition at 0x10000 only; NVS/partition table/bootloader untouched'
$confirm = Read-Host 'Type FLASH to write the app partition'
if($confirm -cne 'FLASH'){throw 'Flash cancelled. Build and backup retained; controller not written.'}

& python -m esptool --chip esp32s3 --port $Port -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x10000 $app
if($LASTEXITCODE -ne 0){throw 'Application flash failed. Do not erase flash; retain the full backup for recovery.'}
Write-Host "FLASH SUCCESS: CORE v$Version / Studio v$StudioVersion installed. Backup retained at $backup"
