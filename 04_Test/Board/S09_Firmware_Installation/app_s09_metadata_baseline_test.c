/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s09_metadata_baseline_test.c
 * @brief S09 AT24C02 Metadata 基线板测实现。
 * @author YaoQian Wang
 * @date 2026-09-21
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include "app_s09_metadata_baseline_test.h"

#define LOG_TAG "s09_metadata"

#include "firmware_storage.h"
#include "platform_at24c02.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_w25q64.h"
#include "platform_i2c.h"
#include "platform_os.h"
#include "platform_time.h"
#include "project_config.h"
#include "service_log.h"

#include <stddef.h>
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S09_METADATA_BASELINE_TASK_STACK_SIZE_BYTES  (4096U)
#define S09_METADATA_BASELINE_WAIT_TIMEOUT_MS        (1000U)
#define S09_METADATA_BASELINE_VERSION_MAJOR         (1U)
#define S09_METADATA_BASELINE_VERSION_MINOR         (0U)
#define S09_METADATA_BASELINE_VERSION_PATCH         (0U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static void s09_metadata_baseline_task(void *argument);
static platform_error_t s09_metadata_baseline_init_storage(void);
static platform_error_t s09_metadata_baseline_commit(
    const firmware_image_header_t *header);
static platform_error_t s09_metadata_baseline_verify(
    const firmware_image_header_t *header);
static platform_error_t s09_metadata_baseline_execute(void);
//******************************** Private Functions ************************//

//******************************** Variables ********************************//
static platform_spi_bus_t g_s09MetadataStorageSpiBus =
    PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_s09MetadataFlash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_s09MetadataEepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s09MetadataEepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s09MetadataEepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s09MetadataEeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_s09MetadataStorage = FIRMWARE_STORAGE_INITIALIZER;
static platform_thread_t g_s09MetadataThread = PLATFORM_OS_OBJECT_INITIALIZER;

static const platform_thread_config_t s_s09_metadata_thread_config = {
    .name = "s09Metadata",
    .entry = s09_metadata_baseline_task,
    .argument = NULL,
    .stackSizeBytes = S09_METADATA_BASELINE_TASK_STACK_SIZE_BYTES,
    .priority = PLATFORM_THREAD_PRIORITY_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static platform_error_t s09_metadata_baseline_init_storage(void)
{
    platform_error_t result;

    result = platform_bsp_spi_construct_storage_bus(
        &g_s09MetadataStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_s09MetadataStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_s09MetadataStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(&g_s09MetadataFlash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(
        &g_s09MetadataFlash,
        &g_s09MetadataStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(
        &g_s09MetadataEepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(
        &g_s09MetadataEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(
        &g_s09MetadataEepromI2c,
        "s09_metadata_i2c",
        &g_s09MetadataEepromScl,
        &g_s09MetadataEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(
        &g_s09MetadataEeprom,
        &g_s09MetadataEepromI2c,
        PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(
        &g_s09MetadataStorage,
        &g_s09MetadataFlash,
        &g_s09MetadataEeprom);
}

static platform_error_t s09_metadata_baseline_commit(
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

    for (commitIndex = 0U; commitIndex < 2U; commitIndex++) {
        result = firmware_storage_commit_metadata(
            &g_s09MetadataStorage,
            &metadata,
            &committedCopy);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t s09_metadata_baseline_verify(
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
        &g_s09MetadataStorage,
        &metadata,
        &sourceCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_validate_image(
        &g_s09MetadataStorage,
        FIRMWARE_SLOT_A,
        &slotAHeader,
        &slotAValidation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_read_header(
        &g_s09MetadataStorage,
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
        (metadata.confirmedVersion.patch != header->version.patch) ||
        (header->version.major != S09_METADATA_BASELINE_VERSION_MAJOR) ||
        (header->version.minor != S09_METADATA_BASELINE_VERSION_MINOR) ||
        (header->version.patch != S09_METADATA_BASELINE_VERSION_PATCH)) {
        return PLATFORM_ERR_IO;
    }

    SERVICE_LOG_I(
        "[S09-METADATA] baseline PASS copy=%u sequence=%lu "
        "Slot A=%u.%u.%u VALID Slot B=EMPTY confirmed=A "
        "pending=NONE upgrade=NONE",
        (unsigned int)sourceCopy,
        (unsigned long)metadata.sequence,
        (unsigned int)slotAHeader.version.major,
        (unsigned int)slotAHeader.version.minor,
        (unsigned int)slotAHeader.version.patch);
    return PLATFORM_ERR_OK;
}

static platform_error_t s09_metadata_baseline_execute(void)
{
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    platform_error_t result;

    SERVICE_LOG_I(
        "[S09-METADATA] start; W25Q64 must be preburned by External Loader");

    result = s09_metadata_baseline_init_storage();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_validate_image(
        &g_s09MetadataStorage,
        FIRMWARE_SLOT_A,
        &header,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return PLATFORM_ERR_CHECKSUM;
    }

    result = s09_metadata_baseline_commit(&header);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return s09_metadata_baseline_verify(&header);
}

static void s09_metadata_baseline_task(void *argument)
{
    platform_error_t result;

    (void)argument;

    result = s09_metadata_baseline_execute();
    SERVICE_LOG_I("[S09-METADATA] worker result=%d", (int)result);

    for (;;) {
        (void)platform_time_delay_ms(S09_METADATA_BASELINE_WAIT_TIMEOUT_MS);
    }
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_s09_metadata_baseline_test_run(void)
{
    return platform_thread_create(
        &g_s09MetadataThread,
        &s_s09_metadata_thread_config);
}
//******************************** Functions ********************************//
