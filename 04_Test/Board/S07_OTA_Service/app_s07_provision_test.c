/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s07_provision_test.c
 * @brief S07 Factory Provisioning 板测实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_s07_provision_test.h"

#define LOG_TAG "s07_provision"

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
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S07_PROVISION_UART_BAUD_RATE             (115200U)
#define S07_PROVISION_UART_TIMEOUT_MS            (1000U)
#define S07_PROVISION_UART_DMA_BUFFER_SIZE      (256U)
#define S07_PROVISION_UART_RING_BUFFER_SIZE     (2048U)
#define S07_PROVISION_UART_WAIT_TIMEOUT_MS      (1000U)
#define S07_PROVISION_TASK_STACK_SIZE_BYTES     (4096U)
#define S07_PROVISION_TARGET_SLOT               FIRMWARE_SLOT_A
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static void s07_provision_test_task(void *argument);
static platform_error_t s07_provision_init_storage(void);
static platform_error_t s07_provision_init_uart(void);
static platform_error_t s07_provision_check_device(
    platform_bool_t *clearLegacySlotB,
    uint32_t *legacySlotBImageSize);
static platform_bool_t s07_provision_receiver_is_active(void);
static platform_error_t s07_provision_process_receiver(void);
static platform_error_t s07_provision_commit_baseline(
    const firmware_image_header_t *header);
static platform_error_t s07_provision_execute(void);
//******************************** Private Functions ************************//

//******************************** Variables ********************************//
static platform_spi_bus_t g_s07ProvisionStorageSpiBus =
    PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_s07ProvisionFlash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_s07ProvisionEepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s07ProvisionEepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s07ProvisionEepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s07ProvisionEeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_s07ProvisionStorage = FIRMWARE_STORAGE_INITIALIZER;
static platform_uart_t g_s07ProvisionUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_s07ProvisionUartService = SERVICE_UART_INITIALIZER;
static platform_thread_t g_s07ProvisionOwnerThread =
    PLATFORM_OS_OBJECT_INITIALIZER;
static uint8_t g_s07ProvisionDmaBuffer[
    S07_PROVISION_UART_DMA_BUFFER_SIZE] = {0U};
static uint8_t g_s07ProvisionRingBuffer[
    S07_PROVISION_UART_RING_BUFFER_SIZE] = {0U};
static ota_firmware_sink_t g_s07ProvisionSink = OTA_FIRMWARE_SINK_INITIALIZER;
static ymodem_receiver_t g_s07ProvisionReceiver = YMODEM_RECEIVER_INITIALIZER;
static platform_thread_t g_s07ProvisionThread = PLATFORM_OS_OBJECT_INITIALIZER;

static const platform_thread_config_t s_s07_provision_thread_config = {
    .name = "s07Provision",
    .entry = s07_provision_test_task,
    .argument = NULL,
    .stackSizeBytes = S07_PROVISION_TASK_STACK_SIZE_BYTES,
    .priority = PLATFORM_THREAD_PRIORITY_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static platform_error_t s07_provision_init_storage(void)
{
    platform_error_t result;

    result = platform_bsp_spi_construct_storage_bus(
        &g_s07ProvisionStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_s07ProvisionStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_s07ProvisionStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(&g_s07ProvisionFlash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(
        &g_s07ProvisionFlash,
        &g_s07ProvisionStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(
        &g_s07ProvisionEepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(
        &g_s07ProvisionEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(
        &g_s07ProvisionEepromI2c,
        "s07_provision_i2c",
        &g_s07ProvisionEepromScl,
        &g_s07ProvisionEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(
        &g_s07ProvisionEeprom,
        &g_s07ProvisionEepromI2c,
        PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(
        &g_s07ProvisionStorage,
        &g_s07ProvisionFlash,
        &g_s07ProvisionEeprom);
}

static platform_error_t s07_provision_init_uart(void)
{
    const platform_uart_config_t uartConfig = {
        S07_PROVISION_UART_BAUD_RATE,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        S07_PROVISION_UART_TIMEOUT_MS
    };
    service_uart_config_t serviceConfig = {0};
    platform_error_t result;

    result = platform_thread_get_current(&g_s07ProvisionOwnerThread);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_uart_construct_communication(
        &g_s07ProvisionUart,
        &uartConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((g_s07ProvisionUart.device.lifecycle == NULL) ||
        (g_s07ProvisionUart.device.lifecycle->init == NULL) ||
        (g_s07ProvisionUart.device.lifecycle->start == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = g_s07ProvisionUart.device.lifecycle->init(
        &g_s07ProvisionUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    serviceConfig.uart = &g_s07ProvisionUart;
    serviceConfig.dmaRxBuffer = g_s07ProvisionDmaBuffer;
    serviceConfig.dmaRxBufferSize = sizeof(g_s07ProvisionDmaBuffer);
    serviceConfig.ringBufferStorage = g_s07ProvisionRingBuffer;
    serviceConfig.ringBufferStorageSize = sizeof(g_s07ProvisionRingBuffer);
    serviceConfig.ownerThread = &g_s07ProvisionOwnerThread;

    result = service_uart_init(
        &g_s07ProvisionUartService,
        &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = g_s07ProvisionUart.device.lifecycle->start(
        &g_s07ProvisionUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return service_uart_start(&g_s07ProvisionUartService);
}

static platform_error_t s07_provision_check_device(
    platform_bool_t *clearLegacySlotB,
    uint32_t *legacySlotBImageSize)
{
    firmware_metadata_t metadata = {0};
    firmware_image_header_t slotAHeader = {0};
    firmware_image_header_t slotBHeader = {0};
    firmware_image_validation_t validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    firmware_metadata_copy_id_t sourceCopy = FIRMWARE_METADATA_COPY_NONE;
    platform_bool_t metadataPresent = PLATFORM_FALSE;
    platform_error_t result;

    if ((clearLegacySlotB == NULL) || (legacySlotBImageSize == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *clearLegacySlotB = PLATFORM_FALSE;
    *legacySlotBImageSize = 0U;

    result = firmware_storage_load_metadata(
        &g_s07ProvisionStorage,
        &metadata,
        &sourceCopy);
    if ((result != PLATFORM_ERR_OK) && (result != PLATFORM_ERR_NOT_FOUND)) {
        return result;
    }
    metadataPresent = (result == PLATFORM_ERR_OK) ?
                      PLATFORM_TRUE : PLATFORM_FALSE;

    result = firmware_storage_read_header(
        &g_s07ProvisionStorage,
        FIRMWARE_SLOT_A,
        &slotAHeader,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (validation != FIRMWARE_IMAGE_VALIDATION_EMPTY) {
        SERVICE_LOG_E(
            "[S07-PROVISION] Slot A is not empty validation=%u",
            (unsigned int)validation);
        return PLATFORM_ERR_INVALID_STATE;
    }

    validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    result = firmware_storage_read_header(
        &g_s07ProvisionStorage,
        FIRMWARE_SLOT_B,
        &slotBHeader,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (metadataPresent == PLATFORM_FALSE) {
        if (validation != FIRMWARE_IMAGE_VALIDATION_EMPTY) {
            SERVICE_LOG_E(
                "[S07-PROVISION] empty Metadata but Slot B is not empty validation=%u",
                (unsigned int)validation);
            return PLATFORM_ERR_INVALID_STATE;
        }
        return PLATFORM_ERR_OK;
    }

    if ((metadata.confirmedSlot != FIRMWARE_SLOT_NONE) ||
        (metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (metadata.slotAState != FIRMWARE_SLOT_STATE_EMPTY) ||
        (metadata.slotBState != FIRMWARE_SLOT_STATE_VALID) ||
        (metadata.upgradeState != FIRMWARE_UPGRADE_STATE_NONE) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_VALID) ||
        (metadata.confirmedVersion.major != slotBHeader.version.major) ||
        (metadata.confirmedVersion.minor != slotBHeader.version.minor) ||
        (metadata.confirmedVersion.patch != slotBHeader.version.patch)) {
        SERVICE_LOG_E("[S07-PROVISION] existing state is not approved S04 baseline");
        return PLATFORM_ERR_INVALID_STATE;
    }

    *clearLegacySlotB = PLATFORM_TRUE;
    *legacySlotBImageSize = slotBHeader.imageSize;
    SERVICE_LOG_W(
        "[S07-PROVISION] approved S04 baseline migration Slot B=%u.%u.%u",
        (unsigned int)slotBHeader.version.major,
        (unsigned int)slotBHeader.version.minor,
        (unsigned int)slotBHeader.version.patch);
    return PLATFORM_ERR_OK;
}

static platform_bool_t s07_provision_receiver_is_active(void)
{
    ymodem_receiver_state_t state = g_s07ProvisionReceiver.context.state;

    return ((state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
            (state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) ||
            (state == YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM) ||
            (state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_error_t s07_provision_process_receiver(void)
{
    uint8_t buffer[YMODEM_CFG_UART_READ_BUFFER_SIZE] = {0U};
    platform_size_t readableSize;
    platform_size_t readLength;
    uint32_t events;
    uint32_t nowMs;
    platform_size_t index;
    platform_error_t result;

    while (s07_provision_receiver_is_active() == PLATFORM_TRUE) {
        result = platform_time_get_ms(&nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = ymodem_receiver_tick(&g_s07ProvisionReceiver, nowMs);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = service_uart_get_readable_size(
            &g_s07ProvisionUartService,
            &readableSize);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        if (readableSize == 0U) {
            result = service_uart_wait_event(
                &g_s07ProvisionUartService,
                S07_PROVISION_UART_WAIT_TIMEOUT_MS,
                &events);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return result;
            }
            continue;
        }

        result = service_uart_read(
            &g_s07ProvisionUartService,
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
                &g_s07ProvisionReceiver,
                buffer[index],
                nowMs);
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t s07_provision_commit_baseline(
    const firmware_image_header_t *header)
{
    firmware_metadata_t metadata = {0};
    firmware_metadata_t loadedMetadata = {0};
    firmware_metadata_copy_id_t sourceCopy = FIRMWARE_METADATA_COPY_NONE;
    firmware_metadata_copy_id_t committedCopy = FIRMWARE_METADATA_COPY_NONE;
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

    result = firmware_storage_commit_metadata(
        &g_s07ProvisionStorage,
        &metadata,
        &committedCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_load_metadata(
        &g_s07ProvisionStorage,
        &loadedMetadata,
        &sourceCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((loadedMetadata.confirmedSlot != FIRMWARE_SLOT_A) ||
        (loadedMetadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (loadedMetadata.slotAState != FIRMWARE_SLOT_STATE_VALID) ||
        (loadedMetadata.slotBState != FIRMWARE_SLOT_STATE_EMPTY) ||
        (loadedMetadata.upgradeState != FIRMWARE_UPGRADE_STATE_NONE) ||
        (loadedMetadata.confirmedVersion.major != header->version.major) ||
        (loadedMetadata.confirmedVersion.minor != header->version.minor) ||
        (loadedMetadata.confirmedVersion.patch != header->version.patch)) {
        return PLATFORM_ERR_IO;
    }

    SERVICE_LOG_I(
        "[S07-PROVISION] baseline PASS copy=%u sequence=%lu Slot A=%u.%u.%u",
        (unsigned int)sourceCopy,
        (unsigned long)loadedMetadata.sequence,
        (unsigned int)loadedMetadata.confirmedVersion.major,
        (unsigned int)loadedMetadata.confirmedVersion.minor,
        (unsigned int)loadedMetadata.confirmedVersion.patch);
    return PLATFORM_ERR_OK;
}

static platform_error_t s07_provision_execute(void)
{
    ymodem_receiver_config_t receiverConfig = {0};
    ymodem_sink_t sinkContract = {0};
    ymodem_receiver_status_t receiverStatus = {0};
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    platform_bool_t clearLegacySlotB = PLATFORM_FALSE;
    uint32_t legacySlotBImageSize = 0U;
    platform_error_t result;

    SERVICE_LOG_I("[S07-PROVISION] start target=Slot A");

    result = s07_provision_init_storage();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s07_provision_check_device(
        &clearLegacySlotB,
        &legacySlotBImageSize);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s07_provision_init_uart();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_init(
        &g_s07ProvisionSink,
        &g_s07ProvisionStorage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_set_target_slot(
        &g_s07ProvisionSink,
        S07_PROVISION_TARGET_SLOT);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_get_contract(
        &g_s07ProvisionSink,
        &sinkContract);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    receiverConfig.uart = &g_s07ProvisionUartService;
    receiverConfig.sink = sinkContract;
    result = ymodem_receiver_init(
        &g_s07ProvisionReceiver,
        &receiverConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ymodem_receiver_start(&g_s07ProvisionReceiver);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    SERVICE_LOG_I("[S07-PROVISION] YMODEM_READY");
    result = s07_provision_process_receiver();
    if ((result != PLATFORM_ERR_OK) &&
        (s07_provision_receiver_is_active() == PLATFORM_TRUE)) {
        (void)ymodem_receiver_cancel(&g_s07ProvisionReceiver);
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ymodem_receiver_get_status(
        &g_s07ProvisionReceiver,
        &receiverStatus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (receiverStatus.state != YMODEM_RECEIVER_STATE_FINISHED) {
        return (receiverStatus.lastError == PLATFORM_ERR_OK) ?
               PLATFORM_ERR_INVALID_STATE : receiverStatus.lastError;
    }

    result = firmware_storage_validate_image(
        &g_s07ProvisionStorage,
        S07_PROVISION_TARGET_SLOT,
        &header,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return PLATFORM_ERR_CHECKSUM;
    }

    if (clearLegacySlotB == PLATFORM_TRUE) {
        result = firmware_storage_erase_slot(
            &g_s07ProvisionStorage,
            FIRMWARE_SLOT_B,
            legacySlotBImageSize);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return s07_provision_commit_baseline(&header);
}

static void s07_provision_test_task(void *argument)
{
    platform_error_t result;

    (void)argument;

    result = s07_provision_execute();
    SERVICE_LOG_I("[S07-PROVISION] worker result=%d", (int)result);

    for (;;) {
        (void)platform_time_delay_ms(S07_PROVISION_UART_WAIT_TIMEOUT_MS);
    }
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_s07_provision_test_run(void)
{
    return platform_thread_create(
        &g_s07ProvisionThread,
        &s_s07_provision_thread_config);
}
//******************************** Functions ********************************//
