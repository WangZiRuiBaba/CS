$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
dotnet publish "$root\CS2Tune\CS2Tune.csproj" -c Release -r win-x64 --self-contained false -o "$root\build\Release\Tune"
Write-Host "Output: $root\build\Release\Tune\CS2Tune.exe"
