$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$appRoot = Join-Path $repoRoot '03_Firmware\Application\OTA_APP'
$diagnosticsRoot = Join-Path $appRoot '04_Impl\impl_diagnostics'
$configFile = Join-Path $appRoot '00_Config\diagnostics_config.h'
$diagnosticsHeader = Join-Path $diagnosticsRoot 'diagnostics_fault.h'
$diagnosticsSource = Join-Path $diagnosticsRoot 'diagnostics_fault.c'
$triggerAssembly = Join-Path $diagnosticsRoot 'diagnostics_fault_trigger.S'
$faultAdapter = Join-Path $diagnosticsRoot 'cmbacktrace_fault_handlers.S'
$mainFile = Join-Path $appRoot 'Core\Src\main.c'
$appMainFile = Join-Path $appRoot '01_APP\app_main.c'
$projectFile = Join-Path $appRoot 'MDK-ARM\OTA_APP.uvprojx'
$faultGdb = Join-Path $repoRoot '05_Tools\Debug\GDB\fault_capture.gdb'
$faultBat = Join-Path $repoRoot '05_Tools\Scripts\gdb_fault_capture.bat'
$faultPs1 = Join-Path $repoRoot '05_Tools\Scripts\gdb_fault_capture.ps1'

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
        $failures.Add("forbidden contract: $Description")
    }
}

foreach ($path in @(
        $configFile,
        $diagnosticsHeader,
        $diagnosticsSource,
        $triggerAssembly,
        $faultAdapter,
        $faultGdb,
        $faultBat,
        $faultPs1)) {
    Require-File $path
}

if ($failures.Count -eq 0) {
    $config = Get-Content -LiteralPath $configFile -Raw
    $header = Get-Content -LiteralPath $diagnosticsHeader -Raw
    $source = Get-Content -LiteralPath $diagnosticsSource -Raw
    $trigger = Get-Content -LiteralPath $triggerAssembly -Raw
    $adapter = Get-Content -LiteralPath $faultAdapter -Raw
    $main = Get-Content -LiteralPath $mainFile -Raw
    $appMain = Get-Content -LiteralPath $appMainFile -Raw
    $project = Get-Content -LiteralPath $projectFile -Raw
    $gdb = Get-Content -LiteralPath $faultGdb -Raw
    $bat = Get-Content -LiteralPath $faultBat -Raw
    $ps1 = Get-Content -LiteralPath $faultPs1 -Raw

    Require-Text $config 'DIAG_FAULT_TEST_ENABLE\s+\(?0U?\)?' 'Fault test is disabled by default'
    Require-Text $header 'DIAG_FAULT_NONE' 'Fault type NONE exists'
    Require-Text $header 'DIAG_FAULT_INVALID_ADDRESS' 'Invalid address fault type exists'
    Require-Text $header 'DIAG_FAULT_UNDEFINED_INSTRUCTION' 'Undefined instruction fault type exists'
    Require-Text $header 'DIAG_FAULT_DIV_BY_ZERO' 'Divide by zero fault type exists'
    Require-Text $header 'diagnostics_fault_trigger\s*\(' 'Controlled fault trigger API exists'
    Require-Text $header 'diagnostics_fault_handler\s*\(' 'Fault handler adapter API exists'
    Require-Text $source 'SCB->CCR\s*\|=\s*SCB_CCR_DIV_0_TRP_Msk' 'Divide by zero trapping is enabled'
    Require-Text $source 'DIAG_FAULT_INVALID_ADDRESS' 'Invalid address access is controlled'
    Require-Text $source 'diagnostics_fault_trigger_undefined|\.hword' 'Undefined instruction trigger is called'
    Require-Text $trigger 'DIAG_FAULT_UNDEFINED_OPCODE|DCW\s+0xDEAD' 'Undefined instruction encoding exists'
    Require-Text $source 'CFSR|HFSR|MMFAR|BFAR' 'Fault status registers are captured'
    Require-Text $source 'MSP|PSP|EXC_RETURN' 'Main and process stack context is captured'
    Require-Text $source 'R0|R1|R2|R3|R12|PC|xPSR' 'Stacked core registers are captured'
    Require-Text $source 'cm_backtrace_fault\s*\(' 'CmBacktrace is called after project capture'
    Require-Text $adapter 'EXPORT\s+HardFault_Handler' 'HardFault uses project adapter'
    Require-Text $adapter 'EXPORT\s+MemManage_Handler' 'MemManage uses project adapter'
    Require-Text $adapter 'EXPORT\s+BusFault_Handler' 'BusFault uses project adapter'
    Require-Text $adapter 'EXPORT\s+UsageFault_Handler' 'UsageFault uses project adapter'
    Require-Text $adapter 'diagnostics_fault_handler' 'Fault adapter calls project diagnostics'
    Require-Text $main 'diagnostics_fault_init\s*\(' 'Main initializes fault diagnostics'
    Require-Text $appMain 'DIAG_FAULT_TEST_ENABLE' 'Application test trigger is compile-time gated'
    Require-Text $appMain 'diagnostics_fault_trigger\s*\(' 'Application invokes controlled fault trigger'
    Require-Text $project 'diagnostics_fault\.c' 'Keil project compiles diagnostics source'
    Require-Text $project 'cmbacktrace_fault_handlers\.S' 'Keil project compiles unified fault adapter'
    Forbid-Text $project '05_Vendors/CmBacktrace/fault_handler/keil/cmb_fault\.S' 'Keil project has one HardFault owner'

    Require-Text $gdb '\[FAULT_CONTEXT\]' 'GDB fault marker exists'
    foreach ($field in @('FAULT PC', 'FAULT LR', 'SP', 'MSP', 'PSP', 'xPSR', 'CFSR', 'HFSR', 'MMFAR', 'BFAR', 'SOURCE', 'BACKTRACE', 'STACK')) {
        Require-Text $gdb ([regex]::Escape($field)) "GDB captures $field"
    }
    Require-Text $gdb '(?im)^\s*detach\s*$' 'Fault capture detaches without resuming'
    Forbid-Text $gdb 'continue&' 'Fault capture does not resume the MCU'
    Forbid-Text $gdb '(?im)^\s*load(?:\s|$)' 'Fault capture does not program Flash'
    Require-Text $bat 'gdb_fault_capture\.ps1' 'Fault BAT calls PowerShell wrapper'
    Require-Text $bat 'OTA_APP_fault_gdb\.log' 'Fault GDB log path is declared'
    Require-Text $bat 'OTA_APP_fault_rtt\.log' 'Fault RTT log path is declared'
    Require-Text $ps1 'ServerLog|GdbLog|RttLog' 'Fault wrapper receives diagnostic log paths'
    Require-Text $ps1 'Stop-OwnedProcess' 'Fault wrapper owns process cleanup'
    Require-Text $ps1 'Error in sourced command file|Cannot execute this command' 'Fault wrapper rejects GDB command execution errors'
    Forbid-Text $ps1 'taskkill\s+/IM|Stop-Process\s+-Name' 'Fault wrapper does not globally kill processes'
}

if ($failures.Count -gt 0) {
    Write-Output '[FaultDiag][FAIL] fault diagnostic contract is not satisfied.'
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}

Write-Output '[FaultDiag][PASS] fault diagnostic contract is satisfied.'
exit 0
