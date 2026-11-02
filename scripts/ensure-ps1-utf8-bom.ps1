$enc = New-Object System.Text.UTF8Encoding $true
$files = @(
    (Join-Path $PSScriptRoot "build-trial-esp-5h.ps1"),
    (Join-Path $PSScriptRoot "build-test-retail.ps1"),
    (Join-Path $PSScriptRoot "fix-release-text-encoding.ps1"),
    (Join-Path $PSScriptRoot "write-utf8.ps1")
)
foreach ($f in $files) {
    if (-not (Test-Path -LiteralPath $f)) { continue }
    $t = [System.IO.File]::ReadAllText($f)
    $t = $t.TrimStart([char]0xFEFF)
    [System.IO.File]::WriteAllText($f, $t, $enc)
}
