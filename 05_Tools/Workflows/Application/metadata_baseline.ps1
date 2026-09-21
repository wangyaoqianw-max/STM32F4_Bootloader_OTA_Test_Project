param(
    [string]$ToolsRoot = "",
    [string]$Image = "",
    [switch]$ConfirmDestructive
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
$configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
$projectRoot = $configuration.PROJECT_ROOT
$toolkitPath = Join-Path $ToolsRoot "toolkit.bat"
$projectPath = Resolve-ToolkitProjectPath -ProjectRoot $projectRoot -RelativePath $configuration.PROJECT_KEIL_PROJECT_FILE
$appSystemPath = Join-Path $projectRoot "03_Firmware\Application\OTA_APP\01_APP\system\app_system.c"
$metadataTestRoot = Join-Path $projectRoot "04_Test\Board\S09_Firmware_Installation"
$metadataTestSource = Join-Path $metadataTestRoot "app_s09_metadata_baseline_test.c"
$metadataTestHeader = Join-Path $metadataTestRoot "app_s09_metadata_baseline_test.h"
$externalPreburnPath = Join-Path $projectRoot "05_Tools\ExternalLoader\W25Q64_STM32F411\preburn_w25q64.ps1"
$logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $projectRoot -RelativePath $configuration.PROJECT_LOG_DIR
$baselineLogDirectory = Join-Path $logDirectory "S09_Metadata_Baseline"
New-Item -ItemType Directory -Force -Path $baselineLogDirectory | Out-Null

function Invoke-BaselineCommand {
    param(
        [string[]]$Arguments,
        [string]$LogName
    )

    $logPath = Join-Path $baselineLogDirectory $LogName
    $output = & $toolkitPath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath | ForEach-Object { Write-Host $_ }
    return [int]$exitCode
}

function Invoke-ExternalPreburn {
    param([string]$ImagePath)

    $logPath = Join-Path $baselineLogDirectory "external_preburn.log"
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $externalPreburnPath `
        -Image $ImagePath -ClearSlotBHeader 2>&1
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        throw "External Loader preburn failed with exit code $exitCode. See $logPath"
    }
}

function Assert-CompactFirmwareImage {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64) {
        throw "Firmware image is shorter than the 64-byte Header: $Path"
    }

    $magic = [BitConverter]::ToUInt32($bytes, 0)
    $formatVersion = [BitConverter]::ToUInt16($bytes, 4)
    $headerSize = [BitConverter]::ToUInt16($bytes, 6)
    $major = [BitConverter]::ToUInt16($bytes, 8)
    $minor = [BitConverter]::ToUInt16($bytes, 10)
    $patch = [BitConverter]::ToUInt16($bytes, 12)
    $imageSize = [BitConverter]::ToUInt32($bytes, 16)
    if (($magic -ne 0x4D495746) -or ($formatVersion -ne 1) -or ($headerSize -ne 64) -or
        ($major -ne 1) -or ($minor -ne 0) -or ($patch -ne 0)) {
        throw "Firmware image Header V1 must be version 1.0.0: $Path"
    }
    if (($imageSize -eq 0) -or ($bytes.Length -ne (64 + $imageSize))) {
        throw "Firmware image is not compact [64-byte Header][Payload]: $Path"
    }
}

function Set-TemporaryMetadataProject {
    param(
        [string]$OriginalSystem,
        [string]$OriginalProject
    )

    $testSystem = @'
/******************************************************************************
 * Temporary S09 AT24C02 Metadata baseline composition root.
 ******************************************************************************/

#include "app_system.h"
#include "app_s09_metadata_baseline_test.h"

#include <stddef.h>

platform_error_t app_system_bootstrap(void)
{
    return app_s09_metadata_baseline_test_run();
}

platform_error_t app_system_report_runtime_ready(uint32_t readyFlag)
{
    (void)readyFlag;
    return PLATFORM_ERR_INVALID_STATE;
}

platform_error_t app_system_take_runtime_ready(uint32_t *readyMask)
{
    if (readyMask == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    return PLATFORM_ERR_INVALID_STATE;
}

platform_error_t app_system_report_confirm_result(platform_error_t result)
{
    (void)result;
    return PLATFORM_ERR_INVALID_STATE;
}

platform_error_t app_system_take_confirm_result(platform_error_t *result)
{
    if (result == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    return PLATFORM_ERR_INVALID_STATE;
}

app_health_context_t *app_system_get_health_context(void)
{
    return NULL;
}

'@
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($appSystemPath, $testSystem, $utf8)

    $projectText = [System.IO.File]::ReadAllText($projectPath, [System.Text.Encoding]::UTF8)
    $includeMarker = "../01_APP;"
    $includeAddition = "../01_APP;../../../../04_Test/Board/S09_Firmware_Installation;"
    if (-not $projectText.Contains($includeMarker)) {
        throw "Application project IncludePath marker not found"
    }
    $projectText = $projectText.Replace($includeMarker, $includeAddition)

    $cmsisGroupMarker = "        <Group>`r`n          <GroupName>::CMSIS</GroupName>"
    if (-not $projectText.Contains($cmsisGroupMarker)) {
        throw "Application project CMSIS group marker not found"
    }
    $metadataGroup = @'
        <Group>
          <GroupName>Test/S09 Metadata Baseline</GroupName>
          <Files>
            <File>
              <FileName>app_s09_metadata_baseline_test.c</FileName>
              <FileType>1</FileType>
              <FilePath>../../../../04_Test/Board/S09_Firmware_Installation/app_s09_metadata_baseline_test.c</FilePath>
            </File>
          </Files>
        </Group>
'@
    $projectText = $projectText.Replace(
        $cmsisGroupMarker,
        "$metadataGroup`r`n$cmsisGroupMarker")
    [System.IO.File]::WriteAllText($projectPath, $projectText, $utf8)
}

if (-not $ConfirmDestructive) {
    throw "Metadata baseline writes AT24C02. Re-run with -ConfirmDestructive."
}
if ([string]::IsNullOrWhiteSpace($Image)) {
    $Image = Join-Path $projectRoot "06_Output\Packages\app_v1.0.img"
}
if (-not [System.IO.Path]::IsPathRooted($Image)) {
    $Image = Join-Path $projectRoot $Image
}
if (-not (Test-Path -LiteralPath $Image -PathType Leaf)) {
    throw "Metadata baseline image not found: $Image"
}
if (-not (Test-Path -LiteralPath $externalPreburnPath -PathType Leaf)) {
    throw "External Loader preburn script not found: $externalPreburnPath"
}
if (-not (Test-Path -LiteralPath $metadataTestSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $metadataTestHeader -PathType Leaf)) {
    throw "S09 Metadata baseline board test is incomplete: $metadataTestRoot"
}

Assert-CompactFirmwareImage -Path (Resolve-Path -LiteralPath $Image).Path
$originalSystem = [System.IO.File]::ReadAllText($appSystemPath, [System.Text.Encoding]::UTF8)
$originalProject = [System.IO.File]::ReadAllText($projectPath, [System.Text.Encoding]::UTF8)
$failure = $null

try {
    Write-Host "[METADATA][WARN] External Loader will rewrite Slot A; Metadata test will write AT24C02"
    Invoke-ExternalPreburn -ImagePath (Resolve-Path -LiteralPath $Image).Path

    Set-TemporaryMetadataProject `
        -OriginalSystem $originalSystem `
        -OriginalProject $originalProject
    $testBuildExit = Invoke-BaselineCommand `
        -Arguments @("build", "application") `
        -LogName "metadata_build.log"
    if ($testBuildExit -ne 0) {
        throw "Temporary Metadata baseline build failed: $testBuildExit"
    }

    $testFlashExit = Invoke-BaselineCommand `
        -Arguments @("flash", "application", "run") `
        -LogName "metadata_flash.log"
    if ($testFlashExit -ne 0) {
        throw "Temporary Metadata baseline flash failed: $testFlashExit"
    }

    $rttExit = Invoke-BaselineCommand `
        -Arguments @("rtt", "application", "12") `
        -LogName "metadata_rtt.log"
    if ($rttExit -ne 0) {
        throw "Temporary Metadata baseline RTT failed: $rttExit"
    }
    Copy-Item -LiteralPath (Join-Path $logDirectory "OTA_APP_rtt.log") `
        -Destination (Join-Path $baselineLogDirectory "metadata_rtt_raw.log") -Force
    $metadataRttRawPath = Join-Path $baselineLogDirectory "metadata_rtt_raw.log"
    $metadataRttText = [System.IO.File]::ReadAllText(
        $metadataRttRawPath,
        [System.Text.Encoding]::UTF8)
    if ($metadataRttText -notmatch "\[S09-METADATA\].*baseline PASS") {
        throw "Temporary Metadata baseline did not report the required PASS marker. See $metadataRttRawPath"
    }
}
catch {
    $failure = $_
}
finally {
    [System.IO.File]::WriteAllText($appSystemPath, $originalSystem, (New-Object System.Text.UTF8Encoding($false)))
    [System.IO.File]::WriteAllText($projectPath, $originalProject, (New-Object System.Text.UTF8Encoding($false)))
}

if ($null -ne $failure) {
    try {
        Write-Host "[METADATA][RECOVERY] restoring formal Application"
        $recoveryBuildExit = Invoke-BaselineCommand `
            -Arguments @("build", "application") `
            -LogName "formal_build_after_failure.log"
        if ($recoveryBuildExit -eq 0) {
            $recoveryFlashExit = Invoke-BaselineCommand `
                -Arguments @("flash", "application", "run") `
                -LogName "formal_flash_after_failure.log"
            if ($recoveryFlashExit -ne 0) {
                Write-Host "[METADATA][RECOVERY][WARN] formal Application flash failed: $recoveryFlashExit"
            }
        }
        else {
            Write-Host "[METADATA][RECOVERY][WARN] formal Application build failed: $recoveryBuildExit"
        }
    }
    catch {
        Write-Host "[METADATA][RECOVERY][WARN] formal Application recovery failed: $($_.Exception.Message)"
    }
    throw "Metadata baseline stopped: $($failure.Exception.Message)"
}

$formalBuildExit = Invoke-BaselineCommand `
    -Arguments @("build", "application") `
    -LogName "formal_build.log"
if ($formalBuildExit -ne 0) {
    throw "Formal Application rebuild failed after Metadata baseline: $formalBuildExit"
}
$formalFlashExit = Invoke-BaselineCommand `
    -Arguments @("flash", "application", "run") `
    -LogName "formal_flash.log"
if ($formalFlashExit -ne 0) {
    throw "Formal Application flash failed after Metadata baseline: $formalFlashExit"
}
$formalRttExit = Invoke-BaselineCommand `
    -Arguments @("rtt", "application", "8") `
    -LogName "formal_rtt.log"
if ($formalRttExit -ne 0) {
    throw "Formal Application RTT verification failed after Metadata baseline: $formalRttExit"
}
Copy-Item -LiteralPath (Join-Path $logDirectory "OTA_APP_rtt.log") `
    -Destination (Join-Path $baselineLogDirectory "formal_rtt_raw.log") -Force
Write-Host "[METADATA][PASS] Slot A/B verified by External Loader; AT24C02 Metadata baseline verified"
exit 0
