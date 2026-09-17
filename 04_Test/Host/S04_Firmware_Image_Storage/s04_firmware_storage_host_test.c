#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "firmware_storage.h"

#define TEST_FLASH_BASE       (FIRMWARE_SLOT_B_BASE)
#define TEST_FLASH_SIZE       (0x3000U)
#define TEST_PAYLOAD_SIZE     (300U)

static uint8_t g_flashMemory[TEST_FLASH_SIZE];
static uint8_t g_eepromMemory[PLATFORM_AT24C02_TOTAL_SIZE_BYTES];
static uint32_t g_flashReadCount;
static platform_bool_t g_flashFailRead;
static platform_bool_t g_flashFailErase;
static uint32_t g_eepromWriteCount;
static uint32_t g_eepromFailWriteOnCall;

platform_error_t platform_w25q64_read(
    platform_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength)
{
    uint32_t offset;

    if ((flash == NULL) || (data == NULL) || (address < TEST_FLASH_BASE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    offset = address - TEST_FLASH_BASE;
    if ((offset > TEST_FLASH_SIZE) || (dataLength > (TEST_FLASH_SIZE - offset))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    g_flashReadCount++;
    if (g_flashFailRead != 0U) {
        return PLATFORM_ERR_IO;
    }

    (void)memcpy(data, &g_flashMemory[offset], dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_w25q64_write(
    platform_w25q64_t *flash,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength)
{
    uint32_t offset;

    if ((flash == NULL) || (data == NULL) || (address < TEST_FLASH_BASE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    offset = address - TEST_FLASH_BASE;
    if ((offset > TEST_FLASH_SIZE) || (dataLength > (TEST_FLASH_SIZE - offset))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    (void)memcpy(&g_flashMemory[offset], data, dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_w25q64_sector_erase(platform_w25q64_t *flash, uint32_t sectorAddress)
{
    uint32_t offset;

    if ((flash == NULL) || (sectorAddress < TEST_FLASH_BASE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    offset = sectorAddress - TEST_FLASH_BASE;
    if ((offset > TEST_FLASH_SIZE) || (0x1000U > (TEST_FLASH_SIZE - offset))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (g_flashFailErase != 0U) {
        return PLATFORM_ERR_IO;
    }

    (void)memset(&g_flashMemory[offset], 0xFF, 0x1000U);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_read(
    platform_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength)
{
    if ((eeprom == NULL) || (data == NULL) ||
        (address > PLATFORM_AT24C02_TOTAL_SIZE_BYTES) ||
        (dataLength > (PLATFORM_AT24C02_TOTAL_SIZE_BYTES - address))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    (void)memcpy(data, &g_eepromMemory[address], dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_write(
    platform_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength)
{
    if ((eeprom == NULL) || (data == NULL) ||
        (address > PLATFORM_AT24C02_TOTAL_SIZE_BYTES) ||
        (dataLength > (PLATFORM_AT24C02_TOTAL_SIZE_BYTES - address))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    g_eepromWriteCount++;
    if (g_eepromWriteCount == g_eepromFailWriteOnCall) {
        return PLATFORM_ERR_IO;
    }

    (void)memcpy(&g_eepromMemory[address], data, dataLength);
    return PLATFORM_ERR_OK;
}

static void prepare_valid_image(platform_w25q64_t *flash)
{
    firmware_image_header_t header = {0};
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE];
    uint32_t index;

    (void)flash;
    for (index = 0U; index < TEST_PAYLOAD_SIZE; index++) {
        g_flashMemory[FIRMWARE_PAYLOAD_OFFSET + index] = (uint8_t)index;
    }

    header.version.major = 1U;
    header.imageSize = TEST_PAYLOAD_SIZE;
    header.payloadCrc32 = crc32_iso_hdlc_calculate(
        &g_flashMemory[FIRMWARE_PAYLOAD_OFFSET], TEST_PAYLOAD_SIZE);
    (void)firmware_image_encode_header(&header, rawHeader);
    (void)memcpy(g_flashMemory, rawHeader, sizeof(rawHeader));
}

static int test_image_validation_paths(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = PLATFORM_W25Q64_INITIALIZER;
    platform_at24c02_t eeprom = PLATFORM_AT24C02_INITIALIZER;
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation;

    (void)memset(g_flashMemory, 0xFF, sizeof(g_flashMemory));
    g_flashReadCount = 0U;
    g_flashFailRead = PLATFORM_FALSE;
    g_flashFailErase = PLATFORM_FALSE;
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

    g_flashMemory[FIRMWARE_PAYLOAD_OFFSET] ^= 1U;
    if ((firmware_storage_validate_image(&storage, FIRMWARE_SLOT_B, &header, &validation) != PLATFORM_ERR_OK) ||
        (validation != FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC)) {
        return 1;
    }

    g_flashFailRead = PLATFORM_TRUE;
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
    platform_w25q64_t flash = PLATFORM_W25Q64_INITIALIZER;
    platform_at24c02_t eeprom = PLATFORM_AT24C02_INITIALIZER;
    firmware_metadata_t input = {0};
    firmware_metadata_t loaded = {0};
    firmware_metadata_copy_id_t copy;

    (void)memset(g_eepromMemory, 0xFF, sizeof(g_eepromMemory));
    g_eepromWriteCount = 0U;
    g_eepromFailWriteOnCall = 0U;
    input.confirmedSlot = FIRMWARE_SLOT_NONE;
    input.pendingSlot = FIRMWARE_SLOT_NONE;
    input.slotAState = FIRMWARE_SLOT_STATE_EMPTY;
    input.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    input.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    if ((firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) ||
        (firmware_storage_commit_metadata(&storage, &input, &copy) != PLATFORM_ERR_OK) ||
        (copy != FIRMWARE_METADATA_COPY_A) ||
        (firmware_storage_load_metadata(&storage, &loaded, &copy) != PLATFORM_ERR_OK) ||
        (loaded.sequence != 0U) ||
        (g_eepromMemory[0x04U] != FIRMWARE_METADATA_FORMAT_VERSION_V2)) {
        return 1;
    }

    input.confirmedSlot = FIRMWARE_SLOT_B;
    g_eepromFailWriteOnCall = g_eepromWriteCount + 3U;
    if ((firmware_storage_commit_metadata(&storage, &input, &copy) != PLATFORM_ERR_IO) ||
        (firmware_storage_load_metadata(&storage, &loaded, &copy) != PLATFORM_ERR_OK) ||
        (copy != FIRMWARE_METADATA_COPY_A) || (loaded.sequence != 0U)) {
        return 1;
    }

    g_eepromFailWriteOnCall = 0U;
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
