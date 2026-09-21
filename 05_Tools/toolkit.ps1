param(
    [Parameter(Position = 0, Mandatory = $true)]
    [string]$Command,

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @()
)

$ErrorActionPreference = "Stop"
$toolsRoot = (Resolve-Path $PSScriptRoot).Path

function Test-PositiveInteger {
    param([string]$Value)

    $parsed = 0
    return [int]::TryParse($Value, [ref]$parsed) -and ($parsed -gt 0)
}

function Invoke-ToolkitWorkflow {
    param(
        [string]$Path,
        [string[]]$WorkflowArguments = @()
    )

    $workflowOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Path @WorkflowArguments
    $workflowExitCode = $LASTEXITCODE
    $workflowOutput | ForEach-Object { Write-Host $_ }
    return [int]$workflowExitCode
}

function ConvertTo-ToolkitExitCode {
    param(
        [string]$Workflow,
        [int]$ExitCode
    )

    if ($ExitCode -eq 0) {
        return 0
    }
    if ($Workflow -in @("firmware", "ymodem")) {
        return 50
    }
    if (($Workflow -in @("build", "run")) -and ($ExitCode -eq 1)) {
        return 1
    }
    if ($ExitCode -in @(10, 20, 30, 40, 50, 60)) {
        return $ExitCode
    }
    switch ($Workflow) {
        "build" { return 20 }
        "flash" { return 30 }
        "rtt" { return 30 }
        "run" { return 30 }
        "snapshot" { return 40 }
        "fault" { return 40 }
        "logic" { return 30 }
    }
    return 10
}

function Get-ToolkitConfiguration {
    Import-Module -Name (Join-Path $toolsRoot "Core\Toolkit.Core.psm1") -Force
    return Import-ToolkitConfiguration -ToolsRoot $toolsRoot
}

function Get-ConfiguredPython {
    param([object]$Configuration)

    $property = $Configuration.PSObject.Properties["PYTHON_EXE"]
    if ($null -ne $property -and -not [string]::IsNullOrWhiteSpace([string]$property.Value)) {
        return [string]$property.Value
    }
    return "python"
}

function Get-YmodemProcessTimeoutMilliseconds {
    param(
        [string[]]$Arguments = @()
    )

    $protocolTimeoutSeconds = 1.0
    for ($index = 0; $index -lt ($Arguments.Count - 1); $index++) {
        if ([string]$Arguments[$index] -ne "--timeout") {
            continue
        }

        $parsedTimeout = 0.0
        if ([double]::TryParse([string]$Arguments[$index + 1], [ref]$parsedTimeout) -and
            ($parsedTimeout -gt 0.0)) {
            $protocolTimeoutSeconds = $parsedTimeout
        }
        break
    }

    $protocolWindowMilliseconds = [math]::Ceiling(($protocolTimeoutSeconds * 1000.0) + 30000.0)
    return [int][math]::Max(300000.0, $protocolWindowMilliseconds)
}

function Invoke-ExternalToolkitCommand {
    param(
        [string]$FilePath,
        [string[]]$CommandArguments = @(),
        [string]$WorkingDirectory = $toolsRoot,
        [int]$TimeoutMilliseconds = 60000
    )

    $result = Invoke-ToolkitProcess -FilePath $FilePath -Arguments $CommandArguments -WorkingDirectory $WorkingDirectory -TimeoutMilliseconds $TimeoutMilliseconds
    if (-not [string]::IsNullOrEmpty([string]$result.Stdout)) {
        Write-Host ([string]$result.Stdout).TrimEnd()
    }
    if (-not [string]::IsNullOrEmpty([string]$result.Stderr)) {
        Write-Host ([string]$result.Stderr).TrimEnd()
    }
    return [int]$result.ExitCode
}

function Invoke-FirmwarePack {
    param([string[]]$PackArguments)

    $configuration = Get-ToolkitConfiguration
    $python = Get-ConfiguredPython -Configuration $configuration
    $packer = Join-Path $toolsRoot "Firmware\pack_firmware.py"
    if (-not (Test-Path -LiteralPath $packer -PathType Leaf)) {
        throw "Firmware packer not found: $packer"
    }
    return Invoke-ExternalToolkitCommand -FilePath $python -CommandArguments (@($packer) + $PackArguments) -WorkingDirectory $configuration.PROJECT_ROOT
}

function Invoke-TeraTermYmodem {
    param(
        [object]$Configuration,
        [string[]]$TransferArguments
    )

    if ($TransferArguments.Count -ne 3) {
        throw "Tera Term YMODEM requires: <COMx> <baud> <firmware.img>"
    }
    $port = $TransferArguments[0] -replace '(?i)^COM', ''
    if ((-not (Test-PositiveInteger $port))) {
        throw "COM port must be a positive number or COMx: $($TransferArguments[0])"
    }
    if (-not (Test-PositiveInteger $TransferArguments[1])) {
        throw "Baud rate must be a positive integer: $($TransferArguments[1])"
    }
    $firmware = Resolve-Path -LiteralPath $TransferArguments[2] -ErrorAction Stop
    $teraTerm = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "TERA_TERM_EXE"
    $macro = Join-Path $toolsRoot "TeraTerm\send_ymodem.ttl"
    $macroExe = Join-Path (Split-Path -Parent $teraTerm) "ttpmacro.exe"
    foreach ($path in @($macro, $macroExe)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required Tera Term file not found: $path"
        }
    }
    return Invoke-ExternalToolkitCommand -FilePath $macroExe -CommandArguments @(
        "/V", $macro, $port, $TransferArguments[1], $firmware.Path
    ) -WorkingDirectory $configuration.PROJECT_ROOT
}

function Invoke-Ymodem {
    param([string[]]$YmodemArguments)

    $configuration = Get-ToolkitConfiguration
    $transport = if ($YmodemArguments.Count -gt 0) { $YmodemArguments[0].ToLowerInvariant() } else { "python" }
    if ($transport -eq "tera") {
        return Invoke-TeraTermYmodem -Configuration $configuration -TransferArguments ($YmodemArguments | Select-Object -Skip 1)
    }
    if ($transport -eq "python") {
        $YmodemArguments = @($YmodemArguments | Select-Object -Skip 1)
    }
    $sender = Join-Path $toolsRoot "Ymodem\ymodem_sender.py"
    if (-not (Test-Path -LiteralPath $sender -PathType Leaf)) {
        throw "YMODEM sender not found: $sender"
    }
    $processTimeoutMilliseconds = Get-YmodemProcessTimeoutMilliseconds -Arguments $YmodemArguments
    return Invoke-ExternalToolkitCommand -FilePath (Get-ConfiguredPython -Configuration $configuration) -CommandArguments (@($sender) + $YmodemArguments) -WorkingDirectory $configuration.PROJECT_ROOT -TimeoutMilliseconds $processTimeoutMilliseconds
}

function New-WorkflowArguments {
    param(
        [string]$Workflow,
        [string[]]$WorkflowArguments = @()
    )

    return @("-ToolsRoot", $toolsRoot) + $WorkflowArguments
}

function New-LogicWorkflowArguments {
    param(
        [string[]]$LogicArguments = @()
    )

    if ($LogicArguments.Count -lt 1) {
        throw "logic requires one subcommand: doctor, scan, spi, i2c or decode"
    }
    $action = $LogicArguments[0].ToLowerInvariant()
    if ($action -notin @("doctor", "scan", "spi", "i2c", "decode")) {
        throw "logic subcommand must be one of: doctor, scan, spi, i2c or decode"
    }

    $remaining = @($LogicArguments | Select-Object -Skip 1)
    if ($action -in @("doctor", "scan") -and $remaining.Count -gt 0) {
        throw "logic $action does not accept arguments"
    }
    $workflowArguments = @("-Action", $action)
    if ($action -eq "decode" -and $remaining.Count -gt 0 -and -not $remaining[0].StartsWith("-")) {
        $workflowArguments += @("-CapturePath", $remaining[0])
        $remaining = @($remaining | Select-Object -Skip 1)
    }
    return $workflowArguments + $remaining
}

try {
    $workflowPath = $null
    $workflowArguments = @()
    $workflowCode = 0
    $workflow = $Command.ToLowerInvariant()
    switch ($workflow) {
        "build" {
            if ($Arguments.Count -gt 1) {
                throw "build accepts at most one target: application or bootloader"
            }
            $target = if ($Arguments.Count -eq 0) { "application" } else { $Arguments[0].ToLowerInvariant() }
            if ($target -notin @("application", "bootloader")) {
                throw "build target must be application or bootloader"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\build.ps1"
            $workflowArguments = @("-Target", $target)
        }
        "sync-s08" {
            if ($Arguments.Count -gt 0) {
                throw "sync-s08 does not accept arguments"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\S08_BootloaderFoundation\sync_generated.ps1"
        }
        "metadata" {
            if ($Arguments.Count -lt 1 -or $Arguments[0].ToLowerInvariant() -ne "baseline") {
                throw "metadata command must be: metadata baseline [options]"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\metadata_baseline.ps1"
            $workflowArguments = @($Arguments | Select-Object -Skip 1)
        }
        "flash" {
            $mode = "run"
            $target = "application"
            if ($Arguments.Count -eq 1) {
                $argument = $Arguments[0].ToLowerInvariant()
                if ($argument -in @("run", "prepare")) {
                    $mode = $argument
                }
                elseif ($argument -in @("application", "bootloader")) {
                    $target = $argument
                }
                else {
                    throw "flash argument must be run, prepare, application or bootloader"
                }
            }
            elseif ($Arguments.Count -eq 2) {
                $target = $Arguments[0].ToLowerInvariant()
                $mode = $Arguments[1].ToLowerInvariant()
                if ($target -notin @("application", "bootloader") -or $mode -notin @("run", "prepare")) {
                    throw "flash usage: flash [application|bootloader] [run|prepare]"
                }
            }
            elseif ($Arguments.Count -gt 2) {
                throw "flash accepts target and mode at most"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\flash.ps1"
            $workflowArguments = @("-Mode", $mode, "-Target", $target)
        }
        "rtt" {
            $target = "application"
            $seconds = 0
            if ($Arguments.Count -eq 1) {
                if (Test-PositiveInteger $Arguments[0]) {
                    $seconds = $Arguments[0]
                }
                else {
                    $target = $Arguments[0].ToLowerInvariant()
                }
            }
            elseif ($Arguments.Count -eq 2) {
                $target = $Arguments[0].ToLowerInvariant()
                if (-not (Test-PositiveInteger $Arguments[1])) {
                    throw "rtt duration must be a positive integer"
                }
                $seconds = $Arguments[1]
            }
            elseif ($Arguments.Count -gt 2 -or $target -notin @("application", "bootloader")) {
                throw "rtt usage: rtt [application|bootloader] [seconds]"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\rtt.ps1"
            $workflowArguments = @("-Target", $target)
            if ($seconds -gt 0) {
                $workflowArguments += @("-Seconds", $seconds)
            }
        }
        "run" {
            $target = "application"
            $seconds = 0
            if ($Arguments.Count -eq 1) {
                if (Test-PositiveInteger $Arguments[0]) {
                    $seconds = $Arguments[0]
                }
                else {
                    $target = $Arguments[0].ToLowerInvariant()
                }
            }
            elseif ($Arguments.Count -eq 2) {
                $target = $Arguments[0].ToLowerInvariant()
                if (-not (Test-PositiveInteger $Arguments[1])) {
                    throw "run duration must be a positive integer"
                }
                $seconds = $Arguments[1]
            }
            elseif ($Arguments.Count -gt 2 -or $target -notin @("application", "bootloader")) {
                throw "run usage: run [application|bootloader] [seconds]"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\run.ps1"
            $workflowArguments = @("-Target", $target)
            if ($seconds -gt 0) {
                $workflowArguments += @("-Seconds", $seconds)
            }
        }
        "snapshot" {
            $target = "application"
            $mode = "halt"
            if ($Arguments.Count -eq 1) {
                $argument = $Arguments[0].ToLowerInvariant()
                if ($argument -in @("halt", "resume")) {
                    $mode = $argument
                }
                else {
                    $target = $argument
                }
            }
            elseif ($Arguments.Count -eq 2) {
                $target = $Arguments[0].ToLowerInvariant()
                $mode = $Arguments[1].ToLowerInvariant()
            }
            elseif ($Arguments.Count -gt 2) {
                throw "snapshot usage: snapshot [application|bootloader] [halt|resume]"
            }
            if ($target -notin @("application", "bootloader") -or $mode -notin @("halt", "resume")) {
                throw "snapshot usage: snapshot [application|bootloader] [halt|resume]"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Debug\snapshot.ps1"
            $workflowArguments = @("-Mode", $mode, "-Target", $target)
        }
        "fault" {
            $target = "application"
            $mode = "capture"
            if ($Arguments.Count -eq 1) {
                $argument = $Arguments[0].ToLowerInvariant()
                if ($argument -in @("capture", "trigger")) {
                    $mode = $argument
                }
                else {
                    $target = $argument
                }
            }
            elseif ($Arguments.Count -eq 2) {
                $target = $Arguments[0].ToLowerInvariant()
                $mode = $Arguments[1].ToLowerInvariant()
            }
            elseif ($Arguments.Count -gt 2) {
                throw "fault usage: fault [application|bootloader] [capture|trigger]"
            }
            if ($target -notin @("application", "bootloader") -or $mode -notin @("capture", "trigger")) {
                throw "fault usage: fault [application|bootloader] [capture|trigger]"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Debug\fault_capture.ps1"
            $workflowArguments = @("-Mode", $mode, "-Target", $target)
        }
        "logic" {
            $workflowPath = Join-Path $toolsRoot "Workflows\LogicAnalyzer\logic.ps1"
            $workflowArguments = New-LogicWorkflowArguments -LogicArguments $Arguments
        }
        "firmware" {
            if ($Arguments.Count -lt 1 -or $Arguments[0].ToLowerInvariant() -ne "pack") {
                throw "firmware command must be: firmware pack <arguments>"
            }
            $workflowCode = Invoke-FirmwarePack -PackArguments @($Arguments | Select-Object -Skip 1)
            exit (ConvertTo-ToolkitExitCode -Workflow $workflow -ExitCode $workflowCode)
        }
        "ymodem" {
            $workflowCode = Invoke-Ymodem -YmodemArguments $Arguments
            exit (ConvertTo-ToolkitExitCode -Workflow $workflow -ExitCode $workflowCode)
        }
        default {
            throw "Unknown toolkit command: $Command"
        }
    }

    $workflowCode = Invoke-ToolkitWorkflow -Path $workflowPath -WorkflowArguments (New-WorkflowArguments -Workflow $workflow -WorkflowArguments $workflowArguments)
    exit (ConvertTo-ToolkitExitCode -Workflow $workflow -ExitCode $workflowCode)
}
catch {
    Write-Host "[TOOLKIT][ERROR] $($_.Exception.Message)"
    if ($workflow -in @("firmware", "ymodem")) {
        exit 50
    }
    exit 10
}
