param(
    [string]$ToolsRoot = "",

    [ValidateSet("run", "prepare")]
    [string]$Mode = "run"
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_flash.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $target = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $result = Invoke-JLinkFlash -Configuration $configuration -Mode $Mode -LogPath (Join-Path $logDirectory ("{0}_flash.log" -f $target))
}
catch {
    Write-Host "[FLASH][ERROR] $($_.Exception.Message)"
    exit 10
}

if ($result.ExitCode -eq 0) {
    Write-Host "[FLASH][PASS] J-Link programming command completed successfully."
    exit 0
}

Write-Host "[FLASH][FAIL] J-Link returned ERRORLEVEL=$($result.ExitCode)."
exit 30
