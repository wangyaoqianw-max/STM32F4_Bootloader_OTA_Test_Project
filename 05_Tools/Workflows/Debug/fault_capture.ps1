param(
    [string]$ToolsRoot = "",

    [ValidateSet("capture", "trigger")]
    [string]$Mode = "capture",

    [int]$Seconds = 5,

    [ValidateSet("application", "bootloader")]
    [string]$Target = "application"
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

$serverLog = $null
$gdbLog = $null
$rttLog = $null
$lock = $null
$exitCode = 40
try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Debug\GDB\gdb_session.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")
    . (Join-Path $frameworkRoot "Workflows\S08_BootloaderFoundation\target_config.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $configuration = Set-S08ToolkitTarget -Configuration $configuration -Target $Target
    $keilTarget = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $serverLog = Join-Path $logDirectory ("{0}_gdb_server.log" -f $keilTarget)
    $gdbLog = Join-Path $logDirectory ("{0}_fault_gdb.log" -f $keilTarget)
    $rttLog = Join-Path $logDirectory ("{0}_fault_rtt.log" -f $keilTarget)
    $gdbScriptName = if ($Mode -eq "capture") { "fault_capture.gdb" } else { "fault_trigger_capture.gdb" }
    $gdbScript = Join-Path $frameworkRoot ("Debug\GDB\{0}" -f $gdbScriptName)
    $lock = Enter-ToolkitLock -Path (Get-ToolkitJLinkLockPath -Configuration $configuration)

    if ($Seconds -lt 1) {
        throw "RTT capture duration must be positive: $Seconds"
    }

    $session = Invoke-GdbSession -Configuration $configuration -Mode $Mode -GdbScript $gdbScript -ServerLog $serverLog -ClientLog $gdbLog
    $gdbOutput = [string]$session.ClientOutput
    foreach ($marker in @('GDB FAULT CAPTURE', '[FAULT_CONTEXT]', 'FAULT PC', '[SCB_FAULT_REGISTERS]', '[BACKTRACE]', '[STACK]', 'halt-and-detach')) {
        if ($gdbOutput -notmatch [regex]::Escape($marker)) {
            throw "GDB fault evidence is missing: $marker"
        }
    }

    Write-Host "[J-Link] GDB session released the probe; starting RTT capture."
    Write-Host "[RTT] Capturing retained Fault RTT output..."
    $rttResult = Invoke-JLinkRtt -Configuration $configuration -Seconds $Seconds -OutputPath $rttLog -DiagnosticPath ($rttLog + ".logger")
    if ($rttResult.ExitCode -ne 0) {
        throw "RTT fault evidence was not captured"
    }

    Write-Host "[GDB][PASS] Fault capture completed; MCU remains halted."
    $exitCode = 0
}
catch {
    $message = $_.Exception.Message
    Write-Host "[GDB][FAIL] $message"
    foreach ($path in @($serverLog, $gdbLog, $rttLog)) {
        if ($null -ne $path) {
            Write-ToolkitLog -Path $path -Message "[GDB][FAIL] $message"
        }
    }
    $exitCode = if ($_.Exception.Message -like "Toolkit lock is already held:*") { 30 } else { 40 }
}
finally {
    if ($null -ne $lock) {
        Exit-ToolkitLock -Lock $lock
    }
}

exit $exitCode
