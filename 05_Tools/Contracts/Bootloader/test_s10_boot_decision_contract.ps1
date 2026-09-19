Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw "S10 Boot decision contract failed: $Message"
    }
}

function Get-RequiredIndex {
    param(
        [string]$Content,
        [string]$Needle,
        [string]$Name
    )

    $index = $Content.IndexOf($Needle, [System.StringComparison]::Ordinal)
    Assert-True ($index -ge 0) "$Name is missing"
    return $index
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$bootMainPath = Join-Path $repoRoot '03_Firmware\Bootloader\OTA_Bootloader\Boot\boot_main.c'
Assert-True (Test-Path -LiteralPath $bootMainPath -PathType Leaf) 'boot_main.c is missing'
$bootMain = [System.IO.File]::ReadAllText($bootMainPath)

foreach ($resetFlag in @('RCC_FLAG_BORRST', 'RCC_FLAG_PORRST', 'RCC_FLAG_PINRST',
                         'RCC_FLAG_SFTRST', 'RCC_FLAG_IWDGRST')) {
    Get-RequiredIndex $bootMain $resetFlag "Reset cause $resetFlag" | Out-Null
}
Get-RequiredIndex $bootMain '__HAL_RCC_GET_FLAG' 'Reset cause snapshot' | Out-Null
Get-RequiredIndex $bootMain '__HAL_RCC_CLEAR_RESET_FLAGS' 'Reset cause clear' | Out-Null

$pendingIndex = Get-RequiredIndex $bootMain `
    'if (metadata.upgradeState == BOOT_UPGRADE_STATE_PENDING)' 'PENDING decision branch'
$trialIndex = Get-RequiredIndex $bootMain `
    'else if (metadata.upgradeState == BOOT_UPGRADE_STATE_TRIAL)' 'TRIAL decision branch'
$rollbackIndex = Get-RequiredIndex $bootMain `
    'else if (metadata.upgradeState == BOOT_UPGRADE_STATE_ROLLBACK)' 'ROLLBACK decision branch'
Assert-True (($pendingIndex -lt $trialIndex) -and ($trialIndex -lt $rollbackIndex)) `
    'decision branches must follow PENDING, TRIAL, ROLLBACK order'

$trialBody = $bootMain.Substring($trialIndex, $rollbackIndex - $trialIndex)
$rollbackBody = $bootMain.Substring($rollbackIndex)
$trialPrevalidateIndex = Get-RequiredIndex $trialBody `
    'boot_prevalidate_confirmed' 'TRIAL confirmed prevalidation'
$trialBeginIndex = Get-RequiredIndex $trialBody `
    'boot_metadata_commit_rollback_begin' 'TRIAL rollback_begin'
$trialRestoreIndex = Get-RequiredIndex $trialBody `
    'boot_installer_restore_confirmed' 'TRIAL confirmed restore'
$trialCompleteIndex = Get-RequiredIndex $trialBody `
    'boot_metadata_commit_rollback_complete' 'TRIAL rollback_complete'
Assert-True (($trialPrevalidateIndex -lt $trialBeginIndex) -and
             ($trialBeginIndex -lt $trialRestoreIndex) -and
             ($trialRestoreIndex -lt $trialCompleteIndex)) `
    'TRIAL must validate, persist ROLLBACK, restore, then complete'

Get-RequiredIndex $rollbackBody 'boot_prevalidate_confirmed' 'ROLLBACK confirmed prevalidation' | Out-Null
Get-RequiredIndex $rollbackBody 'boot_installer_restore_confirmed' 'ROLLBACK confirmed restore' | Out-Null
Get-RequiredIndex $rollbackBody 'boot_metadata_commit_rollback_complete' 'ROLLBACK completion' | Out-Null
Assert-True ($rollbackBody -notmatch 'boot_metadata_commit_rollback_begin') `
    'ROLLBACK restart path must not begin a second rollback transaction'

$decisionLines = [regex]::Matches($bootMain, '(?m)^\s*(if|else if) \(metadata\.upgradeState[^\n]*\)')
foreach ($line in $decisionLines) {
    Assert-True ($line.Value -notmatch 'resetCause') `
        'reset cause must not participate in upgrade-state decisions'
}

Write-Output 'S10 Boot decision contract passed.'
