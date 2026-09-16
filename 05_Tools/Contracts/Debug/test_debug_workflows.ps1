$ErrorActionPreference = 'Stop'

$toolsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$debugRoot = Join-Path $toolsRoot 'Debug\GDB'
$adapterRoot = Join-Path $toolsRoot 'Adapters\Debug\GDB'
$workflowRoot = Join-Path $toolsRoot 'Workflows\Debug'
$coreModule = Join-Path $toolsRoot 'Core\Toolkit.Core.psm1'
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

foreach ($path in @(
        $coreModule,
        (Join-Path $adapterRoot 'gdb_session.ps1'),
        (Join-Path $workflowRoot 'snapshot.ps1'),
        (Join-Path $workflowRoot 'fault_capture.ps1'))) {
    Assert-File -Path $path -Message "Required Debug toolkit file is missing: $path"
}

foreach ($name in @('runtime_snapshot_resume.gdb', 'runtime_snapshot_halt.gdb', 'fault_capture.gdb', 'fault_trigger_capture.gdb')) {
    Assert-File -Path (Join-Path $debugRoot $name) -Message "Required GDB contract file is missing: $name"
}

$resume = Get-Content -LiteralPath (Join-Path $debugRoot 'runtime_snapshot_resume.gdb') -Raw
$halt = Get-Content -LiteralPath (Join-Path $debugRoot 'runtime_snapshot_halt.gdb') -Raw
$fault = Get-Content -LiteralPath (Join-Path $debugRoot 'fault_capture.gdb') -Raw
$trigger = Get-Content -LiteralPath (Join-Path $debugRoot 'fault_trigger_capture.gdb') -Raw

Assert-Contains $resume 'continue&' 'Resume GDB script continues asynchronously'
Assert-Contains $resume '(?im)^\s*disconnect\s*$' 'Resume GDB script disconnects cleanly'
Assert-NotContains $resume '(?im)^\s*detach\s*$' 'Resume GDB script does not detach a halted target'
Assert-NotContains $resume '(?im)^\s*load(?:\s|$)' 'Resume GDB script never programs Flash'

Assert-Contains $halt '(?im)^\s*detach\s*$' 'Halt GDB script detaches without resuming'
Assert-NotContains $halt 'continue&' 'Halt GDB script never resumes the target'
Assert-NotContains $halt '(?im)^\s*load(?:\s|$)' 'Halt GDB script never programs Flash'

Assert-Contains $fault '(?im)^\s*detach\s*$' 'Fault capture detaches without resuming'
Assert-NotContains $fault 'continue&' 'Fault capture does not resume the MCU'
Assert-NotContains $fault '(?im)^\s*load(?:\s|$)' 'Fault capture never programs Flash'

Assert-Contains $trigger '(?im)^\s*target\s+remote\s+localhost:2331' 'Fault trigger attaches before target control'
Assert-Contains $trigger '(?im)^\s*break\s+diagnostics_fault_capture_stop\s*$' 'Fault trigger breaks after project capture'
Assert-Contains $trigger '(?im)^\s*monitor\s+reset\s*$' 'Fault trigger resets through the open GDB connection'
Assert-Contains $trigger '(?im)^\s*continue\s*$' 'Fault trigger waits for the test firmware to reach the capture breakpoint'
Assert-Contains $trigger '(?im)^\s*detach\s*$' 'Fault trigger detaches after evidence capture'
Assert-NotContains $trigger '(?im)^\s*load(?:\s|$)' 'Fault trigger never programs Flash'

Import-Module -Name $coreModule -Force
foreach ($name in @('Start-ToolkitProcess', 'Complete-ToolkitProcess', 'Stop-ToolkitProcess')) {
    Assert-True ($null -ne (Get-Command $name -ErrorAction SilentlyContinue)) "Core process API is missing: $name"
}

$gdbSessionPath = Join-Path $adapterRoot 'gdb_session.ps1'
$snapshotWorkflowPath = Join-Path $workflowRoot 'snapshot.ps1'
$faultWorkflowPath = Join-Path $workflowRoot 'fault_capture.ps1'
if ((Test-Path -LiteralPath $gdbSessionPath -PathType Leaf) -and
    (Test-Path -LiteralPath $snapshotWorkflowPath -PathType Leaf) -and
    (Test-Path -LiteralPath $faultWorkflowPath -PathType Leaf)) {
    $gdbSession = Get-Content -LiteralPath $gdbSessionPath -Raw
    $snapshotWorkflow = Get-Content -LiteralPath $snapshotWorkflowPath -Raw
    $faultWorkflow = Get-Content -LiteralPath $faultWorkflowPath -Raw

    Assert-Contains $gdbSession 'Test-ToolkitTcpPort' 'GDB adapter waits for the owned server port'
    Assert-Contains $gdbSession 'Start-ToolkitProcess' 'GDB adapter starts processes through Core'
    Assert-Contains $gdbSession 'Stop-ToolkitProcess' 'GDB adapter stops only its owned processes'
    Assert-NotContains $gdbSession 'Stop-Process\s+-Name|taskkill\s+/IM' 'GDB adapter does not globally kill processes'
    Assert-Contains $snapshotWorkflow 'Invoke-GdbSession' 'Snapshot workflow delegates lifecycle to GDB adapter'
    Assert-Contains $faultWorkflow 'Invoke-GdbSession' 'Fault workflow delegates lifecycle to GDB adapter'
    Assert-Contains $faultWorkflow 'Invoke-JLinkRtt' 'Fault workflow captures RTT after GDB releases J-Link'
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[Debug][FAIL] $_" }
    exit 1
}

'[Debug][PASS] Debug workflow contract checks passed.'
exit 0
