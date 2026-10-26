# OpenTail CORE PR #12 - Windows local firmware build and package.
# Run from the ESP-IDF v5.5.5 PowerShell shortcut. Never flashes/erases a device.
param(
    [string]$ProjectDir = "",
    [ValidateRange(1, 16)][int]$Jobs = 2
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $ProjectDir) { $ProjectDir = $PSScriptRoot }
$ProjectDir = [IO.Path]::GetFullPath($ProjectDir)
if (Test-Path -LiteralPath (Join-Path $ProjectDir "core-firmware\CMakeLists.txt")) {
    $ProjectDir = Join-Path $ProjectDir "core-firmware"
}
$cmake = Join-Path $ProjectDir "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $cmake) -or
    -not (Test-Path -LiteralPath (Join-Path $ProjectDir "main\idf_component.yml"))) {
    throw "Not the OpenTail CORE source folder. Extract PR #12 source, then run this script from core-firmware."
}
if (-not $env:IDF_PATH -or -not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw "ESP-IDF environment is missing. Install ESP-IDF v5.5.5 and launch the 'ESP-IDF 5.5 PowerShell' shortcut, not ordinary PowerShell."
}
$version = (& idf.py --version | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw "Cannot execute the ESP-IDF toolchain." }
Write-Host ("ESP-IDF: " + $version)
if ($version -notmatch "v?5\.5\.5") {
    Write-Warning "The passing GitHub build used ESP-IDF 5.5.5. Other versions are not verified for this branch."
}

function Get-CustomApLine([string]$file) {
    if (-not (Test-Path -LiteralPath $file)) { return $null }
    foreach ($line in (Get-Content -LiteralPath $file)) {
        if ($line -match '^CONFIG_OPENTAIL_AP_PASSWORD="([^"]{8,63})"$') {
            if ($Matches[1] -ne "OpenTailLab-ChangeMe") { return $line }
        }
    }
    return $null
}

$config = Join-Path $ProjectDir "sdkconfig"
if (Test-Path -LiteralPath $config) {
    $target = Get-Content -LiteralPath $config |
        Where-Object { $_ -match '^CONFIG_IDF_TARGET=' } | Select-Object -Last 1
    if ($target -and $target -ne 'CONFIG_IDF_TARGET="esp32s3"') {
        throw "Existing sdkconfig targets something other than ESP32-S3. Back up that file and use a fresh CORE checkout."
    }
}
$apLine = Get-CustomApLine $config
if (-not $apLine) {
    $parent = Split-Path $ProjectDir -Parent
    $old = @(
        "OpenTail-CORE-Alpha-v0.1.23-BLE-Outputs",
        "OpenTail-CORE-Alpha-v0.1.22-Preview-Triggers",
        "OpenTail-CORE-Alpha-v0.1.21-Studio-Zones",
        "OpenTail-CORE-Alpha-v0.1.20-BLE-Control",
        "OpenTail-CORE-Alpha-v0.1.18-Universal-Triggers"
    )
    foreach ($name in $old) {
        $candidate = Join-Path $parent (Join-Path $name "sdkconfig")
        $apLine = Get-CustomApLine $candidate
        if ($apLine) { break }
    }
}
if (-not $apLine) {
    # A new random password, never the source-code default. Private local sdkconfig
    # is gitignored and is intentionally NOT included in the output firmware ZIP.
    $random = New-Object byte[] 16
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    try { $rng.GetBytes($random) } finally { $rng.Dispose() }
    $newPassword = [BitConverter]::ToString($random).Replace("-", "")
    $apLine = 'CONFIG_OPENTAIL_AP_PASSWORD="' + $newPassword + '"'
    Write-Warning "No prior custom CORE Wi-Fi password was found. A NEW random bench AP password will be configured."
    Write-Host ("NEW OpenTail CORE Wi-Fi password (keep private): " + $newPassword)
    Write-Host "Copy it now. It will also remain in this PC's gitignored core-firmware\sdkconfig."
} else {
    Write-Host "Reusing your existing custom CORE Wi-Fi password (not displayed)."
}
$lines = @()
if (Test-Path -LiteralPath $config) {
    $lines = @(Get-Content -LiteralPath $config |
        Where-Object { $_ -notmatch '^CONFIG_OPENTAIL_AP_PASSWORD=' })
}
$lines += $apLine
[IO.File]::WriteAllLines($config, [string[]]$lines, (New-Object System.Text.UTF8Encoding($false)))

$oldLocation = Get-Location
try {
    Set-Location $ProjectDir
    Write-Host "Building OpenTail CORE for ESP32-S3 with $Jobs compilation jobs. No flash will be attempted."
    & idf.py -j $Jobs build
    if ($LASTEXITCODE -ne 0) { throw "ESP-IDF build FAILED. No firmware ZIP was created." }

    $build = Join-Path $ProjectDir "build"
    $flashJson = Join-Path $build "flasher_args.json"
    if (-not (Test-Path -LiteralPath $flashJson)) {
        throw "Missing flasher_args.json after build; refusing incomplete package."
    }
    $manifest = Get-Content -LiteralPath $flashJson -Raw | ConvertFrom-Json
    if (-not $manifest.flash_files) { throw "Missing flash_files offsets in firmware manifest." }
    $entries = @($manifest.flash_files.PSObject.Properties)
    if ($entries.Count -lt 3) { throw "Expected at least bootloader, partitions and app flash images." }

    $dist = Join-Path $ProjectDir "dist"
    $stage = Join-Path $dist "opentail-core-esp32s3-flash"
    New-Item -ItemType Directory -Force -Path $dist | Out-Null
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    $report = @(
        "OpenTail CORE ESP32-S3 - LOCAL BENCH BUILD (NOT ROAD CERTIFIED)",
        ("ESP-IDF: " + $version),
        ("Built on: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz")),
        "Do not use erase-flash. This build has no verified independent fallback.",
        "Flash offsets (from ESP-IDF flasher_args.json):"
    )
    $seenApp = $false
    foreach ($entry in $entries) {
        $rel = [string]$entry.Value
        if (-not $rel -or [IO.Path]::IsPathRooted($rel) -or $rel -match '(^|[\\/])\.\.([\\/]|$)') {
            throw "Unexpected flash file path; refusing package: $rel"
        }
        $rel = $rel.Replace("/", "\")
        $source = Join-Path $build $rel
        if (-not (Test-Path -LiteralPath $source) -or (Get-Item -LiteralPath $source).Length -eq 0) {
            throw "Missing/nonempty flash image required by ESP-IDF: $rel"
        }
        if ($rel -eq "opentail_core_alpha.bin") { $seenApp = $true }
        $target = Join-Path $stage $rel
        $targetDir = Split-Path $target -Parent
        New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
        $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        $report += ([string]$entry.Name + "  " + $rel + "  SHA256=" + $hash)
    }
    if (-not $seenApp) { throw "Expected opentail_core_alpha.bin is not listed in the flash manifest." }
    Copy-Item -LiteralPath $flashJson -Destination (Join-Path $stage "flasher_args.json")
    foreach ($optional in @("flash_args", "flash_project_args", "project_description.json")) {
        $from = Join-Path $build $optional
        if (Test-Path -LiteralPath $from) { Copy-Item -LiteralPath $from -Destination (Join-Path $stage $optional) }
    }
    $report += ""
    $report += "Build output was compiled on this PC. SDK credentials are not included in this ZIP."
    $report += "To flash later, open ESP-IDF PowerShell in the ORIGINAL core-firmware directory:"
    $report += "  idf.py -p COMx flash"
    $report += "Only replace COMx with the connected ESP32-S3 serial port AFTER verifying your wiring."
    [IO.File]::WriteAllLines((Join-Path $stage "BUILD-INFO.txt"), [string[]]$report)

    $zip = Join-Path $dist ("opentail-core-esp32s3-pr12-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".zip")
    # -Path wildcard includes contents while preserving bootloader/ and
    # partition_table/ folders; -LiteralPath must NOT be used with '*'.
    Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force
    if ((Get-Item -LiteralPath $zip).Length -le 0) { throw "Firmware ZIP was empty." }
    Write-Host ""
    Write-Host "SUCCESS - Firmware built and packaged:" -ForegroundColor Green
    Write-Host $zip -ForegroundColor Green
    Write-Host "Includes binary images, flash offsets, SHA256 hashes. No flashing was performed."
} finally {
    Set-Location $oldLocation
}
