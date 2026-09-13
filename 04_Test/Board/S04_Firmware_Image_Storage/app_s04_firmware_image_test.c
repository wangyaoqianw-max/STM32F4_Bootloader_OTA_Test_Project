/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s04_firmware_image_test.c
 * @brief S04 Firmware Image UART 注入板测实现
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_s04_firmware_image_test.h"

#define LOG_TAG "s04_image_test"

#include "crc.h"
#include "firmware_storage.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_uart.h"
#include "platform_bsp_w25q64.h"
#include "platform_i2c.h"
#include "platform_os.h"
#include "project_config.h"
#include "service_log.h"
#include "service_uart.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S04_UART_BAUD_RATE                  (115200U)
#define S04_UART_DEFAULT_TIMEOUT_MS         (1000U)
#define S04_UART_DMA_RX_BUFFER_SIZE         (256U)
#define S04_UART_RING_BUFFER_SIZE           (2048U)
#define S04_UART_READ_BUFFER_SIZE           (256U)
#define S04_UART_WAIT_TIMEOUT_MS            (1000U)
#define S04_PROGRESS_LOG_INTERVAL_BYTES     (16U * 1024U)
#define S04_METADATA_COMMIT_MARKER_OFFSET   (0x7CU)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static platform_spi_bus_t g_s04StorageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_s04Flash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_s04EepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s04EepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s04EepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s04Eeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_s04FirmwareStorage = FIRMWARE_STORAGE_INITIALIZER;
static platform_uart_t g_s04CommunicationUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_s04UartService = SERVICE_UART_INITIALIZER;
static platform_thread_t g_s04OwnerThread = {0};
static uint8_t g_s04UartDmaRxBuffer[S04_UART_DMA_RX_BUFFER_SIZE] = {0U};
static uint8_t g_s04UartRingBuffer[S04_UART_RING_BUFFER_SIZE] = {0U};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
/**
 * @brief 构造并初始化板测使用的 Storage SPI、W25Q64、Software I2C 和 AT24C02
 * @return platform_error_t : 首个失败的构造或初始化结果
 */
static platform_error_t s04_firmware_image_init_storage(void)
{
    platform_error_t result = PLATFORM_ERR_OK;

    result = platform_bsp_spi_construct_storage_bus(&g_s04StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_s04StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_s04StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(&g_s04Flash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(&g_s04Flash, &g_s04StorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(&g_s04EepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(&g_s04EepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(&g_s04EepromI2c,
                               "s04_metadata_i2c",
                               &g_s04EepromScl,
                               &g_s04EepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(&g_s04Eeprom,
                                   &g_s04EepromI2c,
                                   PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(&g_s04FirmwareStorage,
                                 &g_s04Flash,
                                 &g_s04Eeprom);
}

/**
 * @brief 构造并启动通信 UART 与 UART Service
 * @return platform_error_t : UART 生命周期或 Service 初始化结果
 * @note ownerThread 必须绑定当前 appSystem Task，供 ISR 事件唤醒唯一 Consumer。
 */
static platform_error_t s04_firmware_image_init_uart(void)
{
    const platform_uart_config_t uartConfig = {
        S04_UART_BAUD_RATE,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        S04_UART_DEFAULT_TIMEOUT_MS
    };
    service_uart_config_t serviceConfig = {0};
    platform_error_t result = PLATFORM_ERR_OK;

    result = platform_thread_get_current(&g_s04OwnerThread);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_uart_construct_communication(&g_s04CommunicationUart,
                                                       &uartConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((g_s04CommunicationUart.device.lifecycle == NULL) ||
        (g_s04CommunicationUart.device.lifecycle->init == NULL) ||
        (g_s04CommunicationUart.device.lifecycle->start == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = g_s04CommunicationUart.device.lifecycle->init(
        &g_s04CommunicationUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = g_s04CommunicationUart.device.lifecycle->start(
        &g_s04CommunicationUart);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    serviceConfig.uart = &g_s04CommunicationUart;
    serviceConfig.dmaRxBuffer = g_s04UartDmaRxBuffer;
    serviceConfig.dmaRxBufferSize = sizeof(g_s04UartDmaRxBuffer);
    serviceConfig.ringBufferStorage = g_s04UartRingBuffer;
    serviceConfig.ringBufferStorageSize = sizeof(g_s04UartRingBuffer);
    serviceConfig.ownerThread = &g_s04OwnerThread;

    result = service_uart_init(&g_s04UartService, &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return service_uart_start(&g_s04UartService);
}

/**
 * @brief 检查 UART Service 是否仍可安全接收镜像数据
 * @return PLATFORM_ERR_OK 表示无丢包且 Service 仍处于 RUNNING
 * @return PLATFORM_ERR_OVERFLOW 表示当前 RX Session 已发生数据丢失
 * @return 其他值表示状态查询或 UART 运行错误
 */
static platform_error_t s04_firmware_image_check_uart(void)
{
    service_uart_status_t status = {
        SERVICE_UART_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        PLATFORM_FALSE
    };
    platform_error_t result = service_uart_get_status(&g_s04UartService,
                                                      &status);

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

/**
 * @brief 从 UART Service 累计读取恰好指定长度的数据
 * @param[out] buffer : 接收数据输出缓冲区
 * @param[in] dataLength : 必须累计接收的精确字节数
 * @return platform_error_t : 接收、等待或 UART 状态结果
 * @note wait timeout 只用于周期性重查状态，不作为文件结束条件。
 */
static platform_error_t s04_firmware_image_receive_exact(
    uint8_t *buffer,
    platform_size_t dataLength)
{
    platform_size_t receivedLength = 0U;
    platform_size_t readLength = 0U;
    uint32_t events = 0U;
    platform_error_t result = PLATFORM_ERR_OK;

    if ((buffer == NULL) || (dataLength == 0U)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    while (receivedLength < dataLength) {
        result = s04_firmware_image_check_uart();
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = service_uart_read(&g_s04UartService,
                                   &buffer[receivedLength],
                                   dataLength - receivedLength,
                                   &readLength);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        receivedLength += readLength;
        if (receivedLength < dataLength) {
            result = service_uart_wait_event(&g_s04UartService,
                                             S04_UART_WAIT_TIMEOUT_MS,
                                             &events);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return result;
            }
        }
    }

    return PLATFORM_ERR_OK;
}

/**
 * @brief 接收 Payload、逐块写入 Slot B 并同步计算 CRC32
 * @param[in] header : 已完成格式验证的镜像 Header
 * @param[out] calculatedCrc32 : 完整 Payload 的流式 CRC32
 * @return platform_error_t : UART、Flash 写入或长度检查结果
 * @note 每次读取前限制到 remaining，确保写地址与累计长度绝不越过 imageSize。
 */
static platform_error_t s04_firmware_image_receive_payload(
    const firmware_image_header_t *header,
    uint32_t *calculatedCrc32)
{
    crc32_iso_hdlc_context_t crcContext = {0U};
    uint8_t buffer[S04_UART_READ_BUFFER_SIZE] = {0U};
    uint32_t receivedLength = 0U;
    uint32_t nextProgress = S04_PROGRESS_LOG_INTERVAL_BYTES;
    uint32_t events = 0U;
    platform_size_t readableSize = 0U;
    platform_size_t readLength = 0U;
    platform_size_t requestedLength = 0U;
    platform_error_t result = PLATFORM_ERR_OK;

    if ((header == NULL) || (calculatedCrc32 == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    crc32_iso_hdlc_init(&crcContext);
    while (receivedLength < header->imageSize) {
        result = s04_firmware_image_check_uart();
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = service_uart_get_readable_size(&g_s04UartService,
                                                &readableSize);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        if (readableSize == 0U) {
            result = service_uart_wait_event(&g_s04UartService,
                                             S04_UART_WAIT_TIMEOUT_MS,
                                             &events);
            if ((result != PLATFORM_ERR_OK) &&
                (result != PLATFORM_ERR_TIMEOUT)) {
                return result;
            }
            continue;
        }

        requestedLength = header->imageSize - receivedLength;
        if (requestedLength > sizeof(buffer)) {
            requestedLength = sizeof(buffer);
        }

        result = service_uart_read(&g_s04UartService,
                                   buffer,
                                   requestedLength,
                                   &readLength);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        result = platform_w25q64_write(
            &g_s04Flash,
            FIRMWARE_SLOT_B_BASE + FIRMWARE_PAYLOAD_OFFSET + receivedLength,
            buffer,
            readLength);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        crc32_iso_hdlc_update(&crcContext, buffer, readLength);
        receivedLength += readLength;
        if ((receivedLength >= nextProgress) ||
            (receivedLength == header->imageSize)) {
            SERVICE_LOG_I("[S04] payload progress=%lu/%lu",
                          (unsigned long)receivedLength,
                          (unsigned long)header->imageSize);
            nextProgress = receivedLength + S04_PROGRESS_LOG_INTERVAL_BYTES;
        }
    }

    *calculatedCrc32 = crc32_iso_hdlc_finalize(&crcContext);
    return PLATFORM_ERR_OK;
}

/**
 * @brief 连续提交两次 Metadata，并破坏最新副本的 commit marker 验证旧副本恢复
 * @param[in] header : 已从 Slot B 完整回读验证的镜像 Header
 * @return platform_error_t : Metadata 提交、选择或单副本恢复结果
 * @warning 本测试会改写双副本并故意破坏最新副本，仅用于 S04 板测数据。
 */
static platform_error_t s04_firmware_image_test_metadata(
    const firmware_image_header_t *header)
{
    static const uint8_t invalidMarker[4U] = {
        0xFFU, 0xFFU, 0xFFU, 0xFFU
    };
    firmware_metadata_t metadata = {0};
    firmware_metadata_t loadedMetadata = {0};
    firmware_metadata_copy_id_t sourceCopy = FIRMWARE_METADATA_COPY_NONE;
    firmware_metadata_copy_id_t firstCopy = FIRMWARE_METADATA_COPY_NONE;
    firmware_metadata_copy_id_t secondCopy = FIRMWARE_METADATA_COPY_NONE;
    uint32_t corruptAddress = 0U;
    platform_error_t result = PLATFORM_ERR_OK;

    result = firmware_storage_load_metadata(&g_s04FirmwareStorage,
                                            &metadata,
                                            &sourceCopy);
    if (result == PLATFORM_ERR_NOT_FOUND) {
        metadata.sequence = 0U;
        metadata.activeSlot = FIRMWARE_SLOT_NONE;
        metadata.confirmedSlot = FIRMWARE_SLOT_NONE;
        metadata.slotAState = FIRMWARE_SLOT_STATE_EMPTY;
        metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
        metadata.confirmedVersion = header->version;
        SERVICE_LOG_I("[S04] metadata initial state: no valid copy");
    } else if (result == PLATFORM_ERR_OK) {
        metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
        SERVICE_LOG_I("[S04] metadata load copy=%u sequence=%lu",
                      (unsigned int)sourceCopy,
                      (unsigned long)metadata.sequence);
    } else {
        return result;
    }

    result = firmware_storage_commit_metadata(&g_s04FirmwareStorage,
                                              &metadata,
                                              &firstCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_load_metadata(&g_s04FirmwareStorage,
                                            &metadata,
                                            &sourceCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_commit_metadata(&g_s04FirmwareStorage,
                                              &metadata,
                                              &secondCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_load_metadata(&g_s04FirmwareStorage,
                                            &loadedMetadata,
                                            &sourceCopy);
    if ((result != PLATFORM_ERR_OK) || (sourceCopy != secondCopy)) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_IO : result;
    }

    SERVICE_LOG_I("[S04] metadata commits first=%u second=%u selected=%u sequence=%lu",
                  (unsigned int)firstCopy,
                  (unsigned int)secondCopy,
                  (unsigned int)sourceCopy,
                  (unsigned long)loadedMetadata.sequence);

    corruptAddress = (sourceCopy == FIRMWARE_METADATA_COPY_A) ?
                     FIRMWARE_METADATA_COPY_A_ADDRESS :
                     FIRMWARE_METADATA_COPY_B_ADDRESS;
    SERVICE_LOG_I("[S04] destructive metadata copy corruption copy=%u",
                  (unsigned int)sourceCopy);
    result = platform_at24c02_write(
        &g_s04Eeprom,
        corruptAddress + S04_METADATA_COMMIT_MARKER_OFFSET,
        invalidMarker,
        sizeof(invalidMarker));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_load_metadata(&g_s04FirmwareStorage,
                                            &loadedMetadata,
                                            &sourceCopy);
    if ((result != PLATFORM_ERR_OK) || (sourceCopy != firstCopy)) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_IO : result;
    }

    SERVICE_LOG_I("[S04] metadata single-copy recovery selected=%u sequence=%lu PASS",
                  (unsigned int)sourceCopy,
                  (unsigned long)loadedMetadata.sequence);
    return PLATFORM_ERR_OK;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_s04_firmware_image_test_run(void)
{
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE] = {0U};
    firmware_image_header_t receivedHeader = {0};
    firmware_image_header_t validatedHeader = {0};
    firmware_image_validation_t validation =
        FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    uint32_t calculatedCrc32 = 0U;
    platform_error_t result = PLATFORM_ERR_OK;

    SERVICE_LOG_I("[S04] destructive board test start, target=Slot B");

    result = s04_firmware_image_init_storage();
    SERVICE_LOG_I("[S04] storage init result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s04_firmware_image_init_uart();
    SERVICE_LOG_I("[S04] UART service init result=%d baud=%lu",
                  (int)result,
                  (unsigned long)S04_UART_BAUD_RATE);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    SERVICE_LOG_I("[S04] send 64-byte image header");
    result = s04_firmware_image_receive_exact(rawHeader, sizeof(rawHeader));
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("[S04] header receive failed error=%d", (int)result);
        return result;
    }

    validation = firmware_image_validate_header(rawHeader, &receivedHeader);
    SERVICE_LOG_I("[S04] header validation=%u", (unsigned int)validation);
    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return PLATFORM_ERR_CHECKSUM;
    }

    SERVICE_LOG_I("[S04] version=%u.%u.%u size=%lu expected_crc=0x%08lX",
                  (unsigned int)receivedHeader.version.major,
                  (unsigned int)receivedHeader.version.minor,
                  (unsigned int)receivedHeader.version.patch,
                  (unsigned long)receivedHeader.imageSize,
                  (unsigned long)receivedHeader.payloadCrc32);
    SERVICE_LOG_I("[S04] Slot B erase start");
    result = firmware_storage_erase_slot(&g_s04FirmwareStorage,
                                         FIRMWARE_SLOT_B,
                                         receivedHeader.imageSize);
    SERVICE_LOG_I("[S04] Slot B erase result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = s04_firmware_image_check_uart();
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("[S04] UART unsafe after erase; pause sender after header error=%d",
                      (int)result);
        return result;
    }

    SERVICE_LOG_I("[S04] erase complete, send payload bytes=%lu",
                  (unsigned long)receivedHeader.imageSize);
    result = s04_firmware_image_receive_payload(&receivedHeader,
                                                &calculatedCrc32);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("[S04] payload receive/write failed error=%d; header not committed",
                      (int)result);
        return result;
    }

    SERVICE_LOG_I("[S04] payload CRC expected=0x%08lX calculated=0x%08lX",
                  (unsigned long)receivedHeader.payloadCrc32,
                  (unsigned long)calculatedCrc32);
    if (calculatedCrc32 != receivedHeader.payloadCrc32) {
        SERVICE_LOG_E("[S04] payload CRC mismatch; header not committed");
        return PLATFORM_ERR_CHECKSUM;
    }

    result = platform_w25q64_write(&g_s04Flash,
                                   FIRMWARE_SLOT_B_BASE,
                                   rawHeader,
                                   sizeof(rawHeader));
    SERVICE_LOG_I("[S04] header commit result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_validate_image(&g_s04FirmwareStorage,
                                             FIRMWARE_SLOT_B,
                                             &validatedHeader,
                                             &validation);
    SERVICE_LOG_I("[S04] full image reread result=%d validation=%u",
                  (int)result,
                  (unsigned int)validation);
    if ((result != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_VALID)) {
        return (result == PLATFORM_ERR_OK) ? PLATFORM_ERR_CHECKSUM : result;
    }

    result = s04_firmware_image_test_metadata(&validatedHeader);
    SERVICE_LOG_I("[S04] metadata test result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    SERVICE_LOG_I("[S04] final result=PASS");
    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
