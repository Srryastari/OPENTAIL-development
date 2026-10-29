# OpenTail CORE: fetch a CI-approved flash package directly from GitHub.
# No assumptions about the user's Desktop, Downloads, current directory,
# extracted ZIP layout, or previously flashed firmware.
# Requires: GitHub CLI (gh, signed in) and Python with esptool.
# Usage: .\flash-from-github.ps1 -Port COM9
[CmdletBinding()]
param(
    [string]$Port = "COM9",
    [string]$Version = "0.1.31-alpha",
    [Nullable[long]]$RunId = $null,
    [switch]$VerifyOnly,
    [switch]$BenchCandidate
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
    if ($LASTEXITCODE -ne 0) { throw "Could not fetch GitHub build runs." }
    $build = @($runsText | ConvertFrom-Json) | Where-Object {
        $_.status -eq 'completed' -and $_.headBranch -eq $branch
    } | Select-Object -First 1
    if (-not $build) { throw "No completed GitHub build for branch $branch. Nothing was flashed." }
    $RunId = [long]$build.databaseId
}
$runText = & gh run view ([string]$RunId) --repo $repo --json conclusion,status,headBranch,headSha,jobs
if ($LASTEXITCODE -ne 0) { throw "Could not verify GitHub run $RunId." }
$run = $runText | ConvertFrom-Json
# Verification may inspect an explicitly selected PR run before its
# physical release gate completes. Actual flashing remains target-branch-only.
if (-not $VerifyOnly -and ($run.headBranch -ne $branch -or $run.status -ne 'completed')) {
    throw "GitHub run not a completed target branch run. Nothing was flashed."
}
# A software-verified bench candidate must be installable BEFORE its physical
# release gate can pass; otherwise physical acceptance is impossible.
$required = @('change-impact-audit','core-host-tests','core-espidf-build')
if (-not $VerifyOnly) {
    $required += @('web-studio-check','android-debug-apk','github-flash-package-check')
}
foreach ($name in $required) {
    $job = @($run.jobs | Where-Object { $_.name -eq $name } | Select-Object -First 1)
    if ($job.Count -ne 1 -or $job[0].conclusion -ne 'success') {
        throw "Required software CI job '$name' has not passed. Nothing was flashed."
    }
}
if (-not $VerifyOnly -and $run.conclusion -ne 'success' -and -not $BenchCandidate) {
    throw "Physical release approval is not complete. Use -BenchCandidate ONLY for the isolated bench after reviewing the release blockers."
}

# A completed run is not necessarily the current firmware candidate. GitHub
# pull_request CI may use a synthetic merge commit, whose second parent is the
# firmware head. Accept only the exact head or its tested PR merge commit.
# Read-only -VerifyOnly intentionally permits reviewing older/other PR runs.
if (-not $VerifyOnly) {
    $headText = & gh api "repos/$repo/git/ref/heads/$branch" --jq '.object.sha'
    if ($LASTEXITCODE -ne 0) { throw "Could not resolve current firmware branch head. Nothing was flashed." }
    $currentHead = ([string]$headText).Trim()
    if ($currentHead -notmatch '^[0-9a-fA-F]{40}$') {
        throw "Invalid current firmware branch commit. Nothing was flashed."
    }
    if ($run.headSha -ne $currentHead) {
        $parents = @(& gh api "repos/$repo/commits/$($run.headSha)" --jq '.parents[].sha')
        if ($LASTEXITCODE -ne 0 -or $parents -notcontains $currentHead) {
            throw "GitHub run $RunId does not test the current firmware branch head. Nothing was flashed."
        }
    }
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
    Write-Host "BENCH ONLY: flashing CORE $Version on $Port; this does not imply physical release approval. Close serial monitors." -ForegroundColor Yellow
    Push-Location $flashDir
    try {
        & python -m esptool --chip esp32s3 --port $Port --baud 460800 write_flash '@flash_args'
        if ($LASTEXITCODE -ne 0) { throw "esptool failed (code $LASTEXITCODE)." }
        Write-Host "Flash command completed for $Version. Reconnect at http://192.168.4.1/dev and confirm version." -ForegroundColor Green
    } finally { Pop-Location }
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
