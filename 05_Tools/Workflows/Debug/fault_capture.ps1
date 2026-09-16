param(
    [string]$ToolsRoot = "",

    [ValidateSet("capture", "trigger")]
    [string]$Mode = "capture",

    [int]$Seconds = 5
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

$serverLog = $null
$gdbLog = $null
$rttLog = $null
try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Debug\GDB\gdb_session.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $target = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $serverLog = Join-Path $logDirectory ("{0}_gdb_server.log" -f $target)
    $gdbLog = Join-Path $logDirectory ("{0}_fault_gdb.log" -f $target)
    $rttLog = Join-Path $logDirectory ("{0}_fault_rtt.log" -f $target)
    $gdbScript = Join-Path $frameworkRoot ("Debug\GDB\fault_{0}_capture.gdb" -f $Mode)

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
    exit 0
}
catch {
    $message = $_.Exception.Message
    Write-Host "[GDB][FAIL] $message"
    foreach ($path in @($serverLog, $gdbLog, $rttLog)) {
        if ($null -ne $path) {
            Write-ToolkitLog -Path $path -Message "[GDB][FAIL] $message"
        }
    }
    exit 40
}
