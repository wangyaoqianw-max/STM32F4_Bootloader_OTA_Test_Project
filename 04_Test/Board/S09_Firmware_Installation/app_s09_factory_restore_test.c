/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s09_factory_restore_test.c
 * @brief S09 Factory Restore 板测实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include "app_s09_factory_restore_test.h"

#define LOG_TAG "s09_factory"

#include "firmware_storage.h"
#include "ota_firmware_sink.h"
#include "platform_at24c02.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_uart.h"
#include "platform_bsp_w25q64.h"
#include "platform_i2c.h"
#include "platform_os.h"
#include "platform_time.h"
#include "project_config.h"
#include "service_log.h"
#include "service_uart.h"
#include "ymodem_config.h"
#include "ymodem_receiver.h"

#include <stddef.h>
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S09_FACTORY_UART_BAUD_RATE             (115200U)
#define S09_FACTORY_UART_TIMEOUT_MS            (1000U)
#define S09_FACTORY_UART_DMA_BUFFER_SIZE       (256U)
#define S09_FACTORY_UART_RING_BUFFER_SIZE      (2048U)
#define S09_FACTORY_UART_WAIT_TIMEOUT_MS       (1000U)
#define S09_FACTORY_TASK_STACK_SIZE_BYTES      (4096U)
#define S09_FACTORY_TARGET_SLOT                FIRMWARE_SLOT_A
#define S09_FACTORY_METADATA_COMMIT_COUNT      (2U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static void s09_factory_restore_task(void *argument);
static platform_error_t s09_factory_init_storage(void);
static platform_error_t s09_factory_init_uart(void);
static platform_error_t s09_factory_erase_external_slots(void);
static platform_bool_t s09_factory_receiver_is_active(void);
static platform_error_t s09_factory_process_receiver(void);
static platform_error_t s09_factory_commit_baseline(
    const firmware_image_header_t *header);
static platform_error_t s09_factory_verify_baseline(
    const firmware_image_header_t *header);
static platform_error_t s09_factory_execute(void);
//******************************** Private Functions ************************//

//******************************** Variables ********************************//
static platform_spi_bus_t g_s09FactoryStorageSpiBus =
    PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_s09FactoryFlash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_s09FactoryEepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s09FactoryEepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s09FactoryEepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s09FactoryEeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_s09FactoryStorage = FIRMWARE_STORAGE_INITIALIZER;
static platform_uart_t g_s09FactoryUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_s09FactoryUartService = SERVICE_UART_INITIALIZER;
static platform_thread_t g_s09FactoryOwnerThread =
    PLATFORM_OS_OBJECT_INITIALIZER;
static uint8_t g_s09FactoryDmaBuffer[
    S09_FACTORY_UART_DMA_BUFFER_SIZE] = {0U};
static uint8_t g_s09FactoryRingBuffer[
    S09_FACTORY_UART_RING_BUFFER_SIZE] = {0U};
static ota_firmware_sink_t g_s09FactorySink = OTA_FIRMWARE_SINK_INITIALIZER;
static ymodem_receiver_t g_s09FactoryReceiver = YMODEM_RECEIVER_INITIALIZER;
static platform_thread_t g_s09FactoryThread = PLATFORM_OS_OBJECT_INITIALIZER;

static const platform_thread_config_t s_s09_factory_thread_config = {
    .name = "s09Factory",
    .entry = s09_factory_restore_task,
    .argument = NULL,
    .stackSizeBytes = S09_FACTORY_TASK_STACK_SIZE_BYTES,
    .priority = PLATFORM_THREAD_PRIORITY_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static platform_error_t s09_factory_init_storage(void)
{
    platform_error_t result;

    result = platform_bsp_spi_construct_storage_bus(
        &g_s09FactoryStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_s09FactoryStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_s09FactoryStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(&g_s09FactoryFlash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(
        &g_s09FactoryFlash,
        &g_s09FactoryStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(
        &g_s09FactoryEepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(
        &g_s09FactoryEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(
        &g_s09FactoryEepromI2c,
        "s09_factory_i2c",
        &g_s09FactoryEepromScl,
        &g_s09FactoryEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(
        &g_s09FactoryEeprom,
        &g_s09FactoryEepromI2c,
        PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(
        &g_s09FactoryStorage,
        &g_s09FactoryFlash,
        &g_s09FactoryEeprom);
}

static platform_error_t s09_factory_init_uart(void)
{
    const platform_uart_config_t uartConfig = {
        S09_FACTORY_UART_BAUD_RATE,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        S09_FACTORY_UART_TIMEOUT_MS
    };
    service_uart_config_t serviceConfig = {0};
    platform_error_t result;

    result = platform_thread_get_current(&g_s09FactoryOwnerThread);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_uart_construct_communication(
        &g_s09FactoryUart,
        &uartConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((g_s09FactoryUart.device.lifecycle == NULL) ||
        (g_s09FactoryUart.device.lifecycle->init == NULL) ||
        (g_s09FactoryUart.device.lifecycle->start == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = g_s09FactoryUart.device.lifecycle->init(&g_s09FactoryUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    serviceConfig.uart = &g_s09FactoryUart;
    serviceConfig.dmaRxBuffer = g_s09FactoryDmaBuffer;
    serviceConfig.dmaRxBufferSize = sizeof(g_s09FactoryDmaBuffer);
    serviceConfig.ringBufferStorage = g_s09FactoryRingBuffer;
    serviceConfig.ringBufferStorageSize = sizeof(g_s09FactoryRingBuffer);
    serviceConfig.ownerThread = &g_s09FactoryOwnerThread;

    result = service_uart_init(
        &g_s09FactoryUartService,
        &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = g_s09FactoryUart.device.lifecycle->start(&g_s09FactoryUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return service_uart_start(&g_s09FactoryUartService);
}

static platform_error_t s09_factory_erase_external_slots(void)
{
    platform_error_t result;

    SERVICE_LOG_W("[S09-FACTORY] destructive erase Slot A/B start");
    result = firmware_storage_erase_slot(
        &g_s09FactoryStorage,
        FIRMWARE_SLOT_A,
        FIRMWARE_SLOT_PAYLOAD_CAPACITY);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_erase_slot(
        &g_s09FactoryStorage,
        FIRMWARE_SLOT_B,
        FIRMWARE_SLOT_PAYLOAD_CAPACITY);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    SERVICE_LOG_I("[S09-FACTORY] destructive erase Slot A/B PASS");
    return PLATFORM_ERR_OK;
}

static platform_bool_t s09_factory_receiver_is_active(void)
{
    ymodem_receiver_state_t state = g_s09FactoryReceiver.context.state;

    return ((state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
            (state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) ||
            (state == YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM) ||
            (state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_error_t s09_factory_process_receiver(void)
{
    uint8_t buffer[YMODEM_CFG_UART_READ_BUFFER_SIZE] = {0U};
    platform_size_t readableSize;
    platform_size_t readLength;
    uint32_t events;
    uint32_t nowMs;
    platform_size_t index;
    platform_error_t result;

    while (s09_factory_receiver_is_active() == PLATFORM_TRUE) {
        result = platform_time_get_ms(&nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = ymodem_receiver_tick(&g_s09FactoryReceiver, nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = service_uart_get_readable_size(
            &g_s09FactoryUartService,
            &readableSize);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        if (readableSize == 0U) {
            result = service_uart_wait_event(
                &g_s09FactoryUartService,
                S09_FACTORY_UART_WAIT_TIMEOUT_MS,
                &events);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return result;
            }
            continue;
        }

        result = service_uart_read(
            &g_s09FactoryUartService,
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

            result = ymodem_receiver_feed_byte(
                &g_s09FactoryReceiver,
                buffer[index],
                nowMs);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t s09_factory_commit_baseline(
    const firmware_image_header_t *header)
{
    firmware_metadata_t metadata = {0};
    firmware_metadata_copy_id_t committedCopy = FIRMWARE_METADATA_COPY_NONE;
    uint32_t commitIndex;
    platform_error_t result;

    if (header == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    metadata.confirmedSlot = FIRMWARE_SLOT_A;
    metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    metadata.confirmedVersion = header->version;

    for (commitIndex = 0U;
         commitIndex < S09_FACTORY_METADATA_COMMIT_COUNT;
         commitIndex++) {
        result = firmware_storage_commit_metadata(
            &g_s09FactoryStorage,
            &metadata,
            &committedCopy);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t s09_factory_verify_baseline(
    const firmware_image_header_t *header)
{
    firmware_metadata_t metadata = {0};
    firmware_image_header_t slotAHeader = {0};
    firmware_image_header_t slotBHeader = {0};
    firmware_image_validation_t slotAValidation =
        FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    firmware_image_validation_t slotBValidation =
        FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    firmware_metadata_copy_id_t sourceCopy = FIRMWARE_METADATA_COPY_NONE;
    platform_error_t result;

    if (header == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = firmware_storage_load_metadata(
        &g_s09FactoryStorage,
        &metadata,
        &sourceCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_validate_image(
        &g_s09FactoryStorage,
        FIRMWARE_SLOT_A,
        &slotAHeader,
        &slotAValidation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_read_header(
        &g_s09FactoryStorage,
        FIRMWARE_SLOT_B,
        &slotBHeader,
        &slotBValidation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((slotAValidation != FIRMWARE_IMAGE_VALIDATION_VALID) ||
        (slotBValidation != FIRMWARE_IMAGE_VALIDATION_EMPTY) ||
        (metadata.confirmedSlot != FIRMWARE_SLOT_A) ||
        (metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (metadata.slotAState != FIRMWARE_SLOT_STATE_VALID) ||
        (metadata.slotBState != FIRMWARE_SLOT_STATE_EMPTY) ||
        (metadata.upgradeState != FIRMWARE_UPGRADE_STATE_NONE) ||
        (metadata.confirmedVersion.major != header->version.major) ||
        (metadata.confirmedVersion.minor != header->version.minor) ||
        (metadata.confirmedVersion.patch != header->version.patch)) {
        return PLATFORM_ERR_IO;
    }

    SERVICE_LOG_I(
        "[S09-FACTORY] baseline PASS copy=%u sequence=%lu "
        "Slot A=%u.%u.%u VALID Slot B=EMPTY confirmed=A "
        "pending=NONE upgrade=NONE",
        (unsigned int)sourceCopy,
        (unsigned long)metadata.sequence,
        (unsigned int)slotAHeader.version.major,
        (unsigned int)slotAHeader.version.minor,
        (unsigned int)slotAHeader.version.patch);
    return PLATFORM_ERR_OK;
}

static platform_error_t s09_factory_execute(void)
{
    ymodem_receiver_config_t receiverConfig = {0};
    ymodem_sink_t sinkContract = {0};
    ymodem_receiver_status_t receiverStatus = {
        YMODEM_RECEIVER_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        {{0}},
        0U
    };
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    platform_error_t result;

    SERVICE_LOG_I("[S09-FACTORY] start target=Slot A");

    result = s09_factory_init_storage();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s09_factory_erase_external_slots();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s09_factory_init_uart();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_init(
        &g_s09FactorySink,
        &g_s09FactoryStorage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_set_target_slot(
        &g_s09FactorySink,
        S09_FACTORY_TARGET_SLOT);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_get_contract(
        &g_s09FactorySink,
        &sinkContract);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    receiverConfig.uart = &g_s09FactoryUartService;
    receiverConfig.sink = sinkContract;
    result = ymodem_receiver_init(
        &g_s09FactoryReceiver,
        &receiverConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ymodem_receiver_start(&g_s09FactoryReceiver);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    SERVICE_LOG_I("[S09-FACTORY] YMODEM_READY");
    result = s09_factory_process_receiver();
    if ((result != PLATFORM_ERR_OK) &&
        (s09_factory_receiver_is_active() == PLATFORM_TRUE)) {
        (void)ymodem_receiver_cancel(&g_s09FactoryReceiver);
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ymodem_receiver_get_status(
        &g_s09FactoryReceiver,
        &receiverStatus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (receiverStatus.state != YMODEM_RECEIVER_STATE_FINISHED) {
        return (receiverStatus.lastError == PLATFORM_ERR_OK) ?
               PLATFORM_ERR_INVALID_STATE : receiverStatus.lastError;
    }

    result = firmware_storage_validate_image(
        &g_s09FactoryStorage,
        S09_FACTORY_TARGET_SLOT,
        &header,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return PLATFORM_ERR_CHECKSUM;
    }

    result = s09_factory_commit_baseline(&header);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return s09_factory_verify_baseline(&header);
}

static void s09_factory_restore_task(void *argument)
{
    platform_error_t result;

    (void)argument;

    result = s09_factory_execute();
    SERVICE_LOG_I("[S09-FACTORY] worker result=%d", (int)result);

    for (;;) {
        (void)platform_time_delay_ms(S09_FACTORY_UART_WAIT_TIMEOUT_MS);
    }
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_s09_factory_restore_test_run(void)
{
    return platform_thread_create(
        &g_s09FactoryThread,
        &s_s09_factory_thread_config);
}
//******************************** Functions ********************************//
