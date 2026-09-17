function New-SigrokStructuredResult {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Status,

        [Parameter(Mandatory = $true)]
        [string]$Operation,

        [object[]]$Transactions = @(),

        [string]$ErrorClass = $null
    )

    return [PSCustomObject][ordered]@{
        status = $Status
        operation = $Operation
        device = [ordered]@{}
        capture = [ordered]@{}
        mapping = [ordered]@{}
        transactions = @($Transactions)
        error_class = $ErrorClass
        artifacts = [ordered]@{}
    }
}

function ConvertFrom-SigrokDeviceScan {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Output,

        [string]$Driver = "fx2lafw"
    )

    $devices = [System.Collections.Generic.List[string]]::new()
    $driverPattern = [regex]::Escape($Driver)
    foreach ($line in ($Output -split "`r?`n")) {
        if ($line -match ("(?i)(?<selector>{0}:[^\s,;\)]+)" -f $driverPattern)) {
            $selector = $matches.selector
            if (-not $devices.Contains($selector)) {
                $devices.Add($selector)
            }
        }
    }

    if ($devices.Count -eq 0) {
        return New-SigrokStructuredResult -Status "ERROR" -Operation "DEVICE_SCAN" -ErrorClass "DEVICE_NOT_FOUND"
    }

    $result = New-SigrokStructuredResult -Status "SUCCESS" -Operation "DEVICE_SCAN" -Transactions @($devices | ForEach-Object { $_ })
    $result | Add-Member -NotePropertyName devices -NotePropertyValue @($devices)
    return $result
}

function ConvertTo-SigrokByteArray {
    param(
        [AllowEmptyString()]
        [string]$Value
    )

    $bytes = [System.Collections.Generic.List[string]]::new()
    foreach ($match in [regex]::Matches($Value, '(?i)(?:0x)?(?<byte>[0-9a-f]{1,2})')) {
        $number = [Convert]::ToInt32($match.Groups["byte"].Value, 16)
        $bytes.Add(("0x{0:X2}" -f $number))
    }
    Write-Output -NoEnumerate $bytes
}

function ConvertFrom-SigrokSpiOutput {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Output
    )

    if ($Output -match '(?im)^\s*\[(?:MOSI|MISO)\]\s*$') {
        $mosiBytes = [System.Collections.Generic.List[string]]::new()
        $misoBytes = [System.Collections.Generic.List[string]]::new()
        $direction = ""

        foreach ($line in ($Output -split "`r?`n")) {
            $trimmed = $line.Trim()
            if ($trimmed -match '(?i)^\[MOSI\]$') {
                $direction = "mosi"
                continue
            }
            if ($trimmed -match '(?i)^\[MISO\]$') {
                $direction = "miso"
                continue
            }
            if ([string]::IsNullOrWhiteSpace($direction)) {
                continue
            }

            $dataMatch = [regex]::Match($trimmed, '(?i)^spi-\d+\s*:\s*(?<value>(?:[0-9a-f]{2})(?:\s+[0-9a-f]{2})*)$')
            if (-not $dataMatch.Success) {
                continue
            }
            $bytes = ConvertTo-SigrokByteArray -Value $dataMatch.Groups["value"].Value
            foreach ($byte in $bytes) {
                if ($direction -eq "mosi") {
                    $mosiBytes.Add($byte)
                }
                else {
                    $misoBytes.Add($byte)
                }
            }
        }

        $transactions = [System.Collections.Generic.List[object]]::new()
        $transactionCount = [Math]::Max($mosiBytes.Count, $misoBytes.Count)
        for ($index = 0; $index -lt $transactionCount; $index++) {
            $mosi = [System.Collections.Generic.List[string]]::new()
            $miso = [System.Collections.Generic.List[string]]::new()
            if ($index -lt $mosiBytes.Count) {
                $mosi.Add($mosiBytes[$index])
            }
            if ($index -lt $misoBytes.Count) {
                $miso.Add($misoBytes[$index])
            }
            $transactions.Add([PSCustomObject][ordered]@{
                index = $index
                mosi = $mosi
                miso = $miso
                annotation = ("MOSI={0} MISO={1}" -f ($mosi -join " "), ($miso -join " "))
            })
        }

        if ($transactions.Count -eq 0) {
            return New-SigrokStructuredResult -Status "INCONCLUSIVE" -Operation "SPI_DECODE" -Transactions @() -ErrorClass "DECODE_EMPTY"
        }
        return New-SigrokStructuredResult -Status "SUCCESS" -Operation "SPI_DECODE" -Transactions @($transactions)
    }

    $transactions = [System.Collections.Generic.List[object]]::new()
    foreach ($line in ($Output -split "`r?`n")) {
        $mosiMatch = [regex]::Match($line, '(?i)\bMOSI\s*[:=]\s*(?<value>(?:(?:0x)?[0-9a-f]{1,2})(?:\s+(?:(?:0x)?[0-9a-f]{1,2}))*)')
        $misoMatch = [regex]::Match($line, '(?i)\bMISO\s*[:=]\s*(?<value>(?:(?:0x)?[0-9a-f]{1,2})(?:\s+(?:(?:0x)?[0-9a-f]{1,2}))*)')
        if (-not $mosiMatch.Success -and -not $misoMatch.Success) {
            continue
        }

        $mosi = if ($mosiMatch.Success) { ConvertTo-SigrokByteArray -Value $mosiMatch.Groups["value"].Value } else { [System.Collections.Generic.List[string]]::new() }
        $miso = if ($misoMatch.Success) { ConvertTo-SigrokByteArray -Value $misoMatch.Groups["value"].Value } else { [System.Collections.Generic.List[string]]::new() }
        $transactions.Add([PSCustomObject][ordered]@{
            index = $transactions.Count
            mosi = $mosi
            miso = $miso
            annotation = $line.Trim()
        })
    }

    if ($transactions.Count -eq 0) {
        return New-SigrokStructuredResult -Status "INCONCLUSIVE" -Operation "SPI_DECODE" -Transactions @() -ErrorClass "DECODE_EMPTY"
    }

    return New-SigrokStructuredResult -Status "SUCCESS" -Operation "SPI_DECODE" -Transactions @($transactions)
}

function ConvertFrom-SigrokI2cOutput {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Output
    )

    $events = [System.Collections.Generic.List[object]]::new()
    $directions = [System.Collections.Generic.List[string]]::new()
    $address = $null

    foreach ($line in ($Output -split "`r?`n")) {
        $trimmed = $line.Trim()
        if (($trimmed -match '(?i)\bREPEATED\s+START\b') -or
            ($trimmed -match '(?i)\bSTART\s+REPEAT\b')) {
            $events.Add([PSCustomObject][ordered]@{ type = "REPEATED_START"; raw = $trimmed })
        }
        elseif ($trimmed -match '(?i)\bSTART\b') {
            $events.Add([PSCustomObject][ordered]@{ type = "START"; raw = $trimmed })
        }

        if ($trimmed -match '(?i)\bSTOP\b') {
            $events.Add([PSCustomObject][ordered]@{ type = "STOP"; raw = $trimmed })
        }

        $addressMatch = [regex]::Match($trimmed, '(?i)\bADDRESS(?:\s+(?:READ|WRITE))?\s*[:=]\s*(?:0x)?(?<value>[0-9a-f]{1,2})')
        if ($addressMatch.Success) {
            $address = "0x{0:X2}" -f [Convert]::ToInt32($addressMatch.Groups["value"].Value, 16)
            $events.Add([PSCustomObject][ordered]@{ type = "ADDRESS"; value = $address; raw = $trimmed })
        }

        foreach ($direction in @("READ", "WRITE")) {
            if ($trimmed -match ("(?i)\b{0}\b" -f $direction) -and -not $directions.Contains($direction)) {
                $directions.Add($direction)
            }
        }

        $dataMatch = [regex]::Match($trimmed, '(?i)\bDATA\s*[:=]\s*(?<value>(?:0x)?[0-9a-f]{1,2})')
        if ($dataMatch.Success) {
            $data = ConvertTo-SigrokByteArray -Value $dataMatch.Groups["value"].Value
            $events.Add([PSCustomObject][ordered]@{ type = "DATA"; value = $data[0]; raw = $trimmed })
        }

        if ($trimmed -match '(?i)\bNACK\b') {
            $events.Add([PSCustomObject][ordered]@{ type = "NACK"; raw = $trimmed })
        }
        elseif ($trimmed -match '(?i)\bACK\b') {
            $events.Add([PSCustomObject][ordered]@{ type = "ACK"; raw = $trimmed })
        }
    }

    if ($events.Count -eq 0) {
        return New-SigrokStructuredResult -Status "INCONCLUSIVE" -Operation "I2C_DECODE" -Transactions @() -ErrorClass "DECODE_EMPTY"
    }

    $eventTypes = @($events | ForEach-Object { $_.type } | Select-Object -Unique)
    $transaction = [PSCustomObject][ordered]@{
        index = 0
        address = $address
        directions = @($directions)
        event_types = $eventTypes
        events = @($events)
    }
    return New-SigrokStructuredResult -Status "SUCCESS" -Operation "I2C_DECODE" -Transactions @($transaction)
}

function ConvertFrom-SigrokDecodeOutput {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("spi", "i2c")]
        [string]$Protocol,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Output
    )

    if ($Protocol -eq "spi") {
        return ConvertFrom-SigrokSpiOutput -Output $Output
    }
    return ConvertFrom-SigrokI2cOutput -Output $Output
}
