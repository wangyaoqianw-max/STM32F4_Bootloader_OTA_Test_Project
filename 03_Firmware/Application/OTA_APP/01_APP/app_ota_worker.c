/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ota_worker.c
 * @brief S06 otaWorker 创建与通知阻塞骨架实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_ota_worker.h"

#define LOG_TAG "ota_worker"

#include "platform_time.h"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_OTA_WORKER_TASK_STACK_SIZE    (4096U)
#define APP_OTA_WORKER_NOTIFY_FLAGS       \
    (APP_OTA_NOTIFY_UART_RX | APP_OTA_NOTIFY_START | \
     APP_OTA_NOTIFY_CANCEL | APP_OTA_NOTIFY_SHUTDOWN)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static void app_ota_worker_entry(void *argument);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_bool_t g_otaWorkerStarted = PLATFORM_FALSE;
static platform_thread_t g_otaWorkerThread = PLATFORM_OS_OBJECT_INITIALIZER;

static const platform_thread_config_t s_ota_worker_config = {
    .name = "otaWorker",
    .entry = app_ota_worker_entry,
    .argument = (void *)0,
    .stackSizeBytes = APP_OTA_WORKER_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void app_ota_worker_entry(void *argument)
{
    uint32_t receivedFlags = 0U;
    platform_error_t result;

    (void)argument;

    for (;;) {
        result = platform_notify_wait(APP_OTA_WORKER_NOTIFY_FLAGS,
                                       PLATFORM_FALSE,
                                       PLATFORM_TRUE,
                                       PLATFORM_OS_WAIT_FOREVER,
                                       &receivedFlags);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("OTA notify wait failed: %d", result);
            (void)platform_time_delay_ms(1000U);
        }
    }
}
//******************************** Private Functions *************************//

//******************************** Functions ********************************//
platform_error_t app_ota_worker_start(platform_queue_t *displayQueue)
{
    platform_error_t result;

    if (displayQueue == (platform_queue_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_otaWorkerStarted == PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    result = platform_thread_create(&g_otaWorkerThread,
                                    &s_ota_worker_config);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_otaWorkerStarted = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
