param(
    [string]$ResultPath = ""
)

$ErrorActionPreference = "Stop"

function Test-At24C02TransactionResult {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Result
    )

    if ([string]$Result.status -ne "SUCCESS" -or @($Result.transactions).Count -eq 0) {
        return [PSCustomObject][ordered]@{
            verdict = "ERROR"
            assertion = "AT24C02_I2C_TRANSACTION"
            reason = "Generic Logic Workflow did not produce a successful I2C transaction"
        }
    }

    $transaction = @($Result.transactions)[0]
    $eventTypes = @($transaction.event_types)
    $directions = @($transaction.directions)
    $requiredEvents = @("START", "REPEATED_START", "STOP", "ACK", "NACK")
    $missingEvents = @($requiredEvents | Where-Object { $_ -notin $eventTypes })
    $missingDirections = @(@("READ", "WRITE") | Where-Object { $_ -notin $directions })
    $passed = ([string]$transaction.address).ToUpperInvariant() -eq "0X50" -and $missingEvents.Count -eq 0 -and $missingDirections.Count -eq 0

    return [PSCustomObject][ordered]@{
        verdict = if ($passed) { "PASS" } else { "FAIL" }
        assertion = "AT24C02_I2C_TRANSACTION"
        address = "0x50"
        observed_address = $transaction.address
        required_events = $requiredEvents
        observed_events = $eventTypes
        required_directions = @("READ", "WRITE")
        observed_directions = $directions
        missing_events = $missingEvents
        missing_directions = $missingDirections
        reason = if ($passed) { "Read-only I2C transaction structure matched" } else { "I2C transaction structure did not match" }
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    if ([string]::IsNullOrWhiteSpace($ResultPath)) {
        throw "I2C project assertion requires -ResultPath"
    }
    $result = Get-Content -LiteralPath $ResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $verdict = Test-At24C02TransactionResult -Result $result
    $verdict | ConvertTo-Json -Depth 8
    if ($verdict.verdict -eq "PASS") { exit 0 }
    if ($verdict.verdict -eq "FAIL") { exit 1 }
    exit 10
}
