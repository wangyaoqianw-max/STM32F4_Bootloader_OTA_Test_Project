/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07_worker_contract_host_test.c
 * @brief S07 otaWorker Display/Reset Contract Host Test
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>

#include "app_runtime_contract.h"
//******************************** Includes *********************************//

//******************************** Private Functions ************************//
static int test_worker_contract(void)
{
    if ((APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL >= APP_DISPLAY_EVENT_MAX) ||
        (APP_DISPLAY_EVENT_OTA_RESET_REQUIRED >= APP_DISPLAY_EVENT_MAX)) {
        return 1;
    }

    return (APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL !=
            APP_DISPLAY_EVENT_OTA_RESET_REQUIRED) ? 0 : 1;
}
//******************************** Private Functions ************************//

//******************************** Public Functions *************************//
int main(void)
{
    if (test_worker_contract() != 0) {
        (void)printf("S07 Worker contract host test failed.\n");
        return 1;
    }

    (void)printf("S07 Worker contract host test passed.\n");
    return 0;
}
//******************************** Public Functions *************************//
