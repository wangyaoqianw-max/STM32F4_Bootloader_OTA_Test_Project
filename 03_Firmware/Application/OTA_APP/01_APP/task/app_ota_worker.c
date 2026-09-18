/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ota_worker.c
 * @brief S07 otaWorker RTOS 执行壳与 OTA Service 驱动实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_ota_worker.h"

#define LOG_TAG "ota_worker"

#include "app_ota_runtime.h"
#include "app_startup.h"
#include "platform_key.h"
#include "platform_mcu_reset.h"
#include "platform_os.h"
#include "platform_types.h"
#include "platform_time.h"
#include "service_log.h"
#include "service_ota.h"
#include "ymodem_config.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_OTA_WORKER_TASK_STACK_SIZE       (4096U)
#define APP_OTA_WORKER_KEY_DEBOUNCE_MS       (40U)
#define APP_OTA_WORKER_DISPLAY_TIMEOUT_MS    (20U)
#define APP_OTA_WORKER_UART_WAIT_TIMEOUT_MS  (1000U)
#define APP_OTA_WORKER_NOTIFY_FLAGS          \
    (APP_OTA_NOTIFY_UART_RX | APP_OTA_NOTIFY_START | APP_OTA_NOTIFY_CANCEL | \
     APP_OTA_NOTIFY_SHUTDOWN | APP_OTA_NOTIFY_KEY_1)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static void app_ota_worker_entry(void *argument);
static void app_ota_worker_terminate(void);
static platform_error_t app_ota_worker_init_key(void);
static platform_error_t app_ota_worker_process_session(void);
static platform_error_t app_ota_worker_abort_session(
    platform_error_t error);
static app_display_target_slot_t app_ota_worker_map_target_slot(
    firmware_slot_t slot);
static platform_error_t app_ota_worker_post_display_event(
    app_display_event_type_t type,
    app_display_target_slot_t targetSlot,
    uint32_t progress,
    uint32_t imageSize,
    platform_error_t errorCode);
static platform_error_t app_ota_worker_publish_service_event(
    platform_bool_t *resetRequired);
static platform_error_t app_ota_worker_handle_key(void);
static platform_bool_t app_ota_worker_accept_key_event(void);
static void app_ota_worker_wait_for_command(void);
static void app_ota_worker_key_event_callback(
    platform_key_id_t key,
    void *context);
static void app_ota_worker_log_status(void);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_bool_t g_otaWorkerStarted = PLATFORM_FALSE;
static platform_bool_t g_otaKeyTimestampValid = PLATFORM_FALSE;
static uint32_t g_otaLastKeyTimestampMs = 0U;
static platform_thread_t g_otaWorkerThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t *g_otaDisplayQueue = NULL;

static const platform_thread_config_t s_ota_worker_thread_config = {
    .name = "otaWorker",
    .entry = app_ota_worker_entry,
    .argument = NULL,
    .stackSizeBytes = APP_OTA_WORKER_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void app_ota_worker_terminate(void)
{
    platform_error_t result;

    result = platform_thread_terminate(&g_otaWorkerThread);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("OTA worker termination failed: %d", (int)result);
    }

    for (;;) {
        (void)platform_time_delay_ms(1000U);
    }
}

static void app_ota_worker_key_event_callback(
    platform_key_id_t key,
    void *context)
{
    (void)context;

    if (key == PLATFORM_KEY_ID_1) {
        (void)platform_notify_set_from_isr(
            &g_otaWorkerThread,
            APP_OTA_NOTIFY_KEY_1);
    }
}

static platform_error_t app_ota_worker_init_key(void)
{
    return platform_key_init(app_ota_worker_key_event_callback, NULL);
}

static platform_bool_t app_ota_worker_accept_key_event(void)
{
    uint32_t nowMs;

    if (platform_time_get_ms(&nowMs) != PLATFORM_ERR_OK) {
        return PLATFORM_FALSE;
    }

    if ((g_otaKeyTimestampValid == PLATFORM_TRUE) &&
        ((nowMs - g_otaLastKeyTimestampMs) < APP_OTA_WORKER_KEY_DEBOUNCE_MS)) {
        return PLATFORM_FALSE;
    }

    g_otaLastKeyTimestampMs = nowMs;
    g_otaKeyTimestampValid = PLATFORM_TRUE;
    return PLATFORM_TRUE;
}

static app_display_target_slot_t app_ota_worker_map_target_slot(
    firmware_slot_t slot)
{
    switch (slot) {
        case FIRMWARE_SLOT_A:
            return APP_DISPLAY_TARGET_SLOT_A;

        case FIRMWARE_SLOT_B:
            return APP_DISPLAY_TARGET_SLOT_B;

        default:
            return APP_DISPLAY_TARGET_SLOT_UNKNOWN;
    }
}

static platform_error_t app_ota_worker_post_display_event(
    app_display_event_type_t type,
    app_display_target_slot_t targetSlot,
    uint32_t progress,
    uint32_t imageSize,
    platform_error_t errorCode)
{
    app_display_event_t event = {
        .type = type,
        .targetSlot = targetSlot,
        .progress = progress,
        .imageSize = imageSize,
        .errorCode = errorCode
    };
    uint32_t timeoutMs = PLATFORM_OS_NO_WAIT;
    platform_error_t result;

    if (g_otaDisplayQueue == NULL) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((type == APP_DISPLAY_EVENT_OTA_FAILED) ||
        (type == APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL) ||
        (type == APP_DISPLAY_EVENT_OTA_RESET_REQUIRED)) {
        timeoutMs = APP_OTA_WORKER_DISPLAY_TIMEOUT_MS;
    }

    result = platform_queue_send(g_otaDisplayQueue, &event, timeoutMs);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_W("OTA display event dropped type=%d progress=%lu error=%d result=%d",
                      (int)type,
                      (unsigned long)progress,
                      (int)errorCode,
                      (int)result);
        return result;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t app_ota_worker_publish_service_event(
    platform_bool_t *resetRequired)
{
    service_ota_t *service;
    service_ota_status_t status;
    app_display_event_type_t displayType;
    app_display_target_slot_t targetSlot;
    platform_error_t result;

    if (resetRequired == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *resetRequired = PLATFORM_FALSE;
    service = app_ota_runtime_get_service();
    if (service == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = service_ota_get_status(service, &status);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    switch (status.event) {
        case SERVICE_OTA_EVENT_STARTED:
        case SERVICE_OTA_EVENT_PROGRESS:
            displayType = APP_DISPLAY_EVENT_OTA_RECEIVING;
            break;

        case SERVICE_OTA_EVENT_VERIFYING:
            displayType = APP_DISPLAY_EVENT_OTA_VERIFYING;
            break;

        case SERVICE_OTA_EVENT_READY_TO_INSTALL:
            displayType = APP_DISPLAY_EVENT_OTA_READY_TO_INSTALL;
            break;

        case SERVICE_OTA_EVENT_RESET_REQUIRED:
            displayType = APP_DISPLAY_EVENT_OTA_RESET_REQUIRED;
            *resetRequired = PLATFORM_TRUE;
            break;

        case SERVICE_OTA_EVENT_FAILED:
            displayType = APP_DISPLAY_EVENT_OTA_FAILED;
            break;

        case SERVICE_OTA_EVENT_NONE:
        default:
            return PLATFORM_ERR_OK;
    }

    targetSlot = app_ota_worker_map_target_slot(status.targetSlot);

    result = app_ota_worker_post_display_event(
        displayType,
        targetSlot,
        status.progressPercent,
        status.expectedBytes,
        status.lastError);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return service_ota_clear_event(service);
}

static platform_error_t app_ota_worker_process_session(void)
{
    uint8_t buffer[YMODEM_CFG_UART_READ_BUFFER_SIZE] = {0U};
    service_ota_t *service;
    platform_size_t readableSize;
    platform_size_t readLength;
    uint32_t receivedFlags;
    uint32_t nowMs;
    platform_size_t index;
    platform_error_t result;
    platform_bool_t resetRequired;

    service = app_ota_runtime_get_service();
    if (service == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    while (app_ota_runtime_session_is_active() == PLATFORM_TRUE) {
        result = app_ota_runtime_check_uart();
        if (result != PLATFORM_ERR_OK) {
            return app_ota_worker_abort_session(result);
        }

        result = app_ota_runtime_get_readable_size(&readableSize);
        if (result != PLATFORM_ERR_OK) {
            return app_ota_worker_abort_session(result);
        }

        if (readableSize == 0U) {
            result = platform_notify_wait(
                APP_OTA_WORKER_NOTIFY_FLAGS,
                PLATFORM_FALSE,
                PLATFORM_TRUE,
                APP_OTA_WORKER_UART_WAIT_TIMEOUT_MS,
                &receivedFlags);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return app_ota_worker_abort_session(result);
            }

            if ((result == PLATFORM_ERR_OK) &&
                ((receivedFlags & (APP_OTA_NOTIFY_SHUTDOWN |
                                   APP_OTA_NOTIFY_CANCEL)) != 0U)) {
                return app_ota_worker_abort_session(PLATFORM_ERR_CANCELED);
            }

            result = platform_time_get_ms(&nowMs);
            if (result != PLATFORM_ERR_OK) {
                return app_ota_worker_abort_session(result);
            }

            result = service_ota_process(service, NULL, 0U, nowMs);
            if (result != PLATFORM_ERR_OK) {
                (void)app_ota_worker_publish_service_event(&resetRequired);
                return result;
            }

            result = app_ota_worker_publish_service_event(&resetRequired);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
            continue;
        }

        result = app_ota_runtime_read(
            buffer,
            sizeof(buffer),
            &readLength);
        if (result != PLATFORM_ERR_OK) {
            return app_ota_worker_abort_session(result);
        }

        for (index = 0U; index < readLength; index++) {
            result = platform_time_get_ms(&nowMs);
            if (result != PLATFORM_ERR_OK) {
                return app_ota_worker_abort_session(result);
            }

            result = service_ota_process(
                service,
                &buffer[index],
                1U,
                nowMs);
            if (result != PLATFORM_ERR_OK) {
                (void)app_ota_worker_publish_service_event(&resetRequired);
                return result;
            }

            result = app_ota_worker_publish_service_event(&resetRequired);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t app_ota_worker_abort_session(platform_error_t error)
{
    service_ota_t *service;
    platform_error_t result;
    platform_bool_t resetRequired;

    service = app_ota_runtime_get_service();
    if (service == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = service_ota_abort(service, error);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    (void)app_ota_worker_publish_service_event(&resetRequired);
    return error;
}

static platform_error_t app_ota_worker_handle_key(void)
{
    service_ota_t *service;
    service_ota_status_t status;
    platform_error_t result;
    platform_error_t stopResult;
    platform_bool_t resetRequired;

    service = app_ota_runtime_get_service();
    if (service == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = service_ota_get_status(service, &status);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((status.state == SERVICE_OTA_STATE_IDLE) ||
        (status.state == SERVICE_OTA_STATE_FAILED)) {
        result = app_ota_runtime_start_session();
        if (result != PLATFORM_ERR_OK) {
            (void)app_ota_worker_publish_service_event(&resetRequired);
            return result;
        }

        (void)app_ota_worker_publish_service_event(&resetRequired);
        result = app_ota_worker_process_session();
        (void)app_ota_worker_publish_service_event(&resetRequired);

        stopResult = app_ota_runtime_stop_session();
        if ((result == PLATFORM_ERR_OK) &&
            (stopResult != PLATFORM_ERR_OK)) {
            result = stopResult;
        }
        return result;
    }

    if (status.state != SERVICE_OTA_STATE_READY_TO_INSTALL) {
        return PLATFORM_ERR_OK;
    }

    result = service_ota_confirm_install(service);
    if (result != PLATFORM_ERR_OK) {
        (void)app_ota_worker_publish_service_event(&resetRequired);
        return result;
    }

    result = app_ota_worker_publish_service_event(&resetRequired);
    if (resetRequired == PLATFORM_TRUE) {
        platform_mcu_reset();
    }

    return result;
}

static void app_ota_worker_log_status(void)
{
    service_ota_t *service;
    service_ota_status_t status;
    service_uart_statistics_t statistics = {0};

    service = app_ota_runtime_get_service();
    if (service != NULL) {
        if (service_ota_get_status(service, &status) == PLATFORM_ERR_OK) {
            SERVICE_LOG_I(
                "OTA state=%d event=%d target=%d progress=%lu/%lu error=%d",
                (int)status.state,
                (int)status.event,
                (int)status.targetSlot,
                (unsigned long)status.receivedBytes,
                (unsigned long)status.expectedBytes,
                (int)status.lastError);
        }
    }

    if (app_ota_runtime_get_uart_statistics(&statistics) == PLATFORM_ERR_OK) {
        SERVICE_LOG_I(
            "OTA UART rx_events=%lu rx_bytes=%lu buffered=%lu read=%lu dropped=%lu errors=%lu tx_bytes=%lu",
            (unsigned long)statistics.rxEventCount,
            (unsigned long)statistics.rxBytesReceived,
            (unsigned long)statistics.rxBytesBuffered,
            (unsigned long)statistics.rxBytesRead,
            (unsigned long)statistics.rxBytesDropped,
            (unsigned long)statistics.uartErrorCount,
            (unsigned long)statistics.txBytesCompleted);
    }
}

static void app_ota_worker_wait_for_command(void)
{
    uint32_t receivedFlags;
    platform_bool_t keyHandled;
    platform_error_t result;

    for (;;) {
        result = platform_notify_wait(
            APP_OTA_WORKER_NOTIFY_FLAGS,
            PLATFORM_FALSE,
            PLATFORM_TRUE,
            PLATFORM_OS_WAIT_FOREVER,
            &receivedFlags);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("OTA notify wait failed: %d", (int)result);
            continue;
        }

        if ((receivedFlags & APP_OTA_NOTIFY_SHUTDOWN) != 0U) {
            return;
        }

        keyHandled = PLATFORM_FALSE;
        if ((receivedFlags & APP_OTA_NOTIFY_KEY_1) != 0U) {
            if (app_ota_worker_accept_key_event() == PLATFORM_TRUE) {
                result = app_ota_worker_handle_key();
                keyHandled = PLATFORM_TRUE;
                if (result != PLATFORM_ERR_OK) {
                    SERVICE_LOG_I("OTA key action result: %d", (int)result);
                }
                app_ota_worker_log_status();
            }
        }

        if (((receivedFlags & APP_OTA_NOTIFY_START) != 0U) &&
            (keyHandled == PLATFORM_FALSE)) {
            result = app_ota_worker_handle_key();
            if (result != PLATFORM_ERR_OK) {
                SERVICE_LOG_I("OTA start action result: %d", (int)result);
            }
            app_ota_worker_log_status();
        }
    }
}

static void app_ota_worker_entry(void *argument)
{
    platform_error_t initResult = PLATFORM_ERR_OK;
    platform_error_t result;
    const app_startup_context_t *startupContext;
    uint32_t freeStackBytes;

    (void)argument;

    if (g_otaDisplayQueue == NULL) {
        SERVICE_LOG_E("OTA worker display queue is not bound");
        initResult = PLATFORM_ERR_INVALID_STATE;
    } else {
        initResult = platform_thread_get_current(&g_otaWorkerThread);
        if (initResult != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("OTA worker current thread failed: %d",
                          (int)initResult);
        }
    }

    if (initResult == PLATFORM_ERR_OK) {
        initResult = app_ota_worker_init_key();
        if (initResult != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("OTA key init failed: %d", (int)initResult);
        }
    }

    if (initResult == PLATFORM_ERR_OK) {
        initResult = app_ota_runtime_init(&g_otaWorkerThread);
        SERVICE_LOG_I("OTA runtime init result=%d", (int)initResult);
    }

    result = app_startup_report_ota(initResult);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("OTA startup report failed: %d", (int)result);
        app_ota_worker_terminate();
    }

    result = app_startup_wait_for_decision();
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_I("OTA startup aborted: %d", (int)result);
        app_ota_worker_terminate();
    }

    startupContext = app_startup_get_context();
    if ((initResult != PLATFORM_ERR_OK) ||
        (startupContext->systemState == APP_SYSTEM_STATE_FAILED)) {
        app_ota_worker_terminate();
    }

    result = platform_thread_get_stack_space(
        &g_otaWorkerThread,
        &freeStackBytes);
    if (result == PLATFORM_ERR_OK) {
        SERVICE_LOG_I("otaWorker stack free=%lu B",
                      (unsigned long)freeStackBytes);
    } else {
        SERVICE_LOG_W("otaWorker stack query failed: %d", (int)result);
    }

    app_ota_worker_wait_for_command();
    app_ota_worker_terminate();
}

//******************************** Functions *********************************//
platform_error_t app_ota_worker_start(platform_queue_t *displayQueue)
{
    platform_error_t result;

    if (displayQueue == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_otaWorkerStarted == PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    g_otaDisplayQueue = displayQueue;
    result = platform_thread_create(
        &g_otaWorkerThread,
        &s_ota_worker_thread_config);
    if (result != PLATFORM_ERR_OK) {
        g_otaDisplayQueue = NULL;
        return result;
    }

    g_otaWorkerStarted = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
