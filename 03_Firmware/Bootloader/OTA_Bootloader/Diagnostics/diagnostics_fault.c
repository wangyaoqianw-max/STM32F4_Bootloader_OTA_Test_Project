/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file diagnostics_fault.c
 * @brief Bootloader Cortex-M Fault 诊断实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#include "diagnostics_fault.h"

//******************************** Includes *********************************//
#include "SEGGER_RTT.h"
#include "cm_backtrace.h"
#include "stm32f4xx.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define DIAG_FAULT_INVALID_ADDRESS_PTR  \
    ((volatile uint32_t *)DIAG_FAULT_INVALID_ADDRESS_VALUE)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
volatile diagnostics_fault_context_t g_diagnostics_fault_context = {0};
//******************************** Variables ********************************//

//******************************** Functions ********************************//
#if (DIAG_FAULT_TEST_ENABLE != 0U)
extern void diagnostics_fault_trigger_undefined(void);
#endif

static void diagnostics_fault_print_context(void)
{
    (void)SEGGER_RTT_WriteString(0U, "\r\n[DIAG_FAULT_CONTEXT]\r\n");
    (void)SEGGER_RTT_printf(0U, "EXC_RETURN = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.exceptionReturn);
    (void)SEGGER_RTT_printf(0U, "STACKED_SP = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.stackedSp);
    (void)SEGGER_RTT_printf(0U, "MSP = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.msp);
    (void)SEGGER_RTT_printf(0U, "PSP = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.psp);
    (void)SEGGER_RTT_printf(0U, "CFSR = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.cfsr);
    (void)SEGGER_RTT_printf(0U, "HFSR = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.hfsr);
    (void)SEGGER_RTT_printf(0U, "MMFAR = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.mmfar);
    (void)SEGGER_RTT_printf(0U, "BFAR = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.bfar);
    (void)SEGGER_RTT_printf(0U, "R0 = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.r0);
    (void)SEGGER_RTT_printf(0U, "R1 = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.r1);
    (void)SEGGER_RTT_printf(0U, "R2 = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.r2);
    (void)SEGGER_RTT_printf(0U, "R3 = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.r3);
    (void)SEGGER_RTT_printf(0U, "R12 = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.r12);
    (void)SEGGER_RTT_printf(0U, "LR = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.lr);
    (void)SEGGER_RTT_printf(0U, "FAULT PC = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.pc);
    (void)SEGGER_RTT_printf(0U, "xPSR = 0x%08lx\r\n",
                            (unsigned long)g_diagnostics_fault_context.xpsr);
}

void diagnostics_fault_init(void)
{
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk |
                  SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk;

#if (DIAG_FAULT_TEST_ENABLE != 0U)
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
#endif
}

#if (DIAG_FAULT_TEST_ENABLE != 0U)
static void diagnostics_fault_trigger_level_c(diagnostics_fault_type_t type)
{
    switch (type) {
        case DIAG_FAULT_INVALID_ADDRESS: {
            volatile uint32_t *invalidAddress = DIAG_FAULT_INVALID_ADDRESS_PTR;
            *invalidAddress = DIAG_FAULT_WRITE_PATTERN;
            break;
        }

        case DIAG_FAULT_UNDEFINED_INSTRUCTION:
            diagnostics_fault_trigger_undefined();
            break;

        case DIAG_FAULT_DIV_BY_ZERO: {
            volatile uint32_t numerator = 1U;
            volatile uint32_t denominator = 0U;
            volatile uint32_t *denominatorAddress = &denominator;
            volatile uint32_t result = numerator / *denominatorAddress;
            (void)result;
            break;
        }

        case DIAG_FAULT_NONE:
        default:
            break;
    }
}

static void diagnostics_fault_trigger_level_b(diagnostics_fault_type_t type)
{
    diagnostics_fault_trigger_level_c(type);
}

static void diagnostics_fault_trigger_level_a(diagnostics_fault_type_t type)
{
    diagnostics_fault_trigger_level_b(type);
}
#endif

void diagnostics_fault_trigger(diagnostics_fault_type_t type)
{
#if (DIAG_FAULT_TEST_ENABLE != 0U)
    diagnostics_fault_trigger_level_a(type);
#else
    (void)type;
#endif
}

void diagnostics_fault_handler(uint32_t exceptionReturn, uint32_t stackedSp)
{
    const uint32_t *stackedRegisters = (const uint32_t *)stackedSp;

    g_diagnostics_fault_context.exceptionReturn = exceptionReturn;
    g_diagnostics_fault_context.stackedSp = stackedSp;
    g_diagnostics_fault_context.msp = __get_MSP();
    g_diagnostics_fault_context.psp = __get_PSP();
    g_diagnostics_fault_context.cfsr = SCB->CFSR;
    g_diagnostics_fault_context.hfsr = SCB->HFSR;
    g_diagnostics_fault_context.mmfar = SCB->MMFAR;
    g_diagnostics_fault_context.bfar = SCB->BFAR;
    g_diagnostics_fault_context.r0 = stackedRegisters[0];
    g_diagnostics_fault_context.r1 = stackedRegisters[1];
    g_diagnostics_fault_context.r2 = stackedRegisters[2];
    g_diagnostics_fault_context.r3 = stackedRegisters[3];
    g_diagnostics_fault_context.r12 = stackedRegisters[4];
    g_diagnostics_fault_context.lr = stackedRegisters[5];
    g_diagnostics_fault_context.pc = stackedRegisters[6];
    g_diagnostics_fault_context.xpsr = stackedRegisters[7];

    diagnostics_fault_print_context();
    cm_backtrace_fault(exceptionReturn, stackedSp);
}
//******************************** Functions ********************************//
