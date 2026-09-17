/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ota_worker.c
 * @brief S06 otaWorker 后台 YMODEM 接收与 Firmware Storage 执行实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_ota_worker.h"

#define LOG_TAG "ota_worker"

#include "firmware_storage.h"
#include "platform_at24c02.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_uart.h"
#include "platform_bsp_w25q64.h"
#include "platform_i2c.h"
#include "platform_os.h"
#include "platform_time.h"
#include "platform_uart.h"
#include "platform_w25q64.h"
#include "project_config.h"
#include "service_log.h"
#include "service_uart.h"
#include "s05_ymodem_flash_sink.h"
#include "ymodem_config.h"
#include "ymodem_receiver.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_OTA_WORKER_TASK_STACK_SIZE          (4096U)
#define APP_OTA_WORKER_UART_BAUD_RATE           (115200U)
#define APP_OTA_WORKER_UART_TIMEOUT_MS          (1000U)
#define APP_OTA_WORKER_UART_DMA_BUFFER_SIZE     (256U)
#define APP_OTA_WORKER_UART_RING_BUFFER_SIZE    (2048U)
#define APP_OTA_WORKER_UART_WAIT_TIMEOUT_MS     (1000U)
#define APP_OTA_WORKER_DISPLAY_PROGRESS_STEP    (5U)
#define APP_OTA_WORKER_DISPLAY_PROGRESS_PERIOD_MS (200U)
#define APP_OTA_WORKER_DISPLAY_TERMINAL_TIMEOUT_MS (20U)
#define APP_OTA_WORKER_NOTIFY_FLAGS             \
    (APP_OTA_NOTIFY_START | APP_OTA_NOTIFY_CANCEL | \
     APP_OTA_NOTIFY_SHUTDOWN)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static platform_bool_t g_otaWorkerStarted = PLATFORM_FALSE;
static platform_thread_t g_otaWorkerThread = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_queue_t *g_otaDisplayQueue = NULL;

static platform_spi_bus_t g_otaStorageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_otaFlash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_otaEepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_otaEepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_otaEepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_otaEeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_otaFirmwareStorage = FIRMWARE_STORAGE_INITIALIZER;

static platform_uart_t g_otaCommunicationUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_otaUartService = SERVICE_UART_INITIALIZER;
static uint8_t g_otaUartDmaBuffer[APP_OTA_WORKER_UART_DMA_BUFFER_SIZE] = {0U};
static uint8_t g_otaUartRingBuffer[APP_OTA_WORKER_UART_RING_BUFFER_SIZE] = {0U};

static s05_ymodem_flash_sink_t g_otaFlashSink =
    S05_YMODEM_FLASH_SINK_INITIALIZER;
static ymodem_receiver_t g_otaReceiver = YMODEM_RECEIVER_INITIALIZER;
static ymodem_receiver_config_t g_otaReceiverConfig = {0};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void app_ota_worker_entry(void *argument);
static platform_error_t app_ota_worker_init_storage(void);
static platform_error_t app_ota_worker_init_uart(void);
static platform_error_t app_ota_worker_init_receiver(void);
static platform_error_t app_ota_worker_start_session(void);
static platform_error_t app_ota_worker_process_session(void);
static platform_error_t app_ota_worker_finish_session(void);
static platform_error_t app_ota_worker_complete_session(
    platform_error_t sessionResult);
static platform_error_t app_ota_worker_stop_uart(void);
static platform_bool_t app_ota_worker_session_is_active(void);
static platform_error_t app_ota_worker_check_uart(void);
static platform_error_t app_ota_worker_post_display_event(
    app_display_event_type_t type,
    uint32_t progress,
    uint32_t imageSize,
    platform_error_t errorCode);
static uint32_t app_ota_worker_calculate_progress(void);
static platform_error_t app_ota_worker_update_display_progress(
    uint32_t nowMs,
    uint32_t *lastProgress,
    uint32_t *lastProgressMs,
    platform_bool_t force);
static void app_ota_worker_log_session(void);
static void app_ota_worker_wait_for_command(void);

static const platform_thread_config_t s_ota_worker_thread_config = {
    .name = "otaWorker",
    .entry = app_ota_worker_entry,
    .argument = NULL,
    .stackSizeBytes = APP_OTA_WORKER_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
};
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
static platform_error_t app_ota_worker_init_storage(void)
{
    platform_error_t result;

    result = platform_bsp_spi_construct_storage_bus(&g_otaStorageSpiBus);
    SERVICE_LOG_I("OTA Storage SPI construct result: %d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_otaStorageSpiBus);
    SERVICE_LOG_I("OTA Storage SPI init result: %d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_otaStorageSpiBus);
    SERVICE_LOG_I("OTA Storage SPI start result: %d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(&g_otaFlash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(&g_otaFlash, &g_otaStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(&g_otaEepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(&g_otaEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(&g_otaEepromI2c,
                               "ota_metadata_i2c",
                               &g_otaEepromScl,
                               &g_otaEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(&g_otaEeprom,
                                   &g_otaEepromI2c,
                                   PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(&g_otaFirmwareStorage,
                                 &g_otaFlash,
                                 &g_otaEeprom);
}

static platform_error_t app_ota_worker_init_uart(void)
{
    const platform_uart_config_t uartConfig = {
        APP_OTA_WORKER_UART_BAUD_RATE,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        APP_OTA_WORKER_UART_TIMEOUT_MS
    };
    service_uart_config_t serviceConfig = {0};
    platform_error_t result;

    result = platform_bsp_uart_construct_communication(&g_otaCommunicationUart,
                                                       &uartConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((g_otaCommunicationUart.device.lifecycle == NULL) ||
        (g_otaCommunicationUart.device.lifecycle->init == NULL) ||
        (g_otaCommunicationUart.device.lifecycle->start == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = g_otaCommunicationUart.device.lifecycle->init(
        &g_otaCommunicationUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    serviceConfig.uart = &g_otaCommunicationUart;
    serviceConfig.dmaRxBuffer = g_otaUartDmaBuffer;
    serviceConfig.dmaRxBufferSize = sizeof(g_otaUartDmaBuffer);
    serviceConfig.ringBufferStorage = g_otaUartRingBuffer;
    serviceConfig.ringBufferStorageSize = sizeof(g_otaUartRingBuffer);
    serviceConfig.ownerThread = &g_otaWorkerThread;

    result = service_uart_init(&g_otaUartService, &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = g_otaCommunicationUart.device.lifecycle->start(
        &g_otaCommunicationUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = service_uart_start(&g_otaUartService);
    SERVICE_LOG_I("OTA UART service start result: %d", (int)result);
    return result;
}

static platform_error_t app_ota_worker_init_receiver(void)
{
    ymodem_sink_t sinkContract = {0};
    platform_error_t result;

    result = s05_ymodem_flash_sink_init(&g_otaFlashSink,
                                        &g_otaFirmwareStorage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s05_ymodem_flash_sink_get_contract(&g_otaFlashSink,
                                                &sinkContract);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_otaReceiverConfig.uart = &g_otaUartService;
    g_otaReceiverConfig.sink = sinkContract;

    return ymodem_receiver_init(&g_otaReceiver,
                                &g_otaReceiverConfig);
}

static platform_error_t app_ota_worker_start_session(void)
{
    platform_error_t result = ymodem_receiver_start(&g_otaReceiver);

    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("OTA YMODEM start failed: %d", (int)result);
        return result;
    }

    SERVICE_LOG_I("OTA YMODEM_READY target=Slot B");
    (void)app_ota_worker_post_display_event(
        APP_DISPLAY_EVENT_OTA_RECEIVING,
        0U,
        0U,
        PLATFORM_ERR_OK);
    return PLATFORM_ERR_OK;
}

static platform_bool_t app_ota_worker_session_is_active(void)
{
    switch (g_otaReceiver.context.state) {
        case YMODEM_RECEIVER_STATE_WAIT_HEADER:
        case YMODEM_RECEIVER_STATE_RECEIVE_DATA:
        case YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM:
        case YMODEM_RECEIVER_STATE_WAIT_END_HEADER:
            return PLATFORM_TRUE;

        default:
            return PLATFORM_FALSE;
    }
}

static platform_error_t app_ota_worker_check_uart(void)
{
    service_uart_status_t status = {
        SERVICE_UART_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        PLATFORM_FALSE
    };
    platform_error_t result;

    result = service_uart_get_status(&g_otaUartService, &status);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (status.dataLossOccurred == PLATFORM_TRUE) {
        return PLATFORM_ERR_OVERFLOW;
    }

    if (status.state == SERVICE_UART_STATE_ERROR) {
        return (status.lastError == PLATFORM_ERR_OK) ?
               PLATFORM_ERR_IO : status.lastError;
    }

    if (status.state != SERVICE_UART_STATE_RUNNING) {
        return PLATFORM_ERR_CANCELED;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t app_ota_worker_post_display_event(
    app_display_event_type_t type,
    uint32_t progress,
    uint32_t imageSize,
    platform_error_t errorCode)
{
    app_display_event_t event = {
        type,
        progress,
        imageSize,
        errorCode
    };
    uint32_t timeoutMs = PLATFORM_OS_NO_WAIT;
    platform_error_t result;

    if (g_otaDisplayQueue == NULL) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((type == APP_DISPLAY_EVENT_OTA_SUCCESS) ||
        (type == APP_DISPLAY_EVENT_OTA_FAILED)) {
        timeoutMs = APP_OTA_WORKER_DISPLAY_TERMINAL_TIMEOUT_MS;
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

    SERVICE_LOG_I("OTA display event type=%d progress=%lu size=%lu error=%d",
                  (int)type,
                  (unsigned long)progress,
                  (unsigned long)imageSize,
                  (int)errorCode);
    return PLATFORM_ERR_OK;
}

static uint32_t app_ota_worker_calculate_progress(void)
{
    uint32_t imageSize = g_otaReceiver.context.block0Metadata.fileSize;
    uint32_t receivedSize = g_otaReceiver.context.receivedSize;
    uint32_t progress;

    if (imageSize == 0U) {
        return 0U;
    }

    if (receivedSize >= imageSize) {
        return 100U;
    }

    if (imageSize < 100U) {
        progress = (receivedSize * 100U) / imageSize;
    } else {
        progress = receivedSize / (imageSize / 100U);
    }

    return (progress > 100U) ? 100U : progress;
}

static platform_error_t app_ota_worker_update_display_progress(
    uint32_t nowMs,
    uint32_t *lastProgress,
    uint32_t *lastProgressMs,
    platform_bool_t force)
{
    uint32_t progress;
    uint32_t elapsedMs;

    if ((lastProgress == NULL) || (lastProgressMs == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    progress = app_ota_worker_calculate_progress();
    elapsedMs = nowMs - *lastProgressMs;
    if ((force == PLATFORM_FALSE) &&
        (progress < (*lastProgress + APP_OTA_WORKER_DISPLAY_PROGRESS_STEP)) &&
        (elapsedMs < APP_OTA_WORKER_DISPLAY_PROGRESS_PERIOD_MS)) {
        return PLATFORM_ERR_OK;
    }

    (void)app_ota_worker_post_display_event(
        APP_DISPLAY_EVENT_OTA_RECEIVING,
        progress,
        g_otaReceiver.context.block0Metadata.fileSize,
        PLATFORM_ERR_OK);
    *lastProgress = progress;
    *lastProgressMs = nowMs;
    return PLATFORM_ERR_OK;
}

static platform_error_t app_ota_worker_process_session(void)
{
    uint8_t buffer[YMODEM_CFG_UART_READ_BUFFER_SIZE] = {0U};
    platform_size_t readableSize;
    platform_size_t readLength;
    uint32_t events;
    uint32_t nowMs;
    uint32_t lastProgress = 0U;
    uint32_t lastProgressMs = 0U;
    platform_bool_t progressClockInitialized = PLATFORM_FALSE;
    platform_size_t index;
    platform_error_t result;

    while (app_ota_worker_session_is_active() == PLATFORM_TRUE) {
        result = platform_time_get_ms(&nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        if (progressClockInitialized == PLATFORM_FALSE) {
            lastProgressMs = nowMs;
            progressClockInitialized = PLATFORM_TRUE;
        }

        result = ymodem_receiver_tick(&g_otaReceiver, nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = app_ota_worker_check_uart();
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = service_uart_get_readable_size(&g_otaUartService,
                                                &readableSize);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        if (readableSize == 0U) {
            result = service_uart_wait_event(&g_otaUartService,
                                             APP_OTA_WORKER_UART_WAIT_TIMEOUT_MS,
                                             &events);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return result;
            }
            continue;
        }

        result = service_uart_read(&g_otaUartService,
                                   buffer,
                                   sizeof(buffer),
                                   &readLength);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        for (index = 0U; index < readLength; index++) {
            result = platform_time_get_ms(&nowMs);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }

            result = ymodem_receiver_feed_byte(&g_otaReceiver,
                                               buffer[index],
                                               nowMs);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
        }

        if (g_otaReceiver.context.state == YMODEM_RECEIVER_STATE_FINISHED) {
            SERVICE_LOG_I("OTA progress=%lu/%lu",
                          (unsigned long)g_otaReceiver.context.receivedSize,
                          (unsigned long)g_otaReceiver.context.block0Metadata.fileSize);
        }

        result = app_ota_worker_update_display_progress(
            nowMs,
            &lastProgress,
            &lastProgressMs,
            (g_otaReceiver.context.state == YMODEM_RECEIVER_STATE_FINISHED) ?
                PLATFORM_TRUE : PLATFORM_FALSE);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}

static void app_ota_worker_log_session(void)
{
    ymodem_receiver_status_t status = {
        YMODEM_RECEIVER_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        {{0U}},
        0U
    };
    ymodem_receiver_statistics_t statistics = {0};
    service_uart_statistics_t uartStatistics = {0};

    if (ymodem_receiver_get_status(&g_otaReceiver, &status) == PLATFORM_ERR_OK) {
        SERVICE_LOG_I(
            "OTA session state=%d error=%d filename=%s file_size=%lu received=%lu",
            (int)status.state,
            (int)status.lastError,
            status.block0Metadata.filename,
            (unsigned long)status.block0Metadata.fileSize,
            (unsigned long)status.receivedSize);
    }

    if (ymodem_receiver_get_statistics(&g_otaReceiver, &statistics) == PLATFORM_ERR_OK) {
        SERVICE_LOG_I(
            "OTA packets received=%lu accepted=%lu bytes=%lu/%lu crc=%lu sequence=%lu duplicate=%lu",
            (unsigned long)statistics.packetReceivedCount,
            (unsigned long)statistics.packetAcceptedCount,
            (unsigned long)statistics.bytesReceived,
            (unsigned long)statistics.bytesWritten,
            (unsigned long)statistics.crcErrorCount,
            (unsigned long)statistics.sequenceErrorCount,
            (unsigned long)statistics.duplicatePacketCount);
        SERVICE_LOG_I(
            "OTA timeout=%lu retry=%lu cancel=%lu",
            (unsigned long)statistics.timeoutCount,
            (unsigned long)statistics.retryCount,
            (unsigned long)statistics.cancelCount);
    }

    if (service_uart_get_statistics(&g_otaUartService,
                                    &uartStatistics) == PLATFORM_ERR_OK) {
        SERVICE_LOG_I(
            "OTA UART rx_events=%lu rx_bytes=%lu buffered=%lu read=%lu dropped=%lu errors=%lu tx_bytes=%lu",
            (unsigned long)uartStatistics.rxEventCount,
            (unsigned long)uartStatistics.rxBytesReceived,
            (unsigned long)uartStatistics.rxBytesBuffered,
            (unsigned long)uartStatistics.rxBytesRead,
            (unsigned long)uartStatistics.rxBytesDropped,
            (unsigned long)uartStatistics.uartErrorCount,
            (unsigned long)uartStatistics.txBytesCompleted);
    }

    SERVICE_LOG_I("OTA flash payload=%lu header_commit=%u error=%d",
                  (unsigned long)g_otaFlashSink.payloadWritten,
                  (unsigned int)g_otaFlashSink.headerCommitted,
                  (int)g_otaFlashSink.lastError);
}

static platform_error_t app_ota_worker_finish_session(void)
{
    firmware_image_header_t validatedHeader = {0};
    firmware_image_validation_t validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    ymodem_receiver_status_t status = {
        YMODEM_RECEIVER_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        {{0U}},
        0U
    };
    platform_error_t result;

    if (ymodem_receiver_get_status(&g_otaReceiver, &status) != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_IO;
    }

    if (status.state != YMODEM_RECEIVER_STATE_FINISHED) {
        SERVICE_LOG_E("OTA YMODEM session did not finish state=%d error=%d",
                      (int)status.state,
                      (int)status.lastError);
        return (status.lastError == PLATFORM_ERR_OK) ?
               PLATFORM_ERR_INVALID_STATE : status.lastError;
    }

    result = firmware_storage_validate_image(&g_otaFirmwareStorage,
                                             FIRMWARE_SLOT_B,
                                             &validatedHeader,
                                             &validation);
    SERVICE_LOG_I("OTA Slot B validation result=%d validation=%u",
                  (int)result,
                  (unsigned int)validation);
    if ((result != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_VALID)) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_CHECKSUM : result;
    }

    SERVICE_LOG_I("OTA YMODEM session complete final result=PASS");
    return PLATFORM_ERR_OK;
}

static platform_error_t app_ota_worker_complete_session(
    platform_error_t sessionResult)
{
    if ((sessionResult != PLATFORM_ERR_OK) &&
        (app_ota_worker_session_is_active() == PLATFORM_TRUE)) {
        (void)ymodem_receiver_cancel(&g_otaReceiver);
    }

    app_ota_worker_log_session();
    if ((sessionResult == PLATFORM_ERR_OK) &&
        (g_otaReceiver.context.state == YMODEM_RECEIVER_STATE_FINISHED)) {
        (void)app_ota_worker_post_display_event(
            APP_DISPLAY_EVENT_OTA_VERIFYING,
            app_ota_worker_calculate_progress(),
            g_otaReceiver.context.block0Metadata.fileSize,
            PLATFORM_ERR_OK);
        sessionResult = app_ota_worker_finish_session();
    }

    if (sessionResult == PLATFORM_ERR_OK) {
        (void)app_ota_worker_post_display_event(
            APP_DISPLAY_EVENT_OTA_SUCCESS,
            100U,
            g_otaReceiver.context.block0Metadata.fileSize,
            PLATFORM_ERR_OK);
    } else {
        (void)app_ota_worker_post_display_event(
            APP_DISPLAY_EVENT_OTA_FAILED,
            app_ota_worker_calculate_progress(),
            g_otaReceiver.context.block0Metadata.fileSize,
            sessionResult);
    }

    return sessionResult;
}

static platform_error_t app_ota_worker_stop_uart(void)
{
    platform_error_t result;
    uint32_t previousFlags = 0U;

    result = service_uart_stop(&g_otaUartService);
    if ((result != PLATFORM_ERR_OK) &&
        (result != PLATFORM_ERR_INVALID_STATE)) {
        return result;
    }

    return platform_notify_clear(APP_OTA_NOTIFY_UART_RX, &previousFlags);
}

static void app_ota_worker_wait_for_command(void)
{
    uint32_t receivedFlags = 0U;
    platform_error_t result;

    for (;;) {
        result = platform_notify_wait(APP_OTA_WORKER_NOTIFY_FLAGS,
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

        if ((receivedFlags & APP_OTA_NOTIFY_START) == 0U) {
            continue;
        }

        result = service_uart_start(&g_otaUartService);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("OTA UART restart failed: %d", (int)result);
            continue;
        }

        result = app_ota_worker_init_receiver();
        if (result == PLATFORM_ERR_OK) {
            result = app_ota_worker_start_session();
        }
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E("OTA session restart failed: %d", (int)result);
            (void)app_ota_worker_post_display_event(
                APP_DISPLAY_EVENT_OTA_FAILED,
                app_ota_worker_calculate_progress(),
                g_otaReceiver.context.block0Metadata.fileSize,
                result);
            (void)app_ota_worker_stop_uart();
            continue;
        }

        result = app_ota_worker_process_session();
        result = app_ota_worker_complete_session(result);
        SERVICE_LOG_I("OTA session return=%d", (int)result);
        (void)app_ota_worker_stop_uart();
    }
}

static void app_ota_worker_entry(void *argument)
{
    platform_error_t result;

    (void)argument;

    if (g_otaDisplayQueue == NULL) {
        SERVICE_LOG_E("OTA worker display queue is not bound");
        app_ota_worker_wait_for_command();
        return;
    }

    result = platform_thread_get_current(&g_otaWorkerThread);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("OTA worker current thread failed: %d", (int)result);
        app_ota_worker_wait_for_command();
        return;
    }

    result = app_ota_worker_init_storage();
    SERVICE_LOG_I("OTA storage init result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        app_ota_worker_wait_for_command();
        return;
    }

    result = app_ota_worker_init_uart();
    SERVICE_LOG_I("OTA UART init result=%d baud=%lu",
                  (int)result,
                  (unsigned long)APP_OTA_WORKER_UART_BAUD_RATE);
    if (result != PLATFORM_ERR_OK) {
        app_ota_worker_wait_for_command();
        return;
    }

    result = app_ota_worker_init_receiver();
    if (result == PLATFORM_ERR_OK) {
        result = app_ota_worker_start_session();
    }
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("OTA worker session init failed: %d", (int)result);
        (void)app_ota_worker_post_display_event(
            APP_DISPLAY_EVENT_OTA_FAILED,
            app_ota_worker_calculate_progress(),
            g_otaReceiver.context.block0Metadata.fileSize,
            result);
        (void)app_ota_worker_stop_uart();
        app_ota_worker_wait_for_command();
        return;
    }

    result = app_ota_worker_process_session();
    result = app_ota_worker_complete_session(result);
    SERVICE_LOG_I("OTA initial session return=%d", (int)result);
    (void)app_ota_worker_stop_uart();

    app_ota_worker_wait_for_command();
}

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
    result = platform_thread_create(&g_otaWorkerThread,
                                    &s_ota_worker_thread_config);
    if (result != PLATFORM_ERR_OK) {
        g_otaDisplayQueue = NULL;
        return result;
    }

    g_otaWorkerStarted = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
