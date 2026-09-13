#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "firmware_storage.h"

#define TEST_FLASH_BASE       (FIRMWARE_SLOT_B_BASE)
#define TEST_FLASH_SIZE       (0x3000U)
#define TEST_PAYLOAD_SIZE     (300U)

static uint8_t g_flashMemory[TEST_FLASH_SIZE];

platform_error_t platform_w25q64_read(
    platform_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength)
{
    uint32_t offset;

    if ((flash == NULL) || (data == NULL) || (address < flash->baseAddress)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    offset = address - flash->baseAddress;
    if ((offset > flash->sizeBytes) || (dataLength > (flash->sizeBytes - offset))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    flash->readCount++;
    if (flash->failRead != 0U) {
        return PLATFORM_ERR_IO;
    }

    (void)memcpy(data, &flash->memory[offset], dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_w25q64_sector_erase(platform_w25q64_t *flash, uint32_t sectorAddress)
{
    uint32_t offset;

    if ((flash == NULL) || (sectorAddress < flash->baseAddress)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    offset = sectorAddress - flash->baseAddress;
    if ((offset > flash->sizeBytes) || (0x1000U > (flash->sizeBytes - offset))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (flash->failErase != 0U) {
        return PLATFORM_ERR_IO;
    }

    (void)memset(&flash->memory[offset], 0xFF, 0x1000U);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_read(
    platform_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength)
{
    if ((eeprom == NULL) || (data == NULL) || (address > 256U) || (dataLength > (256U - address))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    (void)memcpy(data, &eeprom->memory[address], dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_write(
    platform_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength)
{
    if ((eeprom == NULL) || (data == NULL) || (address > 256U) || (dataLength > (256U - address))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    eeprom->writeCount++;
    if (eeprom->writeCount == eeprom->failWriteOnCall) {
        return PLATFORM_ERR_IO;
    }

    (void)memcpy(&eeprom->memory[address], data, dataLength);
    return PLATFORM_ERR_OK;
}

static void prepare_valid_image(platform_w25q64_t *flash)
{
    firmware_image_header_t header = {0};
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE];
    uint32_t index;

    for (index = 0U; index < TEST_PAYLOAD_SIZE; index++) {
        flash->memory[FIRMWARE_PAYLOAD_OFFSET + index] = (uint8_t)index;
    }

    header.version.major = 1U;
    header.imageSize = TEST_PAYLOAD_SIZE;
    header.payloadCrc32 = crc32_iso_hdlc_calculate(
        &flash->memory[FIRMWARE_PAYLOAD_OFFSET], TEST_PAYLOAD_SIZE);
    (void)firmware_image_encode_header(&header, rawHeader);
    (void)memcpy(flash->memory, rawHeader, sizeof(rawHeader));
}

static int test_image_validation_paths(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = {g_flashMemory, TEST_FLASH_BASE, TEST_FLASH_SIZE, 0U, 0U, 0U};
    platform_at24c02_t eeprom = {{0}, 0U, 0U};
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation;

    (void)memset(g_flashMemory, 0xFF, sizeof(g_flashMemory));
    if (firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((firmware_storage_validate_image(&storage, FIRMWARE_SLOT_B, &header, &validation) != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_EMPTY)) {
        return 1;
    }

    prepare_valid_image(&flash);
    if ((firmware_storage_validate_image(&storage, FIRMWARE_SLOT_B, &header, &validation) != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_VALID)) {
        return 1;
    }

    flash.memory[FIRMWARE_PAYLOAD_OFFSET] ^= 1U;
    if ((firmware_storage_validate_image(&storage, FIRMWARE_SLOT_B, &header, &validation) != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC)) {
        return 1;
    }

    flash.failRead = 1U;
    validation = FIRMWARE_IMAGE_VALIDATION_VALID;
    if ((firmware_storage_validate_image(&storage, FIRMWARE_SLOT_B, &header, &validation) != PLATFORM_ERR_IO) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_UNKNOWN)) {
        return 1;
    }

    return 0;
}

static int test_metadata_commit_recovery(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = {g_flashMemory, TEST_FLASH_BASE, TEST_FLASH_SIZE, 0U, 0U, 0U};
    platform_at24c02_t eeprom = {{0}, 0U, 0U};
    firmware_metadata_t input = {0};
    firmware_metadata_t loaded = {0};
    firmware_metadata_copy_id_t copy;

    (void)memset(eeprom.memory, 0xFF, sizeof(eeprom.memory));
    input.activeSlot = FIRMWARE_SLOT_NONE;
    input.confirmedSlot = FIRMWARE_SLOT_NONE;
    input.slotAState = FIRMWARE_SLOT_STATE_EMPTY;
    input.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    if ((firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) ||
        (firmware_storage_commit_metadata(&storage, &input, &copy) != PLATFORM_ERR_OK) ||
        (copy != FIRMWARE_METADATA_COPY_A) ||
        (firmware_storage_load_metadata(&storage, &loaded, &copy) != PLATFORM_ERR_OK) ||
        (loaded.sequence != 0U)) {
        return 1;
    }

    input.activeSlot = FIRMWARE_SLOT_B;
    eeprom.failWriteOnCall = eeprom.writeCount + 3U;
    if ((firmware_storage_commit_metadata(&storage, &input, &copy) != PLATFORM_ERR_IO) ||
        (firmware_storage_load_metadata(&storage, &loaded, &copy) != PLATFORM_ERR_OK) ||
        (copy != FIRMWARE_METADATA_COPY_A) || (loaded.sequence != 0U)) {
        return 1;
    }

    eeprom.failWriteOnCall = 0U;
    if ((firmware_storage_commit_metadata(&storage, &input, &copy) != PLATFORM_ERR_OK) ||
        (copy != FIRMWARE_METADATA_COPY_B) ||
        (firmware_storage_load_metadata(&storage, &loaded, &copy) != PLATFORM_ERR_OK) ||
        (copy != FIRMWARE_METADATA_COPY_B) || (loaded.sequence != 1U)) {
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_image_validation_paths() != 0) {
        (void)printf("Firmware Storage image validation test failed.\n");
        return 1;
    }

    if (test_metadata_commit_recovery() != 0) {
        (void)printf("Firmware Storage metadata commit recovery test failed.\n");
        return 1;
    }

    (void)printf("S04 firmware storage host test passed.\n");
    return 0;
}
