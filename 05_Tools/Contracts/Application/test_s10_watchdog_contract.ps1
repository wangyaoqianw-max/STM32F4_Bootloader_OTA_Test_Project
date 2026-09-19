$ErrorActionPreference = "Stop"

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw "S10 watchdog contract failed: $Message"
    }
}

function Read-RequiredFile {
    param([string]$Path)

    Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) "Required file is missing: $Path"
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$applicationRoot = Join-Path $repoRoot "03_Firmware\Application\OTA_APP"
$configPath = Join-Path $applicationRoot "00_Config\project_config.h"
$platformHeaderPath = Join-Path $applicationRoot "03_Platform\platform_mcu\watchdog\platform_watchdog.h"
$implPath = Join-Path $applicationRoot "04_Impl\impl_mcu\impl_platform_watchdog.c"
$halConfigPath = Join-Path $applicationRoot "Core\Inc\stm32f4xx_hal_conf.h"
$mainPath = Join-Path $applicationRoot "Core\Src\main.c"
$projectPath = Join-Path $applicationRoot "MDK-ARM\OTA_APP.uvprojx"

$config = Read-RequiredFile $configPath
$platformHeader = Read-RequiredFile $platformHeaderPath
$impl = Read-RequiredFile $implPath
$halConfig = Read-RequiredFile $halConfigPath
$main = Read-RequiredFile $mainPath
$project = Read-RequiredFile $projectPath

Assert-True ($config -match '(?m)^#define\s+PROJECT_WATCHDOG_TIMEOUT_MS\s+\(10000U\)') "watchdog timeout must be configured as 10000 ms"
Assert-True ($platformHeader -match 'platform_error_t\s+platform_watchdog_start\s*\(uint32_t\s+timeoutMs\)') "Platform start API is missing"
Assert-True ($platformHeader -match 'platform_error_t\s+platform_watchdog_feed\s*\(void\)') "Platform feed API is missing"
Assert-True ($platformHeader -notmatch 'stm32f4xx_hal|IWDG_HandleTypeDef') "Platform header must not expose HAL"
Assert-True ($impl -match 'HAL_IWDG_Init\s*\(') "Impl must initialize the HAL IWDG"
Assert-True ($impl -match 'HAL_IWDG_Refresh\s*\(') "Impl must refresh the HAL IWDG"
Assert-True ($impl -match '__HAL_DBGMCU_FREEZE_IWDG\s*\(\)') "Impl must freeze IWDG while the CPU is halted"
Assert-True ($impl -match 'IWDG_PRESCALER_256') "Impl must use the fixed LSI prescaler from the S10 design"
Assert-True ($impl -match 'PLATFORM_ERR_INVALID_PARAM') "Impl must reject timeout values outside its supported range"
Assert-True (($halConfig -match '(?m)^\s*#define\s+HAL_IWDG_MODULE_ENABLED\s*$') -or
             ($project -match '<Define>[^<]*HAL_IWDG_MODULE_ENABLED')) "HAL IWDG module must be enabled"
Assert-True ($project -match 'stm32f4xx_hal_iwdg\.c') "Keil project must compile stm32f4xx_hal_iwdg.c"
Assert-True ($project -match 'impl_platform_watchdog\.c') "Keil project must compile the watchdog Impl"
Assert-True ($project -match '\.\./03_Platform/platform_mcu/watchdog') "Keil project must include the watchdog Platform header path"

$halInitIndex = $main.IndexOf("  HAL_Init();", [System.StringComparison]::Ordinal)
$watchdogStartIndex = $main.IndexOf("platform_watchdog_start(", [System.StringComparison]::Ordinal)
$clockIndex = $main.IndexOf("  SystemClock_Config();", [System.StringComparison]::Ordinal)
Assert-True (($halInitIndex -ge 0) -and ($watchdogStartIndex -gt $halInitIndex) -and ($clockIndex -gt $watchdogStartIndex)) "watchdog must start after HAL_Init and before SystemClock_Config"
Assert-True (([regex]::Matches($main, 'platform_watchdog_feed\s*\(')).Count -ge 2) "startup must contain controlled watchdog checkpoints"

Write-Output "S10 watchdog contract passed."
