param(
    [string]$ToolsRoot = "",

    [int]$Seconds = 0
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $target = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    if ($Seconds -lt 1) {
        $Seconds = if ($configuration.PSObject.Properties["RTT_CAPTURE_SECONDS"]) { [int]$configuration.RTT_CAPTURE_SECONDS } else { 10 }
    }
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $result = Invoke-JLinkRtt -Configuration $configuration -Seconds $Seconds -OutputPath (Join-Path $logDirectory ("{0}_rtt.log" -f $target)) -DiagnosticPath (Join-Path $logDirectory ("{0}_rtt_logger.log" -f $target))
}
catch {
    Write-Host "[RTT][ERROR] $($_.Exception.Message)"
    exit 10
}

if ($result.ExitCode -eq 0) {
    Write-Host "[RTT][PASS] RTT data captured."
    exit 0
}

Write-Host "[RTT][FAIL] RTT capture returned ERRORLEVEL=$($result.ExitCode)."
exit 30
