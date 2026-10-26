$ErrorActionPreference = 'Stop'
$project = $PSScriptRoot
Set-Location $project

function Get-JavaMajor([string]$javaExe) {
    if (-not (Test-Path $javaExe)) { return 0 }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $v = (& $javaExe -version 2>&1 | Out-String) } finally { $ErrorActionPreference = $old }
    if ($v -match 'version\s+"(?:1\.)?(\d+)') { return [int]$Matches[1] }
    return 0
}

Write-Host 'Checking JDK 17 or 21 ...'
$candidates = [System.Collections.Generic.List[string]]::new()
if ($env:JAVA_HOME) { $candidates.Add($env:JAVA_HOME) }
foreach ($parent in @(
   "$env:USERPROFILE\.jdks",
   "$env:ProgramFiles\Eclipse Adoptium",
   "$env:ProgramFiles\Java",
   "$env:ProgramFiles\Microsoft",
   "$env:LOCALAPPDATA\Programs\Eclipse Adoptium",
   "$env:ProgramFiles\Android\Android Studio"
)) {
    if (-not (Test-Path $parent)) { continue }
    if (Test-Path (Join-Path $parent 'bin\java.exe')) { $candidates.Add($parent) }
    Get-ChildItem $parent -Directory -ErrorAction SilentlyContinue | ForEach-Object { $candidates.Add($_.FullName) }
}
$javaCmd = Get-Command java.exe -ErrorAction SilentlyContinue
if ($javaCmd) { $candidates.Add((Split-Path (Split-Path $javaCmd.Source -Parent) -Parent)) }
$jdk = $null
foreach ($c in ($candidates | Select-Object -Unique)) {
    $major = Get-JavaMajor (Join-Path $c 'bin\java.exe')
    if ($major -eq 17 -or $major -eq 21) { $jdk = $c; break }
}
if (-not $jdk) { throw 'Java 17/21 was not located. In Android Studio check the Gradle JDK 17 location; install a JDK 17 and try again. Do NOT use Java 27.' }
$env:JAVA_HOME = $jdk
$env:Path = (Join-Path $jdk 'bin') + ';' + $env:Path
Write-Host "Using JDK at $jdk"

$sdk = $null
foreach ($p in @($env:ANDROID_SDK_ROOT, $env:ANDROID_HOME, "$env:LOCALAPPDATA\Android\Sdk")) {
    if ($p -and (Test-Path (Join-Path $p 'platforms'))) { $sdk = $p; break }
}
if (-not $sdk) { throw 'Android SDK not found. Open the project in Android Studio once so the SDK can install, then try again.' }
$env:ANDROID_HOME = $sdk
$env:ANDROID_SDK_ROOT = $sdk
$sdkForward = $sdk.Replace('\','/')
[System.IO.File]::WriteAllText((Join-Path $project 'local.properties'), "sdk.dir=$sdkForward`n", [System.Text.UTF8Encoding]::new($false))
Write-Host "Using Android SDK at $sdk"

$gradle = $null
$installed = Get-Command gradle.bat -ErrorAction SilentlyContinue
if ($installed -and $installed.Source -match '8[.]11[.]1') { $gradle = $installed.Source }
if (-not $gradle) {
    $cache = Join-Path $env:USERPROFILE '.gradle\wrapper\dists\gradle-8.11.1-bin'
    if (Test-Path $cache) {
        $gradle = Get-ChildItem $cache -Recurse -Filter gradle.bat -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'gradle-8[.]11[.]1\\bin\\gradle[.]bat$' } |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $gradle) {
    $dest = Join-Path $env:USERPROFILE '.gradle\opentail-dist'
    $zip = Join-Path $dest 'gradle-8.11.1-bin.zip'
    $gradle = Join-Path $dest 'gradle-8.11.1\bin\gradle.bat'
    if (-not (Test-Path $gradle)) {
        New-Item -ItemType Directory -Path $dest -Force | Out-Null
        Write-Host 'Downloading official Gradle 8.11.1 (first build only)...'
        Invoke-WebRequest 'https://services.gradle.org/distributions/gradle-8.11.1-bin.zip' -OutFile $zip
        Expand-Archive -LiteralPath $zip -DestinationPath $dest -Force
        Remove-Item $zip -Force -ErrorAction SilentlyContinue
    }
}
if (-not (Test-Path $gradle)) { throw 'Gradle 8.11.1 executable not found after setup.' }
Write-Host "Gradle: $gradle"
Write-Host 'Building APK (first build may download Android dependencies)...'
& $gradle --no-daemon :app:assembleDebug
if ($LASTEXITCODE -ne 0) { throw "Gradle build failed with exit code $LASTEXITCODE" }
$apk = Join-Path $project 'app\build\outputs\apk\debug\app-debug.apk'
if (-not (Test-Path $apk)) { throw 'Gradle returned success but app-debug.apk was not found.' }
Write-Host ''
Write-Host 'SUCCESS! Your APK is here:' -ForegroundColor Green
Write-Host $apk -ForegroundColor Green
Start-Process explorer.exe -ArgumentList @('/select,', ('"' + $apk + '"'))
