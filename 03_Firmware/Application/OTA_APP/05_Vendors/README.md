# Application Vendor Components

## CmBacktrace

- Source: [armink/CmBacktrace](https://github.com/armink/CmBacktrace)
- Upstream library version: `1.5.0` (`CMB_SW_VERSION`)
- Purpose: Cortex-M fault diagnosis, register capture and call-stack backtrace
- Project target: STM32F411CE, Cortex-M4, Keil/ARMCC, FreeRTOS
- Output: minimal fault diagnostics through SEGGER RTT

The upstream CmBacktrace source under `CmBacktrace/` is kept in its original
directory and format. Project-specific configuration is in
`00_Config/cmb_user_cfg.h`. The project-owned fault adapter is in
`04_Impl/impl_diagnostics/cmbacktrace_fault_handlers.S`.

FreeRTOS `tasks.c` contains a small, guarded compatibility patch exposing the
current task stack address, stack size and task name required by CmBacktrace.
`configRECORD_STACK_HIGH_ADDRESS` is enabled in `FreeRTOSConfig.h` so the
stack size can be derived without changing the TCB layout.

The CubeMX-generated `Core/Src/stm32f4xx_it.c` Fault handler bodies are removed
only to avoid duplicate vector ownership; the project-owned
`04_Impl/impl_diagnostics/cmbacktrace_fault_handlers.S` provides the four
Cortex-M Fault entry points. If the generated file is regenerated, preserve
this ownership arrangement.
