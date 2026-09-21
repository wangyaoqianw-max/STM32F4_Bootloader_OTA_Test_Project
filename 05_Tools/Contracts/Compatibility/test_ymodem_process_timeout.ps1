$ErrorActionPreference = "Stop"
$toolsRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
$routerPath = Join-Path $toolsRoot "toolkit.ps1"
$router = Get-Content -LiteralPath $routerPath -Raw

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

Assert-True ($router -match '(?s)function\s+Invoke-ExternalToolkitCommand.*?\[int\]\$TimeoutMilliseconds\s*=\s*60000') `
    "External toolkit command must accept an explicit timeout"
Assert-True ($router -match '(?s)function\s+Invoke-ExternalToolkitCommand.*?Invoke-ToolkitProcess.*?-TimeoutMilliseconds\s+\$TimeoutMilliseconds') `
    "External toolkit command must forward its timeout to the process helper"
Assert-True ($router -match 'function\s+Get-YmodemProcessTimeoutMilliseconds') `
    "YMODEM must have a process timeout policy separate from the protocol timeout"
Assert-True ($router -match '(?s)function\s+Invoke-Ymodem.*?Get-YmodemProcessTimeoutMilliseconds') `
    "YMODEM must use its long-running process timeout policy"

Write-Output "[YMODEM-TIMEOUT][PASS] Long-running sender timeout contract is present."
exit 0
