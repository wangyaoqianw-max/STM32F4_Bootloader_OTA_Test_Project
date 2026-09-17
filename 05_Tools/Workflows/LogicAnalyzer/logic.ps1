param(
    [string]$ToolsRoot = "",

    [string]$Action = "",

    [string]$Protocol = "",

    [string]$Profile = "",

    [string]$CapturePath = "",

    [string]$OutputRoot = "",

    [string]$DeviceSelector = "",

    [string]$DecoderSpec = "",

    [string]$SampleRate = "48MHz",

    [int]$Samples = 500000,

    [int]$CaptureTimeMilliseconds = 0,

    [int]$TimeoutSeconds = 60,

    [string]$CsChannel = "",

    [string]$ClkChannel = "",

    [string]$MisoChannel = "",

    [string]$MosiChannel = "",

    [string]$SclChannel = "",

    [string]$SdaChannel = ""
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
. (Join-Path $frameworkRoot "Adapters\LogicAnalyzer\Sigrok\sigrok_exec.ps1")
. (Join-Path $frameworkRoot "Adapters\LogicAnalyzer\Sigrok\sigrok_parse.ps1")

function Write-LogicJson {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $json = $Value | ConvertTo-Json -Depth 12
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $json, $utf8)
}

function Get-LogicProfiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $profilePath = Join-Path $Root "Config\logic_analyzer.profiles.json"
    if (-not (Test-Path -LiteralPath $profilePath -PathType Leaf)) {
        throw "Logic analyzer profile file not found: $profilePath"
    }
    return Get-Content -LiteralPath $profilePath -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Get-LogicProfileProperty {
    param(
        [Parameter(Mandatory = $true)]
        [object]$ProfileObject,

        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $property = $ProfileObject.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "Logic analyzer profile property is missing: $Name"
    }
    return $property.Value
}

function Resolve-LogicExecutionContext {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [string]$RequestedProtocol = "",

        [string]$RequestedProfile = ""
    )

    $profiles = Get-LogicProfiles -Root $Root
    $profileName = $RequestedProfile
    if ([string]::IsNullOrWhiteSpace($profileName)) {
        if ($RequestedProtocol -eq "spi") {
            $profileName = "spi2_flash"
        }
        elseif ($RequestedProtocol -eq "i2c") {
            $profileName = "i2c_eeprom"
        }
        else {
            throw "Protocol or profile is required for a Logic Analyzer action"
        }
    }

    $profileProperty = $profiles.profiles.PSObject.Properties[$profileName]
    if ($null -eq $profileProperty) {
        throw "Logic analyzer profile not found: $profileName"
    }
    $profile = $profileProperty.Value
    $profileProtocol = [string](Get-LogicProfileProperty -ProfileObject $profile -Name "protocol").ToLowerInvariant()
    if ($profileProtocol -notin @("spi", "i2c")) {
        throw "Unsupported Logic Analyzer profile protocol: $profileProtocol"
    }
    if (-not [string]::IsNullOrWhiteSpace($RequestedProtocol) -and $RequestedProtocol.ToLowerInvariant() -ne $profileProtocol) {
        throw "Logic analyzer profile protocol mismatch: $profileName is $profileProtocol, requested $RequestedProtocol"
    }

    return [PSCustomObject]@{
        Name = $profileName
        Protocol = $profileProtocol
        Profile = $profile
    }
}

function Resolve-LogicChannel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Signal,

        [Parameter(Mandatory = $true)]
        [object]$SignalObject,

        [string]$Override = ""
    )

    $channel = if ([string]::IsNullOrWhiteSpace($Override)) {
        [string](Get-LogicProfileProperty -ProfileObject $SignalObject -Name "logic_channel")
    }
    else {
        $Override
    }
    if ($channel -notmatch '^D[0-7]$') {
        throw ("Invalid Logic Analyzer channel for {0}: {1}" -f $Signal, $channel)
    }
    return $channel.ToUpperInvariant()
}

function Resolve-LogicMapping {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Context,

        [string]$Cs = "",

        [string]$Clk = "",

        [string]$Miso = "",

        [string]$Mosi = "",

        [string]$Scl = "",

        [string]$Sda = ""
    )

    $signals = Get-LogicProfileProperty -ProfileObject $Context.Profile -Name "signals"
    $mapping = [ordered]@{}
    $overrides = [ordered]@{
        cs = $Cs
        clk = $Clk
        miso = $Miso
        mosi = $Mosi
        scl = $Scl
        sda = $Sda
    }
    $requiredSignals = if ($Context.Protocol -eq "spi") { @("cs", "clk", "miso", "mosi") } else { @("scl", "sda") }
    $channels = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($signal in $requiredSignals) {
        $signalProperty = $signals.PSObject.Properties[$signal]
        if ($null -eq $signalProperty) {
            throw "Logic analyzer profile signal is missing: $signal"
        }
        $signalObject = $signalProperty.Value
        $channel = Resolve-LogicChannel -Signal $signal -SignalObject $signalObject -Override $overrides[$signal]
        if (-not $channels.Add($channel)) {
            throw "Logic Analyzer channels must be unique; duplicate channel: $channel"
        }
        $mapping[$signal] = [ordered]@{
            mcu_pin = [string](Get-LogicProfileProperty -ProfileObject $signalObject -Name "mcu_pin")
            logic_channel = $channel
        }
    }
    return $mapping
}

function Get-LogicDecoderSpec {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Protocol,

        [Parameter(Mandatory = $true)]
        [object]$Mapping,

        [string]$Override = ""
    )

    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        return $Override
    }
    if ($Protocol -eq "spi") {
        return "spi:clk={0}:cs={1}:mosi={2}:miso={3}" -f $Mapping.clk.logic_channel, $Mapping.cs.logic_channel, $Mapping.mosi.logic_channel, $Mapping.miso.logic_channel
    }
    return "i2c:scl={0}:sda={1}" -f $Mapping.scl.logic_channel, $Mapping.sda.logic_channel
}

function New-LogicRunDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    New-Item -ItemType Directory -Path $Root -Force | Out-Null
    $runName = "{0}_{1}" -f (Get-Date -Format "yyyyMMdd_HHmmss_fff"), ([Guid]::NewGuid().ToString("N").Substring(0, 8))
    $runDirectory = Join-Path $Root $runName
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    return $runDirectory
}

function New-LogicEffectiveConfig {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Context,

        [Parameter(Mandatory = $true)]
        [object]$Mapping,

        [Parameter(Mandatory = $true)]
        [string]$Decoder,

        [Parameter(Mandatory = $true)]
        [string]$RunDirectory,

        [string]$SelectedDevice = "",

        [int]$CaptureTimeMilliseconds = 0
    )

    return [PSCustomObject][ordered]@{
        protocol = $Context.Protocol
        profile = $Context.Name
        sample_rate = $SampleRate
        samples = $Samples
        capture_time_ms = if ($CaptureTimeMilliseconds -gt 0) { $CaptureTimeMilliseconds } else { $null }
        device = [ordered]@{
            driver = "fx2lafw"
            selector = if ([string]::IsNullOrWhiteSpace($SelectedDevice)) { $null } else { $SelectedDevice }
        }
        decoder = $Decoder
        mapping = $Mapping
        capture_input = $null
        artifacts = [ordered]@{
            effective_config = Join-Path $RunDirectory "effective_config.json"
            capture = Join-Path $RunDirectory "capture.sr"
            decode = Join-Path $RunDirectory "decode.json"
            result = Join-Path $RunDirectory "result.json"
        }
    }
}

function New-LogicWorkflowResult {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Status,

        [Parameter(Mandatory = $true)]
        [string]$Operation,

        [Parameter(Mandatory = $true)]
        [object]$Context,

        [Parameter(Mandatory = $true)]
        [object]$Mapping,

        [string]$ErrorClass = $null,

        [object[]]$Transactions = @(),

        [string]$SelectedDevice = "",

        [object]$Capture = $null,

        [object]$Artifacts = $null
    )

    return [PSCustomObject][ordered]@{
        status = $Status
        operation = $Operation
        device = [ordered]@{
            driver = "fx2lafw"
            selector = if ([string]::IsNullOrWhiteSpace($SelectedDevice)) { $null } else { $SelectedDevice }
        }
        capture = if ($null -eq $Capture) { [ordered]@{} } else { $Capture }
        mapping = $Mapping
        transactions = @($Transactions)
        error_class = $ErrorClass
        artifacts = if ($null -eq $Artifacts) { [ordered]@{} } else { $Artifacts }
    }
}

function Get-LogicOutputRoot {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [string]$RequestedRoot = ""
    )

    if ([string]::IsNullOrWhiteSpace($RequestedRoot)) {
        return Join-Path $Configuration.PROJECT_ROOT "06_Output\LogicAnalyzer"
    }
    if ([System.IO.Path]::IsPathRooted($RequestedRoot)) {
        return $RequestedRoot
    }
    return Join-Path $Configuration.PROJECT_ROOT $RequestedRoot
}

function Invoke-LogicCaptureAction {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [object]$Context,

        [Parameter(Mandatory = $true)]
        [object]$Mapping,

        [Parameter(Mandatory = $true)]
        [string]$RunDirectory
    )

    $decoder = Get-LogicDecoderSpec -Protocol $Context.Protocol -Mapping $Mapping -Override $DecoderSpec
    $scan = Invoke-SigrokScan -Configuration $Configuration -Driver "fx2lafw" -Selector $DeviceSelector -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    $selectedDevice = [string]$scan.SelectedDevice
    $effectiveConfig = New-LogicEffectiveConfig -Context $Context -Mapping $Mapping -Decoder $decoder -RunDirectory $RunDirectory -SelectedDevice $selectedDevice -CaptureTimeMilliseconds $CaptureTimeMilliseconds
    $effectiveConfigPath = $effectiveConfig.artifacts.effective_config
    Write-LogicJson -Value $effectiveConfig -Path $effectiveConfigPath
    Write-Host ("Protocol : {0}" -f $Context.Protocol)
    Write-Host ("Profile  : {0}" -f $Context.Name)
    Write-Host ("Decoder  : {0}" -f $decoder)
    Write-Host ("Mapping  : {0}" -f (($Mapping | ConvertTo-Json -Compress -Depth 5)))

    if ($scan.ErrorClass) {
        $result = New-LogicWorkflowResult -Status "ERROR" -Operation ("{0}_CAPTURE" -f $Context.Protocol.ToUpperInvariant()) -Context $Context -Mapping $Mapping -ErrorClass $scan.ErrorClass -SelectedDevice $selectedDevice -Artifacts $effectiveConfig.artifacts
        Write-LogicJson -Value $result -Path $effectiveConfig.artifacts.result
        return [PSCustomObject]@{ Result = $result; EffectiveConfig = $effectiveConfig }
    }

    $captureProcess = Invoke-SigrokCapture -Configuration $Configuration -OutputPath $effectiveConfig.artifacts.capture -Driver "fx2lafw" -DeviceSelector $selectedDevice -SampleRate $SampleRate -Samples $Samples -CaptureTimeMilliseconds $CaptureTimeMilliseconds -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    if ($captureProcess.TimedOut -or $captureProcess.ExitCode -ne 0) {
        $errorClass = if ($captureProcess.TimedOut) { "CAPTURE_TIMEOUT" } else { "CAPTURE_FAILED" }
        $result = New-LogicWorkflowResult -Status "ERROR" -Operation ("{0}_CAPTURE" -f $Context.Protocol.ToUpperInvariant()) -Context $Context -Mapping $Mapping -ErrorClass $errorClass -SelectedDevice $selectedDevice -Capture ([ordered]@{ exit_code = $captureProcess.ExitCode; timed_out = $captureProcess.TimedOut }) -Artifacts $effectiveConfig.artifacts
        Write-LogicJson -Value $result -Path $effectiveConfig.artifacts.result
        return [PSCustomObject]@{ Result = $result; EffectiveConfig = $effectiveConfig }
    }
    if (-not (Test-Path -LiteralPath $effectiveConfig.artifacts.capture -PathType Leaf)) {
        $result = New-LogicWorkflowResult -Status "ERROR" -Operation ("{0}_CAPTURE" -f $Context.Protocol.ToUpperInvariant()) -Context $Context -Mapping $Mapping -ErrorClass "CAPTURE_OUTPUT_MISSING" -SelectedDevice $selectedDevice -Capture ([ordered]@{ exit_code = $captureProcess.ExitCode; timed_out = $captureProcess.TimedOut }) -Artifacts $effectiveConfig.artifacts
        Write-LogicJson -Value $result -Path $effectiveConfig.artifacts.result
        return [PSCustomObject]@{ Result = $result; EffectiveConfig = $effectiveConfig }
    }

    $result = New-LogicWorkflowResult -Status "SUCCESS" -Operation ("{0}_CAPTURE" -f $Context.Protocol.ToUpperInvariant()) -Context $Context -Mapping $Mapping -SelectedDevice $selectedDevice -Capture ([ordered]@{ sample_rate = $SampleRate; samples = $Samples; capture_time_ms = if ($CaptureTimeMilliseconds -gt 0) { $CaptureTimeMilliseconds } else { $null }; artifact = $effectiveConfig.artifacts.capture }) -Artifacts $effectiveConfig.artifacts
    Write-LogicJson -Value $result -Path $effectiveConfig.artifacts.result
    return [PSCustomObject]@{ Result = $result; EffectiveConfig = $effectiveConfig }
}

function Invoke-LogicDecodeAction {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [object]$Context,

        [Parameter(Mandatory = $true)]
        [object]$Mapping,

        [Parameter(Mandatory = $true)]
        [string]$RunDirectory,

        [Parameter(Mandatory = $true)]
        [string]$InputCapturePath,

        [string]$SelectedDevice = "",

        [int]$CaptureTimeMilliseconds = 0
    )

    $resolvedCapturePath = (Resolve-Path -LiteralPath $InputCapturePath -ErrorAction Stop).Path
    $decoder = Get-LogicDecoderSpec -Protocol $Context.Protocol -Mapping $Mapping -Override $DecoderSpec
    $effectiveConfig = New-LogicEffectiveConfig -Context $Context -Mapping $Mapping -Decoder $decoder -RunDirectory $RunDirectory -SelectedDevice $SelectedDevice -CaptureTimeMilliseconds $CaptureTimeMilliseconds
    $artifactCapturePath = $effectiveConfig.artifacts.capture
    if ($resolvedCapturePath -ne $artifactCapturePath) {
        Copy-Item -LiteralPath $resolvedCapturePath -Destination $artifactCapturePath -Force
    }
    $effectiveConfig.capture_input = $resolvedCapturePath
    Write-LogicJson -Value $effectiveConfig -Path $effectiveConfig.artifacts.effective_config
    $decodeProcesses = if ($Context.Protocol -eq "spi") {
        @(
            (Invoke-SigrokDecode -Configuration $Configuration -CapturePath $resolvedCapturePath -DecoderSpec $decoder -Annotation "spi=mosi-data" -TimeoutMilliseconds ($TimeoutSeconds * 1000)),
            (Invoke-SigrokDecode -Configuration $Configuration -CapturePath $resolvedCapturePath -DecoderSpec $decoder -Annotation "spi=miso-data" -TimeoutMilliseconds ($TimeoutSeconds * 1000))
        )
    }
    else {
        @(Invoke-SigrokDecode -Configuration $Configuration -CapturePath $resolvedCapturePath -DecoderSpec $decoder -Annotation "i2c" -TimeoutMilliseconds ($TimeoutSeconds * 1000))
    }
    $failedDecode = $decodeProcesses | Where-Object { $_.TimedOut -or $_.ExitCode -ne 0 } | Select-Object -First 1
    if ($null -ne $failedDecode) {
        $errorClass = if ($failedDecode.TimedOut) { "DECODE_TIMEOUT" } else { "DECODE_FAILED" }
        $result = New-LogicWorkflowResult -Status "ERROR" -Operation ("{0}_DECODE" -f $Context.Protocol.ToUpperInvariant()) -Context $Context -Mapping $Mapping -ErrorClass $errorClass -SelectedDevice $SelectedDevice -Capture ([ordered]@{ artifact = $artifactCapturePath }) -Artifacts $effectiveConfig.artifacts
        Write-LogicJson -Value $result -Path $effectiveConfig.artifacts.result
        return $result
    }

    $decodeOutput = if ($Context.Protocol -eq "spi") {
        "[MOSI]`r`n$($decodeProcesses[0].Stdout)`r`n[MISO]`r`n$($decodeProcesses[1].Stdout)"
    }
    else {
        $decodeProcesses[0].Stdout
    }
    $parsed = ConvertFrom-SigrokDecodeOutput -Protocol $Context.Protocol -Output $decodeOutput
    $parsed.device = [ordered]@{ driver = "fx2lafw"; selector = if ([string]::IsNullOrWhiteSpace($SelectedDevice)) { $null } else { $SelectedDevice } }
    $parsed.capture = [ordered]@{ artifact = $artifactCapturePath }
    $parsed.mapping = $Mapping
    $parsed.artifacts = $effectiveConfig.artifacts
    Write-LogicJson -Value $parsed -Path $effectiveConfig.artifacts.decode
    Write-LogicJson -Value $parsed -Path $effectiveConfig.artifacts.result
    return [PSCustomObject]@{ Result = $parsed; EffectiveConfig = $effectiveConfig }
}

function Invoke-LogicDoctorAction {
    param([object]$Configuration)

    $version = Invoke-SigrokVersion -Configuration $Configuration -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    $status = if ($version.TimedOut -or $version.ExitCode -ne 0) { "ERROR" } else { "SUCCESS" }
    $errorClass = if ($version.TimedOut) { "VERSION_TIMEOUT" } elseif ($version.ExitCode -ne 0) { "TOOL_NOT_FOUND" } else { $null }
    return [PSCustomObject][ordered]@{
        status = $status
        operation = "SIGROK_DOCTOR"
        device = [ordered]@{}
        capture = [ordered]@{}
        mapping = [ordered]@{}
        transactions = @()
        error_class = $errorClass
        artifacts = [ordered]@{}
        version = $version.Stdout.Trim()
    }
}

function Invoke-LogicScanAction {
    param([object]$Configuration)

    return Invoke-SigrokScan -Configuration $Configuration -Driver "fx2lafw" -Selector $DeviceSelector -TimeoutMilliseconds ($TimeoutSeconds * 1000)
}

try {
    $normalizedAction = $Action.ToLowerInvariant()
    if ($normalizedAction -notin @("doctor", "scan", "capture", "decode", "spi", "i2c")) {
        throw "Logic Analyzer action must be one of: doctor, scan, capture, decode, spi, i2c"
    }
    if ($Samples -lt 1 -or $CaptureTimeMilliseconds -lt 0 -or $TimeoutSeconds -lt 1) {
        throw "Logic Analyzer samples, capture time and timeout must be valid"
    }

    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    if ($normalizedAction -eq "doctor") {
        $result = Invoke-LogicDoctorAction -Configuration $configuration
        $result | ConvertTo-Json -Depth 12
        $exitCode = if ($result.status -eq "SUCCESS") { 0 } else { 10 }
        exit $exitCode
    }
    if ($normalizedAction -eq "scan") {
        $result = Invoke-LogicScanAction -Configuration $configuration
        $result | ConvertTo-Json -Depth 12
        $exitCode = if ([string]::IsNullOrWhiteSpace([string]$result.ErrorClass)) { 0 } else { 30 }
        exit $exitCode
    }

    $effectiveProtocol = if ($normalizedAction -in @("spi", "i2c")) { $normalizedAction } else { $Protocol.ToLowerInvariant() }
    $context = Resolve-LogicExecutionContext -Root $ToolsRoot -RequestedProtocol $effectiveProtocol -RequestedProfile $Profile
    $mapping = Resolve-LogicMapping -Context $context -Cs $CsChannel -Clk $ClkChannel -Miso $MisoChannel -Mosi $MosiChannel -Scl $SclChannel -Sda $SdaChannel
    $outputRootPath = Get-LogicOutputRoot -Configuration $configuration -RequestedRoot $OutputRoot
    $runDirectory = New-LogicRunDirectory -Root $outputRootPath

    if ($normalizedAction -eq "decode") {
        if ([string]::IsNullOrWhiteSpace($CapturePath)) {
            throw "Decode action requires -CapturePath"
        }
        $execution = Invoke-LogicDecodeAction -Configuration $configuration -Context $context -Mapping $mapping -RunDirectory $runDirectory -InputCapturePath $CapturePath -SelectedDevice $DeviceSelector -CaptureTimeMilliseconds $CaptureTimeMilliseconds
    }
    elseif ($normalizedAction -eq "capture") {
        $execution = Invoke-LogicCaptureAction -Configuration $configuration -Context $context -Mapping $mapping -RunDirectory $runDirectory
    }
    else {
        $captureExecution = Invoke-LogicCaptureAction -Configuration $configuration -Context $context -Mapping $mapping -RunDirectory $runDirectory
        if ($captureExecution.Result.status -ne "SUCCESS") {
            $execution = $captureExecution
        }
        else {
            $execution = Invoke-LogicDecodeAction -Configuration $configuration -Context $context -Mapping $mapping -RunDirectory $runDirectory -InputCapturePath $captureExecution.EffectiveConfig.artifacts.capture -SelectedDevice $captureExecution.Result.device.selector -CaptureTimeMilliseconds $CaptureTimeMilliseconds
        }
    }

    $execution.Result | ConvertTo-Json -Depth 12
    if ($execution.Result.status -eq "ERROR") {
        exit 10
    }
    exit 0
}
catch {
    Write-Host ("[LOGIC][ERROR] {0}" -f $_.Exception.Message)
    exit 10
}
