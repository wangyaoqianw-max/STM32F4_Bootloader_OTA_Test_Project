;******************************************************************************
;* Copyright (C) 2026 YaoQian Wang                                            *
;*                                                                            *
;* All Rights Reserved.                                                       *
;*                                                                            *
;* @file boot_jump_asm.S                                                      *
;* @brief 设置 Application MSP 并立即跳转到 Reset_Handler。                   *
;* @author YaoQian Wang                                                       *
;* @date 2026-09-18                                                          *
;* @version V1.0                                                             *
;******************************************************************************

        AREA    |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXPORT  boot_jump_set_msp_and_branch [CODE]

boot_jump_set_msp_and_branch PROC
        MSR     MSP, r0
        BX      r1
        ENDP

        END
