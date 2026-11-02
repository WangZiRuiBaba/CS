$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot "write-utf8.ps1")

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$cmake = $null
if (Test-Path $vswhere) {
    $install = & $vswhere -latest -property installationPath
    if ($install) {
        $candidate = Join-Path $install "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $candidate) { $cmake = $candidate }
    }
}
if (-not $cmake) { $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source }
if (-not $cmake) { throw "cmake not found" }

$hours = 5
if ($args.Count -ge 1) { $hours = [double]$args[0] }
$seconds = [int][Math]::Round($hours * 3600)
if ($seconds -lt 60) { throw "expiry must be at least 60 seconds" }

Get-Process -Name CS2_ESP -ErrorAction SilentlyContinue | Stop-Process -Force

& $cmake -B "$root\build" -A x64 `
    -DCS2_ESP_RETAIL=ON `
    -DCS2_ESP_EXPIRY_SECONDS=$seconds `
    -DCS2_ESP_LICENSE_VARIANT_TEST=OFF `
    -DCS2_ESP_LICENSE_VARIANT_TRIAL_ESP=ON `
    -DCS2_ESP_ESP_ONLY=ON
& $cmake --build "$root\build" --config Release

$out = "$root\build\Release\CS2_ESP.exe"
$destDir = "$root\release\trial_esp_5h"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
Copy-Item -Force $out "$destDir\CS2_ESP.exe"

$readme = Read-TextTemplate -Path (Join-Path $PSScriptRoot "templates\trial-esp-readme.txt") -Replacements @{
    '{{HOURS}}' = $hours
}
$readmeFile = Get-ReadmeFileName
$readmePath = Join-Path $destDir $readmeFile
Get-ChildItem -LiteralPath $destDir -Filter "*.txt" -ErrorAction SilentlyContinue | Remove-Item -Force
Write-Utf8TextFile -Path $readmePath -Content $readme

$zip = Join-Path $root "release\CS2_ESP_trial_esp_${hours}h.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
$zipItems = @(
    (Join-Path $destDir "CS2_ESP.exe"),
    $readmePath
)
Compress-Archive -LiteralPath $zipItems -DestinationPath $zip -Force

Write-Host "Trial ESP-only build (${hours}h / ${seconds}s, machine-bound): $destDir\CS2_ESP.exe"
Write-Host "Zip: $zip"
