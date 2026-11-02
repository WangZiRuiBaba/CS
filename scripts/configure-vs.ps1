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

& $cmake -S $root -B "$root\build" -A x64
Write-Host "Visual Studio native projects: $root\build\CS2_ESP.vcxproj"
Write-Host "Open CS2.sln and build project CS2_ESP (Release|x64)."
