$coreModulePath = Join-Path $PSScriptRoot "..\..\..\Core\Toolkit.Core.psm1"
if ($null -eq (Get-Command Invoke-ToolkitProcess -ErrorAction SilentlyContinue)) {
    Import-Module -Name (Resolve-Path -LiteralPath $coreModulePath -ErrorAction Stop).Path -Force
}

function Get-SigrokCliPath {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration
    )

    $configuredPath = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "SIGROK_CLI_EXE"
    if (Test-Path -LiteralPath $configuredPath -PathType Leaf) {
        return (Resolve-Path -LiteralPath $configuredPath).Path
    }

    $command = Get-Command $configuredPath -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Sigrok CLI executable not found: $configuredPath"
}

function Invoke-SigrokCli {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [string[]]$Arguments = @(),

        [int]$TimeoutMilliseconds = 60000
    )

    $sigrok = Get-SigrokCliPath -Configuration $Configuration
    return Invoke-ToolkitProcess -FilePath $sigrok -Arguments $Arguments -TimeoutMilliseconds $TimeoutMilliseconds -WorkingDirectory $Configuration.PROJECT_ROOT
}

function Invoke-SigrokVersion {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [int]$TimeoutMilliseconds = 10000
    )

    return Invoke-SigrokCli -Configuration $Configuration -Arguments @("--version") -TimeoutMilliseconds $TimeoutMilliseconds
}

function Get-SigrokDeviceSelectors {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Output,

        [Parameter(Mandatory = $true)]
        [string]$Driver
    )

    $driverPattern = [regex]::Escape($Driver)
    $selectors = [System.Collections.Generic.List[string]]::new()
    foreach ($line in ($Output -split "`r?`n")) {
        if ($line -match ("(?i)^\s*(?<selector>{0}:[^\s]+)" -f $driverPattern)) {
            $selector = $matches.selector
            if (-not $selectors.Contains($selector)) {
                $selectors.Add($selector)
            }
        }
        elseif ($line -match ("(?i)^\s*{0}(?:\s+-|\s*$)" -f $driverPattern)) {
            $selectors.Add($Driver)
        }
    }

    return @($selectors)
}

function Invoke-SigrokScan {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [string]$Driver = "fx2lafw",

        [string]$Selector = "",

        [int]$TimeoutMilliseconds = 15000
    )

    $result = Invoke-SigrokCli -Configuration $Configuration -Arguments @("--scan") -TimeoutMilliseconds $TimeoutMilliseconds
    $devices = @(Get-SigrokDeviceSelectors -Output $result.Stdout -Driver $Driver)
    $selectedDevice = $null
    $errorClass = $null

    if ($result.TimedOut) {
        $errorClass = "SCAN_TIMEOUT"
    }
    elseif ($result.ExitCode -ne 0) {
        $errorClass = "SCAN_FAILED"
    }
    elseif (-not [string]::IsNullOrWhiteSpace($Selector)) {
        if ($devices -contains $Selector) {
            $selectedDevice = $Selector
        }
        else {
            $errorClass = "DEVICE_NOT_FOUND"
        }
    }
    elseif ($devices.Count -eq 0) {
        $errorClass = "DEVICE_NOT_FOUND"
    }
    elseif ($devices.Count -eq 1) {
        $selectedDevice = $devices[0]
    }
    else {
        $errorClass = "AMBIGUOUS_DEVICE"
    }

    return [PSCustomObject]@{
        ExitCode = $result.ExitCode
        Stdout = $result.Stdout
        Stderr = $result.Stderr
        TimedOut = $result.TimedOut
        ProcessId = $result.ProcessId
        Devices = $devices
        SelectedDevice = $selectedDevice
        ErrorClass = $errorClass
    }
}

function Invoke-SigrokCapture {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [string]$OutputPath,

        [string]$Driver = "fx2lafw",

        [string]$DeviceSelector = "",

        [string]$SampleRate = "48MHz",

        [int]$Samples = 500000,

        [int]$CaptureTimeMilliseconds = 0,

        [int]$TimeoutMilliseconds = 60000
    )

    if ($Samples -lt 1 -or $CaptureTimeMilliseconds -lt 0) {
        throw "Sigrok sample count must be positive: $Samples"
    }
    $outputDirectory = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }

    $driverValue = $Driver
    if (-not [string]::IsNullOrWhiteSpace($DeviceSelector)) {
        $driverValue = $DeviceSelector
    }
    $acquisitionArguments = if ($CaptureTimeMilliseconds -gt 0) {
        @("--time", [string]$CaptureTimeMilliseconds)
    }
    else {
        @("--samples", [string]$Samples)
    }
    $arguments = @(
        "--driver", $driverValue,
        "--config", ("samplerate={0}" -f $SampleRate)
    ) + $acquisitionArguments + @(
        "--output-format", "srzip",
        "--output-file", $OutputPath
    )
    return Invoke-SigrokCli -Configuration $Configuration -Arguments $arguments -TimeoutMilliseconds $TimeoutMilliseconds
}

function Invoke-SigrokDecode {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [string]$CapturePath,

        [Parameter(Mandatory = $true)]
        [string]$DecoderSpec,

        [string]$Annotation = "",

        [int]$TimeoutMilliseconds = 60000
    )

    if (-not (Test-Path -LiteralPath $CapturePath -PathType Leaf)) {
        throw "Sigrok capture file not found: $CapturePath"
    }
    $arguments = @(
        "--input-file", $CapturePath,
        "--protocol-decoders", $DecoderSpec
    )
    if (-not [string]::IsNullOrWhiteSpace($Annotation)) {
        $arguments += @("--protocol-decoder-annotations", $Annotation)
    }
    return Invoke-SigrokCli -Configuration $Configuration -Arguments $arguments -TimeoutMilliseconds $TimeoutMilliseconds
}
