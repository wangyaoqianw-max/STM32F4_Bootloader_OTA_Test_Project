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
    $process = Start-Process `
        -FilePath $Exe `
        -ArgumentList $arguments `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput $Diagnostic `
        -RedirectStandardError $stderr

    $finished = $process.WaitForExit($DurationSeconds * 1000)

    if (-not $finished) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }

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
