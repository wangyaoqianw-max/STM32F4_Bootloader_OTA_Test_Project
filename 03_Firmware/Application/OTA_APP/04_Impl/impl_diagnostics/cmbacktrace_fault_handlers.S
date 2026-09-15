;******************************************************************************
;* Copyright (C) 2026 YaoQian Wang                                            *
;*                                                                            *
;* All Rights Reserved.                                                       *
;*                                                                            *
;* @file cmbacktrace_fault_handlers.S                                         *
;* @brief 将可配置 Cortex-M Fault 转交给 CmBacktrace。                       *
;******************************************************************************

    AREA |.text|, CODE, READONLY, ALIGN=2
    THUMB
    REQUIRE8
    PRESERVE8

    IMPORT cm_backtrace_fault
    EXPORT MemManage_Handler
    EXPORT BusFault_Handler
    EXPORT UsageFault_Handler

MemManage_Handler    PROC
    B       cmbacktrace_fault_entry
    ENDP

BusFault_Handler     PROC
    B       cmbacktrace_fault_entry
    ENDP

UsageFault_Handler   PROC
    B       cmbacktrace_fault_entry
    ENDP

cmbacktrace_fault_entry PROC
    MOV     r0, lr
    MOV     r1, sp
    BL      cm_backtrace_fault

Fault_Loop
    B       Fault_Loop
    ENDP

    END
