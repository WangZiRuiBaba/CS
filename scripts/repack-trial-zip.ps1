$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "write-utf8.ps1")
$root = Split-Path $PSScriptRoot -Parent
$hours = 5
$trialDir = Join-Path $root "release\trial_esp_5h"
$readmePath = Join-Path $trialDir (Get-ReadmeFileName)
$zip = Join-Path $root "release\CS2_ESP_trial_esp_${hours}h.zip"
$exe = Join-Path $trialDir "CS2_ESP.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing $exe" }
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -LiteralPath @($exe, $readmePath) -DestinationPath $zip -Force
Write-Host "Zip: $zip"
