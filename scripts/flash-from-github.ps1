# OpenTail CORE: fetch a CI-approved flash package directly from GitHub.
# No assumptions about the user's Desktop, Downloads, current directory,
# extracted ZIP layout, or previously flashed firmware.
# Requires: GitHub CLI (gh, signed in) and Python with esptool.
# Usage: .\flash-from-github.ps1 -Port COM9
[CmdletBinding()]
param(
    [string]$Port = "COM9",
    [string]$Version = "0.1.29-alpha",
    [Nullable[long]]$RunId = $null,
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
$repo = 'Srryastari/OPENTAIL-development'
$branch = 'integration/core-v0.1.25-unified-effects-status'
$artifactName = "opentail-core-v$Version-esp32s3-bench-firmware"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI not found. Install GitHub CLI and sign in with 'gh auth login'. Nothing was flashed."
}
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw "Python (with esptool) not found. Nothing was flashed."
}
gh auth status *> $null
if ($LASTEXITCODE -ne 0) { throw "Sign in to GitHub CLI using 'gh auth login'. Nothing was flashed." }

if (-not $RunId) {
    $runsText = & gh run list --repo $repo --workflow 'Verify OpenTail active sources' --branch $branch --limit 30 --json databaseId,conclusion,status,headBranch
    if ($LASTEXITCODE -ne 0) { throw "Could not fetch successful GitHub build runs." }
    $build = @($runsText | ConvertFrom-Json) | Where-Object {
        $_.conclusion -eq 'success' -and $_.status -eq 'completed' -and $_.headBranch -eq $branch
    } | Select-Object -First 1
    if (-not $build) { throw "No successful GitHub build for branch $branch. Nothing was flashed." }
    $RunId = [long]$build.databaseId
}
$runText = & gh run view ([string]$RunId) --repo $repo --json conclusion,status,headBranch,headSha
if ($LASTEXITCODE -ne 0) { throw "Could not verify GitHub run $RunId." }
$run = $runText | ConvertFrom-Json
if ($run.status -ne 'completed' -or $run.conclusion -ne 'success' -or $run.headBranch -ne $branch) {
    throw "GitHub run not approved (successful completed build on $branch required). Nothing was flashed."
}

# Stage the artifact in a temporary directory only. Never overwrite an old
# build or depend on the console's current working directory.
$work = Join-Path ([IO.Path]::GetTempPath()) ("OpenTail-GitHub-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
try {
    & gh run download ([string]$RunId) --repo $repo --name $artifactName --dir $work
    if ($LASTEXITCODE -ne 0) { throw "GitHub artifact $artifactName unavailable. Nothing was flashed." }
    $manifests = @(Get-ChildItem -LiteralPath $work -Recurse -Filter flash_args -File)
    if ($manifests.Count -ne 1) { throw "Expected exactly one flash_args manifest; found $($manifests.Count). Nothing was flashed." }
    $flashDir = $manifests[0].DirectoryName
    $app = Join-Path $flashDir 'opentail_core_alpha.bin'
    $json = Join-Path $flashDir 'flasher_args.json'
    if (-not (Test-Path -LiteralPath $app) -or -not (Test-Path -LiteralPath $json)) {
        throw "GitHub package is missing the app or flash metadata. Nothing was flashed."
    }
    if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($app)).Contains($Version)) {
        throw "Downloaded firmware does not contain expected version $Version. Nothing was flashed."
    }
    $flashSpec = Get-Content -LiteralPath $json -Raw | ConvertFrom-Json
    $files = $flashSpec.flash_files
    if ($null -eq $files) { throw "Flash metadata missing flash_files. Nothing was flashed." }
    $images = @($files.PSObject.Properties)
    if ($images.Count -lt 3) { throw "Flash metadata missing bootloader/partition/app images. Nothing was flashed." }
    foreach ($file in $images) {
        $name = [string]$file.Value
        if ([IO.Path]::IsPathRooted($name) -or $name.Contains('..')) {
            throw "Unsafe flash image path in manifest. Nothing was flashed."
        }
        $p = Join-Path $flashDir $name
        if (-not (Test-Path -LiteralPath $p -PathType Leaf) -or (Get-Item -LiteralPath $p).Length -eq 0) {
            throw "Flash image missing: $name. Nothing was flashed."
        }
    }
    Write-Host "GitHub run $RunId ($($run.headSha)) verified: CORE $Version, $($images.Count) images." -ForegroundColor Green
    if ($VerifyOnly) {
        Write-Host 'Verification only: no serial port opened and nothing flashed.'
        return
    }
    Write-Host "Flashing CORE $Version on $Port. Close any serial monitor first."
    Push-Location $flashDir
    try {
        & python -m esptool --chip esp32s3 --port $Port --baud 460800 write_flash '@flash_args'
        if ($LASTEXITCODE -ne 0) { throw "esptool failed (code $LASTEXITCODE)." }
        Write-Host "Flash command completed for $Version. Reconnect at http://192.168.4.1/dev and confirm version." -ForegroundColor Green
    } finally { Pop-Location }
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
