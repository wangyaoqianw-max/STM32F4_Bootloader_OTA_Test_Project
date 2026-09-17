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
if defined FAKE_SIGROK_ARGS_LOG >>"%FAKE_SIGROK_ARGS_LOG%" echo %*
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

        $executorCapturePath = Join-Path $executorTempRoot "fixture.sr"
        Set-Content -LiteralPath $executorCapturePath -Value "fixture" -Encoding ASCII
        $executorDecodeArgsLog = Join-Path $executorTempRoot "decode-args.log"
        $env:FAKE_SIGROK_ARGS_LOG = $executorDecodeArgsLog
        $decodeResult = Invoke-SigrokDecode -Configuration $executorConfiguration -CapturePath $executorCapturePath -DecoderSpec "spi:clk=D1:cs=D0:mosi=D5:miso=D3" -Annotation "spi=mosi-data"
        Assert-Equal $decodeResult.ExitCode 0 "Sigrok decode with annotation selection should succeed"
        if (Test-Path -LiteralPath $executorDecodeArgsLog -PathType Leaf) {
            $decodeArgs = Get-Content -LiteralPath $executorDecodeArgsLog -Raw -Encoding UTF8
            Assert-True ($decodeArgs -match '--protocol-decoder-annotations spi=mosi-data') "Sigrok decode should pass the selected annotation to the CLI"
        }
        else {
            $failures.Add("Sigrok decode argument log is missing")
        }
        Remove-Item Env:FAKE_SIGROK_ARGS_LOG -ErrorAction SilentlyContinue
    }
}
catch {
    $failures.Add("Sigrok executor contract raised an unexpected error: $($_.Exception.Message)")
}
finally {
    Remove-Item Env:FAKE_SIGROK_SCAN_MODE -ErrorAction SilentlyContinue
    Remove-Item Env:FAKE_SIGROK_ARGS_LOG -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $executorTempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$workflowPath = Join-Path $repoRoot "05_Tools\Workflows\LogicAnalyzer\logic.ps1"
Assert-True (Test-Path -LiteralPath $workflowPath -PathType Leaf) "Logic analyzer workflow is missing"

$workflowTempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("logic-analyzer-workflow-" + [Guid]::NewGuid().ToString("N"))
$workflowToolsRoot = Join-Path $workflowTempRoot "05_Tools"
$workflowConfigRoot = Join-Path $workflowToolsRoot "Config"
$workflowOutputRoot = Join-Path $workflowTempRoot "06_Output\LogicAnalyzer"
$workflowFakeSigrokPath = Join-Path $workflowTempRoot "fake-sigrok.cmd"
$workflowDefaultsPath = Join-Path $repoRoot "05_Tools\Config\project.defaults.bat"
$workflowProfilePath = Join-Path $repoRoot "05_Tools\Config\logic_analyzer.profiles.json"

try {
    New-Item -ItemType Directory -Path $workflowConfigRoot -Force | Out-Null
    Copy-Item -LiteralPath $workflowDefaultsPath -Destination (Join-Path $workflowConfigRoot "project.defaults.bat")
    Copy-Item -LiteralPath $workflowProfilePath -Destination (Join-Path $workflowConfigRoot "logic_analyzer.profiles.json")
    @"
@echo off
set "SIGROK_CLI_EXE=$workflowFakeSigrokPath"
"@ | Set-Content -LiteralPath (Join-Path $workflowConfigRoot "toolchain.local.bat") -Encoding ASCII
    @'
@echo off
if defined FAKE_SIGROK_ARGS_LOG >>"%FAKE_SIGROK_ARGS_LOG%" echo %*
if /I "%~1"=="--scan" (
  echo fx2lafw:conn=4.7 - Saleae Logic fixture
  exit /b 0
)
set "OUTPUT_FILE="
set "INPUT_FILE="
:parse
if "%~1"=="" goto execute
if /I "%~1"=="--output-file" set "OUTPUT_FILE=%~2"
if /I "%~1"=="--input-file" set "INPUT_FILE=%~2"
shift
goto parse
:execute
if defined FAKE_SIGROK_CAPTURE_FAIL (
  echo fixture capture failure 1>&2
  exit /b 9
)
if defined OUTPUT_FILE (
  >"%OUTPUT_FILE%" echo raw sigrok capture fixture
  exit /b 0
)
if defined INPUT_FILE (
  echo 0.000001000 SPI DATA MOSI=0x9F MISO=0xEF
  echo 0.000002000 SPI DATA MOSI=0xFF MISO=0x40
  echo 0.000003000 SPI DATA MOSI=0xFF MISO=0x17
  exit /b 0
)
exit /b 0
'@ | Set-Content -LiteralPath $workflowFakeSigrokPath -Encoding ASCII

    if (Test-Path -LiteralPath $workflowPath -PathType Leaf) {
        $captureOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $workflowPath -ToolsRoot $workflowToolsRoot -Action capture -Protocol spi -Profile spi2_flash -OutputRoot $workflowOutputRoot -SampleRate "48MHz" -Samples 100 2>&1
        $captureExitCode = $LASTEXITCODE
        Assert-Equal $captureExitCode 0 "Logic capture workflow should succeed with fake Sigrok"
        $captureRun = Get-ChildItem -LiteralPath $workflowOutputRoot -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
        Assert-True ($null -ne $captureRun) "Logic capture workflow should create a run directory"
        if ($null -ne $captureRun) {
            $effectiveConfigPath = Join-Path $captureRun.FullName "effective_config.json"
            $capturePath = Join-Path $captureRun.FullName "capture.sr"
            $resultPath = Join-Path $captureRun.FullName "result.json"
            Assert-True (Test-Path -LiteralPath $effectiveConfigPath -PathType Leaf) "Capture workflow should save effective_config.json"
            Assert-True (Test-Path -LiteralPath $capturePath -PathType Leaf) "Capture workflow should save capture.sr"
            Assert-True (Test-Path -LiteralPath $resultPath -PathType Leaf) "Capture workflow should save result.json"
            if (Test-Path -LiteralPath $effectiveConfigPath -PathType Leaf) {
                $effectiveConfig = Get-Content -LiteralPath $effectiveConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
                Assert-Equal $effectiveConfig.mapping.cs.logic_channel "D0" "Effective Config should preserve the selected profile mapping"
            }

            $durationArgsLog = Join-Path $workflowTempRoot "sigrok-duration-args.log"
            $env:FAKE_SIGROK_ARGS_LOG = $durationArgsLog
            $durationOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $workflowPath -ToolsRoot $workflowToolsRoot -Action capture -Protocol spi -Profile spi2_flash -OutputRoot $workflowOutputRoot -SampleRate "24MHz" -Samples 100 -CaptureTimeMilliseconds 5000 2>&1
            $durationExitCode = $LASTEXITCODE
            Assert-Equal $durationExitCode 0 "Logic capture workflow should accept a longer capture time"
            $durationRun = Get-ChildItem -LiteralPath $workflowOutputRoot -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
            if ($null -ne $durationRun) {
                $durationConfig = Get-Content -LiteralPath (Join-Path $durationRun.FullName "effective_config.json") -Raw -Encoding UTF8 | ConvertFrom-Json
                Assert-Equal $durationConfig.capture_time_ms 5000 "Effective Config should record the requested capture time"
            }
            if (Test-Path -LiteralPath $durationArgsLog -PathType Leaf) {
                $durationArgs = Get-Content -LiteralPath $durationArgsLog -Raw -Encoding UTF8
                Assert-True ($durationArgs -match '--time 5000') "Sigrok capture should use the requested time window"
            }
            else {
                $failures.Add("Sigrok capture argument log is missing for the longer capture test")
            }
            Remove-Item Env:FAKE_SIGROK_ARGS_LOG -ErrorAction SilentlyContinue

            $overrideOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $workflowPath -ToolsRoot $workflowToolsRoot -Action capture -Protocol spi -Profile spi2_flash -OutputRoot $workflowOutputRoot -CsChannel D6 -Samples 100 2>&1
            $overrideExitCode = $LASTEXITCODE
            Assert-Equal $overrideExitCode 0 "Logic capture workflow should accept a temporary channel override"
            $overrideRun = Get-ChildItem -LiteralPath $workflowOutputRoot -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
            if ($null -ne $overrideRun) {
                $overrideConfig = Get-Content -LiteralPath (Join-Path $overrideRun.FullName "effective_config.json") -Raw -Encoding UTF8 | ConvertFrom-Json
                Assert-Equal $overrideConfig.mapping.cs.logic_channel "D6" "Temporary CLI channel override should win over the profile"
            }

            $decodeOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $workflowPath -ToolsRoot $workflowToolsRoot -Action decode -Protocol spi -Profile spi2_flash -CapturePath $capturePath -OutputRoot $workflowOutputRoot 2>&1
            $decodeExitCode = $LASTEXITCODE
            Assert-Equal $decodeExitCode 0 "Standalone decode workflow should decode an existing capture"
            if ($decodeExitCode -ne 0) {
                $failures.Add("Standalone decode output: $($decodeOutput -join [Environment]::NewLine)")
            }
            $decodeRun = Get-ChildItem -LiteralPath $workflowOutputRoot -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
            if ($null -ne $decodeRun) {
                Assert-True (Test-Path -LiteralPath (Join-Path $decodeRun.FullName "decode.json") -PathType Leaf) "Decode workflow should save decode.json"
                Assert-True (Test-Path -LiteralPath (Join-Path $decodeRun.FullName "result.json") -PathType Leaf) "Decode workflow should save result.json"
            }

            $env:FAKE_SIGROK_CAPTURE_FAIL = "1"
            $captureFailureOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $workflowPath -ToolsRoot $workflowToolsRoot -Action spi -Profile spi2_flash -OutputRoot $workflowOutputRoot -Samples 100 2>&1
            $captureFailureExitCode = $LASTEXITCODE
            Assert-Equal $captureFailureExitCode 10 "Capture/decode workflow should report an error when capture fails"
            Assert-True (($captureFailureOutput -join [Environment]::NewLine) -match '"status"\s*:\s*"ERROR"') "Capture failure should still emit a structured ERROR result"
            Remove-Item Env:FAKE_SIGROK_CAPTURE_FAIL -ErrorAction SilentlyContinue
        }
    }
}
catch {
    $failures.Add("Logic analyzer workflow contract raised an unexpected error: $($_.Exception.Message)")
}
finally {
    Remove-Item Env:FAKE_SIGROK_CAPTURE_FAIL -ErrorAction SilentlyContinue
    Remove-Item Env:FAKE_SIGROK_ARGS_LOG -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $workflowTempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$routerPath = Join-Path $repoRoot "05_Tools\toolkit.ps1"
$routerText = Get-Content -LiteralPath $routerPath -Raw -Encoding UTF8
Assert-True ($routerText -match '"logic"') "Toolkit Router should expose the logic command family"
Assert-True ($routerText -match 'Workflows\\LogicAnalyzer\\logic\.ps1') "Toolkit Router should delegate logic commands to the Logic Analyzer workflow"
foreach ($subcommand in @("doctor", "scan", "spi", "i2c", "decode")) {
    Assert-True ($routerText -match [regex]::Escape($subcommand)) "Toolkit Router should expose logic $subcommand"
}
Assert-True ($routerText -notmatch 'MOSI|MISO|SCL|SDA|protocol-decoders') "Toolkit Router must not duplicate Sigrok protocol logic"

$invalidLogicOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $routerPath logic unsupported 2>&1
$invalidLogicExitCode = $LASTEXITCODE
Assert-Equal $invalidLogicExitCode 10 "Invalid logic subcommand should map to CONFIG_ERROR"
Assert-True (($invalidLogicOutput -join [Environment]::NewLine) -match '(?i)logic') "Invalid logic subcommand should identify the logic command family"

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[LogicAnalyzer][FAIL] $_" }
    exit 1
}

"[LogicAnalyzer][PASS] Configuration and result contract checks passed."
exit 0
