function Write-Utf8TextFile {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$Content
    )
    $enc = New-Object System.Text.UTF8Encoding $true
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Content, $enc)
}

function Get-ReadmeFileName {
    # 使用说明.txt — avoids .ps1 source encoding issues on Windows PowerShell 5.1
    return -join ([char]0x4F7F, [char]0x7528, [char]0x8BF4, [char]0x660E, '.txt')
}

function Get-ProductDescFileName {
    return -join ([char]0x5546, [char]0x54C1, [char]0x8BF4, [char]0x660E, '.txt')
}

function Read-TextTemplate {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [hashtable]$Replacements = @{}
    )
    $enc = New-Object System.Text.UTF8Encoding $false
    $text = [System.IO.File]::ReadAllText($Path, $enc)
    foreach ($key in $Replacements.Keys) {
        $text = $text.Replace($key, [string]$Replacements[$key])
    }
    return $text
}
