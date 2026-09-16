param(
    [Parameter(Mandatory = $true)]
    [string]$Gdb,

    [Parameter(Mandatory = $true)]
    [string]$JLinkGdbServer,

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
    [ValidateSet("resume", "halt")]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$GdbScript,

    [Parameter(Mandatory = $true)]
    [string]$ServerLog,

    [Parameter(Mandatory = $true)]
    [string]$SnapshotLog
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\")).Path

try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Debug\GDB\gdb_session.ps1")

    $configuration = [PSCustomObject]@{
        ARM_GDB = $Gdb
        JLINK_GDB_SERVER = $JLinkGdbServer
        PROJECT_APP_AXF = $Axf
        JLINK_DEVICE = $Device
        JLINK_IF = $Interface
        JLINK_SPEED = $Speed
        GDB_PORT = $Port
        GDB_START_TIMEOUT_SECONDS = $StartTimeoutSeconds
        PROJECT_ROOT = (Resolve-Path (Join-Path $frameworkRoot "..\")).Path
    }

    $session = Invoke-GdbSession -Configuration $configuration -Mode $Mode -GdbScript $GdbScript -ServerLog $ServerLog -ClientLog $SnapshotLog
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
    foreach ($path in @($ServerLog, $SnapshotLog)) {
        if ($null -ne $path) {
            Write-ToolkitLog -Path $path -Message "[GDB][FAIL] $message"
        }
    }
    exit 1
}
