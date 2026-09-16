$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$failures = [System.Collections.Generic.List[string]]::new()
$coreModule = Join-Path $repoRoot "05_Tools\Core\Toolkit.Core.psm1"
Import-Module -Name $coreModule -Force
. (Join-Path $repoRoot "05_Tools\Adapters\Probe\JLink\jlink_rtt.ps1")

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

function Invoke-Workflow {
    param(
        [string]$Script,
        [string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Script @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = ($output -join [Environment]::NewLine)
    }
}

function Assert-LogContains {
    param(
        [string]$Path,
        [string]$Text,
        [string]$Message
    )

    Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) "$Message; log is missing: $Path"
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $content = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
        Assert-True (($null -ne $content) -and $content.Contains($Text)) "$Message; missing '$Text'"
    }
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("toolkit-application-test-" + [Guid]::NewGuid().ToString("N"))
$toolsRoot = Join-Path $tempRoot "05_Tools"
$configRoot = Join-Path $toolsRoot "Config"
$fakeRoot = Join-Path $tempRoot "fake tools"
$projectRoot = $tempRoot
$fakeLog = Join-Path $tempRoot "fake tool invocations.log"
$buildLog = Join-Path $tempRoot "logs\FixtureTarget_build.log"
$flashLog = Join-Path $tempRoot "logs\FixtureTarget_flash.log"
$rttLog = Join-Path $tempRoot "logs\FixtureTarget_rtt.log"
$rttDiagnostic = Join-Path $tempRoot "logs\FixtureTarget_rtt_diagnostic.log"
$projectFile = Join-Path $projectRoot "firmware\Fixture Project.uvprojx"
$hexFile = Join-Path $projectRoot "out\Fixture Project.hex"
$axfFile = Join-Path $projectRoot "out\Fixture Project.axf"
$binFile = Join-Path $projectRoot "out\Fixture Project.bin"

$buildWorkflow = Join-Path $repoRoot "05_Tools\Workflows\Application\build.ps1"
$flashWorkflow = Join-Path $repoRoot "05_Tools\Workflows\Application\flash.ps1"
$rttWorkflow = Join-Path $repoRoot "05_Tools\Workflows\Application\rtt.ps1"
$runWorkflow = Join-Path $repoRoot "05_Tools\Workflows\Application\run.ps1"

try {
    New-Item -ItemType Directory -Path $configRoot,$fakeRoot,(Split-Path -Parent $projectFile),(Split-Path -Parent $hexFile) -Force | Out-Null
    New-Item -ItemType File -Path $projectFile,$hexFile,$axfFile,$binFile -Force | Out-Null
    New-Item -ItemType File -Path $fakeLog -Force | Out-Null

    @'
@echo off
>>"%FAKE_TOOL_LOG%" echo KEIL %*
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $fakeRoot "fake keil.cmd") -Encoding ASCII

    @'
@echo off
set "COMMAND_FILE="
:parse
if "%~1"=="" goto write_log
if /I "%~1"=="-CommandFile" set "COMMAND_FILE=%~2"
shift
goto parse
:write_log
>>"%FAKE_TOOL_LOG%" echo JLINK %*
if defined COMMAND_FILE type "%COMMAND_FILE%" >> "%FAKE_TOOL_LOG%"
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $fakeRoot "fake jlink.cmd") -Encoding ASCII

    @'
@echo off
set "OUTPUT_FILE="
:parse
if "%~1"=="" goto write_output
set "OUTPUT_FILE=%~1"
shift
goto parse
:write_output
>>"%FAKE_TOOL_LOG%" echo RTT %*
if not defined FAKE_RTT_EMPTY if defined OUTPUT_FILE >"%OUTPUT_FILE%" echo RTT_PAYLOAD
if defined FAKE_RTT_EMPTY if defined OUTPUT_FILE type nul >"%OUTPUT_FILE%"
:wait_for_timeout
goto wait_for_timeout
'@ | Set-Content -LiteralPath (Join-Path $fakeRoot "fake rtt.cmd") -Encoding ASCII

    @'
@echo off
set "PROJECT_KEIL_PROJECT_FILE=firmware\Fixture Project.uvprojx"
set "PROJECT_KEIL_TARGET=FixtureTarget"
set "PROJECT_OUTPUT_DIR=out"
set "PROJECT_APP_AXF=out\Fixture Project.axf"
set "PROJECT_APP_HEX=out\Fixture Project.hex"
set "PROJECT_APP_BIN=out\Fixture Project.bin"
set "PROJECT_LOG_DIR=logs"
set "JLINK_DEVICE=FixtureDevice"
set "JLINK_IF=SWD"
'@ | Set-Content -LiteralPath (Join-Path $configRoot "project.defaults.bat") -Encoding ASCII

    @'
@echo off
set "JLINK_SPEED=1234"
set "JLINK_RTT_CHANNEL=2"
set "GDB_PORT=2444"
set "SERIAL_PORT=COM7"
'@ | Set-Content -LiteralPath (Join-Path $configRoot "project.local.bat") -Encoding ASCII

    $keilPath = Join-Path $fakeRoot "fake keil.cmd"
    $jlinkPath = Join-Path $fakeRoot "fake jlink.cmd"
    $rttPath = Join-Path $fakeRoot "fake rtt.cmd"
    @"
@echo off
set "KEIL_UV4=$keilPath"
set "JLINK_EXE=$jlinkPath"
set "JLINK_GDB_SERVER=$jlinkPath"
set "JLINK_RTT_LOGGER=$rttPath"
set "ARM_GDB=$keilPath"
set "PYTHON_EXE="
set "TERA_TERM_EXE=$keilPath"
"@ | Set-Content -LiteralPath (Join-Path $configRoot "toolchain.local.bat") -Encoding ASCII

    $env:FAKE_TOOL_LOG = $fakeLog
    $buildResult = Invoke-Workflow -Script $buildWorkflow -Arguments @("-ToolsRoot", $toolsRoot)
    Assert-Equal $buildResult.ExitCode 0 "Build workflow should succeed with the fake Keil tool"
    Assert-LogContains -Path $fakeLog -Text "KEIL" -Message "Build workflow should invoke Keil"
    Assert-LogContains -Path $fakeLog -Text "-b" -Message "Keil invocation should include project build switch"
    Assert-LogContains -Path $fakeLog -Text "FixtureTarget" -Message "Keil invocation should use configured target"
    Assert-LogContains -Path $fakeLog -Text "Fixture Project.uvprojx" -Message "Keil invocation should use configured project"

    Clear-Content -LiteralPath $fakeLog
    $flashResult = Invoke-Workflow -Script $flashWorkflow -Arguments @("-ToolsRoot", $toolsRoot, "-Mode", "run")
    Assert-Equal $flashResult.ExitCode 0 "Flash workflow should succeed with the fake J-Link tool"
    Assert-LogContains -Path $fakeLog -Text "JLINK" -Message "Flash workflow should invoke J-Link"
    Assert-LogContains -Path $fakeLog -Text "FixtureDevice" -Message "J-Link invocation should use configured device"
    Assert-LogContains -Path $fakeLog -Text "-if" -Message "J-Link invocation should use configured interface switch"
    Assert-LogContains -Path $fakeLog -Text "-speed" -Message "J-Link invocation should use configured speed switch"
    Assert-LogContains -Path $fakeLog -Text "SWD" -Message "J-Link invocation should use configured interface"
    Assert-LogContains -Path $fakeLog -Text "1234" -Message "J-Link invocation should use configured speed"
    Assert-LogContains -Path $fakeLog -Text "loadfile" -Message "Flash command file should load the configured HEX"
    Assert-LogContains -Path $fakeLog -Text "r" -Message "Run mode should reset the target"
    Assert-LogContains -Path $fakeLog -Text "g" -Message "Run mode should start the target"
    Assert-True (-not (Get-ChildItem -LiteralPath ([System.IO.Path]::GetTempPath()) -Filter "toolkit_flash_*.jlink" -File -ErrorAction SilentlyContinue)) "Flash command file should be cleaned up"

    Clear-Content -LiteralPath $fakeLog
    $prepareResult = Invoke-Workflow -Script $flashWorkflow -Arguments @("-ToolsRoot", $toolsRoot, "-Mode", "prepare")
    Assert-Equal $prepareResult.ExitCode 0 "Flash prepare workflow should succeed with the fake J-Link tool"
    $prepareLog = Get-Content -LiteralPath $fakeLog -Raw -Encoding UTF8
    if ($null -eq $prepareLog) { $prepareLog = "" }
    Assert-True (-not $prepareLog.Contains("`nr`n")) "Prepare mode should not reset the target"
    Assert-True (-not $prepareLog.Contains("`ng`n")) "Prepare mode should not start the target"

    Clear-Content -LiteralPath $fakeLog
    $rttResult = Invoke-Workflow -Script $rttWorkflow -Arguments @("-ToolsRoot", $toolsRoot, "-Seconds", "2")
    Assert-Equal $rttResult.ExitCode 0 "RTT workflow should succeed with the fake RTT logger"
    Assert-LogContains -Path $fakeLog -Text "RTT" -Message "RTT workflow should invoke the RTT adapter"
    Assert-LogContains -Path $fakeLog -Text "-RTTChannel" -Message "RTT invocation should use configured channel switch"
    Assert-LogContains -Path $fakeLog -Text "2" -Message "RTT invocation should use configured channel"
    Assert-True (Test-Path -LiteralPath $rttLog -PathType Leaf) "RTT workflow should write the configured output path"

    $env:FAKE_RTT_EMPTY = "1"
    $configuration = Import-ToolkitConfiguration -ToolsRoot $toolsRoot
    $emptyAdapterResult = Invoke-JLinkRtt -Configuration $configuration -Seconds 1 -OutputPath (Join-Path $tempRoot "logs\empty-rtt.log") -DiagnosticPath (Join-Path $tempRoot "logs\empty-rtt-diagnostic.log")
    Assert-Equal $emptyAdapterResult.ExitCode 32 "RTT adapter should preserve the no-payload result for a timed-out empty capture"
    $emptyRttResult = Invoke-Workflow -Script $rttWorkflow -Arguments @("-ToolsRoot", $toolsRoot, "-Seconds", "1")
    Assert-Equal $emptyRttResult.ExitCode 30 "RTT workflow should classify a timed-out empty capture as a probe failure"
    Remove-Item Env:FAKE_RTT_EMPTY -ErrorAction SilentlyContinue

    $ownershipLockPath = Join-Path (Split-Path -Parent $flashLog) "toolkit_jlink.lock"
    $ownershipLock = Enter-ToolkitLock -Path $ownershipLockPath
    try {
        Clear-Content -LiteralPath $fakeLog
        $flashConflictResult = Invoke-Workflow -Script $flashWorkflow -Arguments @("-ToolsRoot", $toolsRoot, "-Mode", "run")
        Assert-Equal $flashConflictResult.ExitCode 30 "Flash workflow should report a Probe error when the shared J-Link lock is held"
        $conflictLog = Get-Content -LiteralPath $fakeLog -Raw -Encoding UTF8
        if ($null -eq $conflictLog) { $conflictLog = "" }
        Assert-True (-not $conflictLog.Contains("JLINK")) "Flash workflow must not start a second J-Link owner after lock conflict"
    }
    finally {
        Exit-ToolkitLock -Lock $ownershipLock
    }
    Assert-True (-not (Test-Path -LiteralPath $ownershipLockPath)) "Workflow lock test should release its held lock"

    Clear-Content -LiteralPath $fakeLog
    $runResult = Invoke-Workflow -Script $runWorkflow -Arguments @("-ToolsRoot", $toolsRoot, "-Seconds", "2")
    Assert-Equal $runResult.ExitCode 0 "Application run workflow should succeed"
    $runLog = Get-Content -LiteralPath $fakeLog -Raw -Encoding UTF8
    if ($null -eq $runLog) { $runLog = "" }
    $buildIndex = $runLog.IndexOf("KEIL")
    $flashIndex = $runLog.IndexOf("JLINK")
    $rttIndex = $runLog.IndexOf("RTT")
    Assert-True (($buildIndex -ge 0) -and ($buildIndex -lt $flashIndex) -and ($flashIndex -lt $rttIndex)) "Run workflow should execute Build, Flash, RTT in order"
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item Env:FAKE_TOOL_LOG -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[TEST][FAIL] $_" }
    exit 1
}

"[TEST][PASS] Application adapter/workflow configuration contract checks passed."
exit 0
