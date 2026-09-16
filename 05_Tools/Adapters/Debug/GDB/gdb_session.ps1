function ConvertTo-GdbInteger {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [int]$Minimum = 1
    )

    $value = Assert-ToolkitRequiredValue -Configuration $Configuration -Name $Name
    $parsed = 0
    if (-not [int]::TryParse($value, [ref]$parsed) -or ($parsed -lt $Minimum)) {
        throw "$Name must be an integer greater than or equal to ${Minimum}: $value"
    }
    return $parsed
}

function Assert-GdbScriptContract {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Content,

        [Parameter(Mandatory = $true)]
        [ValidateSet("resume", "halt", "capture", "trigger")]
        [string]$Mode
    )

    if ($Content -match '(?im)^\s*load(?:\s|$)') {
        throw "GDB script contains forbidden load command"
    }

    switch ($Mode) {
        "resume" {
            if ($Content -notmatch 'continue&') {
                throw "Resume GDB script must contain continue&"
            }
            if ($Content -match '(?im)^\s*detach(?:\s|$)') {
                throw "Resume GDB script must not contain detach"
            }
        }
        "halt" {
            if ($Content -notmatch '(?im)^\s*detach(?:\s|$)') {
                throw "Halt GDB script must contain detach"
            }
            if ($Content -match 'continue&') {
                throw "Halt GDB script must not contain continue&"
            }
        }
        "capture" {
            if ($Content -match 'continue&') {
                throw "Fault capture GDB script must not resume the target"
            }
            if ($Content -notmatch '(?im)^\s*detach(?:\s|$)') {
                throw "Fault GDB script must contain detach"
            }
        }
        "trigger" {
            foreach ($pattern in @(
                    '(?im)^\s*monitor\s+reset\s*$',
                    'continue&',
                    '(?im)^\s*break\s+diagnostics_fault_capture_stop\s*$')) {
                if ($Content -notmatch $pattern) {
                    throw "Fault trigger GDB script is missing required command: $pattern"
                }
            }
            if ($Content -notmatch '(?im)^\s*detach\s*$') {
                throw "Fault trigger GDB script must contain detach"
            }
        }
    }
}

function Invoke-GdbSession {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [ValidateSet("resume", "halt", "capture", "trigger")]
        [string]$Mode,

        [Parameter(Mandatory = $true)]
        [string]$GdbScript,

        [Parameter(Mandatory = $true)]
        [string]$ServerLog,

        [Parameter(Mandatory = $true)]
        [string]$ClientLog,

        [int]$ClientTimeoutMilliseconds = 60000
    )

    if ($ClientTimeoutMilliseconds -lt 1) {
        throw "GDB client timeout must be positive: $ClientTimeoutMilliseconds"
    }

    $gdb = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "ARM_GDB"
    $server = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_GDB_SERVER"
    $axfRelativePath = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "PROJECT_APP_AXF"
    $device = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_DEVICE"
    $interface = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_IF"
    $speed = ConvertTo-GdbInteger -Configuration $Configuration -Name "JLINK_SPEED"
    $port = ConvertTo-GdbInteger -Configuration $Configuration -Name "GDB_PORT" -Minimum 0
    if (($port -lt 1) -or ($port -gt 65535)) {
        throw "GDB_PORT is outside the valid range: $port"
    }
    $startTimeoutSeconds = if ($null -ne $Configuration.PSObject.Properties["GDB_START_TIMEOUT_SECONDS"] -and
        -not [string]::IsNullOrWhiteSpace([string]$Configuration.GDB_START_TIMEOUT_SECONDS)) {
        ConvertTo-GdbInteger -Configuration $Configuration -Name "GDB_START_TIMEOUT_SECONDS"
    }
    else {
        10
    }
    $axf = Resolve-ToolkitProjectPath -ProjectRoot $Configuration.PROJECT_ROOT -RelativePath $axfRelativePath

    foreach ($path in @($gdb, $server, $axf, $GdbScript)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required file not found: $path"
        }
    }

    $scriptContent = [System.IO.File]::ReadAllText($GdbScript)
    Assert-GdbScriptContract -Content $scriptContent -Mode $Mode

    foreach ($logPath in @($ServerLog, $ClientLog)) {
        Remove-Item -LiteralPath $logPath -Force -ErrorAction SilentlyContinue
        New-ToolkitLogDirectory -Path (Split-Path -Parent ([System.IO.Path]::GetFullPath($logPath))) | Out-Null
    }

    $serverCapture = $null
    $clientCapture = $null
    $serverResult = $null
    $clientResult = $null
    $temporaryGdbScript = Join-Path ([System.IO.Path]::GetTempPath()) ("toolkit_gdb_" + [Guid]::NewGuid().ToString("N") + ".gdb")

    try {
        $gdbFileCommand = 'file "' + $axf.Replace('\', '/') + '"'
        [System.IO.File]::WriteAllText(
            $temporaryGdbScript,
            $gdbFileCommand + [Environment]::NewLine + $scriptContent,
            (New-Object System.Text.UTF8Encoding($false)))

        $serverArguments = @(
            "-device", $device,
            "-if", $interface,
            "-speed", $speed.ToString(),
            "-port", $port.ToString(),
            "-swoport", "2332",
            "-telnetport", "2333",
            "-nogui"
        )

        Write-Host "[GDB] Starting J-Link GDB Server..."
        $serverCapture = Start-ToolkitProcess -FilePath $server -Arguments $serverArguments -WorkingDirectory $Configuration.PROJECT_ROOT
        $serverReady = $false
        $deadline = (Get-Date).AddSeconds($startTimeoutSeconds)
        while ((Get-Date) -lt $deadline) {
            $serverCapture.Process.Refresh()
            if ($serverCapture.Process.HasExited) {
                throw "J-Link GDB Server exited before port $port became ready (exit=$($serverCapture.Process.ExitCode))"
            }
            if (Test-ToolkitTcpPort -HostName "127.0.0.1" -PortNumber $port) {
                $serverReady = $true
                break
            }
            Start-Sleep -Milliseconds 100
        }
        if (-not $serverReady) {
            throw "GDB Server startup timeout on port $port"
        }

        $gdbArguments = @("-q", "-nx", "-x", $temporaryGdbScript)
        Write-Host "[GDB] Starting GDB session ($Mode)..."
        $clientCapture = Start-ToolkitProcess -FilePath $gdb -Arguments $gdbArguments -WorkingDirectory $Configuration.PROJECT_ROOT
        $clientResult = Complete-ToolkitProcess -Capture $clientCapture -TimeoutMilliseconds $ClientTimeoutMilliseconds
        Write-ToolkitProcessResultLog -Path $ClientLog -Result $clientResult

        if ($clientResult.ExitCode -ne 0) {
            throw "GDB returned ERRORLEVEL=$($clientResult.ExitCode)"
        }

        $clientOutput = ([string]$clientResult.Stdout) + [Environment]::NewLine + ([string]$clientResult.Stderr)
        if ($clientOutput -match 'Cannot execute this command while the target is running|Remote communication error|Connection timed out|Connection refused|Error in sourced command file|Cannot execute this command') {
            throw "GDB output contains an execution failure"
        }

        return [PSCustomObject]@{
            ClientResult = $clientResult
            ClientOutput = $clientOutput
            ServerResult = $null
            ServerLog = $ServerLog
            ClientLog = $ClientLog
        }
    }
    catch {
        $failureMessage = $_.Exception.Message
        throw
    }
    finally {
        if ($null -ne $clientCapture -and $null -eq $clientResult) {
            $clientResult = Stop-ToolkitProcess -Capture $clientCapture
            Write-ToolkitProcessResultLog -Path $ClientLog -Result $clientResult
        }
        if ($null -ne $serverCapture) {
            $serverResult = Stop-ToolkitProcess -Capture $serverCapture
            Write-ToolkitProcessResultLog -Path $ServerLog -Result $serverResult
        }
        if ($null -ne $failureMessage) {
            Write-ToolkitLog -Path $ClientLog -Message "[GDB][FAIL] $failureMessage"
            Write-ToolkitLog -Path $ServerLog -Message "[GDB][FAIL] $failureMessage"
        }
        Remove-Item -LiteralPath $temporaryGdbScript -Force -ErrorAction SilentlyContinue
    }
}
