$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$appRoot = Join-Path $repoRoot '03_Firmware\Application\OTA_APP'
$vendorRoot = Join-Path $appRoot '05_Vendors\CmBacktrace'
$projectFile = Join-Path $appRoot 'MDK-ARM\OTA_APP.uvprojx'
$mainFile = Join-Path $appRoot 'Core\Src\main.c'
$interruptFile = Join-Path $appRoot 'Core\Src\stm32f4xx_it.c'
$freertosConfigFile = Join-Path $appRoot 'Core\Inc\FreeRTOSConfig.h'
$tasksFile = Join-Path $appRoot 'Middlewares\Third_Party\FreeRTOS\Source\tasks.c'
$vendorReadme = Join-Path $appRoot '05_Vendors\README.md'
$faultAdapterFile = Join-Path $appRoot '04_Impl\impl_diagnostics\cmbacktrace_fault_handlers.S'

$failures = [System.Collections.Generic.List[string]]::new()

function Require-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        $failures.Add("missing file: $Path")
    }
}

function Require-Text([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        $failures.Add("missing contract: $Description")
    }
}

function Forbid-Text([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -match $Pattern) {
        $failures.Add("forbidden legacy code: $Description")
    }
}

Require-File (Join-Path $vendorRoot 'cm_backtrace.c')
Require-File (Join-Path $vendorRoot 'cm_backtrace.h')
Require-File (Join-Path $vendorRoot 'cmb_cfg.h')
Require-File (Join-Path $vendorRoot 'cmb_def.h')
Require-File (Join-Path $vendorRoot 'fault_handler\keil\cmb_fault.S')
Require-File (Join-Path $vendorRoot 'LICENSE')
Require-File (Join-Path $appRoot '00_Config\cmb_user_cfg.h')
Require-File (Join-Path $appRoot '04_Impl\impl_diagnostics\cmbacktrace_port.c')
Require-File (Join-Path $appRoot '04_Impl\impl_diagnostics\cmbacktrace_port.h')
Require-File (Join-Path $appRoot '04_Impl\impl_diagnostics\cmbacktrace_fault_handlers.S')

$project = Get-Content -LiteralPath $projectFile -Raw
$main = Get-Content -LiteralPath $mainFile -Raw
$interrupts = Get-Content -LiteralPath $interruptFile -Raw
$freertosConfig = Get-Content -LiteralPath $freertosConfigFile -Raw
$tasks = Get-Content -LiteralPath $tasksFile -Raw
$readme = Get-Content -LiteralPath $vendorReadme -Raw
$faultAdapters = Get-Content -LiteralPath $faultAdapterFile -Raw

Require-Text $project 'CMB_USER_CFG' 'Keil project defines CMB_USER_CFG'
Require-Text $project '\.\./05_Vendors/CmBacktrace' 'Keil project includes CmBacktrace headers'
Require-Text $project '\.\./05_Vendors/CmBacktrace/cm_backtrace\.c' 'Keil project compiles cm_backtrace.c'
Require-Text $project '\.\./04_Impl/impl_diagnostics/cmbacktrace_port\.c' 'Keil project compiles the project port'
Require-Text $project '\.\./04_Impl/impl_diagnostics/cmbacktrace_fault_handlers\.S' 'Keil project compiles configurable fault adapters'
Require-Text $project '\.\./04_Impl/impl_diagnostics/diagnostics_fault\.c' 'Keil project compiles project fault diagnostics'
Require-Text $main 'cmbacktrace_port_init\s*\(' 'main initializes CmBacktrace'
Require-Text $faultAdapters 'EXPORT\s+HardFault_Handler' 'HardFault uses the project fault adapter'
Require-Text $faultAdapters 'EXPORT\s+(MemManage_Handler|BusFault_Handler|UsageFault_Handler)' 'configurable faults are exported by the assembly adapter'
Forbid-Text $interrupts '(?s)void\s+(HardFault|MemManage|BusFault|UsageFault)_Handler\s*\(' 'legacy C fault handlers must not collide with assembly handlers'
Require-Text $freertosConfig 'configRECORD_STACK_HIGH_ADDRESS\s+1' 'FreeRTOS records the high stack address'
Require-Text $tasks 'uint32_t\s*\*\s*vTaskStackAddr\s*\(' 'FreeRTOS exposes current stack address'
Require-Text $tasks 'uint32_t\s+vTaskStackSize\s*\(' 'FreeRTOS exposes current stack size'
Require-Text $tasks 'char\s*\*\s*vTaskName\s*\(' 'FreeRTOS exposes current task name'
Require-Text $readme 'CmBacktrace' 'vendor README records CmBacktrace integration'
Forbid-Text $project '05_Vendors/CmBacktrace/fault_handler/keil/cmb_fault\.S' 'Keil project has one HardFault owner'

if ($failures.Count -gt 0) {
    Write-Output '[CmBacktrace][FAIL] integration contract is not satisfied.'
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}

Write-Output '[CmBacktrace][PASS] integration contract is satisfied.'
exit 0
