$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$modulePath = Join-Path $repoRoot "05_Tools\Core\Toolkit.Core.psm1"
$failures = [System.Collections.Generic.List[string]]::new()

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

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $failures.Add($Message)
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

if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
    throw "Toolkit Core module is missing: $modulePath"
}

Import-Module -Name $modulePath -Force

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("toolkit-core-test-" + [Guid]::NewGuid().ToString("N"))
$fixtureRoot = Join-Path $tempRoot "05_Tools"
$configRoot = Join-Path $fixtureRoot "Config"
$projectFile = Join-Path $tempRoot "firmware\fixture.uvprojx"

try {
    New-Item -ItemType Directory -Path $configRoot -Force | Out-Null
    New-Item -ItemType Directory -Path (Split-Path -Parent $projectFile) -Force | Out-Null
    New-Item -ItemType File -Path $projectFile -Force | Out-Null

    @'
@echo off
set "PROJECT_KEIL_PROJECT_FILE=firmware\fixture.uvprojx"
set "PROJECT_KEIL_TARGET=FixtureTarget"
set "PROJECT_OUTPUT_DIR=out"
set "PROJECT_APP_AXF=out\fixture.axf"
set "PROJECT_APP_HEX=out\fixture.hex"
set "PROJECT_APP_BIN=out\fixture.bin"
set "PROJECT_LOG_DIR=logs"
set "JLINK_DEVICE=FixtureDevice"
set "JLINK_IF=SWD"
set "JLINK_SPEED=1000"
'@ | Set-Content -LiteralPath (Join-Path $configRoot "project.defaults.bat") -Encoding ASCII

    @'
@echo off
set "JLINK_SPEED=4000"
set "GDB_PORT=2444"
set "SERIAL_PORT=COM7"
'@ | Set-Content -LiteralPath (Join-Path $configRoot "project.local.bat") -Encoding ASCII

    @'
@echo off
set "KEIL_UV4=C:\fake\UV4.exe"
set "JLINK_EXE=C:\fake\JLink.exe"
set "JLINK_GDB_SERVER=C:\fake\JLinkGDBServerCL.exe"
set "JLINK_RTT_LOGGER=C:\fake\JLinkRTTLogger.exe"
set "ARM_GDB=C:\fake\arm-none-eabi-gdb.exe"
set "PYTHON_EXE="
set "TERA_TERM_EXE=C:\fake\ttpmacro.exe"
'@ | Set-Content -LiteralPath (Join-Path $configRoot "toolchain.local.bat") -Encoding ASCII

    $config = Import-ToolkitConfiguration -ToolsRoot $fixtureRoot
    Assert-Equal $config.PROJECT_KEIL_TARGET "FixtureTarget" "Project default target should be loaded"
    Assert-Equal $config.JLINK_SPEED "4000" "Project local override should win"
    Assert-Equal $config.GDB_PORT "2444" "Project local GDB port should be loaded"
    Assert-Equal $config.SERIAL_PORT "COM7" "Project local serial port should be loaded"
    Assert-Equal $config.KEIL_UV4 "C:\fake\UV4.exe" "Machine toolchain path should be loaded"
    Assert-Equal $config.PROJECT_ROOT $tempRoot "Project root should be derived from ToolsRoot"

    $resolvedProject = Resolve-ToolkitProjectPath -ProjectRoot $config.PROJECT_ROOT -RelativePath $config.PROJECT_KEIL_PROJECT_FILE
    Assert-Equal $resolvedProject $projectFile "Relative project path should resolve from project root"

    $logPath = Join-Path $tempRoot "generated-logs"
    $createdLogPath = New-ToolkitLogDirectory -Path $logPath
    Assert-True (Test-Path -LiteralPath $logPath -PathType Container) "Log directory should be created"
    Assert-Equal $createdLogPath $logPath "Log directory helper should return the created path"

    $missingMachineRoot = Join-Path $tempRoot "missing-machine\05_Tools"
    New-Item -ItemType Directory -Path (Join-Path $missingMachineRoot "Config") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $configRoot "project.defaults.bat") -Destination (Join-Path $missingMachineRoot "Config\project.defaults.bat")
    Assert-Throws { Import-ToolkitConfiguration -ToolsRoot $missingMachineRoot } "Missing machine config should fail before external tools start"

    $missingValueRoot = Join-Path $tempRoot "missing-value\05_Tools"
    $missingValueConfigRoot = Join-Path $missingValueRoot "Config"
    New-Item -ItemType Directory -Path $missingValueConfigRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $configRoot "project.defaults.bat") -Destination (Join-Path $missingValueConfigRoot "project.defaults.bat")
    Copy-Item -LiteralPath (Join-Path $configRoot "project.local.bat") -Destination (Join-Path $missingValueConfigRoot "project.local.bat")
    @'
@echo off
set "KEIL_UV4="
'@ | Set-Content -LiteralPath (Join-Path $missingValueConfigRoot "toolchain.local.bat") -Encoding ASCII
    $missingValueConfig = Import-ToolkitConfiguration -ToolsRoot $missingValueRoot
    Assert-Throws { Assert-ToolkitRequiredValue -Configuration $missingValueConfig -Name "KEIL_UV4" } "Missing required configuration value should fail"
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Module -Name Toolkit.Core -Force -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[TEST][FAIL] $_" }
    exit 1
}

"[TEST][PASS] Toolkit Core configuration/path/logging contract checks passed."
exit 0
