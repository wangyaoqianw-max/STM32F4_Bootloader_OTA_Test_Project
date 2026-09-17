/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display_task.c
 * @brief S06 displayTask 创建与阻塞骨架实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_display_task.h"

#define LOG_TAG "display_task"

#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_DISPLAY_TASK_STACK_SIZE    (4096U)
#define APP_DISPLAY_QUEUE_LENGTH       (8U)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static void app_display_task_entry(void *argument);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_bool_t g_displayTaskStarted = PLATFORM_FALSE;
static platform_thread_t g_displayTaskThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t *g_displayQueue = (platform_queue_t *)0;

static const platform_thread_config_t s_display_task_config = {
    .name = "displayTask",
    .entry = app_display_task_entry,
    .argument = (void *)0,
    .stackSizeBytes = APP_DISPLAY_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_BELOW_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void app_display_task_entry(void *argument)
{
    app_display_event_t event;
    platform_error_t result;

    (void)argument;

    for (;;) {
        result = platform_queue_receive(g_displayQueue,
                                        &event,
                                        PLATFORM_OS_WAIT_FOREVER);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("Display queue receive failed: %d", result);
        }
    }
}
//******************************** Private Functions *************************//

//******************************** Functions ********************************//
platform_error_t app_display_task_start(platform_queue_t *displayQueue)
{
    platform_error_t result;
    if (displayQueue == (platform_queue_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_displayTaskStarted == PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    result = platform_queue_create(displayQueue,
                                   APP_DISPLAY_QUEUE_LENGTH,
                                   sizeof(app_display_event_t));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_displayQueue = displayQueue;
    result = platform_thread_create(&g_displayTaskThread,
                                    &s_display_task_config);
    if (result != PLATFORM_ERR_OK) {
        g_displayQueue = (platform_queue_t *)0;
        (void)platform_queue_delete(displayQueue);
        return result;
    }

    g_displayTaskStarted = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
