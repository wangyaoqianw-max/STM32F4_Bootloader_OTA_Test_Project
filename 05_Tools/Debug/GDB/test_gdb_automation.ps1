$ErrorActionPreference = "Stop"

$gdbRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $gdbRoot "..\..\..")).Path
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

function Assert-FileContains {
    param(
        [string]$Path,
        [string]$Text,
        [string]$Message
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        $failures.Add("$Message; file missing: $Path")
        return
    }

    $content = Get-Content -Raw -LiteralPath $Path
    Assert-True ($content.Contains($Text)) $Message
}

function Assert-FileNotContains {
    param(
        [string]$Path,
        [string]$Text,
        [string]$Message
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        $failures.Add("$Message; file missing: $Path")
        return
    }

    $content = Get-Content -Raw -LiteralPath $Path
    Assert-True (-not $content.Contains($Text)) $Message
}

function Assert-SnapshotFailure {
    param(
        [string[]]$Arguments,
        [string]$ExpectedText,
        [string]$Message
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $snapshotPs1 @Arguments 2>&1
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $exitCode = $LASTEXITCODE
    $outputText = $output -join [Environment]::NewLine
    Assert-True ($exitCode -ne 0) "$Message; command unexpectedly passed"
    Assert-True ($outputText.Contains($ExpectedText)) "$Message; expected '$ExpectedText', output was: $outputText"
}

$toolchainExample = Join-Path $repoRoot "05_Tools\Config\toolchain.local.example.bat"
$projectExample = Join-Path $repoRoot "05_Tools\Config\project.local.example.bat"
$resumeScript = Join-Path $gdbRoot "runtime_snapshot_resume.gdb"
$haltScript = Join-Path $gdbRoot "runtime_snapshot_halt.gdb"
$snapshotBat = Join-Path $repoRoot "05_Tools\Scripts\gdb_runtime_snapshot.bat"
$snapshotPs1 = Join-Path $repoRoot "05_Tools\Scripts\gdb_runtime_snapshot.ps1"
$serverBat = Join-Path $repoRoot "05_Tools\Scripts\start_gdb_server.bat"
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("gdb-automation-test-" + [Guid]::NewGuid().ToString("N"))
$testServerLog = Join-Path $tempRoot "server.log"
$testSnapshotLog = Join-Path $tempRoot "snapshot.log"

Assert-FileContains $toolchainExample 'set "ARM_GDB=' 'GDB executable configuration is missing'
Assert-FileContains $toolchainExample 'set "JLINK_GDB_SERVER=' 'J-Link GDB Server configuration is missing'
Assert-FileContains $projectExample 'set "GDB_PORT=' 'GDB port override configuration is missing'
Assert-FileContains $projectExample 'set "JLINK_SPEED=' 'J-Link speed override configuration is missing'

Assert-FileContains $resumeScript 'target remote localhost:2331' 'Resume script must attach to the configured GDB port'
Assert-FileContains $resumeScript 'continue&' 'Resume script must continue asynchronously'
Assert-FileContains $resumeScript 'disconnect' 'Resume script must disconnect cleanly'
Assert-FileNotContains $resumeScript 'load' 'Resume script must not program Flash'
Assert-FileNotContains $resumeScript 'detach' 'Resume script must not detach from a halted target'

Assert-FileContains $haltScript 'target remote localhost:2331' 'Halt script must attach to the configured GDB port'
Assert-FileContains $haltScript 'detach' 'Halt script must detach from the target'
Assert-FileNotContains $haltScript 'load' 'Halt script must not program Flash'
Assert-FileNotContains $haltScript 'continue&' 'Halt script must not resume the target'

Assert-True (Test-Path -LiteralPath $snapshotBat) 'Runtime snapshot BAT entrypoint is missing'
Assert-True (Test-Path -LiteralPath $snapshotPs1) 'Runtime snapshot PowerShell entrypoint is missing'
Assert-True (Test-Path -LiteralPath $serverBat) 'GDB Server BAT entrypoint is missing'

New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$sharedArguments = @(
    "-JLinkGdbServer", $env:ComSpec,
    "-Device", "STM32F411CE",
    "-Interface", "SWD",
    "-Speed", "4000",
    "-StartTimeoutSeconds", "1",
    "-Mode", "halt",
    "-GdbScript", $haltScript,
    "-ServerLog", $testServerLog,
    "-SnapshotLog", $testSnapshotLog
)

Assert-SnapshotFailure ($sharedArguments + @("-Gdb", (Join-Path $tempRoot "missing-gdb.exe"), "-Axf", $snapshotPs1, "-Port", "2331")) 'Required file not found' 'Missing GDB executable must fail'
Assert-SnapshotFailure ($sharedArguments + @("-Gdb", $env:ComSpec, "-Axf", (Join-Path $tempRoot "missing.axf"), "-Port", "2331")) 'Required file not found' 'Missing AXF must fail'
Assert-SnapshotFailure ($sharedArguments + @("-Gdb", $env:ComSpec, "-Axf", $snapshotPs1, "-Port", "0")) 'outside the valid range' 'Invalid GDB port must fail'
Assert-SnapshotFailure ($sharedArguments + @("-Gdb", $env:ComSpec, "-Axf", $snapshotPs1, "-Port", "2331")) 'GDB Server' 'Non-listening GDB Server must fail'

Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[TEST][FAIL] $_" }
    exit 1
}

"[TEST][PASS] GDB automation contract checks passed."
exit 0
