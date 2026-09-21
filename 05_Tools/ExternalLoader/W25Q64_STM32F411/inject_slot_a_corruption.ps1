param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("HeaderInvalid", "PayloadCrcInvalid", "VersionMismatch")]
    [string]$Case,
    [string]$Image = "",
    [string]$Loader = "",
    [string]$CubeProgrammer = "",
    [string]$Python = "",
    [string]$Port = "JLINK",
    [int]$Frequency = 4000,
    [switch]$ConfirmDestructive
)

$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$frameworkRoot = Join-Path $projectRoot "05_Tools"
$configPath = Join-Path $projectRoot "05_Tools\Config\toolchain.local.bat"
$mutationModule = Join-Path $projectRoot "05_Tools\Firmware\slot_a_corruption.py"

function Resolve-ConfiguredTool {
    param(
        [string]$Value,
        [string]$VariableName,
        [switch]$AllowPathLookup
    )

    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        return $Value
    }

    if (Test-Path -LiteralPath $configPath -PathType Leaf) {
        $configText = [System.IO.File]::ReadAllText($configPath)
        $pattern = '(?m)^\s*set\s+"' + [regex]::Escape($VariableName) + '=(?<path>[^"]*)"\s*$'
        $match = [regex]::Match($configText, $pattern)
        if ($match.Success -and -not [string]::IsNullOrWhiteSpace($match.Groups["path"].Value)) {
            return $match.Groups["path"].Value.Trim()
        }
    }

    if ($AllowPathLookup) {
        $command = Get-Command python -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw "$VariableName is not configured"
}

function Get-Sha256Hex {
    param([string]$Path)

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $sha256.ComputeHash([System.IO.File]::ReadAllBytes($Path))
        return (($bytes | ForEach-Object { $_.ToString("X2") }) -join "")
    }
    finally {
        $sha256.Dispose()
    }
}

function Invoke-CubeProgrammer {
    param(
        [string[]]$Arguments,
        [string]$LogName
    )

    $logPath = Join-Path $evidenceDirectory $LogName
    Write-Host "[INJECT] STM32_Programmer_CLI $($Arguments -join ' ')"
    $output = & $cubeProgrammerPath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        throw "STM32CubeProgrammer failed with exit code $exitCode. See $logPath"
    }
}

function Invoke-Python {
    param(
        [string[]]$Arguments,
        [string]$LogName
    )

    $logPath = Join-Path $evidenceDirectory $LogName
    Write-Host "[INJECT] python $($Arguments -join ' ')"
    $output = & $pythonPath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        throw "Python helper failed with exit code $exitCode. See $logPath"
    }
}

function Assert-FileHashEqual {
    param(
        [string]$ExpectedPath,
        [string]$ActualPath,
        [string]$Label
    )

    $expectedHash = Get-Sha256Hex -Path $ExpectedPath
    $actualHash = Get-Sha256Hex -Path $ActualPath
    if ($expectedHash -ne $actualHash) {
        throw "$Label readback mismatch: expected=$expectedHash actual=$actualHash"
    }
}

if (-not $ConfirmDestructive) {
    throw "Slot A corruption writes External Flash. Re-run with -ConfirmDestructive."
}
if ([string]::IsNullOrWhiteSpace($Port) -or $Frequency -le 0) {
    throw "Port and frequency must be valid"
}
if (-not (Test-Path -LiteralPath $mutationModule -PathType Leaf)) {
    throw "Corruption helper not found: $mutationModule"
}

if ([string]::IsNullOrWhiteSpace($Image)) {
    $Image = Join-Path $projectRoot "06_Output\Packages\app_v1.0.img"
}
if ([string]::IsNullOrWhiteSpace($Loader)) {
    $Loader = Join-Path $projectRoot "06_Output\Packages\ExternalLoader\W25Q64_STM32F411.stldr"
}

$cubeProgrammerPath = Resolve-ConfiguredTool -Value $CubeProgrammer -VariableName "STM32_PROGRAMMER_CLI"
$pythonPath = Resolve-ConfiguredTool -Value $Python -VariableName "PYTHON_EXE" -AllowPathLookup
$imagePath = (Resolve-Path -LiteralPath $Image).Path
$loaderPath = (Resolve-Path -LiteralPath $Loader).Path

foreach ($path in @($imagePath, $loaderPath, $cubeProgrammerPath, $pythonPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file not found: $path"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$evidenceDirectory = Join-Path $projectRoot "06_Output\Logs\ExternalLoader\Corruption-$timestamp"
New-Item -ItemType Directory -Force -Path $evidenceDirectory | Out-Null

$imageBytes = [System.IO.File]::ReadAllBytes($imagePath)
if ($imageBytes.Length -lt 64) {
    throw "Compact image is shorter than the 64-byte Header: $imagePath"
}
$imageSize = [BitConverter]::ToUInt32($imageBytes, 16)
if (($imageSize -eq 0) -or ($imageBytes.Length -ne (64 + $imageSize))) {
    throw "Compact image size does not match its Header: $imagePath"
}

$sourceHeaderPath = Join-Path $evidenceDirectory "source_header.bin"
$sourcePayloadPath = Join-Path $evidenceDirectory "source_payload.bin"
$headerBeforePath = Join-Path $evidenceDirectory "slot_a_header_sector_before.bin"
$payloadBeforePath = Join-Path $evidenceDirectory "slot_a_payload_before.bin"
$headerMutatedPath = Join-Path $evidenceDirectory "slot_a_header_sector_corrupted.bin"
$payloadMutatedPath = Join-Path $evidenceDirectory "slot_a_payload_corrupted.bin"
$headerAfterPath = Join-Path $evidenceDirectory "slot_a_header_sector_after.bin"
$payloadAfterPath = Join-Path $evidenceDirectory "slot_a_payload_after.bin"
$manifestPath = Join-Path $evidenceDirectory "mutation_manifest.json"

$sourceHeader = New-Object byte[] 64
$sourcePayload = New-Object byte[] $imageSize
[Array]::Copy($imageBytes, 0, $sourceHeader, 0, 64)
[Array]::Copy($imageBytes, 64, $sourcePayload, 0, $imageSize)
[System.IO.File]::WriteAllBytes($sourceHeaderPath, $sourceHeader)
[System.IO.File]::WriteAllBytes($sourcePayloadPath, $sourcePayload)

$commonArguments = @(
    "-el", $loaderPath,
    "-c", "port=$Port", "freq=$Frequency"
)

$lock = $null
$exitCode = 50
try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    $configuration = Import-ToolkitConfiguration -ToolsRoot $frameworkRoot
    $lock = Enter-ToolkitLock -Path (Get-ToolkitJLinkLockPath -Configuration $configuration)

    Invoke-Python -Arguments @(
        "-B", $mutationModule,
        "--validate-image", $imagePath
    ) -LogName "validate_source_image.log"

    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-u", "0x90000000", "0x1000", $headerBeforePath
    )) -LogName "read_slot_a_header_before.log"
    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-u", "0x90001000", [string]$imageSize, $payloadBeforePath
    )) -LogName "read_slot_a_payload_before.log"

    $headerBeforeBytes = [System.IO.File]::ReadAllBytes($headerBeforePath)
    if ($headerBeforeBytes.Length -ne 0x1000) {
        throw "Slot A Header Sector readback length is not 4096 bytes"
    }
    $headerBefore64Path = Join-Path $evidenceDirectory "slot_a_header_before_64.bin"
    $headerBefore64 = New-Object byte[] 64
    [Array]::Copy($headerBeforeBytes, 0, $headerBefore64, 0, 64)
    [System.IO.File]::WriteAllBytes($headerBefore64Path, $headerBefore64)
    Assert-FileHashEqual -ExpectedPath $sourceHeaderPath -ActualPath $headerBefore64Path -Label "Slot A Header"
    Assert-FileHashEqual -ExpectedPath $sourcePayloadPath -ActualPath $payloadBeforePath -Label "Slot A Payload"

    $moduleCase = @{
        HeaderInvalid = "header-invalid"
        PayloadCrcInvalid = "payload-crc-invalid"
        VersionMismatch = "version-mismatch"
    }[$Case]
    Invoke-Python -Arguments @(
        "-B", $mutationModule,
        "--case", $moduleCase,
        "--header-sector", $headerBeforePath,
        "--payload", $payloadBeforePath,
        "--output-header-sector", $headerMutatedPath,
        "--output-payload", $payloadMutatedPath,
        "--manifest", $manifestPath
    ) -LogName "create_corruption.log"

    if ($Case -eq "PayloadCrcInvalid") {
        Assert-FileHashEqual -ExpectedPath $headerBeforePath -ActualPath $headerMutatedPath -Label "Unchanged Header Sector"
        if ((Get-Sha256Hex -Path $payloadBeforePath) -eq (Get-Sha256Hex -Path $payloadMutatedPath)) {
            throw "Payload corruption artifact is identical to the source payload"
        }
    }
    else {
        Assert-FileHashEqual -ExpectedPath $payloadBeforePath -ActualPath $payloadMutatedPath -Label "Unchanged Payload"
        if ((Get-Sha256Hex -Path $headerBeforePath) -eq (Get-Sha256Hex -Path $headerMutatedPath)) {
            throw "Header corruption artifact is identical to the source Header Sector"
        }
    }

    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-w", $headerMutatedPath, "0x90000000", "-v", "-y"
    )) -LogName "write_corrupted_header_sector.log"
    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-w", $payloadMutatedPath, "0x90001000", "-v", "-y"
    )) -LogName "write_corrupted_payload.log"

    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-u", "0x90000000", "0x1000", $headerAfterPath
    )) -LogName "read_slot_a_header_after.log"
    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-u", "0x90001000", [string]$imageSize, $payloadAfterPath
    )) -LogName "read_slot_a_payload_after.log"

    Assert-FileHashEqual -ExpectedPath $headerMutatedPath -ActualPath $headerAfterPath -Label "Corrupted Header Sector"
    Assert-FileHashEqual -ExpectedPath $payloadMutatedPath -ActualPath $payloadAfterPath -Label "Corrupted Payload"

    $manifest = [ordered]@{
        case = $Case
        image = $imagePath
        header_address = "0x90000000"
        payload_address = "0x90001000"
        payload_size = [int]$imageSize
        source_header_sha256 = Get-Sha256Hex -Path $headerBefore64Path
        source_payload_sha256 = Get-Sha256Hex -Path $payloadBeforePath
        corrupted_header_sector_sha256 = Get-Sha256Hex -Path $headerAfterPath
        corrupted_payload_sha256 = Get-Sha256Hex -Path $payloadAfterPath
        restore_command = "powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\ExternalLoader\W25Q64_STM32F411\preburn_w25q64.ps1 -Image .\06_Output\Packages\app_v1.0.img -ClearSlotBHeader"
        evidence_directory = $evidenceDirectory
    }
    [System.IO.File]::WriteAllText(
        $manifestPath,
        ($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine,
        (New-Object System.Text.UTF8Encoding($false)))

    Write-Host "[INJECT][PASS] Slot A corruption written and read back"
    Write-Host "[INJECT] case=$Case"
    Write-Host "[INJECT] evidence=$evidenceDirectory"
    Write-Host "[INJECT] restore with the command recorded in mutation_manifest.json"
    $exitCode = 0
}
catch {
    Write-Error "[INJECT][FAIL] $($_.Exception.Message)"
    Write-Host "[INJECT][RECOVERY] Do not continue the test. Restore Slot A with preburn_w25q64.ps1 after preserving $evidenceDirectory"
}
finally {
    if ($null -ne $lock) {
        Exit-ToolkitLock -Lock $lock
    }
}

exit $exitCode
