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

Get-Process -Name CS2_ESP -ErrorAction SilentlyContinue | ForEach-Object {
    try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { Write-Warning "Close CS2_ESP manually, then rebuild." }
}
# Daily/dev build: no license timer (retail uses build-retail.ps1 / build-test-retail.ps1).
& $cmake -B "$root\build" -A x64 `
    -DCS2_ESP_RETAIL=OFF `
    -DCS2_ESP_LICENSE_VARIANT_TEST=OFF
& $cmake --build "$root\build" --config Release
Write-Host "Output: $root\build\Release\CS2_ESP.exe"
