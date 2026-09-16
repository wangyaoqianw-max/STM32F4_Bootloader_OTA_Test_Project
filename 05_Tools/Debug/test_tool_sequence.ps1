$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$flashBat = Join-Path $repoRoot '05_Tools\Scripts\flash_app.bat'
$cycleBat = Join-Path $repoRoot '05_Tools\Scripts\run_app_cycle.bat'
$faultBat = Join-Path $repoRoot '05_Tools\Scripts\gdb_fault_capture.bat'
$faultPs1 = Join-Path $repoRoot '05_Tools\Scripts\gdb_fault_capture.ps1'
$gdbSessionPs1 = Join-Path $repoRoot '05_Tools\Adapters\Debug\GDB\gdb_session.ps1'
$routerPs1 = Join-Path $repoRoot '05_Tools\toolkit.ps1'
$faultTriggerGdb = Join-Path $repoRoot '05_Tools\Debug\GDB\fault_trigger_capture.gdb'
$readme = Join-Path $repoRoot '05_Tools\README.md'

$failures = [System.Collections.Generic.List[string]]::new()

function Require-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        $failures.Add("missing file: $Path")
    }
}

function Require-Text([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        $failures.Add("missing contract: $Description")
    }
}

foreach ($path in @($flashBat, $cycleBat, $faultBat, $faultPs1, $gdbSessionPs1, $routerPs1, $faultTriggerGdb, $readme)) {
    Require-File $path
}

if ($failures.Count -eq 0) {
    $flash = Get-Content -LiteralPath $flashBat -Raw
    $cycle = Get-Content -LiteralPath $cycleBat -Raw
    $faultBatText = Get-Content -LiteralPath $faultBat -Raw
    $faultPs = Get-Content -LiteralPath $faultPs1 -Raw
    $gdbSession = Get-Content -LiteralPath $gdbSessionPs1 -Raw
    $router = Get-Content -LiteralPath $routerPs1 -Raw
    $trigger = Get-Content -LiteralPath $faultTriggerGdb -Raw
    $toolsReadme = Get-Content -LiteralPath $readme -Raw

    if (($trigger.IndexOf('target remote') -lt 0) -or
        ($trigger.IndexOf('monitor reset') -le $trigger.IndexOf('target remote'))) {
        $failures.Add('GDB client must attach before monitor reset')
    }
    if (($gdbSession.IndexOf('Starting J-Link GDB Server') -lt 0) -or
        ($gdbSession.IndexOf('Start-ToolkitProcess') -le $gdbSession.IndexOf('Starting J-Link GDB Server'))) {
        $failures.Add('GDB Server must start before the GDB client')
    }
    if (($faultPs.IndexOf('Invoke-GdbSession') -lt 0) -or
        ($faultPs.IndexOf('Invoke-JLinkRtt') -le $faultPs.IndexOf('Invoke-GdbSession'))) {
        $failures.Add('RTT Logger must start after GDB releases J-Link')
    }

    Require-Text $router 'flash' 'Flash route exists'
    Require-Text $router 'prepare' 'Flash route supports prepare mode'
    Require-Text $router 'run' 'Flash route supports run mode'
    Require-Text $cycle 'toolkit\.bat.*run' 'Normal cycle explicitly uses run mode'
    Require-Text $router 'fault' 'Fault route exists'
    Require-Text $router 'trigger' 'Fault route supports reset-before-trigger mode'
    Require-Text $trigger 'monitor reset' 'Trigger GDB session resets through an already-open GDB connection'
    Require-Text $trigger 'continue&' 'Trigger GDB session runs the test firmware'
    Require-Text $trigger 'diagnostics_fault_capture_stop' 'Trigger GDB session breaks after project Fault capture'
    Require-Text $trigger '(?im)^\s*detach\s*$' 'Trigger GDB session detaches without running again'
    Require-Text $faultBatText 'toolkit\.bat' 'Fault legacy entry delegates to Router'
    Require-Text (Get-Content -LiteralPath (Join-Path $repoRoot '05_Tools\Workflows\Debug\fault_capture.ps1') -Raw) 'J-Link' 'Fault workflow records J-Link ownership ordering'
    Require-Text $toolsReadme 'prepare.*reset|reset.*prepare' 'Tool README records pre-reset ordering'
    Require-Text $toolsReadme 'J-Link.*(占用|ownership|one)' 'Tool README records single J-Link ownership'
}

if ($failures.Count -gt 0) {
    Write-Output '[ToolSequence][FAIL] tool ordering contract is not satisfied.'
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}

Write-Output '[ToolSequence][PASS] tool ordering contract is satisfied.'
exit 0
