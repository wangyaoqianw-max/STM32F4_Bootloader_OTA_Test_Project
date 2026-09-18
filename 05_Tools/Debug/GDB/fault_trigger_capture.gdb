set pagination off
set confirm off
set print pretty on

target remote localhost:2331

echo ===== GDB FAULT CAPTURE =====\n
echo [GDB] Trigger mode: reset and run through the already-open GDB session.\n
break diagnostics_fault_capture_stop
commands
silent
echo \n[FAULT_CONTEXT]\n
printf "FAULT PC = 0x%08x\n", g_diagnostics_fault_context.pc
printf "FAULT LR = 0x%08x\n", g_diagnostics_fault_context.lr
printf "SP = 0x%08x\n", g_diagnostics_fault_context.stackedSp
printf "MSP = 0x%08x\n", g_diagnostics_fault_context.msp
printf "PSP = 0x%08x\n", g_diagnostics_fault_context.psp
printf "EXC_RETURN = 0x%08x\n", g_diagnostics_fault_context.exceptionReturn
printf "xPSR = 0x%08x\n", g_diagnostics_fault_context.xpsr

echo \n[SCB_FAULT_REGISTERS]\n
printf "CFSR = 0x%08x\n", *(unsigned int *)0xE000ED28
printf "HFSR = 0x%08x\n", *(unsigned int *)0xE000ED2C
printf "MMFAR = 0x%08x\n", *(unsigned int *)0xE000ED34
printf "BFAR = 0x%08x\n", *(unsigned int *)0xE000ED38

echo \n[STACKED_REGISTERS]\n
printf "R0 = 0x%08x\n", g_diagnostics_fault_context.r0
printf "R1 = 0x%08x\n", g_diagnostics_fault_context.r1
printf "R2 = 0x%08x\n", g_diagnostics_fault_context.r2
printf "R3 = 0x%08x\n", g_diagnostics_fault_context.r3
printf "R12 = 0x%08x\n", g_diagnostics_fault_context.r12
printf "LR = 0x%08x\n", g_diagnostics_fault_context.lr
printf "PC = 0x%08x\n", g_diagnostics_fault_context.pc
printf "xPSR = 0x%08x\n", g_diagnostics_fault_context.xpsr

echo \n[SOURCE]\n
info line *g_diagnostics_fault_context.pc

echo \n[BACKTRACE]\n
bt

echo \n[STACK]\n
set $fault_sp = g_diagnostics_fault_context.stackedSp
x/16wx $fault_sp

echo \n[EXIT]\n
echo halt-and-detach\n
delete
detach
quit
end
monitor reset
monitor halt
continue
