$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$parserPath = Join-Path $repoRoot "05_Tools\Adapters\LogicAnalyzer\Sigrok\sigrok_parse.ps1"
$fixtureRoot = Join-Path $PSScriptRoot "fixtures"
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

Assert-True (Test-Path -LiteralPath $parserPath -PathType Leaf) "Sigrok parser is missing"

if (Test-Path -LiteralPath $parserPath -PathType Leaf) {
    . $parserPath
    try {
        $scanText = Get-Content -LiteralPath (Join-Path $fixtureRoot "device_scan.txt") -Raw -Encoding UTF8
        $scan = ConvertFrom-SigrokDeviceScan -Output $scanText -Driver "fx2lafw"
        Assert-Equal $scan.Status "SUCCESS" "Device scan fixture should parse successfully"
        Assert-Equal $scan.Devices.Count 1 "Device scan should contain one device"
        Assert-Equal $scan.Devices[0] "fx2lafw:conn=4.7" "Device selector should be parsed without display text"

        $spiText = Get-Content -LiteralPath (Join-Path $fixtureRoot "spi_decode.txt") -Raw -Encoding UTF8
        $spi = ConvertFrom-SigrokDecodeOutput -Protocol "spi" -Output $spiText
        Assert-Equal $spi.Status "SUCCESS" "SPI fixture should parse successfully"
        Assert-Equal $spi.Operation "SPI_DECODE" "SPI result operation should be normalized"
        Assert-Equal $spi.Transactions.Count 3 "SPI fixture should produce three data transactions"
        Assert-Equal $spi.Transactions[0].Mosi[0] "0x9F" "SPI MOSI command should be parsed"
        Assert-Equal $spi.Transactions[0].Miso[0] "0xEF" "SPI MISO first response byte should be parsed"
        Assert-Equal $spi.Transactions[2].Miso[0] "0x17" "SPI MISO last response byte should be parsed"
        Assert-True ($spi.PSObject.Properties.Name -contains "error_class") "Structured result must expose error_class"

        $sigrokSpiText = Get-Content -LiteralPath (Join-Path $fixtureRoot "spi_sigrok_annotations.txt") -Raw -Encoding UTF8
        $sigrokSpi = ConvertFrom-SigrokDecodeOutput -Protocol "spi" -Output $sigrokSpiText
        Assert-Equal $sigrokSpi.Status "SUCCESS" "Real Sigrok SPI annotations should parse successfully"
        Assert-Equal $sigrokSpi.Transactions.Count 4 "Real Sigrok SPI annotations should pair MOSI and MISO bytes"
        Assert-Equal $sigrokSpi.Transactions[0].Mosi[0] "0x9F" "Real Sigrok MOSI annotation should be parsed"
        Assert-Equal $sigrokSpi.Transactions[0].Miso[0] "0xEF" "Real Sigrok MISO annotation should be parsed"
        Assert-Equal $sigrokSpi.Transactions[2].Miso[0] "0x17" "Real Sigrok MISO response should be parsed"

        $i2cText = Get-Content -LiteralPath (Join-Path $fixtureRoot "i2c_decode.txt") -Raw -Encoding UTF8
        $i2c = ConvertFrom-SigrokDecodeOutput -Protocol "i2c" -Output $i2cText
        Assert-Equal $i2c.Status "SUCCESS" "I2C fixture should parse successfully"
        Assert-Equal $i2c.Operation "I2C_DECODE" "I2C result operation should be normalized"
        Assert-Equal $i2c.Transactions.Count 1 "I2C fixture should produce one transaction"
        Assert-Equal $i2c.Transactions[0].Address "0x50" "I2C device address should be parsed"
        Assert-True ($i2c.Transactions[0].event_types -contains "START") "I2C transaction should contain START"
        Assert-True ($i2c.Transactions[0].event_types -contains "REPEATED_START") "I2C transaction should contain repeated START"
        Assert-True ($i2c.Transactions[0].event_types -contains "STOP") "I2C transaction should contain STOP"
        Assert-True ($i2c.Transactions[0].event_types -contains "ACK") "I2C transaction should contain ACK"
        Assert-True ($i2c.Transactions[0].event_types -contains "NACK") "I2C transaction should contain NACK"
        Assert-True ($i2c.Transactions[0].Directions -contains "READ") "I2C transaction should contain READ direction"
        Assert-True ($i2c.Transactions[0].Directions -contains "WRITE") "I2C transaction should contain WRITE direction"

        $emptyText = Get-Content -LiteralPath (Join-Path $fixtureRoot "decode_empty.txt") -Raw -Encoding UTF8
        $empty = ConvertFrom-SigrokDecodeOutput -Protocol "spi" -Output $emptyText
        Assert-Equal $empty.Status "INCONCLUSIVE" "Empty decode must be inconclusive"
        Assert-Equal $empty.error_class "DECODE_EMPTY" "Empty decode should expose DECODE_EMPTY"
        Assert-Equal $empty.Transactions.Count 0 "Empty decode should contain no transactions"
    }
    catch {
        $failures.Add("Sigrok parser test raised an unexpected error: $($_.Exception.Message)")
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[SigrokParser][FAIL] $_" }
    exit 1
}

"[SigrokParser][PASS] Device scan, SPI, I2C and empty decode fixtures passed."
exit 0
