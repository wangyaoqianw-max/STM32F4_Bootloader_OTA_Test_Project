/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display_task.c
 * @brief S06 displayTask 显示资源所有权与渲染实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_display_task.h"

#define LOG_TAG "display_task"

#include "platform_bsp_spi.h"
#include "platform_bsp_st7789.h"
#include "platform_font_ascii_8x16.h"
#include "platform_graphics.h"
#include "service_log.h"

#include <stdio.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_DISPLAY_TASK_STACK_SIZE    (4096U)
#define APP_DISPLAY_QUEUE_LENGTH       (8U)
#define APP_DISPLAY_TEXT_BUFFER_SIZE   (32U)
#define APP_DISPLAY_LINE_HEIGHT        (16U)
#define APP_DISPLAY_TEXT_X             (8U)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static void app_display_task_entry(void *argument);
static void app_display_model_initialize(display_model_t *model);
static const char_t *app_display_state_text(app_display_event_type_t state);
static const char_t *app_display_target_slot_text(
    app_display_target_slot_t targetSlot);
static platform_error_t app_display_task_initialize(
    display_model_t *model);
static platform_error_t app_display_render_line(
    uint16_t y, const char_t *text);
static platform_error_t app_display_render_initial(
    const display_model_t *model);
static platform_error_t app_display_render_event(
    const display_model_t *model);
static platform_error_t app_display_update_model(
    display_model_t *model, const app_display_event_t *event);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_bool_t g_displayTaskStarted = PLATFORM_FALSE;
static platform_thread_t g_displayTaskThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t *g_displayQueue = (platform_queue_t *)0;
static platform_spi_bus_t g_displaySpiBus = PLATFORM_SPI_BUS_INITIALIZER;
static platform_st7789_t g_display = PLATFORM_ST7789_INITIALIZER;

static const platform_thread_config_t s_display_task_config = {
    .name = "displayTask",
    .entry = app_display_task_entry,
    .argument = (void *)0,
    .stackSizeBytes = APP_DISPLAY_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_BELOW_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void app_display_model_initialize(display_model_t *model)
{
    model->firmwareVersion[0] = 'V';
    model->firmwareVersion[1] = '1';
    model->firmwareVersion[2] = '.';
    model->firmwareVersion[3] = '0';
    model->firmwareVersion[4] = '\0';

    model->systemState[0] = 'R';
    model->systemState[1] = 'U';
    model->systemState[2] = 'N';
    model->systemState[3] = 'N';
    model->systemState[4] = 'I';
    model->systemState[5] = 'N';
    model->systemState[6] = 'G';
    model->systemState[7] = '\0';

    model->otaState = APP_DISPLAY_EVENT_OTA_IDLE;
    model->otaProgress = 0U;
    model->targetSlot = APP_DISPLAY_TARGET_SLOT_B;
    model->lastError = PLATFORM_ERR_OK;
}

static const char_t *app_display_state_text(app_display_event_type_t state)
{
    switch (state) {
    case APP_DISPLAY_EVENT_OTA_IDLE:
        return "IDLE";
    case APP_DISPLAY_EVENT_OTA_RECEIVING:
        return "RECEIVING";
    case APP_DISPLAY_EVENT_OTA_VERIFYING:
        return "VERIFYING";
    case APP_DISPLAY_EVENT_OTA_SUCCESS:
        return "SUCCESS";
    case APP_DISPLAY_EVENT_OTA_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

static const char_t *app_display_target_slot_text(
    app_display_target_slot_t targetSlot)
{
    switch (targetSlot) {
    case APP_DISPLAY_TARGET_SLOT_A:
        return "SLOT A";
    case APP_DISPLAY_TARGET_SLOT_B:
        return "SLOT B";
    default:
        return "UNKNOWN";
    }
}

static platform_error_t app_display_task_initialize(
    display_model_t *model)
{
    platform_error_t result;

    app_display_model_initialize(model);

    result = platform_bsp_spi_construct_display_bus(&g_displaySpiBus);
    SERVICE_LOG_I("Display SPI construct result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_displaySpiBus);
    SERVICE_LOG_I("Display SPI init result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_displaySpiBus);
    SERVICE_LOG_I("Display SPI start result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_st7789_construct_display(&g_display);
    SERVICE_LOG_I("Display construct result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_st7789_init(&g_display, &g_displaySpiBus);
    SERVICE_LOG_I("Display init result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_display_render_initial(model);
    SERVICE_LOG_I("Display initial render result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_st7789_backlight_on(&g_display);
    SERVICE_LOG_I("Display backlight on result: %d", result);
    return result;
}

static platform_error_t app_display_render_line(
    uint16_t y, const char_t *text)
{
    platform_error_t result;

    result = platform_st7789_fill_rect(
        &g_display,
        0U,
        y,
        g_display.width,
        APP_DISPLAY_LINE_HEIGHT,
        PLATFORM_ST7789_COLOR_BLACK);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return platform_graphics_draw_string(
        &g_display,
        APP_DISPLAY_TEXT_X,
        y,
        text,
        &g_platformFontAscii8x16,
        PLATFORM_ST7789_COLOR_WHITE,
        PLATFORM_ST7789_COLOR_BLACK);
}

static platform_error_t app_display_render_initial(
    const display_model_t *model)
{
    char_t line[APP_DISPLAY_TEXT_BUFFER_SIZE];
    platform_error_t result;

    result = platform_st7789_fill(
        &g_display,
        PLATFORM_ST7789_COLOR_BLACK);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "FW VERSION : %s",
                   model->firmwareVersion);
    result = app_display_render_line(16U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "SYSTEM     : %s",
                   model->systemState);
    result = app_display_render_line(40U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "OTA STATE  : %s",
                   app_display_state_text(model->otaState));
    result = app_display_render_line(64U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "TARGET     : %s",
                   app_display_target_slot_text(model->targetSlot));
    result = app_display_render_line(88U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "PROGRESS   : %lu%%",
                   (unsigned long)model->otaProgress);
    result = app_display_render_line(112U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return app_display_render_line(136U, "RESULT     : NONE");
}

static platform_error_t app_display_render_event(
    const display_model_t *model)
{
    char_t line[APP_DISPLAY_TEXT_BUFFER_SIZE];
    platform_error_t result;

    (void)snprintf(line,
                   sizeof(line),
                   "OTA STATE  : %s",
                   app_display_state_text(model->otaState));
    result = app_display_render_line(64U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "PROGRESS   : %lu%%",
                   (unsigned long)model->otaProgress);
    result = app_display_render_line(112U, line);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (model->otaState == APP_DISPLAY_EVENT_OTA_SUCCESS) {
        return app_display_render_line(136U, "RESULT     : SUCCESS");
    }
    if (model->otaState == APP_DISPLAY_EVENT_OTA_FAILED) {
        (void)snprintf(line,
                       sizeof(line),
                       "RESULT     : ERR %d",
                       (int)model->lastError);
        return app_display_render_line(136U, line);
    }
    return app_display_render_line(136U, "RESULT     : NONE");
}

static platform_error_t app_display_update_model(
    display_model_t *model, const app_display_event_t *event)
{
    if ((model == (display_model_t *)0) ||
        (event == (const app_display_event_t *)0)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (event->type >= APP_DISPLAY_EVENT_MAX) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    model->otaState = event->type;
    model->otaProgress = event->progress;
    model->lastError = (event->type == APP_DISPLAY_EVENT_OTA_FAILED) ?
                       event->errorCode : PLATFORM_ERR_OK;
    return PLATFORM_ERR_OK;
}

static void app_display_task_entry(void *argument)
{
    display_model_t model;
    app_display_event_t event;
    platform_bool_t displayReady = PLATFORM_FALSE;
    platform_error_t result;

    (void)argument;

    result = app_display_task_initialize(&model);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("DISPLAY DEGRADED: %d", result);
    } else {
        displayReady = PLATFORM_TRUE;
    }

    for (;;) {
        result = platform_queue_receive(g_displayQueue,
                                        &event,
                                        PLATFORM_OS_WAIT_FOREVER);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("Display queue receive failed: %d", result);
            continue;
        }

        result = app_display_update_model(&model, &event);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("Display event rejected: %d", result);
            continue;
        }

        if (displayReady == PLATFORM_TRUE) {
            result = app_display_render_event(&model);
            if (result != PLATFORM_ERR_OK) {
                SERVICE_LOG_E("Display event render failed: %d", result);
            }
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
