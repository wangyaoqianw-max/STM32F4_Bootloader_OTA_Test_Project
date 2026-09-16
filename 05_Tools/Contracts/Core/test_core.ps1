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

    $sharedLockPath = Get-ToolkitJLinkLockPath -Configuration $config
    Assert-Equal $sharedLockPath (Join-Path $tempRoot "logs\toolkit_jlink.lock") "J-Link ownership should use the configured project log directory"

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

    $successResult = Invoke-ToolkitProcess -FilePath $env:ComSpec -Arguments @("/d", "/c", "echo TOOLKIT_STDOUT & echo TOOLKIT_STDERR 1>&2") -TimeoutMilliseconds 5000
    Assert-Equal $successResult.ExitCode 0 "Process result should preserve successful exit code"
    Assert-True ($successResult.Stdout.Contains("TOOLKIT_STDOUT")) "Process result should capture stdout"
    Assert-True ($successResult.Stderr.Contains("TOOLKIT_STDERR")) "Process result should capture stderr"
    Assert-True (-not $successResult.TimedOut) "Successful process should not be marked timed out"
    Assert-True ($successResult.ProcessId -gt 0) "Process result should include a process id"

    $failureResult = Invoke-ToolkitProcess -FilePath $env:ComSpec -Arguments @("/d", "/c", "exit 7") -TimeoutMilliseconds 5000
    Assert-Equal $failureResult.ExitCode 7 "Process result should preserve non-zero exit code"

    $timeoutResult = Invoke-ToolkitProcess -FilePath (Get-Command powershell.exe).Source -Arguments @("-NoProfile", "-Command", "Start-Sleep -Seconds 30") -TimeoutMilliseconds 250
    Assert-True $timeoutResult.TimedOut "Long-running process should be marked timed out"

    $listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Loopback, 0)
    $listener.Start()
    try {
        $listenerPort = $listener.LocalEndpoint.Port
        Assert-True (Test-ToolkitTcpPort -HostName "127.0.0.1" -PortNumber $listenerPort) "TCP probe should detect a listening port"
    }
    finally {
        $listener.Stop()
    }
    Assert-True (-not (Test-ToolkitTcpPort -HostName "127.0.0.1" -PortNumber $listenerPort)) "TCP probe should reject a closed port"

    $ownedProcess = Start-Process -FilePath (Get-Command powershell.exe).Source -ArgumentList "-NoProfile", "-Command", "Start-Sleep -Seconds 30" -PassThru
    try {
        Start-Sleep -Milliseconds 100
        Stop-ToolkitOwnedProcess -Process $ownedProcess
        $ownedProcess.Refresh()
        Assert-True $ownedProcess.HasExited "Owned cleanup should stop only the supplied process"
    }
    finally {
        Stop-ToolkitOwnedProcess -Process $ownedProcess
    }

    $lockPath = Join-Path $tempRoot "toolkit.lock"
    $lock = Enter-ToolkitLock -Path $lockPath
    try {
        Assert-True (Test-Path -LiteralPath $lockPath -PathType Leaf) "Lock acquisition should create the lock file"
        Assert-Throws { Enter-ToolkitLock -Path $lockPath } "A second owner should not acquire the same lock"
    }
    finally {
        Exit-ToolkitLock -Lock $lock
    }
    Assert-True (-not (Test-Path -LiteralPath $lockPath)) "Lock release should remove the lock file"
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Module -Name Toolkit.Core -Force -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[TEST][FAIL] $_" }
    exit 1
}

"[TEST][PASS] Toolkit Core configuration/path/process/lock/logging contract checks passed."
exit 0
