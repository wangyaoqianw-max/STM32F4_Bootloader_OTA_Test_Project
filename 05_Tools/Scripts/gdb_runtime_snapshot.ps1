param(
    [Parameter(Mandatory = $true)]
    [string]$Gdb,

    [Parameter(Mandatory = $true)]
    [string]$JLinkGdbServer,

    [Parameter(Mandatory = $true)]
    [string]$Axf,

    [Parameter(Mandatory = $true)]
    [string]$Device,

    [Parameter(Mandatory = $true)]
    [string]$Interface,

    [Parameter(Mandatory = $true)]
    [int]$Speed,

    [Parameter(Mandatory = $true)]
    [int]$Port,

    [Parameter(Mandatory = $true)]
    [int]$StartTimeoutSeconds,

    [Parameter(Mandatory = $true)]
    [ValidateSet("resume", "halt")]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$GdbScript,

    [Parameter(Mandatory = $true)]
    [string]$ServerLog,

    [Parameter(Mandatory = $true)]
    [string]$SnapshotLog
)

$ErrorActionPreference = "Stop"

$server = $null
$gdbProcess = $null
$serverCapture = $null
$gdbCapture = $null
$serverStdout = $null
$serverStderr = $null
$gdbStdout = $null
$gdbStderr = $null
$temporaryGdbScript = $null
$failureMessage = $null
$resultCode = 1

function Write-Utf8File {
    param(
        [string]$Path,
        [string]$Content
    )

    $directory = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

function Merge-ProcessLogs {
    param(
        [string]$OutputPath,
        [string]$StandardOutputPath,
        [string]$StandardErrorPath
    )

    $parts = New-Object System.Collections.Generic.List[string]
    foreach ($path in @($StandardOutputPath, $StandardErrorPath)) {
        if ((-not [string]::IsNullOrWhiteSpace($path)) -and (Test-Path -LiteralPath $path)) {
            $content = [System.IO.File]::ReadAllText($path)
            if (-not [string]::IsNullOrEmpty($content)) {
                $parts.Add($content)
            }
        }
    }

    Write-Utf8File -Path $OutputPath -Content ($parts -join ([Environment]::NewLine))
}

function Test-TcpPort {
    param(
        [string]$HostName,
        [int]$PortNumber
    )

    $client = New-Object System.Net.Sockets.TcpClient
    $asyncResult = $null
    try {
        $asyncResult = $client.BeginConnect($HostName, $PortNumber, $null, $null)
        if (-not $asyncResult.AsyncWaitHandle.WaitOne(250)) {
            return $false
        }

        $client.EndConnect($asyncResult)
        return $true
    }
    catch {
        return $false
    }
    finally {
        $client.Close()
    }
}

function Stop-OwnedProcess {
    param(
        [System.Diagnostics.Process]$Process
    )

    if ($null -eq $Process) {
        return
    }

    try {
        $Process.Refresh()
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            $Process.WaitForExit()
        }
    }
    catch {
    }
}

function ConvertTo-ProcessArgumentString {
    param(
        [string[]]$Arguments
    )

    return (($Arguments | ForEach-Object {
        '"' + ($_ -replace '"', '\\"') + '"'
    }) -join ' ')
}

function Start-CapturedProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = ConvertTo-ProcessArgumentString -Arguments $Arguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    return [PSCustomObject]@{
        Process = $process
        StdoutTask = $stdoutTask
        StderrTask = $stderrTask
    }
}

function Save-CapturedProcessOutput {
    param(
        [PSCustomObject]$Capture,
        [string]$StandardOutputPath,
        [string]$StandardErrorPath
    )

    if ($null -eq $Capture) {
        return
    }

    try {
        $Capture.Process.WaitForExit()
    }
    catch {
    }

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($StandardOutputPath, $Capture.StdoutTask.GetAwaiter().GetResult(), $utf8)
    [System.IO.File]::WriteAllText($StandardErrorPath, $Capture.StderrTask.GetAwaiter().GetResult(), $utf8)
}

try {
    foreach ($path in @($Gdb, $JLinkGdbServer, $Axf, $GdbScript)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required file not found: $path"
        }
    }

    if (($Port -lt 1) -or ($Port -gt 65535)) {
        throw "GDB port is outside the valid range: $Port"
    }

    if ($Speed -lt 1) {
        throw "J-Link speed must be positive: $Speed"
    }

    if ($StartTimeoutSeconds -lt 1) {
        throw "GDB Server startup timeout must be positive: $StartTimeoutSeconds"
    }

    $scriptContent = Get-Content -Raw -LiteralPath $GdbScript
    if ($scriptContent -match '(?im)^\s*load(?:\s|$)') {
        throw "GDB script contains forbidden load command: $GdbScript"
    }

    if ($Mode -eq "resume") {
        if ($scriptContent -notmatch 'continue&') {
            throw "Resume GDB script must contain continue&"
        }
        if ($scriptContent -match '(?im)^\s*detach(?:\s|$)') {
            throw "Resume GDB script must not contain detach"
        }
    }
    else {
        if ($scriptContent -notmatch '(?im)^\s*detach(?:\s|$)') {
            throw "Halt GDB script must contain detach"
        }
        if ($scriptContent -match 'continue&') {
            throw "Halt GDB script must not contain continue&"
        }
    }

    Remove-Item -LiteralPath $ServerLog -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $SnapshotLog -Force -ErrorAction SilentlyContinue

    $serverStdout = [System.IO.Path]::GetTempFileName()
    $serverStderr = [System.IO.Path]::GetTempFileName()
    $gdbStdout = [System.IO.Path]::GetTempFileName()
    $gdbStderr = [System.IO.Path]::GetTempFileName()
    $temporaryGdbScript = [System.IO.Path]::GetTempFileName()

    $gdbFileCommand = 'file "' + $Axf.Replace('\', '/') + '"'
    $runtimeScript = $gdbFileCommand + [Environment]::NewLine + $scriptContent
    Write-Utf8File -Path $temporaryGdbScript -Content $runtimeScript

    $serverArguments = @(
        "-device", $Device,
        "-if", $Interface,
        "-speed", $Speed.ToString(),
        "-port", $Port.ToString(),
        "-swoport", "2332",
        "-telnetport", "2333",
        "-nogui"
    )

    Write-Host "[GDB] Starting J-Link GDB Server..."
    $serverCapture = Start-CapturedProcess -FilePath $JLinkGdbServer -Arguments $serverArguments
    $server = $serverCapture.Process

    $serverReady = $false
    $deadline = (Get-Date).AddSeconds($StartTimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $server.Refresh()
        if ($server.HasExited) {
            throw "J-Link GDB Server exited before port $Port became ready (exit=$($server.ExitCode))"
        }

        if (Test-TcpPort -HostName "127.0.0.1" -PortNumber $Port) {
            $serverReady = $true
            break
        }

        Start-Sleep -Milliseconds 100
    }

    if (-not $serverReady) {
        throw "GDB Server startup timeout on port $Port"
    }

    $gdbArguments = @(
        "-q",
        "-nx",
        "-x", $temporaryGdbScript
    )

    Write-Host "[GDB] Starting runtime snapshot ($Mode)..."
    $gdbCapture = Start-CapturedProcess -FilePath $Gdb -Arguments $gdbArguments
    $gdbProcess = $gdbCapture.Process

    $gdbFinished = $gdbProcess.WaitForExit(60000)
    if (-not $gdbFinished) {
        Stop-OwnedProcess -Process $gdbProcess
        throw "GDB runtime snapshot timeout"
    }

    Save-CapturedProcessOutput -Capture $gdbCapture -StandardOutputPath $gdbStdout -StandardErrorPath $gdbStderr
    Merge-ProcessLogs -OutputPath $SnapshotLog -StandardOutputPath $gdbStdout -StandardErrorPath $gdbStderr
    $gdbOutput = Get-Content -Raw -LiteralPath $SnapshotLog

    if ($gdbProcess.ExitCode -ne 0) {
        throw "GDB returned ERRORLEVEL=$($gdbProcess.ExitCode)"
    }

    if ($gdbOutput -match 'Cannot execute this command while the target is running|Remote communication error|Connection timed out|Connection refused') {
        throw "GDB output contains a runtime failure"
    }

    if ($gdbOutput -notmatch 'GDB RUNTIME SNAPSHOT') {
        throw "GDB snapshot marker is missing"
    }

    if (($Mode -eq "resume") -and ($gdbOutput -notmatch 'resume-and-disconnect|Ending remote debugging')) {
        throw "Resume exit evidence is missing"
    }

    if (($Mode -eq "halt") -and ($gdbOutput -notmatch 'halt-and-detach')) {
        throw "Halt exit evidence is missing"
    }

    Write-Host "[GDB][PASS] GDB runtime snapshot completed."
    $resultCode = 0
}
catch {
    $failureMessage = $_.Exception.Message
    Write-Host "[GDB][FAIL] $failureMessage"
}
finally {
    Stop-OwnedProcess -Process $gdbProcess
    Stop-OwnedProcess -Process $server

    Save-CapturedProcessOutput -Capture $gdbCapture -StandardOutputPath $gdbStdout -StandardErrorPath $gdbStderr
    Save-CapturedProcessOutput -Capture $serverCapture -StandardOutputPath $serverStdout -StandardErrorPath $serverStderr

    if ($null -ne $serverStdout) {
        Merge-ProcessLogs -OutputPath $ServerLog -StandardOutputPath $serverStdout -StandardErrorPath $serverStderr
    }

    if ($null -eq $failureMessage) {
        if (-not (Test-Path -LiteralPath $ServerLog)) {
            Write-Utf8File -Path $ServerLog -Content "[GDB][FAIL] Server log was not generated."
        }
        if (-not (Test-Path -LiteralPath $SnapshotLog)) {
            Write-Utf8File -Path $SnapshotLog -Content "[GDB][FAIL] Snapshot log was not generated."
        }
    }
    else {
        $failureLog = "[GDB][FAIL] $failureMessage"
        if (Test-Path -LiteralPath $ServerLog) {
            Add-Content -LiteralPath $ServerLog -Value $failureLog
        }
        else {
            Write-Utf8File -Path $ServerLog -Content $failureLog
        }

        if (-not (Test-Path -LiteralPath $SnapshotLog)) {
            Write-Utf8File -Path $SnapshotLog -Content $failureLog
        }
    }

    foreach ($temporaryFile in @($serverStdout, $serverStderr, $gdbStdout, $gdbStderr, $temporaryGdbScript)) {
        if ($null -ne $temporaryFile) {
            Remove-Item -LiteralPath $temporaryFile -Force -ErrorAction SilentlyContinue
        }
    }
}

exit $resultCode
