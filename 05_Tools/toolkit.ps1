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

function Invoke-ExternalToolkitCommand {
    param(
        [string]$FilePath,
        [string[]]$CommandArguments = @(),
        [string]$WorkingDirectory = $toolsRoot
    )

    $result = Invoke-ToolkitProcess -FilePath $FilePath -Arguments $CommandArguments -WorkingDirectory $WorkingDirectory
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
    return Invoke-ExternalToolkitCommand -FilePath (Get-ConfiguredPython -Configuration $configuration) -CommandArguments (@($sender) + $YmodemArguments) -WorkingDirectory $configuration.PROJECT_ROOT
}

function New-WorkflowArguments {
    param(
        [string]$Workflow,
        [string[]]$WorkflowArguments = @()
    )

    return @("-ToolsRoot", $toolsRoot) + $WorkflowArguments
}

try {
    $workflowPath = $null
    $workflowArguments = @()
    $workflowCode = 0
    $workflow = $Command.ToLowerInvariant()
    switch ($workflow) {
        "build" {
            if ($Arguments.Count -gt 0) {
                throw "build does not accept arguments"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\build.ps1"
        }
        "flash" {
            $mode = if ($Arguments.Count -eq 0) { "run" } elseif ($Arguments.Count -eq 1) { $Arguments[0].ToLowerInvariant() } else { throw "flash accepts at most one mode: run or prepare" }
            if ($mode -notin @("run", "prepare")) {
                throw "flash mode must be run or prepare"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\flash.ps1"
            $workflowArguments = @("-Mode", $mode)
        }
        "rtt" {
            if ($Arguments.Count -gt 1 -or (($Arguments.Count -eq 1) -and (-not (Test-PositiveInteger $Arguments[0])))) {
                throw "rtt accepts one positive duration in seconds"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\rtt.ps1"
            if ($Arguments.Count -eq 1) {
                $workflowArguments = @("-Seconds", $Arguments[0])
            }
        }
        "run" {
            if ($Arguments.Count -gt 1 -or (($Arguments.Count -eq 1) -and (-not (Test-PositiveInteger $Arguments[0])))) {
                throw "run accepts one positive RTT duration in seconds"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Application\run.ps1"
            if ($Arguments.Count -eq 1) {
                $workflowArguments = @("-Seconds", $Arguments[0])
            }
        }
        "snapshot" {
            $mode = if ($Arguments.Count -eq 0) { "halt" } elseif ($Arguments.Count -eq 1) { $Arguments[0].ToLowerInvariant() } else { throw "snapshot accepts at most one mode: halt or resume" }
            if ($mode -notin @("halt", "resume")) {
                throw "snapshot mode must be halt or resume"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Debug\snapshot.ps1"
            $workflowArguments = @("-Mode", $mode)
        }
        "fault" {
            $mode = if ($Arguments.Count -eq 0) { "capture" } elseif ($Arguments.Count -eq 1) { $Arguments[0].ToLowerInvariant() } else { throw "fault accepts at most one mode: capture or trigger" }
            if ($mode -notin @("capture", "trigger")) {
                throw "fault mode must be capture or trigger"
            }
            $workflowPath = Join-Path $toolsRoot "Workflows\Debug\fault_capture.ps1"
            $workflowArguments = @("-Mode", $mode)
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
