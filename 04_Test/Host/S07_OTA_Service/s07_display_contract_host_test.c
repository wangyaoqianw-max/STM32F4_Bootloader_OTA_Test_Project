/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07_display_contract_host_test.c
 * @brief S07 Display Event Contract Host Test
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>

#include "contract/app_runtime_contract.h"
//******************************** Includes *********************************//

//******************************** Private Functions ************************//
static int test_display_event_contract(void)
{
    app_display_event_t event = {
        .type = APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL,
        .targetSlot = APP_DISPLAY_TARGET_SLOT_B,
        .progress = 100U,
        .imageSize = 1024U,
        .errorCode = PLATFORM_ERR_OK
    };

    if ((APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL >= APP_DISPLAY_EVENT_MAX) ||
        (APP_DISPLAY_EVENT_OTA_RESET_REQUIRED >= APP_DISPLAY_EVENT_MAX)) {
        return 1;
    }
    if (APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL ==
        APP_DISPLAY_EVENT_OTA_RESET_REQUIRED) {
        return 1;
    }
    if ((APP_DISPLAY_TARGET_SLOT_A >= APP_DISPLAY_TARGET_SLOT_MAX) ||
        (APP_DISPLAY_TARGET_SLOT_B >= APP_DISPLAY_TARGET_SLOT_MAX)) {
        return 1;
    }
    if ((event.type != APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL) ||
        (event.targetSlot != APP_DISPLAY_TARGET_SLOT_B) ||
        (event.progress != 100U) ||
        (event.errorCode != PLATFORM_ERR_OK)) {
        return 1;
    }

    return 0;
}
//******************************** Private Functions ************************//

//******************************** Public Functions *************************//
int main(void)
{
    if (test_display_event_contract() != 0) {
        (void)printf("S07 Display contract host test failed.\n");
        return 1;
    }

    (void)printf("S07 Display contract host test passed.\n");
    return 0;
}
//******************************** Public Functions *************************//
