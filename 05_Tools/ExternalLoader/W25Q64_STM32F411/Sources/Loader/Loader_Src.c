/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file Loader_Src.c
 * @brief STM32F411 SPI2 W25Q64JV External Loader 实现
 * @author YaoQian Wang
 * @date 2026-09-21
 * @version V1.0
 *
 * @note 本文件不依赖 HAL、RTOS 或 Application。CubeProgrammer 将其下载到
 *       STM32 SRAM 后通过约定入口直接调用。
 ******************************************************************************/

#include "Loader_Src.h"

#include "stm32f411xe.h"

//******************************** Defines **********************************//
#define W25Q64_BASE_ADDRESS                 (0x90000000UL)
#define W25Q64_TOTAL_SIZE_BYTES             (0x00800000UL)
#define W25Q64_PAGE_SIZE_BYTES              (0x00000100UL)
#define W25Q64_SECTOR_SIZE_BYTES            (0x00001000UL)

#define W25Q64_JEDEC_MANUFACTURER_ID       (0xEFU)
#define W25Q64_JEDEC_MEMORY_TYPE            (0x40U)
#define W25Q64_JEDEC_CAPACITY_ID           (0x17U)

#define W25Q64_CMD_READ_JEDEC_ID           (0x9FU)
#define W25Q64_CMD_READ_STATUS             (0x05U)
#define W25Q64_CMD_WRITE_ENABLE            (0x06U)
#define W25Q64_CMD_READ_DATA               (0x03U)
#define W25Q64_CMD_PAGE_PROGRAM            (0x02U)
#define W25Q64_CMD_SECTOR_ERASE            (0x20U)
#define W25Q64_CMD_CHIP_ERASE              (0xC7U)

#define W25Q64_STATUS_BUSY_MASK            (0x01U)
#define W25Q64_STATUS_WEL_MASK             (0x02U)

#define LOADER_SPI_TIMEOUT_LOOPS           (1000000UL)
#define W25Q64_SECTOR_READY_POLLS          (200000UL)
#define W25Q64_CHIP_READY_POLLS            (20000000UL)
#define W25Q64_POLL_DELAY_LOOPS            (64UL)
#define LOADER_READ_CHUNK_SIZE             (64UL)
//******************************** Defines **********************************//

//******************************** Declaring ********************************//
static void loader_delay(uint32_t loops);
static void loader_spi_cs_low(void);
static void loader_spi_cs_high(void);
static int loader_spi_transfer(uint8_t value, uint8_t *response);
static int loader_spi_send(uint8_t value);
static void loader_spi_send_address(uint32_t address);
static int loader_normalize_address(uint32_t address, uint32_t *offset);
static int loader_normalize_range(
    uint32_t address,
    uint32_t size,
    uint32_t *offset);
static int w25q64_read_status(uint8_t *status);
static int w25q64_wait_ready(uint32_t pollLimit);
static int w25q64_write_enable(void);
static int w25q64_read_jedec_id(uint8_t *jedecId);
static int w25q64_read_data(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size);
static int w25q64_page_program(
    uint32_t address,
    const uint8_t *buffer,
    uint32_t size);
static int w25q64_sector_erase(uint32_t address);
static int w25q64_chip_erase(void);
static uint32_t loader_checksum(
    uint32_t address,
    uint32_t size,
    uint32_t initValue);
//******************************** Declaring ********************************//

//******************************** Private Functions *************************//
static void loader_delay(uint32_t loops)
{
    volatile uint32_t index;

    for (index = 0U; index < loops; index++) {
        __NOP();
    }
}

static void loader_spi_cs_low(void)
{
    GPIOB->BSRR = (1UL << (12U + 16U));
}

static void loader_spi_cs_high(void)
{
    GPIOB->BSRR = (1UL << 12U);
}

static int loader_spi_transfer(uint8_t value, uint8_t *response)
{
    uint32_t timeout = LOADER_SPI_TIMEOUT_LOOPS;

    if (response == 0) {
        return 0;
    }

    while ((SPI2->SR & SPI_SR_TXE) == 0U) {
        if (timeout == 0U) {
            return 0;
        }
        timeout--;
    }

    SPI2->DR = (uint16_t)value;
    timeout = LOADER_SPI_TIMEOUT_LOOPS;
    while ((SPI2->SR & SPI_SR_RXNE) == 0U) {
        if (timeout == 0U) {
            return 0;
        }
        timeout--;
    }

    *response = (uint8_t)SPI2->DR;
    return 1;
}

static int loader_spi_send(uint8_t value)
{
    uint8_t response = 0U;

    return loader_spi_transfer(value, &response);
}

static void loader_spi_send_address(uint32_t address)
{
    (void)loader_spi_send((uint8_t)(address >> 16U));
    (void)loader_spi_send((uint8_t)(address >> 8U));
    (void)loader_spi_send((uint8_t)address);
}

static int loader_normalize_address(uint32_t address, uint32_t *offset)
{
    if (offset == 0) {
        return 0;
    }

    if (address >= W25Q64_BASE_ADDRESS) {
        address -= W25Q64_BASE_ADDRESS;
    }

    if (address >= W25Q64_TOTAL_SIZE_BYTES) {
        return 0;
    }

    *offset = address;
    return 1;
}

static int loader_normalize_range(
    uint32_t address,
    uint32_t size,
    uint32_t *offset)
{
    if (!loader_normalize_address(address, offset)) {
        return 0;
    }

    if (size > (W25Q64_TOTAL_SIZE_BYTES - *offset)) {
        return 0;
    }

    return 1;
}

static int w25q64_read_status(uint8_t *status)
{
    uint8_t response = 0U;
    int result = 1;

    if (status == 0) {
        return 0;
    }

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_READ_STATUS, &response) ||
        !loader_spi_transfer(0xFFU, status)) {
        result = 0;
    }
    loader_spi_cs_high();
    return result;
}

static int w25q64_wait_ready(uint32_t pollLimit)
{
    uint8_t status = 0U;
    uint32_t pollIndex;

    for (pollIndex = 0U; pollIndex < pollLimit; pollIndex++) {
        if (!w25q64_read_status(&status)) {
            return 0;
        }

        if ((status & W25Q64_STATUS_BUSY_MASK) == 0U) {
            return 1;
        }

        loader_delay(W25Q64_POLL_DELAY_LOOPS);
    }

    return 0;
}

static int w25q64_write_enable(void)
{
    uint8_t status = 0U;
    uint8_t response = 0U;

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_WRITE_ENABLE, &response)) {
        loader_spi_cs_high();
        return 0;
    }
    loader_spi_cs_high();

    if (!w25q64_read_status(&status)) {
        return 0;
    }

    return ((status & W25Q64_STATUS_WEL_MASK) != 0U) ? 1 : 0;
}

static int w25q64_read_jedec_id(uint8_t *jedecId)
{
    uint8_t response = 0U;
    int result = 1;

    if (jedecId == 0) {
        return 0;
    }

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_READ_JEDEC_ID, &response) ||
        !loader_spi_transfer(0xFFU, &jedecId[0]) ||
        !loader_spi_transfer(0xFFU, &jedecId[1]) ||
        !loader_spi_transfer(0xFFU, &jedecId[2])) {
        result = 0;
    }
    loader_spi_cs_high();
    return result;
}

static int w25q64_read_data(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size)
{
    uint8_t response = 0U;
    uint32_t index;

    if ((buffer == 0) || !loader_normalize_range(address, size, &address)) {
        return 0;
    }

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_READ_DATA, &response)) {
        loader_spi_cs_high();
        return 0;
    }
    loader_spi_send_address(address);

    for (index = 0U; index < size; index++) {
        if (!loader_spi_transfer(0xFFU, &buffer[index])) {
            loader_spi_cs_high();
            return 0;
        }
    }

    loader_spi_cs_high();
    return 1;
}

static int w25q64_page_program(
    uint32_t address,
    const uint8_t *buffer,
    uint32_t size)
{
    uint8_t response = 0U;
    uint32_t pageOffset;
    uint32_t index;

    if ((buffer == 0) || (size == 0U) ||
        (size > W25Q64_PAGE_SIZE_BYTES) ||
        !loader_normalize_range(address, size, &address)) {
        return 0;
    }

    pageOffset = address % W25Q64_PAGE_SIZE_BYTES;
    if (size > (W25Q64_PAGE_SIZE_BYTES - pageOffset)) {
        return 0;
    }

    if (!w25q64_wait_ready(W25Q64_SECTOR_READY_POLLS) ||
        !w25q64_write_enable()) {
        return 0;
    }

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_PAGE_PROGRAM, &response)) {
        loader_spi_cs_high();
        return 0;
    }
    loader_spi_send_address(address);

    for (index = 0U; index < size; index++) {
        if (!loader_spi_send(buffer[index])) {
            loader_spi_cs_high();
            return 0;
        }
    }

    loader_spi_cs_high();
    return w25q64_wait_ready(W25Q64_SECTOR_READY_POLLS);
}

static int w25q64_sector_erase(uint32_t address)
{
    uint8_t response = 0U;

    if (!loader_normalize_address(address, &address) ||
        ((address % W25Q64_SECTOR_SIZE_BYTES) != 0U) ||
        !w25q64_wait_ready(W25Q64_SECTOR_READY_POLLS) ||
        !w25q64_write_enable()) {
        return 0;
    }

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_SECTOR_ERASE, &response)) {
        loader_spi_cs_high();
        return 0;
    }
    loader_spi_send_address(address);
    loader_spi_cs_high();

    return w25q64_wait_ready(W25Q64_SECTOR_READY_POLLS);
}

static int w25q64_chip_erase(void)
{
    uint8_t response = 0U;

    if (!w25q64_wait_ready(W25Q64_SECTOR_READY_POLLS) ||
        !w25q64_write_enable()) {
        return 0;
    }

    loader_spi_cs_low();
    if (!loader_spi_transfer(W25Q64_CMD_CHIP_ERASE, &response)) {
        loader_spi_cs_high();
        return 0;
    }
    loader_spi_cs_high();

    return w25q64_wait_ready(W25Q64_CHIP_READY_POLLS);
}

static uint32_t loader_checksum(
    uint32_t address,
    uint32_t size,
    uint32_t initValue)
{
    uint8_t buffer[LOADER_READ_CHUNK_SIZE];
    uint32_t remaining = size;
    uint32_t currentAddress = address;
    uint32_t chunkSize;
    uint32_t index;

    while (remaining > 0U) {
        chunkSize = (remaining > LOADER_READ_CHUNK_SIZE) ?
                    LOADER_READ_CHUNK_SIZE : remaining;
        if (!w25q64_read_data(currentAddress, buffer, chunkSize)) {
            return initValue;
        }

        for (index = 0U; index < chunkSize; index++) {
            initValue += buffer[index];
        }

        currentAddress += chunkSize;
        remaining -= chunkSize;
    }

    return initValue;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
int Init(void)
{
    uint8_t jedecId[3] = {0U, 0U, 0U};

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    GPIOB->MODER &= ~((3UL << (12U * 2U)) |
                      (3UL << (13U * 2U)) |
                      (3UL << (14U * 2U)) |
                      (3UL << (15U * 2U)));
    GPIOB->MODER |= (1UL << (12U * 2U)) |
                    (2UL << (13U * 2U)) |
                    (2UL << (14U * 2U)) |
                    (2UL << (15U * 2U));
    GPIOB->OTYPER &= ~((1UL << 12U) |
                       (1UL << 13U) |
                       (1UL << 14U) |
                       (1UL << 15U));
    GPIOB->OSPEEDR |= (3UL << (12U * 2U)) |
                      (3UL << (13U * 2U)) |
                      (3UL << (14U * 2U)) |
                      (3UL << (15U * 2U));
    GPIOB->PUPDR &= ~((3UL << (12U * 2U)) |
                      (3UL << (13U * 2U)) |
                      (3UL << (14U * 2U)) |
                      (3UL << (15U * 2U)));
    GPIOB->AFR[1] &= ~((0xFUL << 16U) |
                       (0xFUL << 20U) |
                       (0xFUL << 24U) |
                       (0xFUL << 28U));
    GPIOB->AFR[1] |= (5UL << 16U) |
                     (5UL << 20U) |
                     (5UL << 24U) |
                     (5UL << 28U);
    loader_spi_cs_high();

    SPI2->CR1 = SPI_CR1_MSTR |
                SPI_CR1_SSM |
                SPI_CR1_SSI |
                SPI_CR1_BR_1 |
                SPI_CR1_BR_0;
    SPI2->CR2 = 0U;
    SPI2->CR1 |= SPI_CR1_SPE;
    loader_delay(10000U);

    if (!w25q64_read_jedec_id(jedecId)) {
        return 0;
    }

    return ((jedecId[0] == W25Q64_JEDEC_MANUFACTURER_ID) &&
            (jedecId[1] == W25Q64_JEDEC_MEMORY_TYPE) &&
            (jedecId[2] == W25Q64_JEDEC_CAPACITY_ID)) ? 1 : 0;
}

int Write(uint32_t address, uint32_t size, uint8_t *buffer)
{
    uint32_t currentAddress = address;
    uint32_t remaining = size;
    uint32_t pageOffset;
    uint32_t pageRemaining;
    uint32_t chunkSize;

    if ((buffer == 0) || !loader_normalize_range(address, size, &currentAddress)) {
        return 0;
    }

    while (remaining > 0U) {
        pageOffset = currentAddress % W25Q64_PAGE_SIZE_BYTES;
        pageRemaining = W25Q64_PAGE_SIZE_BYTES - pageOffset;
        chunkSize = (remaining < pageRemaining) ? remaining : pageRemaining;
        if (!w25q64_page_program(currentAddress, buffer, chunkSize)) {
            return 0;
        }

        currentAddress += chunkSize;
        buffer += chunkSize;
        remaining -= chunkSize;
    }

    return 1;
}

int Read(uint32_t address, uint32_t size, uint8_t *buffer)
{
    return w25q64_read_data(address, buffer, size);
}

int SectorErase(uint32_t eraseStartAddress, uint32_t eraseEndAddress)
{
    uint32_t startAddress;
    uint32_t endAddress;

    if (!loader_normalize_address(eraseStartAddress, &startAddress) ||
        !loader_normalize_address(eraseEndAddress, &endAddress)) {
        return 0;
    }

    startAddress -= startAddress % W25Q64_SECTOR_SIZE_BYTES;
    endAddress -= endAddress % W25Q64_SECTOR_SIZE_BYTES;
    while (startAddress <= endAddress) {
        if (!w25q64_sector_erase(startAddress)) {
            return 0;
        }
        startAddress += W25Q64_SECTOR_SIZE_BYTES;
    }

    return 1;
}

int MassErase(uint32_t parallelism)
{
    (void)parallelism;
    return w25q64_chip_erase();
}

uint32_t CheckSum(uint32_t startAddress, uint32_t size, uint32_t initValue)
{
    uint32_t address;

    if (!loader_normalize_range(startAddress, size, &address)) {
        return initValue;
    }

    return loader_checksum(address, size, initValue);
}

uint64_t Verify(
    uint32_t memoryAddress,
    uint32_t ramBufferAddress,
    uint32_t size,
    uint32_t missalignment)
{
    uint8_t buffer[LOADER_READ_CHUNK_SIZE];
    uint32_t startOffset = missalignment & 0x0FU;
    uint32_t endOffset = (missalignment >> 16U) & 0x0FU;
    uint32_t byteCount = size * 4U;
    uint32_t compareLength;
    uint32_t currentAddress;
    uint32_t currentOffset = 0U;
    uint32_t chunkSize;
    uint32_t index;
    uint32_t checksum;

    if (byteCount < endOffset) {
        return 0U;
    }

    compareLength = byteCount - endOffset;
    currentAddress = memoryAddress + startOffset;
    if (!loader_normalize_range(currentAddress, compareLength, &currentAddress)) {
        return 0U;
    }

    checksum = loader_checksum(currentAddress, compareLength, 0U);
    while (currentOffset < compareLength) {
        chunkSize = compareLength - currentOffset;
        if (chunkSize > LOADER_READ_CHUNK_SIZE) {
            chunkSize = LOADER_READ_CHUNK_SIZE;
        }
        if (!w25q64_read_data(currentAddress + currentOffset,
                              buffer,
                              chunkSize)) {
            return ((uint64_t)checksum << 32U) |
                   (uint64_t)(currentAddress + currentOffset);
        }

        for (index = 0U; index < chunkSize; index++) {
            if (buffer[index] !=
                ((const uint8_t *)ramBufferAddress)[currentOffset + index]) {
                return ((uint64_t)checksum << 32U) |
                       (uint64_t)(currentAddress + currentOffset + index);
            }
        }
        currentOffset += chunkSize;
    }

    return (uint64_t)checksum << 32U;
}
//******************************** Functions *********************************//
