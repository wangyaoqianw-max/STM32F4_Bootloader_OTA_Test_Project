param(
    [Parameter(Mandatory = $true)]
    [string]$Gdb,

    [Parameter(Mandatory = $true)]
    [string]$JLinkGdbServer,

    [Parameter(Mandatory = $true)]
    [string]$RttLogger,

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
    [int]$RttChannel,

    [Parameter(Mandatory = $false)]
    [ValidateSet("capture", "trigger")]
    [string]$Mode = "capture",

    [Parameter(Mandatory = $true)]
    [string]$GdbScript,

    [Parameter(Mandatory = $true)]
    [string]$ServerLog,

    [Parameter(Mandatory = $true)]
    [string]$GdbLog,

    [Parameter(Mandatory = $true)]
    [string]$RttLog
)

$ErrorActionPreference = "Stop"

$server = $null
$gdbProcess = $null
$rttProcess = $null
$serverCapture = $null
$gdbCapture = $null
$rttCapture = $null
$serverStdout = $null
$serverStderr = $null
$gdbStdout = $null
$gdbStderr = $null
$rttStdout = $null
$rttStderr = $null
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
    $startInfo.RedirectStandardInput = $true
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
    foreach ($path in @($Gdb, $JLinkGdbServer, $RttLogger, $Axf, $GdbScript)) {
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
    if ($RttChannel -lt 0) {
        throw "RTT channel must not be negative: $RttChannel"
    }

    $scriptContent = Get-Content -Raw -LiteralPath $GdbScript
    if ($scriptContent -match '(?im)^\s*load(?:\s|$)') {
        throw "GDB script contains forbidden load command: $GdbScript"
    }
    if ($Mode -eq "trigger") {
        if ($scriptContent -notmatch '(?im)^\s*monitor reset\s*$') {
            throw "Trigger GDB script must reset through the open GDB connection"
        }
        if ($scriptContent -notmatch 'continue&') {
            throw "Trigger GDB script must continue asynchronously"
        }
        if ($scriptContent -notmatch '(?im)^\s*break\s+diagnostics_fault_capture_stop\s*$') {
            throw "Trigger GDB script must break after the Fault handler captured context"
        }
    }
    else {
        if ($scriptContent -match 'continue&') {
            throw "Fault capture GDB script must not resume the target"
        }
    }
    if ($scriptContent -notmatch '(?im)^\s*detach\s*$') {
        throw "Fault GDB script must contain detach"
    }

    foreach ($path in @($ServerLog, $GdbLog, $RttLog)) {
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }

    $serverStdout = [System.IO.Path]::GetTempFileName()
    $serverStderr = [System.IO.Path]::GetTempFileName()
    $gdbStdout = [System.IO.Path]::GetTempFileName()
    $gdbStderr = [System.IO.Path]::GetTempFileName()
    $rttStdout = [System.IO.Path]::GetTempFileName()
    $rttStderr = [System.IO.Path]::GetTempFileName()
    $temporaryGdbScript = [System.IO.Path]::GetTempFileName()

    $gdbFileCommand = 'file "' + $Axf.Replace('\', '/') + '"'
    Write-Utf8File -Path $temporaryGdbScript -Content ($gdbFileCommand + [Environment]::NewLine + $scriptContent)

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

    Write-Host "[GDB] Capturing stopped Fault..."
    $gdbCapture = Start-CapturedProcess -FilePath $Gdb -Arguments $gdbArguments
    $gdbProcess = $gdbCapture.Process
    if (-not $gdbProcess.WaitForExit(60000)) {
        Stop-OwnedProcess -Process $gdbProcess
        throw "GDB fault capture timeout"
    }

    Save-CapturedProcessOutput -Capture $gdbCapture -StandardOutputPath $gdbStdout -StandardErrorPath $gdbStderr
    Merge-ProcessLogs -OutputPath $GdbLog -StandardOutputPath $gdbStdout -StandardErrorPath $gdbStderr
    $gdbOutput = Get-Content -Raw -LiteralPath $GdbLog

    if ($gdbProcess.ExitCode -ne 0) {
        throw "GDB returned ERRORLEVEL=$($gdbProcess.ExitCode)"
    }
    if ($gdbOutput -match 'Remote communication error|Connection timed out|Connection refused|Error in sourced command file|Cannot execute this command') {
        throw "GDB output contains an execution failure"
    }
    foreach ($marker in @('GDB FAULT CAPTURE', '[FAULT_CONTEXT]', 'FAULT PC', '[SCB_FAULT_REGISTERS]', '[BACKTRACE]', '[STACK]', 'halt-and-detach')) {
        if ($gdbOutput -notmatch [regex]::Escape($marker)) {
            throw "GDB fault evidence is missing: $marker"
        }
    }

    Stop-OwnedProcess -Process $gdbProcess
    $gdbProcess = $null
    Stop-OwnedProcess -Process $server
    Save-CapturedProcessOutput -Capture $serverCapture -StandardOutputPath $serverStdout -StandardErrorPath $serverStderr
    $serverCapture = $null
    $server = $null
    Merge-ProcessLogs -OutputPath $ServerLog -StandardOutputPath $serverStdout -StandardErrorPath $serverStderr

    Write-Host "[RTT] Capturing retained Fault RTT output..."
    $rttArguments = @(
        "-Device", $Device,
        "-If", $Interface,
        "-Speed", $Speed.ToString(),
        "-RTTChannel", $RttChannel.ToString(),
        $RttLog
    )
    $rttCapture = Start-CapturedProcess -FilePath $RttLogger -Arguments $rttArguments
    $rttProcess = $rttCapture.Process
    if (-not $rttProcess.WaitForExit(5000)) {
        Stop-OwnedProcess -Process $rttProcess
    }
    Save-CapturedProcessOutput -Capture $rttCapture -StandardOutputPath $rttStdout -StandardErrorPath $rttStderr
    if (-not (Test-Path -LiteralPath $RttLog) -or ((Get-Item -LiteralPath $RttLog).Length -eq 0)) {
        throw "RTT fault evidence was not captured"
    }

    Write-Host "[GDB][PASS] Fault capture completed; MCU remains halted."
    $resultCode = 0
}
catch {
    $failureMessage = $_.Exception.Message
    Write-Host "[GDB][FAIL] $failureMessage"
}
finally {
    Stop-OwnedProcess -Process $gdbProcess
    Stop-OwnedProcess -Process $server
    Stop-OwnedProcess -Process $rttProcess

    Save-CapturedProcessOutput -Capture $gdbCapture -StandardOutputPath $gdbStdout -StandardErrorPath $gdbStderr
    Save-CapturedProcessOutput -Capture $serverCapture -StandardOutputPath $serverStdout -StandardErrorPath $serverStderr
    Save-CapturedProcessOutput -Capture $rttCapture -StandardOutputPath $rttStdout -StandardErrorPath $rttStderr

    if ($null -ne $serverStdout) {
        Merge-ProcessLogs -OutputPath $ServerLog -StandardOutputPath $serverStdout -StandardErrorPath $serverStderr
    }

    if ($null -ne $failureMessage) {
        $failureLog = "[GDB][FAIL] $failureMessage"
        if (Test-Path -LiteralPath $GdbLog) {
            Add-Content -LiteralPath $GdbLog -Value $failureLog
        }
        else {
            Write-Utf8File -Path $GdbLog -Content $failureLog
        }
        if (-not (Test-Path -LiteralPath $RttLog)) {
            Write-Utf8File -Path $RttLog -Content "[RTT][FAIL] $failureMessage"
        }
        if (-not (Test-Path -LiteralPath $ServerLog)) {
            Write-Utf8File -Path $ServerLog -Content $failureLog
        }
    }

    foreach ($temporaryFile in @(
            $serverStdout,
            $serverStderr,
            $gdbStdout,
            $gdbStderr,
            $rttStdout,
            $rttStderr,
            $temporaryGdbScript)) {
        if ($null -ne $temporaryFile) {
            Remove-Item -LiteralPath $temporaryFile -Force -ErrorAction SilentlyContinue
        }
    }
}

exit $resultCode
