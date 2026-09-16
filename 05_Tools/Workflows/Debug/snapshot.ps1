param(
    [string]$ToolsRoot = "",

    [ValidateSet("resume", "halt")]
    [string]$Mode = "halt"
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

$serverLog = $null
$snapshotLog = $null
try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Debug\GDB\gdb_session.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $target = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $serverLog = Join-Path $logDirectory ("{0}_gdb_server.log" -f $target)
    $snapshotLog = Join-Path $logDirectory ("{0}_gdb_snapshot.log" -f $target)
    $gdbScript = Join-Path $frameworkRoot ("Debug\GDB\runtime_snapshot_{0}.gdb" -f $Mode)

    $session = Invoke-GdbSession -Configuration $configuration -Mode $Mode -GdbScript $gdbScript -ServerLog $serverLog -ClientLog $snapshotLog
    $output = [string]$session.ClientOutput
    if ($output -match 'Cannot execute this command while the target is running|Remote communication error|Connection timed out|Connection refused|Error in sourced command file|Cannot execute this command') {
        throw "GDB output contains a runtime failure"
    }
    if ($output -notmatch 'GDB RUNTIME SNAPSHOT') {
        throw "GDB snapshot marker is missing"
    }
    if (($Mode -eq "resume") -and ($output -notmatch 'resume-and-disconnect|Ending remote debugging')) {
        throw "Resume exit evidence is missing"
    }
    if (($Mode -eq "halt") -and ($output -notmatch 'halt-and-detach')) {
        throw "Halt exit evidence is missing"
    }

    Write-Host "[GDB][PASS] GDB runtime snapshot completed."
    exit 0
}
catch {
    $message = $_.Exception.Message
    Write-Host "[GDB][FAIL] $message"
    if ($null -ne $snapshotLog) {
        Write-ToolkitLog -Path $snapshotLog -Message "[GDB][FAIL] $message"
    }
    if ($null -ne $serverLog) {
        Write-ToolkitLog -Path $serverLog -Message "[GDB][FAIL] $message"
    }
    exit 40
}
