/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.c
 * @brief Application 系统任务启动实现。
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_system.h"

#define LOG_TAG "app_system"

#include "app_display_task.h"
#include "app_main.h"
#include "app_ota_worker.h"
#include "platform_os.h"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define APP_SYSTEM_TASK_STACK_SIZE    (4096U)
//******************************** Defines *********************************//

//******************************** Private Functions *************************//
static void app_system_task(void *argument);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_bool_t g_appSystemStarted = 0U;
static platform_thread_t g_appSystemThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t g_displayQueue = PLATFORM_OS_OBJECT_INITIALIZER;

static const platform_thread_config_t s_app_system_thread_config = {
    .name = "appSystem",
    .entry = app_system_task,
    .argument = (void *)0,
    .stackSizeBytes = APP_SYSTEM_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
/**
 * @brief Application 系统任务入口。
 * @param[in] argument 任务参数，当前未使用。
 */
static void app_system_task(void *argument)
{
    platform_error_t result;

    (void)argument;

    result = app_display_task_start(&g_displayQueue);
    SERVICE_LOG_I("displayTask start result: %d", result);

    result = app_ota_worker_start(&g_displayQueue);
    SERVICE_LOG_I("otaWorker start result: %d", result);

    app_main();
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_system_start(void)
{
    platform_error_t result;

    if (g_appSystemStarted != 0U) {
        return PLATFORM_ERR_OK;
    }

    result = platform_thread_create(&g_appSystemThread,
                                   &s_app_system_thread_config);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_appSystemStarted = 1U;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
