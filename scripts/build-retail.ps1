$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

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

$days = 3
if ($args.Count -ge 1) { $days = [int]$args[0] }

Get-Process -Name CS2_ESP -ErrorAction SilentlyContinue | Stop-Process -Force

& $cmake -B "$root\build" -A x64 -DCS2_ESP_RETAIL=ON "-DCS2_ESP_EXPIRY_DAYS=$days" `
    -DCS2_ESP_EXPIRY_SECONDS=0 -DCS2_ESP_LICENSE_VARIANT_TEST=OFF
& $cmake --build "$root\build" --config Release

$out = "$root\build\Release\CS2_ESP.exe"
$destDir = "$root\release"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
Copy-Item -Force $out "$destDir\CS2_ESP.exe"
Write-Host "Retail build (${days}-day expiry): $destDir\CS2_ESP.exe"
