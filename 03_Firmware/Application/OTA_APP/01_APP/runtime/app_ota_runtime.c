/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ota_runtime.c
 * @brief OTA Application 运行时资源绑定实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_ota_runtime.h"

#define LOG_TAG "ota_runtime"

#include "firmware_storage.h"
#include "firmware_lifecycle.h"
#include "platform_at24c02.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_uart.h"
#include "platform_bsp_w25q64.h"
#include "platform_i2c.h"
#include "platform_uart.h"
#include "platform_w25q64.h"
#include "project_config.h"
#include "service_log.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_OTA_RUNTIME_UART_BAUD_RATE        (115200U)
#define APP_OTA_RUNTIME_UART_TIMEOUT_MS       (1000U)
#define APP_OTA_RUNTIME_UART_DMA_BUFFER_SIZE  (256U)
#define APP_OTA_RUNTIME_UART_RING_BUFFER_SIZE (2048U)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static platform_error_t app_ota_runtime_init_storage(void);
static platform_error_t app_ota_runtime_init_uart(
    platform_thread_t *ownerThread);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_bool_t g_otaRuntimeInitialized = PLATFORM_FALSE;
static platform_bool_t g_otaRuntimeTrial = PLATFORM_FALSE;
static platform_spi_bus_t g_otaStorageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_otaFlash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_otaEepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_otaEepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_otaEepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_otaEeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_otaFirmwareStorage = FIRMWARE_STORAGE_INITIALIZER;

static platform_uart_t g_otaCommunicationUart = PLATFORM_UART_INITIALIZER;
static service_uart_t g_otaUartService = SERVICE_UART_INITIALIZER;
static uint8_t g_otaUartDmaBuffer[APP_OTA_RUNTIME_UART_DMA_BUFFER_SIZE] = {0U};
static uint8_t g_otaUartRingBuffer[APP_OTA_RUNTIME_UART_RING_BUFFER_SIZE] = {0U};
static service_ota_t g_otaService = SERVICE_OTA_INITIALIZER;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static platform_error_t app_ota_runtime_init_storage(void)
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

    result = platform_i2c_init(
        &g_otaEepromI2c,
        "ota_metadata_i2c",
        &g_otaEepromScl,
        &g_otaEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(
        &g_otaEeprom,
        &g_otaEepromI2c,
        PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(
        &g_otaFirmwareStorage,
        &g_otaFlash,
        &g_otaEeprom);
}

static platform_error_t app_ota_runtime_init_uart(
    platform_thread_t *ownerThread)
{
    const platform_uart_config_t uartConfig = {
        APP_OTA_RUNTIME_UART_BAUD_RATE,
        PLATFORM_UART_DATA_BITS_8,
        PLATFORM_UART_STOP_BITS_1,
        PLATFORM_UART_PARITY_NONE,
        PLATFORM_UART_FLOW_CONTROL_NONE,
        APP_OTA_RUNTIME_UART_TIMEOUT_MS
    };
    service_uart_config_t serviceConfig = {0};
    platform_error_t result;

    if (ownerThread == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = platform_bsp_uart_construct_communication(
        &g_otaCommunicationUart,
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
    serviceConfig.ownerThread = ownerThread;

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
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_ota_runtime_init(platform_thread_t *ownerThread)
{
    firmware_metadata_t lifecycleMetadata;
    firmware_metadata_copy_id_t lifecycleMetadataCopy;
    service_ota_config_t serviceConfig;
    platform_error_t result;

    if (ownerThread == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_otaRuntimeInitialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = app_ota_runtime_init_storage();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_load_metadata(
        &g_otaFirmwareStorage,
        &lifecycleMetadata,
        &lifecycleMetadataCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (lifecycleMetadata.upgradeState == FIRMWARE_UPGRADE_STATE_NONE) {
        g_otaRuntimeTrial = PLATFORM_FALSE;
    } else if (lifecycleMetadata.upgradeState == FIRMWARE_UPGRADE_STATE_TRIAL) {
        g_otaRuntimeTrial = PLATFORM_TRUE;
    } else {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = app_ota_runtime_init_uart(ownerThread);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    serviceConfig.storage = &g_otaFirmwareStorage;
    serviceConfig.uart = &g_otaUartService;
    result = service_ota_init(&g_otaService, &serviceConfig);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = service_uart_stop(&g_otaUartService);
    if ((result != PLATFORM_ERR_OK) &&
        (result != PLATFORM_ERR_INVALID_STATE)) {
        return result;
    }

    g_otaRuntimeInitialized = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

platform_error_t app_ota_runtime_start_session(void)
{
    platform_error_t result;

    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = service_uart_start(&g_otaUartService);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = service_ota_start(&g_otaService);
    if (result != PLATFORM_ERR_OK) {
        (void)service_uart_stop(&g_otaUartService);
        return result;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t app_ota_runtime_stop_session(void)
{
    platform_error_t result;

    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = service_uart_stop(&g_otaUartService);
    if (result == PLATFORM_ERR_INVALID_STATE) {
        return PLATFORM_ERR_OK;
    }

    return result;
}

platform_bool_t app_ota_runtime_session_is_active(void)
{
    service_ota_status_t status;

    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_FALSE;
    }

    if (service_ota_get_status(&g_otaService, &status) != PLATFORM_ERR_OK) {
        return PLATFORM_FALSE;
    }

    return (status.state == SERVICE_OTA_STATE_RECEIVING) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

platform_error_t app_ota_runtime_check_uart(void)
{
    service_uart_status_t status = {
        SERVICE_UART_STATE_UNINITIALIZED,
        PLATFORM_ERR_OK,
        PLATFORM_FALSE
    };
    platform_error_t result;

    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

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

platform_error_t app_ota_runtime_get_readable_size(
    platform_size_t *readableSize)
{
    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return service_uart_get_readable_size(
        &g_otaUartService,
        readableSize);
}

platform_error_t app_ota_runtime_read(
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength)
{
    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return service_uart_read(
        &g_otaUartService,
        buffer,
        bufferSize,
        readLength);
}

platform_error_t app_ota_runtime_confirm_trial(void)
{
    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return firmware_lifecycle_confirm(&g_otaFirmwareStorage);
}

platform_error_t app_ota_runtime_get_trial_status(platform_bool_t *trial)
{
    if (trial == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    *trial = g_otaRuntimeTrial;
    return PLATFORM_ERR_OK;
}

service_ota_t *app_ota_runtime_get_service(void)
{
    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return NULL;
    }

    return &g_otaService;
}

platform_error_t app_ota_runtime_get_uart_statistics(
    service_uart_statistics_t *statistics)
{
    if (g_otaRuntimeInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return service_uart_get_statistics(&g_otaUartService, statistics);
}
//******************************** Functions *********************************//
