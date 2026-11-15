param([string]$Port="COM9")
$ErrorActionPreference="Stop"
Set-StrictMode -Version Latest
try {
    $dir=$PSScriptRoot
    if($Port -notmatch '^COM[0-9]+$'){throw "Invalid COM port"}
    $manifest=Join-Path $dir "manifest.json"
    $hashes=Join-Path $dir "SHA256SUMS.txt"
    if(!(Test-Path -LiteralPath $manifest) -or !(Test-Path -LiteralPath $hashes)){
        throw "Missing manifest or checksums. Nothing flashed."
    }
    $meta=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if($meta.test -cne "gpio16-rmt-smoke-1" -or $meta.gpio -ne 16 -or
       $meta.pixels -ne 8 -or $meta.brightness -ne 204 -or
       $meta.dma -ne $false -or $meta.nvs_erase -ne $false -or
       $meta.nvs_offset -cne "0x9000" -or
       $meta.source_sha -notmatch '^[0-9a-f]{40}$'){
        throw "Unexpected or unsafe diagnostic manifest. Nothing flashed."
    }
    $required=@(
        "bootloader/bootloader.bin",
        "partition_table/partition-table.bin",
        "ota_data_initial.bin",
        "opentail_gpio16_smoke.bin",
        "flasher_args.json",
        "manifest.json"
    )
    $lines=@(Get-Content -LiteralPath $hashes)
    if($lines.Count -ne $required.Count){throw "Unexpected number of SHA256 entries."}
    foreach($rel in $required){
        $path=Join-Path $dir $rel
        if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing file: $rel"}
        $matches=@($lines | Where-Object { $_ -match ('  '+[regex]::Escape($rel)+'$') })
        if($matches.Count -ne 1){throw "Missing or duplicate hash for $rel"}
        $expected=($matches[0] -split '\s+',2)[0]
        if($expected -notmatch '^[0-9a-fA-F]{64}$'){throw "Malformed SHA256 for $rel"}
        $actual=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if($expected -ine $actual){throw "SHA256 mismatch for $rel"}
    }
    $flashJson=Get-Content -LiteralPath (Join-Path $dir "flasher_args.json") -Raw | ConvertFrom-Json
    $offsets=@("0x0","0x8000","0xf000","0x20000")
    $names=@("bootloader/bootloader.bin","partition_table/partition-table.bin",
        "ota_data_initial.bin","opentail_gpio16_smoke.bin")
    $items=@($flashJson.flash_files.PSObject.Properties)
    if($items.Count -ne 4){throw "Flash manifest must contain exactly 4 offsets."}
    for($i=0;$i -lt 4;$i++){
        if([string]$flashJson.flash_files.($offsets[$i]) -cne $names[$i]){
            throw "Flash offset mismatch at $($offsets[$i])."
        }
    }
    if(!(Get-Command py -ErrorAction SilentlyContinue)){
        throw "Python launcher missing. Install Python 3 and esptool."
    }
    & py -3 -m esptool version
    if($LASTEXITCODE -ne 0){throw "esptool missing: py -3 -m pip install esptool"}
    Write-Host "PASS - six SHA256s, exact source, offsets, GPIO16, brightness 204." -ForegroundColor Green
    Write-Host "Standalone NON-DMA WS2815 test, FIRST EIGHT LEDs ONLY, 80% channel brightness."
    Write-Host "No Panel v4 or BLE. GPIO17 is unused. NVS at 0x9000 is NOT erased."
    Write-Warning "FLASH writes bootloader 0x0, table 0x8000, OTA-data 0xF000 and test app 0x20000."
    Write-Host "Close COM9 serial monitors. Turn OFF the separate 12V LED supply during USB flashing."
    $confirm=Read-Host ("Type FLASH "+$Port+" to install the diagnostic firmware")
    if($confirm -cne ("FLASH "+$Port)){throw "Cancelled. Nothing flashed."}
    $arguments=@("-3","-m","esptool","--chip","esp32s3","--port",$Port,
        "--baud","115200","write_flash","--flash_mode","dio","--flash_freq","80m",
        "--flash_size","16MB")
    for($i=0;$i -lt 4;$i++){
        $arguments+=@($offsets[$i],(Join-Path $dir $names[$i]))
    }
    & py @arguments
    if($LASTEXITCODE -ne 0){throw "esptool flash failed. No erase command was used."}
    Write-Host "FLASH COMPLETE. Restore the separate 12V LED supply after normal boot." -ForegroundColor Green
    Write-Host "Watch the first 8 LEDs: RED, GREEN, BLUE, WHITE, 3 rounds, then OFF."
    Read-Host "After observing the complete sequence, press ENTER" | Out-Null
    $answer=Read-Host "Did the first 8 physical LEDs illuminate? (YES / NO / PARTIAL)"
    $result="UNVERIFIED"
    if($answer -ieq "YES"){$result="YES"}
    if($answer -ieq "NO"){$result="NO"}
    if($answer -ieq "PARTIAL"){$result="PARTIAL"}
    $report=Join-Path $dir "GPIO16_SMOKE_RESULT.txt"
    @(
        "OpenTail GPIO16 bare-bones diagnostic"
        "Source SHA: $($meta.source_sha)"
        "COM port: $Port"
        "Pattern: 8px GRB, R/G/B/W, 3 rounds, then OFF"
        "Brightness: 204/255 per enabled channel (80%)"
        "Backend: ESP-IDF 5.5.5 RMT non-DMA, WS2815 timings, GPIO16"
        "Physical optical result (owner observed): $result"
        "No NVS erase. OTA-data 0xf000 was reset by USB flash."
        "Restore normal firmware with its own verified EasyFlash package."
    ) | Set-Content -LiteralPath $report -Encoding UTF8
    Write-Host ("Report: "+$report) -ForegroundColor Green
} catch {
    Write-Host ("STOP: "+$_.Exception.Message) -ForegroundColor Red
    exit 1
}
