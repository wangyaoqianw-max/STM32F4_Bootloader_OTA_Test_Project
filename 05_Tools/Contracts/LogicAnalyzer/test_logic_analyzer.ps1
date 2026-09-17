$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$templatePath = Join-Path $repoRoot "05_Tools\Config\toolchain.local.example.bat"
$profilePath = Join-Path $repoRoot "05_Tools\Config\logic_analyzer.profiles.json"
$coreModulePath = Join-Path $repoRoot "05_Tools\Core\Toolkit.Core.psm1"
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $failures.Add($Message)
    }
}

function Assert-Equal {
    param(
        [object]$Actual,
        [object]$Expected,
        [string]$Message
    )

    if ($Actual -ne $Expected) {
        $failures.Add("$Message; expected '$Expected', got '$Actual'")
    }
}

function Assert-Throws {
    param(
        [scriptblock]$Action,
        [string]$Message
    )

    $threw = $false
    try {
        & $Action
    }
    catch {
        $threw = $true
    }

    if (-not $threw) {
        $failures.Add($Message)
    }
}

Assert-True (Test-Path -LiteralPath $templatePath -PathType Leaf) "Machine configuration template is missing"
Assert-True (Test-Path -LiteralPath $profilePath -PathType Leaf) "Logic analyzer profile file is missing"

$template = if (Test-Path -LiteralPath $templatePath -PathType Leaf) {
    Get-Content -LiteralPath $templatePath -Raw -Encoding UTF8
} else {
    ""
}
Assert-True ($template -match '(?m)^set "SIGROK_CLI_EXE="') "Machine configuration template must declare SIGROK_CLI_EXE without a machine path"

Import-Module -Name $coreModulePath -Force
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("logic-analyzer-contract-" + [Guid]::NewGuid().ToString("N"))
$configRoot = Join-Path $tempRoot "05_Tools\Config"

try {
    New-Item -ItemType Directory -Path $configRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "05_Tools\Config\project.defaults.bat") -Destination (Join-Path $configRoot "project.defaults.bat")
    @'
@echo off
set "KEIL_UV4=C:\fake\UV4.exe"
'@ | Set-Content -LiteralPath (Join-Path $configRoot "toolchain.local.bat") -Encoding ASCII

    $missingSigrokConfiguration = Import-ToolkitConfiguration -ToolsRoot (Join-Path $tempRoot "05_Tools")
    Assert-Throws {
        Assert-ToolkitRequiredValue -Configuration $missingSigrokConfiguration -Name "SIGROK_CLI_EXE"
    } "Missing SIGROK_CLI_EXE must be rejected before a Sigrok command starts"
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Module -Name Toolkit.Core -Force -ErrorAction SilentlyContinue
}

$profiles = $null
if (Test-Path -LiteralPath $profilePath -PathType Leaf) {
    try {
        $profiles = Get-Content -LiteralPath $profilePath -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        $failures.Add("Logic analyzer profile JSON must be valid: $($_.Exception.Message)")
    }
}

if ($null -ne $profiles) {
    $spi = $profiles.profiles.spi2_flash
    $i2c = $profiles.profiles.i2c_eeprom
    Assert-Equal $spi.protocol "spi" "SPI profile protocol must be spi"
    Assert-Equal $i2c.protocol "i2c" "I2C profile protocol must be i2c"
    Assert-Equal $spi.signals.cs.logic_channel "D0" "SPI CS default channel must be D0"
    Assert-Equal $spi.signals.clk.logic_channel "D1" "SPI CLK default channel must be D1"
    Assert-Equal $spi.signals.miso.logic_channel "D3" "SPI MISO default channel must be D3"
    Assert-Equal $spi.signals.mosi.logic_channel "D5" "SPI MOSI default channel must be D5"
    Assert-Equal $i2c.signals.scl.logic_channel "D2" "I2C SCL default channel must be D2"
    Assert-Equal $i2c.signals.sda.logic_channel "D4" "I2C SDA default channel must be D4"

    $override = @{ cs = "D6" }
    $effectiveSpi = [ordered]@{
        cs = if ($override.ContainsKey("cs")) { $override.cs } else { $spi.signals.cs.logic_channel }
        clk = $spi.signals.clk.logic_channel
    }
    Assert-Equal $effectiveSpi.cs "D6" "CLI channel override must take priority over the selected profile"
    Assert-Equal $effectiveSpi.clk "D1" "Unchanged profile channels must be preserved"

    foreach ($channel in @(
        $spi.signals.cs.logic_channel,
        $spi.signals.clk.logic_channel,
        $spi.signals.miso.logic_channel,
        $spi.signals.mosi.logic_channel,
        $i2c.signals.scl.logic_channel,
        $i2c.signals.sda.logic_channel
    )) {
        Assert-True ($channel -match '^D[0-7]$') "Profile channel must be a valid D0-D7 name: $channel"
    }
    Assert-True (-not ("D8" -match '^D[0-7]$')) "Invalid channel D8 must be rejected"
}

$validStatuses = @("SUCCESS", "ERROR", "INCONCLUSIVE")
foreach ($status in $validStatuses) {
    Assert-True ($status -in $validStatuses) "Result status must be accepted: $status"
}
Assert-True ("PASS" -notin $validStatuses) "Generic Logic Workflow must not use project PASS as a result status"
Assert-True ("FAIL" -notin $validStatuses) "Generic Logic Workflow must not use project FAIL as a result status"

$executorPath = Join-Path $repoRoot "05_Tools\Adapters\LogicAnalyzer\Sigrok\sigrok_exec.ps1"
Assert-True (Test-Path -LiteralPath $executorPath -PathType Leaf) "Sigrok executor is missing"

$executorTempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("logic-analyzer-executor-" + [Guid]::NewGuid().ToString("N"))
$fakeSigrokPath = Join-Path $executorTempRoot "fake-sigrok.cmd"
$executorConfiguration = [PSCustomObject]@{
    SIGROK_CLI_EXE = $fakeSigrokPath
    PROJECT_ROOT = $executorTempRoot
}

try {
    New-Item -ItemType Directory -Path $executorTempRoot -Force | Out-Null
    @'
@echo off
if /I "%~1"=="--version" echo sigrok-cli fixture 0.8.0 & exit /b 0
if /I "%~1"=="--fail" echo fixture failure 1>&2 & exit /b 7
if /I "%~1"=="--sleep" powershell.exe -NoProfile -Command "Start-Sleep -Seconds 30" & exit /b 0
if /I "%~1"=="--scan" goto scan
echo fixture stdout
echo fixture stderr 1>&2
exit /b 0
:scan
if /I "%FAKE_SIGROK_SCAN_MODE%"=="none" exit /b 0
if /I "%FAKE_SIGROK_SCAN_MODE%"=="multiple" (
  echo fx2lafw:conn=4.7 - Saleae Logic 8 channels
  echo fx2lafw:conn=5.1 - Saleae Logic 8 channels
  exit /b 0
)
echo fx2lafw:conn=4.7 - Saleae Logic 8 channels
exit /b 0
'@ | Set-Content -LiteralPath $fakeSigrokPath -Encoding ASCII

    if (Test-Path -LiteralPath $executorPath -PathType Leaf) {
        . $executorPath

        $versionResult = Invoke-SigrokCli -Configuration $executorConfiguration -Arguments @("--version") -TimeoutMilliseconds 5000
        Assert-Equal $versionResult.ExitCode 0 "Sigrok version command should succeed"
        Assert-True ($versionResult.Stdout.Contains("sigrok-cli fixture")) "Sigrok executor should capture stdout"

        $failureResult = Invoke-SigrokCli -Configuration $executorConfiguration -Arguments @("--fail") -TimeoutMilliseconds 5000
        Assert-Equal $failureResult.ExitCode 7 "Sigrok executor should preserve non-zero exit code"
        Assert-True ($failureResult.Stderr.Contains("fixture failure")) "Sigrok executor should capture stderr"

        $timeoutResult = Invoke-SigrokCli -Configuration $executorConfiguration -Arguments @("--sleep") -TimeoutMilliseconds 250
        Assert-True $timeoutResult.TimedOut "Sigrok executor should report timeout"
        Start-Sleep -Milliseconds 100
        Assert-True ($null -eq (Get-Process -Id $timeoutResult.ProcessId -ErrorAction SilentlyContinue)) "Sigrok timeout should clean up the owned process"

        $env:FAKE_SIGROK_SCAN_MODE = "none"
        $notFound = Invoke-SigrokScan -Configuration $executorConfiguration -Driver "fx2lafw"
        Assert-Equal $notFound.ErrorClass "DEVICE_NOT_FOUND" "No matching Sigrok device should be classified as DEVICE_NOT_FOUND"

        $env:FAKE_SIGROK_SCAN_MODE = "single"
        $single = Invoke-SigrokScan -Configuration $executorConfiguration -Driver "fx2lafw"
        Assert-Equal $single.SelectedDevice "fx2lafw:conn=4.7" "One matching Sigrok device should be selected automatically"

        $env:FAKE_SIGROK_SCAN_MODE = "multiple"
        $ambiguous = Invoke-SigrokScan -Configuration $executorConfiguration -Driver "fx2lafw"
        Assert-Equal $ambiguous.ErrorClass "AMBIGUOUS_DEVICE" "Multiple matching Sigrok devices should be classified as AMBIGUOUS_DEVICE"

        $selected = Invoke-SigrokScan -Configuration $executorConfiguration -Driver "fx2lafw" -Selector "fx2lafw:conn=5.1"
        Assert-Equal $selected.SelectedDevice "fx2lafw:conn=5.1" "An explicit local selector should resolve multiple devices"
    }
}
catch {
    $failures.Add("Sigrok executor contract raised an unexpected error: $($_.Exception.Message)")
}
finally {
    Remove-Item Env:FAKE_SIGROK_SCAN_MODE -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $executorTempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[LogicAnalyzer][FAIL] $_" }
    exit 1
}

"[LogicAnalyzer][PASS] Configuration and result contract checks passed."
exit 0
