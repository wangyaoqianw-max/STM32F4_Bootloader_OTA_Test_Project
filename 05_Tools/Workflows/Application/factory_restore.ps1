param(
    [string]$ToolsRoot = "",
    [string]$Image = "",
    [string]$Port = "",
    [int]$Baud = 115200,
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
$restoreTestRoot = Join-Path $projectRoot "04_Test\Board\S09_Firmware_Installation"
$restoreTestSource = Join-Path $restoreTestRoot "app_s09_factory_restore_test.c"
$restoreTestHeader = Join-Path $restoreTestRoot "app_s09_factory_restore_test.h"
$logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $projectRoot -RelativePath $configuration.PROJECT_LOG_DIR
$factoryLogDirectory = Join-Path $logDirectory "S09_Factory_Restore"
New-Item -ItemType Directory -Force -Path $factoryLogDirectory | Out-Null

function Invoke-FactoryCommand {
    param(
        [string[]]$Arguments,
        [string]$LogName
    )

    $logPath = Join-Path $factoryLogDirectory $LogName
    $output = & $toolkitPath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath | ForEach-Object { Write-Host $_ }
    return [int]$exitCode
}

function Invoke-FactoryBuild {
    param([string]$LogName)

    for ($attempt = 1; $attempt -le 2; $attempt++) {
        $exitCode = Invoke-FactoryCommand -Arguments @("build", "application") -LogName $LogName
        if ($exitCode -eq 0) {
            return 0
        }
        if (($exitCode -ne 1) -or ($attempt -eq 2)) {
            return $exitCode
        }
        Start-Sleep -Milliseconds 500
    }
    return 20
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
    $imageSize = [BitConverter]::ToUInt32($bytes, 16)
    if (($magic -ne 0x4D495746) -or ($formatVersion -ne 1) -or ($headerSize -ne 64)) {
        throw "Firmware image Header V1 fields are invalid: $Path"
    }
    if (($imageSize -eq 0) -or ($bytes.Length -ne (64 + $imageSize))) {
        throw "Firmware image is not compact [64-byte Header][Payload]: $Path"
    }
}

function Set-TemporaryFactoryProject {
    param(
        [string]$OriginalSystem,
        [string]$OriginalProject
    )

    $testSystem = @'
/******************************************************************************
 * Temporary S09 Factory Restore composition root.
 ******************************************************************************/

#include "app_system.h"
#include "app_s09_factory_restore_test.h"

platform_error_t app_system_bootstrap(void)
{
    return app_s09_factory_restore_test_run();
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
    $restoreGroup = @'
        <Group>
          <GroupName>Test/S09 Factory Restore</GroupName>
          <Files>
            <File>
              <FileName>app_s09_factory_restore_test.c</FileName>
              <FileType>1</FileType>
              <FilePath>../../../../04_Test/Board/S09_Firmware_Installation/app_s09_factory_restore_test.c</FilePath>
            </File>
          </Files>
        </Group>
'@
    $projectText = $projectText.Replace(
        $cmsisGroupMarker,
        "$restoreGroup`r`n$cmsisGroupMarker")
    [System.IO.File]::WriteAllText($projectPath, $projectText, $utf8)
}

if (-not $ConfirmDestructive) {
    throw "Factory Restore is destructive. Re-run with -ConfirmDestructive."
}
if ([string]::IsNullOrWhiteSpace($Image)) {
    $Image = Join-Path $projectRoot "06_Output\Packages\app_v1.0.img"
}
if (-not [System.IO.Path]::IsPathRooted($Image)) {
    $Image = Join-Path $projectRoot $Image
}
if (-not (Test-Path -LiteralPath $Image -PathType Leaf)) {
    throw "Factory Restore image not found: $Image"
}
if ([string]::IsNullOrWhiteSpace($Port)) {
    $Port = $configuration.SERIAL_PORT
}
if ([string]::IsNullOrWhiteSpace($Port)) {
    throw "Serial port is not configured"
}
if (($Baud -le 0) -or [string]::IsNullOrWhiteSpace($Port)) {
    throw "Baud rate and serial port must be valid"
}
if (-not (Test-Path -LiteralPath $restoreTestSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $restoreTestHeader -PathType Leaf)) {
    throw "S09 Factory Restore board test is incomplete: $restoreTestRoot"
}

Assert-CompactFirmwareImage -Path (Resolve-Path -LiteralPath $Image).Path
$projectConfig = Join-Path $projectRoot "03_Firmware\Application\OTA_APP\00_Config\project_config.h"
$configText = [System.IO.File]::ReadAllText($projectConfig, [System.Text.Encoding]::UTF8)
if (($configText -notmatch 'PROJECT_STATUS_LED_BLINK_ON_MS\s+\(500U\)') -or
    ($configText -notmatch 'PROJECT_STATUS_LED_BLINK_OFF_MS\s+\(500U\)')) {
    throw "Factory Restore requires Application v1.0 500/500 ms configuration"
}

$originalSystem = [System.IO.File]::ReadAllText($appSystemPath, [System.Text.Encoding]::UTF8)
$originalProject = [System.IO.File]::ReadAllText($projectPath, [System.Text.Encoding]::UTF8)
$sender = $null
$success = $false
try {
    Write-Host "[FACTORY][WARN] destructive restore: Internal APP, Slot A/B and Metadata will be reset"
    $buildExit = Invoke-FactoryBuild -LogName "formal_build_before_restore.log"
    if ($buildExit -ne 0) {
        throw "Formal Application build failed before Factory Restore: $buildExit"
    }

    Set-TemporaryFactoryProject -OriginalSystem $originalSystem -OriginalProject $originalProject
    $testBuildExit = Invoke-FactoryBuild -LogName "provision_build.log"
    if ($testBuildExit -ne 0) {
        throw "Temporary Factory Restore build failed: $testBuildExit"
    }

    $senderLog = Join-Path $factoryLogDirectory "ymodem.log"
    $senderErrorLog = Join-Path $factoryLogDirectory "ymodem_error.log"
    $senderArguments = @(
        "ymodem", "python", "send", (Resolve-Path -LiteralPath $Image).Path,
        "--port", $Port, "--baud", [string]$Baud, "--timeout", "30", "--json"
    )
    $sender = Start-Process -FilePath $toolkitPath -ArgumentList $senderArguments `
        -RedirectStandardOutput $senderLog -RedirectStandardError $senderErrorLog -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 750

    $flashExit = Invoke-FactoryCommand -Arguments @("flash", "application", "run") -LogName "provision_flash.log"
    if ($flashExit -ne 0) {
        throw "Temporary Factory Restore flash failed: $flashExit"
    }
    $sender.WaitForExit()
    $senderResult = Get-Content -LiteralPath $senderLog -Encoding UTF8 |
        Where-Object { $_ -match '^\s*\{' } |
        Select-Object -Last 1 |
        ConvertFrom-Json
    if (($null -eq $senderResult) -or ($senderResult.ok -ne $true) -or ($senderResult.exit_code -ne 0)) {
        $senderReason = if ($null -eq $senderResult) { "no JSON result" } else { [string]$senderResult.error }
        throw "Factory Restore YMODEM failed: $senderReason"
    }

    $rttExit = Invoke-FactoryCommand -Arguments @("rtt", "application", "15") -LogName "provision_rtt.log"
    if ($rttExit -ne 0) {
        throw "Factory Restore RTT verification failed: $rttExit"
    }
    Copy-Item -LiteralPath (Join-Path $logDirectory "OTA_APP_rtt.log") `
        -Destination (Join-Path $factoryLogDirectory "provision_rtt_raw.log") -Force
    $success = $true
}
finally {
    if ($null -ne $sender -and -not $sender.HasExited) {
        Stop-Process -Id $sender.Id -Force
    }
    [System.IO.File]::WriteAllText($appSystemPath, $originalSystem, (New-Object System.Text.UTF8Encoding($false)))
    [System.IO.File]::WriteAllText($projectPath, $originalProject, (New-Object System.Text.UTF8Encoding($false)))
}

if (-not $success) {
    throw "Factory Restore stopped before baseline verification"
}

$formalBuildExit = Invoke-FactoryBuild -LogName "formal_build_after_restore.log"
if ($formalBuildExit -ne 0) {
    throw "Formal Application rebuild failed after Factory Restore: $formalBuildExit"
}
$formalFlashExit = Invoke-FactoryCommand -Arguments @("flash", "application", "run") -LogName "formal_flash.log"
if ($formalFlashExit -ne 0) {
    throw "Formal v1.0 Application flash failed after Factory Restore: $formalFlashExit"
}
$formalRttExit = Invoke-FactoryCommand -Arguments @("rtt", "application", "5") -LogName "formal_rtt.log"
if ($formalRttExit -ne 0) {
    throw "Formal Application RTT verification failed after Factory Restore: $formalRttExit"
}
Copy-Item -LiteralPath (Join-Path $logDirectory "OTA_APP_rtt.log") `
    -Destination (Join-Path $factoryLogDirectory "formal_rtt_raw.log") -Force
Write-Host "[FACTORY][PASS] v1.0 Internal APP + Slot A baseline + Slot B empty + Metadata V2 baseline verified"
exit 0
