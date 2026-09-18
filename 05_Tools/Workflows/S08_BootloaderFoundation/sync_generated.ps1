param(
    [Parameter(Mandatory = $true)]
    [string]$ToolsRoot
)

$ErrorActionPreference = "Stop"

function Get-ProjectText {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $hasUtf8Bom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    $offset = if ($hasUtf8Bom) { 3 } else { 0 }
    $text = [System.Text.Encoding]::UTF8.GetString($bytes, $offset, $bytes.Length - $offset)
    return [PSCustomObject]@{
        Text = $text
        Utf8Bom = $hasUtf8Bom
    }
}

function Set-ProjectText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][bool]$Utf8Bom
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    $content = $encoding.GetBytes($Text)
    if ($Utf8Bom) {
        $bom = [byte[]](0xEF, 0xBB, 0xBF)
        $content = $bom + $content
    }
    [System.IO.File]::WriteAllBytes($Path, $content)
}

function Replace-ProjectText {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][scriptblock]$Replacement,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $regex = New-Object System.Text.RegularExpressions.Regex($Pattern)
    $match = $regex.Match($Text)
    if (-not $match.Success) {
        throw "S08 sync target not found: $Description"
    }
    $evaluator = [System.Text.RegularExpressions.MatchEvaluator] {
        param($match)
        return (& $Replacement $match)
    }
    return $regex.Replace($Text, $evaluator, 1)
}

function Set-ProjectMemoryRegion {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$NodeName,
        [Parameter(Mandatory = $true)][string]$StartAddress,
        [Parameter(Mandatory = $true)][string]$Size
    )

    $pattern = "(?s)(<$NodeName>\s*<Type>[^<]+</Type>\s*<StartAddress>)[^<]+(</StartAddress>\s*<Size>)[^<]+(</Size>\s*</$NodeName>)"
    return Replace-ProjectText -Text $Text -Pattern $pattern -Description "$NodeName memory region" -Replacement {
        param($match)
        return $match.Groups[1].Value + $StartAddress + $match.Groups[2].Value + $Size + $match.Groups[3].Value
    }
}

function Set-ProjectCpu {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$IromRange
    )

    $pattern = '(?m)(^\s*<Cpu>)[^<]*(</Cpu>)'
    return Replace-ProjectText -Text $Text -Pattern $pattern -Description "Keil CPU memory string" -Replacement {
        param($match)
        return $match.Groups[1].Value + ('IRAM(0x20000000-0x2001FFFF) IROM(' + $IromRange + ') CLOCK(25000000) FPU2 CPUTYPE("Cortex-M4") TZ') + $match.Groups[2].Value
    }
}

function Set-ProjectPathValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Element,
        [Parameter(Mandatory = $true)][string]$Value
    )

    $pattern = "(?m)(^\s*<$Element>)[^<]*(</$Element>)"
    return Replace-ProjectText -Text $Text -Pattern $pattern -Description $Element -Replacement {
        param($match)
        return $match.Groups[1].Value + $Value + $match.Groups[2].Value
    }
}

function Ensure-ProjectIncludePath {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string[]]$RequiredPaths
    )

    $pattern = '(?s)(<IncludePath>)(.*?)(</IncludePath>)'
    $regex = New-Object System.Text.RegularExpressions.Regex($pattern)
    $matches = $regex.Matches($Text)
    $target = $matches | Where-Object { $_.Groups[2].Value -match '\.\./Core/Inc' } | Select-Object -First 1
    if ($null -eq $target) {
        throw "S08 sync target not found: compiler IncludePath"
    }

    $paths = @($target.Groups[2].Value -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    foreach ($requiredPath in $RequiredPaths) {
        if ($paths -notcontains $requiredPath) {
            $paths += $requiredPath
        }
    }
    $newValue = $paths -join ';'
    $replacement = $target.Groups[1].Value + $newValue + $target.Groups[3].Value
    return $Text.Substring(0, $target.Index) + $replacement + $Text.Substring($target.Index + $target.Length)
}

function Ensure-ProjectDefine {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string[]]$RequiredDefines
    )

    $pattern = '(?s)(<Define>)(.*?)(</Define>)'
    $regex = New-Object System.Text.RegularExpressions.Regex($pattern)
    $matches = $regex.Matches($Text)
    $target = $matches | Select-Object -First 1
    if ($null -eq $target) {
        throw "S08 sync target not found: compiler Define"
    }

    $defines = @($target.Groups[2].Value -split ',' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    foreach ($requiredDefine in $RequiredDefines) {
        if ($defines -notcontains $requiredDefine) {
            $defines += $requiredDefine
        }
    }
    $newValue = $defines -join ','
    $replacement = $target.Groups[1].Value + $newValue + $target.Groups[3].Value
    return $Text.Substring(0, $target.Index) + $replacement + $Text.Substring($target.Index + $target.Length)
}

function Remove-StaleRteBlock {
    param([Parameter(Mandatory = $true)][string]$Text)

    $pattern = '(?s)\r?\n\s*<RTE>.*?</RTE>\s*(?=</Project>)'
    return [regex]::Replace($Text, $pattern, "`r`n")
}

function Ensure-BootProjectStructure {
    param([Parameter(Mandatory = $true)][string]$Text)

    $text = [regex]::Replace(
        $Text,
        '(?s)\s*<File>\s*<FileName>stm32f4xx_it\.c</FileName>.*?</File>',
        '',
        1)

    if ($text -match '<GroupName>Boot</GroupName>') {
        return $text
    }

    $lineEnding = if ($text -match "`r`n") { "`r`n" } else { "`n" }
    $bootGroups = @'
        <Group>
          <GroupName>Boot</GroupName>
          <Files>
            <File>
              <FileName>boot_main.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Boot/boot_main.c</FilePath>
            </File>
            <File>
              <FileName>boot_validate.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Boot/boot_validate.c</FilePath>
            </File>
            <File>
              <FileName>boot_jump.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Boot/boot_jump.c</FilePath>
            </File>
            <File>
              <FileName>boot_jump_asm.S</FileName>
              <FileType>2</FileType>
              <FilePath>../Boot/boot_jump_asm.S</FilePath>
            </File>
          </Files>
        </Group>
        <Group>
          <GroupName>Diagnostics</GroupName>
          <Files>
            <File>
              <FileName>boot_log.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Diagnostics/boot_log.c</FilePath>
            </File>
            <File>
              <FileName>cmbacktrace_port.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Diagnostics/cmbacktrace_port.c</FilePath>
            </File>
            <File>
              <FileName>diagnostics_fault.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Diagnostics/diagnostics_fault.c</FilePath>
            </File>
            <File>
              <FileName>diagnostics_fault_trigger.S</FileName>
              <FileType>2</FileType>
              <FilePath>../Diagnostics/diagnostics_fault_trigger.S</FilePath>
            </File>
            <File>
              <FileName>cmbacktrace_fault_handlers.S</FileName>
              <FileType>2</FileType>
              <FilePath>../Diagnostics/cmbacktrace_fault_handlers.S</FilePath>
            </File>
          </Files>
        </Group>
        <Group>
          <GroupName>Vendor/RTT</GroupName>
          <Files>
            <File>
              <FileName>SEGGER_RTT.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Vendor/RTT/SEGGER_RTT.c</FilePath>
            </File>
            <File>
              <FileName>SEGGER_RTT_printf.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Vendor/RTT/SEGGER_RTT_printf.c</FilePath>
            </File>
          </Files>
        </Group>
        <Group>
          <GroupName>Vendor/CmBacktrace</GroupName>
          <Files>
            <File>
              <FileName>cm_backtrace.c</FileName>
              <FileType>1</FileType>
              <FilePath>../Vendor/CmBacktrace/cm_backtrace.c</FilePath>
            </File>
          </Files>
        </Group>
'@
    $bootGroups = $bootGroups -replace "`r?`n", $lineEnding
    return [regex]::Replace($text, "(?m)^\s*</Groups>", ($bootGroups + $lineEnding + '      </Groups>'), 1)
}

function Sync-ApplicationSystemFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $content = Get-ProjectText -Path $Path
    $text = $content.Text
    $directInclude = '(?m)^\s*#include "\.\./\.\./\.\./\.\./Shared/memory_layout\.h"\s*$'
    if (-not [regex]::IsMatch($text, $directInclude)) {
        $text = Replace-ProjectText -Text $text -Pattern '(?m)^(\s*#include "stm32f4xx\.h"\s*\r?\n)' -Description "Application memory contract include" -Replacement {
            param($match)
            return $match.Groups[1].Value + '#include "../../../../Shared/memory_layout.h"' + [Environment]::NewLine
        }
    }

    $commentedUserVector = '(?m)^\s*/\*\s*#define USER_VECT_TAB_ADDRESS\s*\*/\s*$'
    if ([regex]::IsMatch($text, $commentedUserVector)) {
        $text = [regex]::Replace($text, $commentedUserVector, '#define USER_VECT_TAB_ADDRESS', 1)
    } elseif (-not [regex]::IsMatch($text, '(?m)^\s*#define USER_VECT_TAB_ADDRESS\s*$')) {
        throw "S08 sync target not found: USER_VECT_TAB_ADDRESS"
    }

    $flashOffset = '(?s)(#define\s+VECT_TAB_BASE_ADDRESS\s+FLASH_BASE.*?#define\s+VECT_TAB_OFFSET\s+)(?:0x[0-9A-Fa-f]+U|APP_BASE_ADDR)([^\r\n]*)'
    $text = Replace-ProjectText -Text $text -Pattern $flashOffset -Description "Application FLASH VTOR offset" -Replacement {
        param($match)
        return $match.Groups[1].Value + 'APP_BASE_ADDR' + $match.Groups[2].Value
    }
    Set-ProjectText -Path $Path -Text $text -Utf8Bom $content.Utf8Bom
}

function Sync-KeilProject {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$IromStart,
        [Parameter(Mandatory = $true)][string]$IromEnd,
        [Parameter(Mandatory = $true)][string[]]$IncludePaths,
        [Parameter(Mandatory = $true)][bool]$Application
    )

    $content = Get-ProjectText -Path $Path
    $text = $content.Text
    $text = Set-ProjectCpu -Text $text -IromRange "$IromStart-$IromEnd"
    $iromSize = if ($Application) { '0x70000' } else { '0x10000' }
    $text = Set-ProjectMemoryRegion -Text $text -NodeName "IROM" -StartAddress $IromStart -Size $iromSize
    $text = Set-ProjectMemoryRegion -Text $text -NodeName "OCR_RVCT4" -StartAddress $IromStart -Size $iromSize
    $text = Set-ProjectMemoryRegion -Text $text -NodeName "IRAM" -StartAddress '0x20000000' -Size '0x20000'
    $text = Set-ProjectPathValue -Text $text -Element "OutputDirectory" -Value '.\Objects\'
    $text = Set-ProjectPathValue -Text $text -Element "ListingPath" -Value '.\Listings\'
    $text = Ensure-ProjectIncludePath -Text $text -RequiredPaths $IncludePaths
    $text = Ensure-ProjectDefine -Text $text -RequiredDefines @('CMB_USER_CFG')
    if (-not $Application) {
        $text = Ensure-BootProjectStructure -Text $text
    }
    $text = Remove-StaleRteBlock -Text $text
    Set-ProjectText -Path $Path -Text $text -Utf8Bom $content.Utf8Bom
}

$resolvedToolsRoot = (Resolve-Path -LiteralPath $ToolsRoot -ErrorAction Stop).Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $resolvedToolsRoot '..') -ErrorAction Stop).Path
$appProject = Join-Path $projectRoot '03_Firmware\Application\OTA_APP\MDK-ARM\OTA_APP.uvprojx'
$bootProject = Join-Path $projectRoot '03_Firmware\Bootloader\OTA_Bootloader\MDK-ARM\OTA_Bootloader.uvprojx'
$appSystem = Join-Path $projectRoot '03_Firmware\Application\OTA_APP\Core\Src\system_stm32f4xx.c'

foreach ($path in @($appProject, $bootProject, $appSystem)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "S08 sync input not found: $path"
    }
}

Sync-KeilProject -Path $appProject -IromStart '0x8010000' -IromEnd '0x807FFFF' -IncludePaths @('../05_Vendors/RTT', '../05_Vendors/CmBacktrace') -Application $true
Sync-KeilProject -Path $bootProject -IromStart '0x8000000' -IromEnd '0x80FFFF' -IncludePaths @('../Boot', '../Config', '../Diagnostics', '../Vendor', '../Vendor/RTT', '../Vendor/CmBacktrace') -Application $false
Sync-ApplicationSystemFile -Path $appSystem

$appCheck = Get-ProjectText -Path $appProject
if ($appCheck.Text -notmatch '<OCR_RVCT4>\s*<Type>1</Type>\s*<StartAddress>0x8010000</StartAddress>\s*<Size>0x70000</Size>') {
    throw 'S08 sync verification failed: Application OCR_RVCT4 is not 0x8010000 / 0x70000'
}
if ($appCheck.Text -notmatch '<IROM>\s*<Type>1</Type>\s*<StartAddress>0x8010000</StartAddress>\s*<Size>0x70000</Size>') {
    throw 'S08 sync verification failed: Application IROM is not 0x8010000 / 0x70000'
}
if ($appCheck.Text -match '<RTE>') {
    throw 'S08 sync verification failed: stale Application RTE component remains'
}

Write-Host '[S08_SYNC][PASS] CubeMX-generated project files synchronized.'
Write-Host '[S08_SYNC][PASS] Application IROM/OCR_RVCT4 = 0x08010000 / 448 KiB.'
Write-Host '[S08_SYNC][PASS] Application VTOR source uses APP_BASE_ADDR.'
