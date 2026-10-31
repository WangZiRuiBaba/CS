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

$hours = 3
if ($args.Count -ge 1) { $hours = [double]$args[0] }
$seconds = [int][Math]::Round($hours * 3600)
if ($seconds -lt 60) { throw "expiry must be at least 60 seconds" }

Get-Process -Name CS2_ESP -ErrorAction SilentlyContinue | Stop-Process -Force

& $cmake -B "$root\build" -A x64 `
    -DCS2_ESP_RETAIL=ON `
    -DCS2_ESP_EXPIRY_SECONDS=$seconds `
    -DCS2_ESP_LICENSE_VARIANT_TEST=ON
& $cmake --build "$root\build" --config Release

$out = "$root\build\Release\CS2_ESP.exe"
$destDir = "$root\release\test"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
Copy-Item -Force $out "$destDir\CS2_ESP.exe"

$readme = @"
================================================================================
  CS2 游戏辅助 — 测试版使用说明
================================================================================

【测试版说明】
  本包为内部/体验测试版。自本机首次运行 CS2_ESP.exe 起，可用时长约 ${hours} 小时
  （到期后提示与正式版相同，需联系卖家获取正式版）。

  测试版授权与本机正式版分开记录，互不影响。

【功能与用法】
  与正式版相同：透视（方框、骨骼）+ 辅助瞄准。详见同目录功能说明或正式版使用说明。

  1. 先开 CS2，再双击 CS2_ESP.exe
  2. 游戏内：Insert 方框 · Page Up 骨骼 · Home 辅助瞄准常开 · 侧键按住瞄准 · End 退出

【游戏大更新】
  若提示「游戏内容已变化」，需获取适配新游戏版本的程序（与是否测试版无关）。

================================================================================
"@
Set-Content -Path "$destDir\使用说明.txt" -Value $readme -Encoding UTF8

$zip = Join-Path $root "release\CS2_ESP_test.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Push-Location $destDir
try {
    Compress-Archive -Path "CS2_ESP.exe", "使用说明.txt" -DestinationPath $zip -Force
} finally {
    Pop-Location
}

Write-Host "Test retail build (${hours}h / ${seconds}s expiry): $destDir\CS2_ESP.exe"
Write-Host "Zip: $zip"
