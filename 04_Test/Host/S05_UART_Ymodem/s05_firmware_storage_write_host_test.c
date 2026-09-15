#include <stdio.h>
#include <string.h>

#include "firmware_storage.h"

#define TEST_FLASH_SIZE       (FIRMWARE_SLOT_B_BASE + FIRMWARE_SLOT_SIZE_BYTES)

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

    (void)memcpy(data, &flash->memory[offset], dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_w25q64_write(
    platform_w25q64_t *flash,
    uint32_t address,
    const uint8_t *data,
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

    flash->writeCount++;
    flash->lastWriteAddress = address;
    flash->lastWriteLength = dataLength;
    (void)memcpy(&flash->memory[offset], data, dataLength);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_w25q64_sector_erase(
    platform_w25q64_t *flash,
    uint32_t sectorAddress)
{
    (void)flash;
    (void)sectorAddress;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_read(
    platform_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength)
{
    if ((eeprom == NULL) || (data == NULL) || (address > 256U) ||
        (dataLength > (256U - address))) {
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
    if ((eeprom == NULL) || (data == NULL) || (address > 256U) ||
        (dataLength > (256U - address))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    (void)memcpy(&eeprom->memory[address], data, dataLength);
    return PLATFORM_ERR_OK;
}

static int test_payload_write_mapping(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = {g_flashMemory, 0U, sizeof(g_flashMemory), 0U, 0U, 0U, 0U, 0U, 0U};
    platform_at24c02_t eeprom = {{0}, 0U, 0U};
    uint8_t data[8] = {0U};

    if (firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) {
        return 1;
    }

    if (firmware_storage_write_payload(&storage, FIRMWARE_SLOT_A, 0U, data, sizeof(data)) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((flash.lastWriteAddress != (FIRMWARE_SLOT_A_BASE + FIRMWARE_PAYLOAD_OFFSET)) ||
        (flash.lastWriteLength != sizeof(data))) {
        return 1;
    }

    if (firmware_storage_write_payload(&storage, FIRMWARE_SLOT_B, 0U, data, sizeof(data)) != PLATFORM_ERR_OK) {
        return 1;
    }

    if (flash.lastWriteAddress != (FIRMWARE_SLOT_B_BASE + FIRMWARE_PAYLOAD_OFFSET)) {
        return 1;
    }

    return 0;
}

static int test_payload_write_boundaries(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = {g_flashMemory, 0U, sizeof(g_flashMemory), 0U, 0U, 0U, 0U, 0U, 0U};
    platform_at24c02_t eeprom = {{0}, 0U, 0U};
    uint8_t data[4] = {0U};
    uint32_t writeCount;

    if (firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) {
        return 1;
    }

    if (firmware_storage_write_payload(
            &storage, FIRMWARE_SLOT_B, FIRMWARE_SLOT_PAYLOAD_CAPACITY - sizeof(data),
            data, sizeof(data)) != PLATFORM_ERR_OK) {
        return 1;
    }

    writeCount = flash.writeCount;
    if (firmware_storage_write_payload(
            &storage, FIRMWARE_SLOT_B, FIRMWARE_SLOT_PAYLOAD_CAPACITY, data, sizeof(data)) !=
        PLATFORM_ERR_INVALID_PARAM) {
        return 1;
    }

    if (firmware_storage_write_payload(
            &storage, FIRMWARE_SLOT_B, FIRMWARE_SLOT_PAYLOAD_CAPACITY - 1U, data, sizeof(data)) !=
        PLATFORM_ERR_INVALID_PARAM) {
        return 1;
    }

    if (firmware_storage_write_payload(
            &storage, FIRMWARE_SLOT_B, 0xFFFFFFFFUL, data, sizeof(data)) != PLATFORM_ERR_INVALID_PARAM) {
        return 1;
    }

    if (flash.writeCount != writeCount) {
        return 1;
    }

    return 0;
}

static int test_payload_write_parameter_checks(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = {g_flashMemory, 0U, sizeof(g_flashMemory), 0U, 0U, 0U, 0U, 0U, 0U};
    platform_at24c02_t eeprom = {{0}, 0U, 0U};
    uint8_t data[4] = {0U};

    if (firmware_storage_write_payload(&storage, FIRMWARE_SLOT_B, 0U, data, sizeof(data)) !=
        PLATFORM_ERR_NOT_INITIALIZED) {
        return 1;
    }

    if (firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) {
        return 1;
    }

    if (firmware_storage_write_payload(&storage, FIRMWARE_SLOT_NONE, 0U, data, sizeof(data)) !=
        PLATFORM_ERR_INVALID_PARAM) {
        return 1;
    }

    if (firmware_storage_write_payload(&storage, FIRMWARE_SLOT_B, 0U, NULL, sizeof(data)) !=
        PLATFORM_ERR_NULL_POINTER) {
        return 1;
    }

    if (firmware_storage_write_payload(&storage, FIRMWARE_SLOT_B, 0U, data, 0U) !=
        PLATFORM_ERR_INVALID_PARAM) {
        return 1;
    }

    return 0;
}

static int test_header_write_mapping(void)
{
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    platform_w25q64_t flash = {g_flashMemory, 0U, sizeof(g_flashMemory), 0U, 0U, 0U, 0U, 0U, 0U};
    platform_at24c02_t eeprom = {{0}, 0U, 0U};
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE] = {0U};

    if ((firmware_storage_init(&storage, &flash, &eeprom) != PLATFORM_ERR_OK) ||
        (firmware_storage_write_header(&storage, FIRMWARE_SLOT_B, rawHeader) != PLATFORM_ERR_OK)) {
        return 1;
    }

    if ((flash.lastWriteAddress != FIRMWARE_SLOT_B_BASE) ||
        (flash.lastWriteLength != FIRMWARE_IMAGE_HEADER_SIZE)) {
        return 1;
    }

    if (firmware_storage_write_header(&storage, FIRMWARE_SLOT_NONE, rawHeader) != PLATFORM_ERR_INVALID_PARAM) {
        return 1;
    }

    if (firmware_storage_write_header(&storage, FIRMWARE_SLOT_B, NULL) != PLATFORM_ERR_NULL_POINTER) {
        return 1;
    }

    return 0;
}

int main(void)
{
    if ((test_payload_write_mapping() != 0) ||
        (test_payload_write_boundaries() != 0) ||
        (test_payload_write_parameter_checks() != 0) ||
        (test_header_write_mapping() != 0)) {
        (void)printf("S05 firmware storage write host test failed.\n");
        return 1;
    }

    (void)printf("S05 firmware storage write host test passed.\n");
    return 0;
}
