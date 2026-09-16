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

    if ($ExitCode -in @(0, 1, 10, 20, 30, 40, 50, 60)) {
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
        default {
            throw "Unknown toolkit command: $Command"
        }
    }

    $workflowCode = Invoke-ToolkitWorkflow -Path $workflowPath -WorkflowArguments (New-WorkflowArguments -Workflow $workflow -WorkflowArguments $workflowArguments)
    exit (ConvertTo-ToolkitExitCode -Workflow $workflow -ExitCode $workflowCode)
}
catch {
    Write-Host "[TOOLKIT][ERROR] $($_.Exception.Message)"
    exit 10
}
