/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s05_ymodem_test.c
 * @brief S05 UART YMODEM 到 Slot B 板测实现
 * @author YaoQian Wang
 * @date 2026-09-14
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_s05_ymodem_test.h"

#define LOG_TAG "s05_ymodem"

#include "firmware_storage.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_uart.h"
#include "platform_bsp_w25q64.h"
#include "platform_def.h"
#include "platform_i2c.h"
#include "platform_os.h"
#include "platform_time.h"
#include "project_config.h"
#include "service_log.h"
#include "service_uart.h"
#include "s05_ymodem_flash_sink.h"
#include "ymodem_config.h"
#include "ymodem_receiver.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S05_UART_BAUD_RATE                  (115200U)
#define S05_UART_DEFAULT_TIMEOUT_MS         (1000U)
#define S05_UART_DMA_RX_BUFFER_SIZE         (256U)
#define S05_UART_RING_BUFFER_SIZE           (2048U)
#define S05_UART_WAIT_TIMEOUT_MS            (1000U)
#define S05_PROGRESS_LOG_INTERVAL_BYTES     (16U * 1024U)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static platform_spi_bus_t g_s05StorageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_s05Flash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_s05EepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s05EepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s05EepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s05Eeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_s05FirmwareStorage = FIRMWARE_STORAGE_INITIALIZER;
static platform_uart_t g_s05CommunicationUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_s05UartService = SERVICE_UART_INITIALIZER;
static platform_thread_t g_s05OwnerThread = {0};
static uint8_t g_s05UartDmaRxBuffer[S05_UART_DMA_RX_BUFFER_SIZE] = {0U};
static uint8_t g_s05UartRingBuffer[S05_UART_RING_BUFFER_SIZE] = {0U};
static s05_ymodem_flash_sink_t g_s05FlashSink = S05_YMODEM_FLASH_SINK_INITIALIZER;
static ymodem_receiver_t g_s05Receiver = YMODEM_RECEIVER_INITIALIZER;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static platform_error_t s05_ymodem_test_init_storage(void)
{
    platform_error_t result;

    result = platform_bsp_spi_construct_storage_bus(&g_s05StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_s05StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_s05StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(&g_s05Flash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(&g_s05Flash, &g_s05StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(&g_s05EepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(&g_s05EepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(&g_s05EepromI2c,
                               "s05_metadata_i2c",
                               &g_s05EepromScl,
                               &g_s05EepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(&g_s05Eeprom,
                                   &g_s05EepromI2c,
                                   PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(&g_s05FirmwareStorage,
                                 &g_s05Flash,
                                 &g_s05Eeprom);
}

static platform_error_t s05_ymodem_test_init_uart(void)
{
    const platform_uart_config_t uartConfig = {
        S05_UART_BAUD_RATE,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        S05_UART_DEFAULT_TIMEOUT_MS
    };
    service_uart_config_t serviceConfig = {0};
    platform_error_t result;

    result = platform_thread_get_current(&g_s05OwnerThread);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_uart_construct_communication(&g_s05CommunicationUart,
                                                       &uartConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((g_s05CommunicationUart.device.lifecycle == NULL) ||
        (g_s05CommunicationUart.device.lifecycle->init == NULL) ||
        (g_s05CommunicationUart.device.lifecycle->start == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = g_s05CommunicationUart.device.lifecycle->init(
        &g_s05CommunicationUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    serviceConfig.uart = &g_s05CommunicationUart;
    serviceConfig.dmaRxBuffer = g_s05UartDmaRxBuffer;
    serviceConfig.dmaRxBufferSize = sizeof(g_s05UartDmaRxBuffer);
    serviceConfig.ringBufferStorage = g_s05UartRingBuffer;
    serviceConfig.ringBufferStorageSize = sizeof(g_s05UartRingBuffer);
    serviceConfig.ownerThread = &g_s05OwnerThread;

    result = service_uart_init(&g_s05UartService, &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = g_s05CommunicationUart.device.lifecycle->start(
        &g_s05CommunicationUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return service_uart_start(&g_s05UartService);
}

static platform_error_t s05_ymodem_test_check_uart(void)
{
    service_uart_status_t status = {
        SERVICE_UART_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        PLATFORM_FALSE
    };
    platform_error_t result;

    result = service_uart_get_status(&g_s05UartService, &status);
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

static platform_bool_t s05_ymodem_test_is_active(void)
{
    return ((g_s05Receiver.context.state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
            (g_s05Receiver.context.state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) ||
            (g_s05Receiver.context.state == YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM) ||
            (g_s05Receiver.context.state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_error_t s05_ymodem_test_process_receiver(void)
{
    uint8_t buffer[YMODEM_CFG_UART_READ_BUFFER_SIZE] = {0U};
    platform_size_t readableSize;
    platform_size_t readLength;
    uint32_t events;
    uint32_t nowMs;
    uint32_t nextProgress = S05_PROGRESS_LOG_INTERVAL_BYTES;
    platform_size_t index;
    platform_error_t result;

    while (s05_ymodem_test_is_active() == PLATFORM_TRUE) {
        result = platform_time_get_ms(&nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = ymodem_receiver_tick(&g_s05Receiver, nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = s05_ymodem_test_check_uart();
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = service_uart_get_readable_size(&g_s05UartService, &readableSize);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        if (readableSize == 0U) {
            result = service_uart_wait_event(&g_s05UartService,
                                             S05_UART_WAIT_TIMEOUT_MS,
                                             &events);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return result;
            }
            continue;
        }

        result = service_uart_read(&g_s05UartService,
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

            result = ymodem_receiver_feed_byte(&g_s05Receiver,
                                               buffer[index],
                                               nowMs);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
        }

        if ((g_s05Receiver.context.receivedSize >= nextProgress) ||
            (g_s05Receiver.context.state == YMODEM_RECEIVER_STATE_FINISHED)) {
            SERVICE_LOG_I("[S05] progress=%lu/%lu",
                          (unsigned long)g_s05Receiver.context.receivedSize,
                          (unsigned long)g_s05Receiver.context.fileSize);
            nextProgress = g_s05Receiver.context.receivedSize +
                           S05_PROGRESS_LOG_INTERVAL_BYTES;
        }
    }

    return PLATFORM_ERR_OK;
}

static void s05_ymodem_test_log_result(
    const ymodem_receiver_status_t *status,
    const ymodem_receiver_statistics_t *statistics)
{
    SERVICE_LOG_I(
        "[S05] session state=%d error=%d filename=%s file_size=%lu received=%lu",
        (int)status->state,
        (int)status->lastError,
        status->filename,
        (unsigned long)status->fileSize,
        (unsigned long)status->receivedSize);
    SERVICE_LOG_I(
        "[S05] packets received=%lu accepted=%lu bytes=%lu/%lu crc=%lu sequence=%lu duplicate=%lu",
        (unsigned long)statistics->packetReceivedCount,
        (unsigned long)statistics->packetAcceptedCount,
        (unsigned long)statistics->bytesReceived,
        (unsigned long)statistics->bytesWritten,
        (unsigned long)statistics->crcErrorCount,
        (unsigned long)statistics->sequenceErrorCount,
        (unsigned long)statistics->duplicatePacketCount);
    SERVICE_LOG_I(
        "[S05] timeout=%lu retry=%lu cancel=%lu",
        (unsigned long)statistics->timeoutCount,
        (unsigned long)statistics->retryCount,
        (unsigned long)statistics->cancelCount);
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_s05_ymodem_test_run(void)
{
    ymodem_receiver_config_t receiverConfig = {0};
    ymodem_sink_t sinkContract = {0};
    ymodem_receiver_status_t status = {
        YMODEM_RECEIVER_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        0U,
        0U,
        {0}
    };
    ymodem_receiver_statistics_t statistics = {0};
    firmware_image_header_t validatedHeader = {0};
    firmware_image_validation_t validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    platform_error_t result;

    SERVICE_LOG_I("[S05] YMODEM board test start target=Slot B");

    result = s05_ymodem_test_init_storage();
    SERVICE_LOG_I("[S05] storage init result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s05_ymodem_test_init_uart();
    SERVICE_LOG_I("[S05] UART service init result=%d baud=%lu",
                  (int)result,
                  (unsigned long)S05_UART_BAUD_RATE);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s05_ymodem_flash_sink_init(&g_s05FlashSink,
                                        &g_s05FirmwareStorage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s05_ymodem_flash_sink_get_contract(&g_s05FlashSink,
                                                &sinkContract);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    receiverConfig.uart = &g_s05UartService;
    receiverConfig.sink = sinkContract;
    result = ymodem_receiver_init(&g_s05Receiver, &receiverConfig);
    if (result == PLATFORM_ERR_OK) {
        result = ymodem_receiver_start(&g_s05Receiver);
    }
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("[S05] receiver start failed error=%d", (int)result);
        return result;
    }

    SERVICE_LOG_I("[S05] YMODEM_READY");
    result = s05_ymodem_test_process_receiver();
    if ((result != PLATFORM_ERR_OK) &&
        (s05_ymodem_test_is_active() == PLATFORM_TRUE)) {
        (void)ymodem_receiver_cancel(&g_s05Receiver);
    }

    if (ymodem_receiver_get_status(&g_s05Receiver, &status) != PLATFORM_ERR_OK) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_IO : result;
    }
    if (ymodem_receiver_get_statistics(&g_s05Receiver, &statistics) != PLATFORM_ERR_OK) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_IO : result;
    }

    s05_ymodem_test_log_result(&status, &statistics);
    SERVICE_LOG_I(
        "[S05] flash payload=%lu header_commit=%u error=%d",
        (unsigned long)g_s05FlashSink.payloadWritten,
        (unsigned int)g_s05FlashSink.headerCommitted,
        (int)g_s05FlashSink.lastError);

    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("[S05] receiver processing failed error=%d", (int)result);
        return result;
    }

    if (status.state != YMODEM_RECEIVER_STATE_FINISHED) {
        SERVICE_LOG_E("[S05] session did not finish state=%d error=%d",
                      (int)status.state,
                      (int)status.lastError);
        return (status.lastError == PLATFORM_ERR_OK) ?
               PLATFORM_ERR_INVALID_STATE : status.lastError;
    }

    result = firmware_storage_validate_image(&g_s05FirmwareStorage,
                                             FIRMWARE_SLOT_B,
                                             &validatedHeader,
                                             &validation);
    SERVICE_LOG_I("[S05] Slot B validation result=%d validation=%u",
                  (int)result,
                  (unsigned int)validation);
    if ((result != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_VALID)) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_CHECKSUM : result;
    }

    SERVICE_LOG_I("[S05] YMODEM session complete final result=PASS");
    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
