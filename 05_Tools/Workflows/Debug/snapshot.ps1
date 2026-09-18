param(
    [string]$ToolsRoot = "",

    [ValidateSet("resume", "halt")]
    [string]$Mode = "halt",

    [ValidateSet("application", "bootloader")]
    [string]$Target = "application"
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

$serverLog = $null
$snapshotLog = $null
$lock = $null
$exitCode = 40
try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Debug\GDB\gdb_session.ps1")
    . (Join-Path $frameworkRoot "Workflows\S08_BootloaderFoundation\target_config.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $configuration = Set-S08ToolkitTarget -Configuration $configuration -Target $Target
    $keilTarget = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $serverLog = Join-Path $logDirectory ("{0}_gdb_server.log" -f $keilTarget)
    $snapshotLog = Join-Path $logDirectory ("{0}_gdb_snapshot.log" -f $keilTarget)
    $gdbScript = Join-Path $frameworkRoot ("Debug\GDB\runtime_snapshot_{0}.gdb" -f $Mode)
    $lock = Enter-ToolkitLock -Path (Get-ToolkitJLinkLockPath -Configuration $configuration)

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
    $exitCode = 0
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
    $exitCode = if ($_.Exception.Message -like "Toolkit lock is already held:*") { 30 } else { 40 }
}
finally {
    if ($null -ne $lock) {
        Exit-ToolkitLock -Lock $lock
    }
}

exit $exitCode
