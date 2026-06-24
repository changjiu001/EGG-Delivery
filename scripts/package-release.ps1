# EGG-Delivery Windows Release packager (MSYS2 UCRT64 + Qt 6)
# Usage: powershell -ExecutionPolicy Bypass -File scripts/package-release.ps1

$ErrorActionPreference = "Stop"
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Test-Path (Join-Path $root "CMakeLists.txt"))) {
    $root = Split-Path $PSScriptRoot -Parent
}

$msysBin = "C:\msys64\ucrt64\bin"
$env:Path = "$msysBin;C:\msys64\usr\bin;" + $env:Path

$buildDir = Join-Path $root "build-qt-release"
$releaseDir = Join-Path $root "release\LanChatQt"
$exeName = "LanChatQt.exe"

Write-Host "==> Configure Release build"
cmake -S $root -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_QT_GUI=ON
cmake --build $buildDir

Write-Host "==> Prepare release folder"
if (Test-Path $releaseDir) { Remove-Item -Recurse -Force $releaseDir }
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
Copy-Item (Join-Path $buildDir "qt_gui\$exeName") $releaseDir

Push-Location $releaseDir
Write-Host "==> Deploy Qt runtime"
windeployqt --release --compiler-runtime --no-translations $exeName

Write-Host "==> Copy MSYS2/UCRT64 runtime DLLs"
$copied = [System.Collections.Generic.HashSet[string]]::new()
function Copy-Deps([string]$binary) {
    $lines = ldd $binary 2>&1
    foreach ($line in $lines) {
        if ($line -match '=> /ucrt64/bin/([^\s\(]+)') {
            $dll = $Matches[1]
            if ($copied.Add($dll)) {
                Copy-Item (Join-Path $msysBin $dll) $releaseDir -Force
                Copy-Deps (Join-Path $releaseDir $dll)
            }
        }
    }
}
Copy-Deps (Join-Path $releaseDir $exeName)
Pop-Location

$zipPath = Join-Path $root "release\LanChatQt-win64.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path $releaseDir -DestinationPath $zipPath

Write-Host ""
Write-Host "Done."
Write-Host "Folder: $releaseDir"
Write-Host "Zip:    $zipPath"
Write-Host "Send the whole folder or zip to others. Do NOT send only the exe."
