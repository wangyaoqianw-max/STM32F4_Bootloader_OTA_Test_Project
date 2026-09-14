param(
    [Parameter(Mandatory = $true)]
    [string]$Exe,

    [Parameter(Mandatory = $true)]
    [string]$Device,

    [Parameter(Mandatory = $true)]
    [string]$Interface,

    [Parameter(Mandatory = $true)]
    [int]$Speed,

    [Parameter(Mandatory = $true)]
    [int]$Channel,

    [Parameter(Mandatory = $true)]
    [int]$DurationSeconds,

    [Parameter(Mandatory = $true)]
    [string]$Output,

    [Parameter(Mandatory = $true)]
    [string]$Diagnostic
)

$ErrorActionPreference = "Stop"

$stderr = "$Diagnostic.stderr"

Remove-Item -LiteralPath $Output -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $Diagnostic -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $stderr -Force -ErrorAction SilentlyContinue

$arguments = @(
    "-Device", $Device,
    "-If", $Interface,
    "-Speed", $Speed.ToString(),
    "-RTTChannel", $Channel.ToString(),
    $Output
)

try {
    # Windows PowerShell may inherit both PATH and Path from cmd.exe.
    # Start-Process enumerates that environment and fails on the duplicate key.
    $argumentString = ($arguments | ForEach-Object {
        '"' + ($_ -replace '"', '\\"') + '"'
    }) -join ' '

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Exe
    $startInfo.Arguments = $argumentString
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()

    $standardOutputTask = $process.StandardOutput.ReadToEndAsync()
    $standardErrorTask = $process.StandardError.ReadToEndAsync()

    $finished = $process.WaitForExit($DurationSeconds * 1000)

    if (-not $finished) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText(
        $Diagnostic,
        $standardOutputTask.GetAwaiter().GetResult(),
        $utf8
    )
    [IO.File]::WriteAllText(
        $stderr,
        $standardErrorTask.GetAwaiter().GetResult(),
        $utf8
    )

    Start-Sleep -Milliseconds 200
}
catch {
    $_ | Out-String | Add-Content -LiteralPath $Diagnostic
    exit 30
}

if (-not (Test-Path -LiteralPath $Output)) {
    exit 31
}

if ((Get-Item -LiteralPath $Output).Length -eq 0) {
    exit 32
}

exit 0
