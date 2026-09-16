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
    . (Join-Path $frameworkRoot "Adapters\Build\Keil\keil_build.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_flash.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $target = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null

    Write-Host "[CYCLE] Step 1/3: Build"
    $buildResult = Invoke-KeilBuild -Configuration $configuration -LogPath (Join-Path $logDirectory ("{0}_build.log" -f $target))
    if ($buildResult.ExitCode -ge 2) {
        Write-Host "[CYCLE][FAIL] Build failed."
        exit 20
    }

    Write-Host "[CYCLE] Step 2/3: Flash"
    $flashResult = Invoke-JLinkFlash -Configuration $configuration -Mode "run" -LogPath (Join-Path $logDirectory ("{0}_flash.log" -f $target))
    if ($flashResult.ExitCode -ne 0) {
        Write-Host "[CYCLE][FAIL] Flash failed."
        exit 30
    }

    Write-Host "[CYCLE] Step 3/3: RTT capture"
    if ($Seconds -lt 1) {
        $Seconds = if ($configuration.PSObject.Properties["RTT_CAPTURE_SECONDS"]) { [int]$configuration.RTT_CAPTURE_SECONDS } else { 10 }
    }
    $rttResult = Invoke-JLinkRtt -Configuration $configuration -Seconds $Seconds -OutputPath (Join-Path $logDirectory ("{0}_rtt.log" -f $target)) -DiagnosticPath (Join-Path $logDirectory ("{0}_rtt_logger.log" -f $target))
    if ($rttResult.ExitCode -ne 0) {
        Write-Host "[CYCLE][FAIL] RTT capture failed."
        exit 30
    }

    if ($buildResult.ExitCode -eq 1) {
        Write-Host "[CYCLE][WARN] Build, flash, and RTT capture completed with Keil warnings."
        exit 1
    }
    Write-Host "[CYCLE][PASS] Build, flash, and RTT capture commands completed."
    exit 0
}
catch {
    Write-Host "[CYCLE][ERROR] $($_.Exception.Message)"
    exit 10
}
