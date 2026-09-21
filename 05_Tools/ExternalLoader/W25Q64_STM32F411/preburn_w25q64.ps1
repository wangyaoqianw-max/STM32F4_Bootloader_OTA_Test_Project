param(
    [string]$Image = "",
    [string]$Loader = "",
    [string]$CubeProgrammer = "",
    [string]$Port = "JLINK",
    [int]$Frequency = 4000,
    [switch]$ClearSlotBHeader
)

$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$configPath = Join-Path $projectRoot "05_Tools\Config\toolchain.local.bat"

function Resolve-ConfiguredTool {
    param(
        [string]$Value,
        [string]$VariableName
    )

    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        return $Value
    }

    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "Local toolchain configuration not found: $configPath"
    }

    $configText = [System.IO.File]::ReadAllText($configPath)
    $pattern = '(?m)^\s*set\s+"' + [regex]::Escape($VariableName) + '=(?<path>[^"]*)"\s*$'
    $match = [regex]::Match($configText, $pattern)
    if (-not $match.Success -or [string]::IsNullOrWhiteSpace($match.Groups["path"].Value)) {
        throw "$VariableName is not configured in $configPath"
    }

    return $match.Groups["path"].Value.Trim()
}

function Get-Sha256Hex {
    param(
        [string]$Path
    )

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $sha256.ComputeHash([System.IO.File]::ReadAllBytes($Path))
        return (($bytes | ForEach-Object { $_.ToString("X2") }) -join "")
    }
    finally {
        $sha256.Dispose()
    }
}

if ([string]::IsNullOrWhiteSpace($Image)) {
    $Image = Join-Path $projectRoot "06_Output\Packages\app_v1.0.img"
}
if ([string]::IsNullOrWhiteSpace($Loader)) {
    $Loader = Join-Path $projectRoot "06_Output\Packages\ExternalLoader\W25Q64_STM32F411.stldr"
}
$CubeProgrammer = Resolve-ConfiguredTool -Value $CubeProgrammer -VariableName "STM32_PROGRAMMER_CLI"

foreach ($path in @($Image, $Loader, $CubeProgrammer)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file not found: $path"
    }
}
if (($Frequency -le 0) -or [string]::IsNullOrWhiteSpace($Port)) {
    throw "Port and frequency must be valid"
}

$imageBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Image).Path)
if ($imageBytes.Length -lt 64) {
    throw "Compact firmware image is shorter than the 64-byte Header: $Image"
}

$magic = [BitConverter]::ToUInt32($imageBytes, 0)
$formatVersion = [BitConverter]::ToUInt16($imageBytes, 4)
$headerSize = [BitConverter]::ToUInt16($imageBytes, 6)
$imageSize = [BitConverter]::ToUInt32($imageBytes, 16)
if (($magic -ne 0x4D495746) -or ($formatVersion -ne 1) -or ($headerSize -ne 64)) {
    throw "Compact firmware Header V1 fields are invalid: $Image"
}
if (($imageSize -eq 0) -or ($imageBytes.Length -ne (64 + $imageSize))) {
    throw "Compact firmware image size does not match Header: $Image"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$evidenceDirectory = Join-Path $projectRoot "06_Output\Logs\ExternalLoader\Preburn-$timestamp"
New-Item -ItemType Directory -Force -Path $evidenceDirectory | Out-Null
$headerPath = Join-Path $evidenceDirectory "slot_a_header.bin"
$payloadPath = Join-Path $evidenceDirectory "slot_a_payload.bin"
$headerReadbackPath = Join-Path $evidenceDirectory "slot_a_header_readback.bin"
$payloadReadbackPath = Join-Path $evidenceDirectory "slot_a_payload_readback.bin"
$slotBClearPath = Join-Path $evidenceDirectory "slot_b_header_sector_erased.bin"
$slotBReadbackPath = Join-Path $evidenceDirectory "slot_b_header_readback.bin"

$headerBytes = New-Object byte[] 64
$payloadBytes = New-Object byte[] $imageSize
[Array]::Copy($imageBytes, 0, $headerBytes, 0, 64)
[Array]::Copy($imageBytes, 64, $payloadBytes, 0, $imageSize)
[System.IO.File]::WriteAllBytes($headerPath, $headerBytes)
[System.IO.File]::WriteAllBytes($payloadPath, $payloadBytes)

function Invoke-CubeProgrammer {
    param(
        [string[]]$Arguments,
        [string]$LogName
    )

    $logPath = Join-Path $evidenceDirectory $LogName
    Write-Host "[PREBURN] STM32_Programmer_CLI $($Arguments -join ' ')"
    $output = & $CubeProgrammer @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        throw "STM32CubeProgrammer failed with exit code $exitCode. See $logPath"
    }
}

$commonArguments = @(
    "-el", (Resolve-Path -LiteralPath $Loader).Path,
    "-c", "port=$Port", "freq=$Frequency"
)

if ($ClearSlotBHeader) {
    $slotBClearBytes = New-Object byte[] 0x1000
    for ($index = 0; $index -lt $slotBClearBytes.Length; $index++) {
        $slotBClearBytes[$index] = 0xFF
    }
    [System.IO.File]::WriteAllBytes($slotBClearPath, $slotBClearBytes)
    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
            "-w", $slotBClearPath, "0x90080000", "-v", "-y"
        )) -LogName "clear_slot_b_header.log"
    Invoke-CubeProgrammer -Arguments ($commonArguments + @(
            "-u", "0x90080000", "64", $slotBReadbackPath
        )) -LogName "read_slot_b_header.log"
    $slotBHeaderBytes = [System.IO.File]::ReadAllBytes($slotBReadbackPath)
    if (($slotBHeaderBytes | Where-Object { $_ -ne 0xFF }).Count -ne 0) {
        throw "Slot B Header is not erased after External Loader clear"
    }
}

# Payload first, Header last: a power loss cannot publish a valid Header for an incomplete Payload.
Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-w", $payloadPath, "0x90001000", "-v", "-y"
    )) -LogName "write_payload.log"
Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-w", $headerPath, "0x90000000", "-v", "-y"
    )) -LogName "write_header.log"

Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-u", "0x90000000", "64", $headerReadbackPath
    )) -LogName "read_header.log"
Invoke-CubeProgrammer -Arguments ($commonArguments + @(
        "-u", "0x90001000", [string]$imageSize, $payloadReadbackPath
    )) -LogName "read_payload.log"

$sourceHeaderHash = Get-Sha256Hex -Path $headerPath
$readHeaderHash = Get-Sha256Hex -Path $headerReadbackPath
$sourcePayloadHash = Get-Sha256Hex -Path $payloadPath
$readPayloadHash = Get-Sha256Hex -Path $payloadReadbackPath

Write-Host "[PREBURN] header source_sha256=$sourceHeaderHash readback_sha256=$readHeaderHash"
Write-Host "[PREBURN] payload source_sha256=$sourcePayloadHash readback_sha256=$readPayloadHash"
if (($sourceHeaderHash -ne $readHeaderHash) -or ($sourcePayloadHash -ne $readPayloadHash)) {
    throw "Slot A readback does not match the source image"
}

Write-Host "[PREBURN][PASS] Slot A physical Header and Payload read back correctly"
Write-Host "[PREBURN] evidence=$evidenceDirectory"
