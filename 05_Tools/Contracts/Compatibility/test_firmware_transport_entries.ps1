$ErrorActionPreference = 'Stop'

$toolsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$router = Get-Content -LiteralPath (Join-Path $toolsRoot 'toolkit.ps1') -Raw
$routerBat = Get-Content -LiteralPath (Join-Path $toolsRoot 'toolkit.bat') -Raw
$pythonLegacy = Get-Content -LiteralPath (Join-Path $toolsRoot 'Scripts\send_ymodem_python.bat') -Raw
$teraLegacy = Get-Content -LiteralPath (Join-Path $toolsRoot 'Scripts\send_ymodem.bat') -Raw
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $failures.Add($Message)
    }
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    Assert-True ($Text -match $Pattern) $Message
}

Assert-Contains $routerBat 'toolkit\.ps1' 'toolkit.bat remains the stable process entry'
Assert-Contains $router 'Firmware\\pack_firmware\.py' 'Router exposes the existing Firmware packer'
Assert-Contains $router 'Ymodem\\ymodem_sender\.py' 'Router exposes the existing Python YMODEM sender'
Assert-Contains $router 'TeraTerm\\send_ymodem\.ttl' 'Router exposes the existing Tera Term macro'
Assert-Contains $router 'firmware' 'Router accepts the firmware command'
Assert-Contains $router 'ymodem' 'Router accepts the ymodem command'

foreach ($entry in @(@{Text = $pythonLegacy; Name = 'send_ymodem_python.bat'; Mode = 'python'}, @{Text = $teraLegacy; Name = 'send_ymodem.bat'; Mode = 'tera'})) {
    Assert-Contains $entry.Text 'toolkit\.bat' "$($entry.Name) must delegate to toolkit.bat"
    Assert-Contains $entry.Text ("ymodem.*{0}" -f $entry.Mode) "$($entry.Name) must preserve its $($entry.Mode) route"
    Assert-True ($entry.Text -notmatch 'PYTHON_EXE|TERA_TERM_EXE|ttpmacro|TeraTerm|ymodem_sender\.py') "$($entry.Name) must not own transport tool setup"
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[Transport][FAIL] $_" }
    exit 1
}

'[Transport][PASS] Firmware and YMODEM entry contracts passed.'
exit 0
