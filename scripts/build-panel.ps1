$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
dotnet publish "$root\CS2Panel\CS2Panel.csproj" -c Release -r win-x64 --self-contained false -o "$root\build\Release\Panel"
Write-Host "Output: $root\build\Release\Panel\CS2Panel.exe"
Write-Host "锁头参数: $root\scripts\build-tune.ps1 -> build\Release\Tune\CS2Tune.exe"
