param(
    [Parameter(Mandatory = $true)]
    [string]$Gdb,

    [Parameter(Mandatory = $true)]
    [string]$JLinkGdbServer,

    [Parameter(Mandatory = $true)]
    [string]$RttLogger,

    [Parameter(Mandatory = $true)]
    [string]$Axf,

    [Parameter(Mandatory = $true)]
    [string]$Device,

    [Parameter(Mandatory = $true)]
    [string]$Interface,

    [Parameter(Mandatory = $true)]
    [int]$Speed,

    [Parameter(Mandatory = $true)]
    [int]$Port,

    [Parameter(Mandatory = $true)]
    [int]$StartTimeoutSeconds,

    [Parameter(Mandatory = $true)]
    [int]$RttChannel,

    [Parameter(Mandatory = $false)]
    [ValidateSet("capture", "trigger")]
    [string]$Mode = "capture",

    [Parameter(Mandatory = $true)]
    [string]$GdbScript,

    [Parameter(Mandatory = $true)]
    [string]$ServerLog,

    [Parameter(Mandatory = $true)]
    [string]$GdbLog,

    [Parameter(Mandatory = $true)]
    [string]$RttLog
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\")).Path

try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Debug\GDB\gdb_session.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")

    $configuration = [PSCustomObject]@{
        ARM_GDB = $Gdb
        JLINK_GDB_SERVER = $JLinkGdbServer
        JLINK_RTT_LOGGER = $RttLogger
        PROJECT_APP_AXF = $Axf
        JLINK_DEVICE = $Device
        JLINK_IF = $Interface
        JLINK_SPEED = $Speed
        GDB_PORT = $Port
        GDB_START_TIMEOUT_SECONDS = $StartTimeoutSeconds
        JLINK_RTT_CHANNEL = $RttChannel
        PROJECT_ROOT = (Resolve-Path (Join-Path $frameworkRoot "..\")).Path
    }

    $session = Invoke-GdbSession -Configuration $configuration -Mode $Mode -GdbScript $GdbScript -ServerLog $ServerLog -ClientLog $GdbLog
    $gdbOutput = [string]$session.ClientOutput
    foreach ($marker in @('GDB FAULT CAPTURE', '[FAULT_CONTEXT]', 'FAULT PC', '[SCB_FAULT_REGISTERS]', '[BACKTRACE]', '[STACK]', 'halt-and-detach')) {
        if ($gdbOutput -notmatch [regex]::Escape($marker)) {
            throw "GDB fault evidence is missing: $marker"
        }
    }

    Write-Host "[RTT] Capturing retained Fault RTT output after GDB releases J-Link..."
    $rttResult = Invoke-JLinkRtt -Configuration $configuration -Seconds 5 -OutputPath $RttLog -DiagnosticPath ($RttLog + ".logger")
    if ($rttResult.ExitCode -ne 0) {
        throw "RTT fault evidence was not captured"
    }

    Write-Host "[GDB][PASS] Fault capture completed; MCU remains halted."
    exit 0
}
catch {
    $message = $_.Exception.Message
    Write-Host "[GDB][FAIL] $message"
    foreach ($path in @($ServerLog, $GdbLog, $RttLog)) {
        if ($null -ne $path) {
            Write-ToolkitLog -Path $path -Message "[GDB][FAIL] $message"
        }
    }
    exit 1
}
