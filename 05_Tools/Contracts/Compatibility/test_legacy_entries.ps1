$ErrorActionPreference = 'Stop'

$toolsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$routerBat = Join-Path $toolsRoot 'toolkit.bat'
$routerPs1 = Join-Path $toolsRoot 'toolkit.ps1'
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

function Assert-File {
    param(
        [string]$Path,
        [string]$Message
    )

    Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) $Message
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    Assert-True ($Text -match $Pattern) $Message
}

function Assert-NotContains {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    Assert-True ($Text -notmatch $Pattern) $Message
}

Assert-File -Path $routerBat -Message 'toolkit.bat is missing'
Assert-File -Path $routerPs1 -Message 'toolkit.ps1 is missing'

$entries = [ordered]@{
    'build_app.bat' = 'build'
    'flash_app.bat' = 'flash'
    'rtt_capture.bat' = 'rtt'
    'run_app_cycle.bat' = 'run'
    'gdb_runtime_snapshot.bat' = 'snapshot'
    'gdb_fault_capture.bat' = 'fault'
}

foreach ($entry in $entries.GetEnumerator()) {
    $path = Join-Path $toolsRoot ("Scripts\{0}" -f $entry.Key)
    Assert-File -Path $path -Message "Legacy entry is missing: $($entry.Key)"
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        $content = Get-Content -LiteralPath $path -Raw
        Assert-Contains $content 'toolkit\.bat' "$($entry.Key) must delegate to toolkit.bat"
        Assert-NotContains $content 'KEIL_UV4|JLINK_EXE|JLINK_GDB_SERVER|ARM_GDB|JLINK_RTT_LOGGER|powershell\.exe' "$($entry.Key) must not own toolchain commands"
        Assert-Contains $content ([regex]::Escape($entry.Value)) "$($entry.Key) must select the $($entry.Value) command"
    }
}

if ((Test-Path -LiteralPath $routerBat -PathType Leaf) -and (Test-Path -LiteralPath $routerPs1 -PathType Leaf)) {
    $router = Get-Content -LiteralPath $routerPs1 -Raw
    $routerEntry = Get-Content -LiteralPath $routerBat -Raw
    Assert-Contains $routerEntry 'toolkit\.ps1' 'toolkit.bat must invoke toolkit.ps1 beside itself'
    Assert-Contains $router 'build\.ps1' 'Router must expose build workflow'
    Assert-Contains $router 'flash\.ps1' 'Router must expose flash workflow'
    Assert-Contains $router 'rtt\.ps1' 'Router must expose RTT workflow'
    Assert-Contains $router 'run\.ps1' 'Router must expose application cycle workflow'
    Assert-Contains $router 'snapshot\.ps1' 'Router must expose snapshot workflow'
    Assert-Contains $router 'fault_capture\.ps1' 'Router must expose Fault workflow'
    Assert-Contains $router '20|30|40|50|60' 'Router must preserve stage exit classes'
}

if ($failures.Count -eq 0) {
    $unknownOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $routerPs1 unknown 2>&1
    $unknownExitCode = $LASTEXITCODE
    Assert-True ($unknownExitCode -ne 0) 'Unknown toolkit command must fail'
    Assert-Contains ($unknownOutput -join [Environment]::NewLine) 'Unknown|unknown' 'Unknown toolkit command must report an error'
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[Compatibility][FAIL] $_" }
    exit 1
}

'[Compatibility][PASS] Router and legacy entry contracts passed.'
exit 0
