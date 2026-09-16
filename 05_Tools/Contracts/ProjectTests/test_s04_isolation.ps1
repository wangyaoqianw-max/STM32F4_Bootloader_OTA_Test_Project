$ErrorActionPreference = 'Stop'

$toolsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$s04Root = Join-Path $toolsRoot 'Tests\S04_Persistence'
$newRunner = Join-Path $s04Root 's04_persistence_test.py'
$newGdb = Join-Path $s04Root 's04_reset_persistence.gdb'
$newEntry = Join-Path $s04Root 's04_persistence_test.bat'
$legacyRunner = Join-Path $toolsRoot 'Scripts\s04_persistence_test.py'
$legacyGdb = Join-Path $toolsRoot 'Debug\GDB\s04_reset_persistence.gdb'
$legacyEntry = Join-Path $toolsRoot 'Scripts\s04_persistence_test.bat'
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

Assert-True (Test-Path -LiteralPath $newRunner -PathType Leaf) 'S04 runner was not isolated under Tests/S04_Persistence'
Assert-True (Test-Path -LiteralPath $newGdb -PathType Leaf) 'S04 GDB asset was not isolated under Tests/S04_Persistence'
Assert-True (Test-Path -LiteralPath $newEntry -PathType Leaf) 'S04 project-test entry is missing'
Assert-True (Test-Path -LiteralPath $legacyEntry -PathType Leaf) 'S04 legacy BAT entry is missing'
Assert-True (-not (Test-Path -LiteralPath $legacyRunner -PathType Leaf)) 'S04 runner remains in the generic Scripts directory'
Assert-True (-not (Test-Path -LiteralPath $legacyGdb -PathType Leaf)) 'S04 GDB asset remains in the generic Debug directory'

if (Test-Path -LiteralPath $legacyEntry -PathType Leaf) {
    $legacyText = Get-Content -LiteralPath $legacyEntry -Raw
    Assert-Contains $legacyText 'Tests\\S04_Persistence\\s04_persistence_test\.bat' 'Legacy S04 BAT must delegate to the isolated project test entry'
}

$genericText = (Get-ChildItem (Join-Path $toolsRoot 'Core'), (Join-Path $toolsRoot 'Adapters'), (Join-Path $toolsRoot 'Workflows') -Recurse -File |
    ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine
Assert-True ($genericText -notmatch 'S04_PERSISTENCE|app_s04_persistence|s04_persistence|COM9') 'Generic toolkit layers contain S04-specific symbols or paths'

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[S04Isolation][FAIL] $_" }
    exit 1
}

'[S04Isolation][PASS] S04 project-test boundary is isolated.'
exit 0
