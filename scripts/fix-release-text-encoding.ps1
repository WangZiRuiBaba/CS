$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot "write-utf8.ps1")

function Convert-FileToUtf8Bom {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $enc = New-Object System.Text.UTF8Encoding $false
    $raw = [System.IO.File]::ReadAllText($Path, $enc)
    Write-Utf8TextFile -Path $Path -Content $raw
}

$releaseDir = Join-Path $root "release"
$tracked = @(
    (Join-Path $releaseDir (Get-ReadmeFileName)),
    (Join-Path $releaseDir (Get-ProductDescFileName))
)
foreach ($p in $tracked) { Convert-FileToUtf8Bom -Path $p }

$trialDir = Join-Path $root "release\trial_esp_5h"
if (Test-Path -LiteralPath $trialDir) {
    Get-ChildItem -LiteralPath $trialDir -Filter "*.txt" | Remove-Item -Force
    $hours = 5
    $readme = Read-TextTemplate -Path (Join-Path $PSScriptRoot "templates\trial-esp-readme.txt") -Replacements @{
        '{{HOURS}}' = $hours
    }
    $readmePath = Join-Path $trialDir (Get-ReadmeFileName)
    Write-Utf8TextFile -Path $readmePath -Content $readme
    $zip = Join-Path $root "release\CS2_ESP_trial_esp_${hours}h.zip"
    $exe = Join-Path $trialDir "CS2_ESP.exe"
    if (Test-Path -LiteralPath $exe) {
        if (Test-Path $zip) { Remove-Item -Force $zip }
        Compress-Archive -LiteralPath @($exe, $readmePath) -DestinationPath $zip -Force
    }
}

Write-Host "UTF-8 BOM text files updated under release\"
