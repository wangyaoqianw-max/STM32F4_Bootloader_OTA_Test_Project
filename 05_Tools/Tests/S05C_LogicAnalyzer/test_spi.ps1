param(
    [string]$ResultPath = ""
)

$ErrorActionPreference = "Stop"

function Test-W25Q64JedecResult {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Result
    )

    if ([string]$Result.status -ne "SUCCESS") {
        return [PSCustomObject][ordered]@{
            verdict = "ERROR"
            assertion = "W25Q64_JEDEC_ID"
            reason = "Generic Logic Workflow did not produce a successful decode"
        }
    }

    $mosi = [System.Collections.Generic.List[string]]::new()
    $miso = [System.Collections.Generic.List[string]]::new()
    foreach ($transaction in @($Result.transactions)) {
        foreach ($byte in @($transaction.mosi)) {
            $mosi.Add(([string]$byte).ToUpperInvariant())
        }
        foreach ($byte in @($transaction.miso)) {
            $miso.Add(([string]$byte).ToUpperInvariant())
        }
    }

    $commandIndex = -1
    for ($index = 0; $index -lt $mosi.Count; $index++) {
        if ($mosi[$index] -eq "0X9F") {
            $commandIndex = $index
            break
        }
    }
    if ($commandIndex -lt 0) {
        return [PSCustomObject][ordered]@{
            verdict = "FAIL"
            assertion = "W25Q64_JEDEC_ID"
            command = "0x9F"
            expected = @("0xEF", "0x40", "0x17")
            observed = @()
            reason = "JEDEC command was not observed"
        }
    }

    $observed = [System.Collections.Generic.List[string]]::new()
    for ($index = $commandIndex + 1; $index -lt ($commandIndex + 4) -and $index -lt $miso.Count; $index++) {
        $observed.Add($miso[$index].ToUpperInvariant())
    }
    $expected = @("0XEF", "0X40", "0X17")
    $passed = $observed.Count -eq $expected.Count
    for ($index = 0; $passed -and $index -lt $expected.Count; $index++) {
        $passed = $observed[$index] -eq $expected[$index]
    }

    return [PSCustomObject][ordered]@{
        verdict = if ($passed) { "PASS" } else { "FAIL" }
        assertion = "W25Q64_JEDEC_ID"
        command = "0x9F"
        expected = @("0xEF", "0x40", "0x17")
        observed = @($observed)
        reason = if ($passed) { "Read-only JEDEC ID matched" } else { "JEDEC ID response did not match" }
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    if ([string]::IsNullOrWhiteSpace($ResultPath)) {
        throw "SPI project assertion requires -ResultPath"
    }
    $result = Get-Content -LiteralPath $ResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $verdict = Test-W25Q64JedecResult -Result $result
    $verdict | ConvertTo-Json -Depth 8
    if ($verdict.verdict -eq "PASS") { exit 0 }
    if ($verdict.verdict -eq "FAIL") { exit 1 }
    exit 10
}
