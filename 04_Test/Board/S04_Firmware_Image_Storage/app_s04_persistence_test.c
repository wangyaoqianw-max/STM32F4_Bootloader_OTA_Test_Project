/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s04_persistence_test.c
 * @brief S04 Reset / Power-cycle Persistence 只读板测实现
 * @author YaoQian Wang
 * @date 2026-09-16
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_s04_persistence_test.h"

#define LOG_TAG "s04_persist_test"

#include "firmware_metadata.h"
#include "firmware_storage.h"
#include "platform_at24c02.h"
#include "platform_bsp_gpio.h"
#include "platform_bsp_spi.h"
#include "platform_bsp_w25q64.h"
#include "platform_i2c.h"
#include "platform_os.h"
#include "project_config.h"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S04_PERSISTENCE_SAMPLE_INTERVAL_MS    (1000U)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static platform_spi_bus_t g_s04PersistenceStorageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
static platform_w25q64_t g_s04PersistenceFlash = PLATFORM_W25Q64_INITIALIZER;
static platform_gpio_t g_s04PersistenceEepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s04PersistenceEepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s04PersistenceEepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s04PersistenceEeprom = PLATFORM_AT24C02_INITIALIZER;
static firmware_storage_t g_s04PersistenceStorage = FIRMWARE_STORAGE_INITIALIZER;

volatile s04_persistence_snapshot_t g_s04PersistenceSnapshot = {0};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static platform_error_t s04_persistence_test_init_storage(void)
{
    platform_error_t result;

    result = platform_bsp_spi_construct_storage_bus(
        &g_s04PersistenceStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(
        &g_s04PersistenceStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(
        &g_s04PersistenceStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_w25q64_construct_flash(
        &g_s04PersistenceFlash);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_init(
        &g_s04PersistenceFlash,
        &g_s04PersistenceStorageSpiBus);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_scl(
        &g_s04PersistenceEepromScl);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(
        &g_s04PersistenceEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(
        &g_s04PersistenceEepromI2c,
        "s04_persist_i2c",
        &g_s04PersistenceEepromScl,
        &g_s04PersistenceEepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(
        &g_s04PersistenceEeprom,
        &g_s04PersistenceEepromI2c,
        PROJECT_AT24C02_I2C_ADDRESS);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_storage_init(
        &g_s04PersistenceStorage,
        &g_s04PersistenceFlash,
        &g_s04PersistenceEeprom);
}

static platform_error_t s04_persistence_test_read_metadata(
    s04_persistence_snapshot_t *snapshot)
{
    uint8_t rawCopyA[FIRMWARE_METADATA_COPY_SIZE] = {0U};
    uint8_t rawCopyB[FIRMWARE_METADATA_COPY_SIZE] = {0U};
    firmware_metadata_t metadataA = {0};
    firmware_metadata_t metadataB = {0};
    firmware_metadata_t selectedMetadata = {0};
    firmware_metadata_copy_id_t selectedCopy = FIRMWARE_METADATA_COPY_NONE;
    platform_error_t result;
    platform_error_t copyAResult;
    platform_error_t copyBResult;

    result = platform_at24c02_read(
        &g_s04PersistenceEeprom,
        FIRMWARE_METADATA_COPY_A_ADDRESS,
        rawCopyA,
        sizeof(rawCopyA));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_read(
        &g_s04PersistenceEeprom,
        FIRMWARE_METADATA_COPY_B_ADDRESS,
        rawCopyB,
        sizeof(rawCopyB));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    copyAResult = firmware_metadata_decode_committed(rawCopyA, &metadataA);
    copyBResult = firmware_metadata_decode_committed(rawCopyB, &metadataB);

    result = firmware_storage_load_metadata(
        &g_s04PersistenceStorage,
        &selectedMetadata,
        &selectedCopy);
    if ((result != PLATFORM_ERR_OK) && (result != PLATFORM_ERR_NOT_FOUND)) {
        return result;
    }

    snapshot->metadataAValid =
        (copyAResult == PLATFORM_ERR_OK) ? 1U : 0U;
    snapshot->metadataBValid =
        (copyBResult == PLATFORM_ERR_OK) ? 1U : 0U;
    snapshot->selectedCopy = (uint32_t)selectedCopy;
    snapshot->sequence = selectedMetadata.sequence;
    snapshot->activeSlot = (uint32_t)selectedMetadata.activeSlot;
    snapshot->confirmedSlot = (uint32_t)selectedMetadata.confirmedSlot;
    snapshot->slotAState = (uint32_t)selectedMetadata.slotAState;
    snapshot->slotBState = (uint32_t)selectedMetadata.slotBState;
    snapshot->confirmedVersionMajor = selectedMetadata.confirmedVersion.major;
    snapshot->confirmedVersionMinor = selectedMetadata.confirmedVersion.minor;
    snapshot->confirmedVersionPatch = selectedMetadata.confirmedVersion.patch;
    return PLATFORM_ERR_OK;
}

static platform_error_t s04_persistence_test_read_snapshot(void)
{
    s04_persistence_snapshot_t snapshot = {0};
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation =
        FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    platform_error_t result;

    result = firmware_storage_validate_image(
        &g_s04PersistenceStorage,
        FIRMWARE_SLOT_B,
        &header,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    snapshot.imageValidation = (uint32_t)validation;
    snapshot.imageSize = header.imageSize;
    snapshot.imageCrc = header.payloadCrc32;

    result = s04_persistence_test_read_metadata(&snapshot);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_s04PersistenceSnapshot = snapshot;
    SERVICE_LOG_I(
        "[S04-PERSIST] SNAPSHOT image_validation=%lu image_size=%lu "
        "image_crc=0x%08lX metadata_a_valid=%lu metadata_b_valid=%lu "
        "selected_copy=%lu sequence=%lu active_slot=%lu confirmed_slot=%lu "
        "slot_a_state=%lu slot_b_state=%lu confirmed_version=%u.%u.%u",
        (unsigned long)snapshot.imageValidation,
        (unsigned long)snapshot.imageSize,
        (unsigned long)snapshot.imageCrc,
        (unsigned long)snapshot.metadataAValid,
        (unsigned long)snapshot.metadataBValid,
        (unsigned long)snapshot.selectedCopy,
        (unsigned long)snapshot.sequence,
        (unsigned long)snapshot.activeSlot,
        (unsigned long)snapshot.confirmedSlot,
        (unsigned long)snapshot.slotAState,
        (unsigned long)snapshot.slotBState,
        (unsigned int)snapshot.confirmedVersionMajor,
        (unsigned int)snapshot.confirmedVersionMinor,
        (unsigned int)snapshot.confirmedVersionPatch);
    return PLATFORM_ERR_OK;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_s04_persistence_test_run(void)
{
    platform_error_t result;

    SERVICE_LOG_I("[S04-PERSIST] read-only test start");
    result = s04_persistence_test_init_storage();
    SERVICE_LOG_I("[S04-PERSIST] storage init result=%d", (int)result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    SERVICE_LOG_I(
        "[S04-PERSIST] READY interval_ms=%lu writes=0",
        (unsigned long)S04_PERSISTENCE_SAMPLE_INTERVAL_MS);

    for (;;) {
        result = s04_persistence_test_read_snapshot();
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E(
                "[S04-PERSIST] snapshot read failed error=%d",
                (int)result);
        }

        result = platform_time_delay_ms(S04_PERSISTENCE_SAMPLE_INTERVAL_MS);
        if (result != PLATFORM_ERR_OK) {
            SERVICE_LOG_E(
                "[S04-PERSIST] sample delay failed error=%d",
                (int)result);
            return result;
        }
    }
}
//******************************** Functions ********************************//
