Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path

$checks = @(
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/contract/app_runtime_contract.h'; Pattern = 'APP_OTA_NOTIFY_CONFIRM'; Name = 'OTA confirm notification' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/system/app_system.h'; Pattern = 'app_system_report_runtime_ready'; Name = 'runtime-ready report API' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/system/app_system.h'; Pattern = 'app_system_take_confirm_result'; Name = 'confirm-result consume API' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/system/app_system.c'; Pattern = 'app_health_initialize'; Name = 'Health bootstrap' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/task/app_main_task.c'; Pattern = 'platform_watchdog_feed'; Name = 'main watchdog feed' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/task/app_main_task.c'; Pattern = 'app_ota_worker_request_confirm'; Name = 'main confirm request' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/task/app_ota_worker.c'; Pattern = 'app_ota_runtime_confirm_trial'; Name = 'worker confirm execution' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/task/app_ota_worker.c'; Pattern = 'APP_HEALTH_READY_OTA'; Name = 'OTA runtime-ready report' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/task/app_display_task.c'; Pattern = 'APP_HEALTH_READY_DISPLAY'; Name = 'display runtime-ready report' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/runtime/app_ota_runtime.h'; Pattern = 'app_ota_runtime_get_trial_status'; Name = 'lifecycle status API' }
)

foreach ($check in $checks) {
    $file = Join-Path $root $check.Path
    if (-not (Test-Path -LiteralPath $file)) {
        throw "S10 runtime health integration contract failed: missing $($check.Path)"
    }

    $content = [System.IO.File]::ReadAllText($file)
    if ($content -notmatch $check.Pattern) {
        throw "S10 runtime health integration contract failed: $($check.Name)"
    }
}

$forbidden = @(
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/system/app_health.c'; Pattern = 'firmware_storage|platform_w25q64|platform_at24c02'; Name = 'Health Raw Storage access' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/task/app_main_task.c'; Pattern = 'firmware_storage|platform_w25q64|platform_at24c02'; Name = 'main Raw Storage access' },
    @{ Path = '03_Firmware/Application/OTA_APP/01_APP/system/app_system.c'; Pattern = 'firmware_storage_t|platform_w25q64_t|platform_at24c02_t'; Name = 'system Raw Storage ownership' }
)

foreach ($check in $forbidden) {
    $file = Join-Path $root $check.Path
    $content = [System.IO.File]::ReadAllText($file)
    if ($content -match $check.Pattern) {
        throw "S10 runtime health integration contract failed: $($check.Name)"
    }
}

Write-Output 'S10 runtime health integration contract passed.'
