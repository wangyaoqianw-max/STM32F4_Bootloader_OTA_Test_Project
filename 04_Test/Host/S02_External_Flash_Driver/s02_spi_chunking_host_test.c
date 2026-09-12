/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s02_spi_chunking_host_test.c
 * @brief S02 返工验证：STM32 SPI Impl 大长度传输 HAL chunk 拆分 Host Test
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 * @note 本测试在 PC 上运行，不进入 Keil 工程，也不参与固件构建。
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>

#include "spi.h"
#include "platform_spi.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define TEST_WRITE_BUFFER_SIZE_BYTES (0x30000U)
#define TEST_READ_BUFFER_SIZE_BYTES  (0x30000U)
#define TEST_MAX_CHUNK_RECORDS       (8U)
#define TEST_RX_PATTERN_BASE         (0xA0U)
#define TEST_READ_BACK_FILL          (0xFFU)
#define TEST_HAL_TIMEOUT_MS          (1000U)

#define TEST_LENGTH_ONE              (1U)
#define TEST_LENGTH_HAL_MAX          (0xFFFFU)
#define TEST_LENGTH_OVER_HAL_MAX     (0x10000U)
#define TEST_LENGTH_MULTI_CHUNK      (0x30000U)
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
/*单次 HAL 调用记录；用于反推 Impl 实际执行的 HAL chunk 序列。*/
typedef struct
{
    uint16_t size;
    uint8_t isReceive;
    uint8_t firstByte;
    uint8_t lastByte;
} test_chunk_record_t;
//******************************** Declaring *********************************//

//******************************** Variables *********************************//
static test_chunk_record_t g_chunkRecords[TEST_MAX_CHUNK_RECORDS];
static uint8_t g_writeBuffer[TEST_WRITE_BUFFER_SIZE_BYTES];
static uint8_t g_readBuffer[TEST_READ_BUFFER_SIZE_BYTES];

static SPI_TypeDef g_stubSpi1Register;
static SPI_TypeDef g_stubSpi2Register;

SPI_TypeDef *const g_stubSpi1 = &g_stubSpi1Register;
SPI_TypeDef *const g_stubSpi2 = &g_stubSpi2Register;
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

static uint32_t g_chunkCount = 0U;
static uint32_t g_failAtChunk = 0U;
static uint32_t g_lastTimeoutMs = 0U;
static uint32_t g_checkCount = 0U;
static uint32_t g_failureCount = 0U;
static HAL_StatusTypeDef g_failStatus = HAL_OK;
static SPI_HandleTypeDef *g_lastHandle = NULL;
//******************************** Variables *********************************//

//******************************** Private Functions *************************//
static HAL_StatusTypeDef test_record_transfer(
    uint8_t isReceive,
    SPI_HandleTypeDef *hspi,
    uint8_t *pData,
    uint16_t size,
    uint32_t timeout);
static void test_fill_write_buffer(void);
static void test_reset(void);
static void test_print_chunks(void);
static void test_expect(int condition, const char *caseName);
static int test_verify_write_sequence(uint32_t expectedLength);
static int test_verify_read_sequence(uint32_t expectedLength);
static void test_case_write_lengths(void);
static void test_case_read_lengths(void);
static void test_case_error_paths(void);
static void test_case_param_checks(void);
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
HAL_StatusTypeDef HAL_SPI_Transmit(
    SPI_HandleTypeDef *hspi,
    uint8_t *pData,
    uint16_t Size,
    uint32_t Timeout)
{
    return test_record_transfer(0U, hspi, pData, Size, Timeout);
}

HAL_StatusTypeDef HAL_SPI_Receive(
    SPI_HandleTypeDef *hspi,
    uint8_t *pData,
    uint16_t Size,
    uint32_t Timeout)
{
    return test_record_transfer(1U, hspi, pData, Size, Timeout);
}

HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
    return HAL_SPI_STATE_READY;
}

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
    return 50000000U;
}

uint32_t HAL_RCC_GetPCLK2Freq(void)
{
    return 100000000U;
}

/*被测文件内未被本测试触达的 Platform 依赖替身。
 * stm32_spi_write()/stm32_spi_read() 不会调用这些函数；这里只保证
 * Host Test 可以原样包含 impl_platform_spi.c 并完成链接。*/
platform_bool_t platform_object_is_valid(
    const platform_object_t *obj,
    platform_object_type_t type)
{
    (void)obj;
    (void)type;
    return PLATFORM_FALSE;
}

platform_error_t platform_object_set_state(
    platform_object_t *obj,
    platform_object_state_t state)
{
    (void)obj;
    (void)state;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_device_set_power_state(
    platform_device_t *p_dev,
    platform_device_power_state_t power_state)
{
    (void)p_dev;
    (void)power_state;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_spi_bus_init(
    platform_spi_bus_t *bus,
    const platform_spi_bus_init_params_t *params)
{
    (void)bus;
    (void)params;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//

/*被测实现直接包含：stm32_spi_write()/stm32_spi_read() 为文件内 static，
 * 且 Impl 私有 Context 类型不对外暴露，仓库当前没有其他 Host Test 接缝。*/
#include "impl_platform_spi.c"

/*Impl 私有 Context 的实例，绑定到 Host Test 提供的 hspi2。*/
static stm32_spi_impl_context_t g_testContext = { &hspi2 };

//******************************** Private Functions *************************//
static HAL_StatusTypeDef test_record_transfer(
    uint8_t isReceive,
    SPI_HandleTypeDef *hspi,
    uint8_t *pData,
    uint16_t size,
    uint32_t timeout)
{
    uint32_t index = 0U;
    uint32_t recordIndex = g_chunkCount;

    g_lastHandle = hspi;
    g_lastTimeoutMs = timeout;

    if (recordIndex < TEST_MAX_CHUNK_RECORDS) {
        g_chunkRecords[recordIndex].size = size;
        g_chunkRecords[recordIndex].isReceive = isReceive;
        if (size > 0U) {
            g_chunkRecords[recordIndex].firstByte = pData[0];
            g_chunkRecords[recordIndex].lastByte = pData[size - 1U];
        }
    }
    g_chunkCount++;

    /*接收替身按 chunk 序号填充固定模式，用于校验目标指针推进是否正确。*/
    if (isReceive != 0U) {
        for (index = 0U; index < (uint32_t)size; index++) {
            pData[index] = (uint8_t)(TEST_RX_PATTERN_BASE + (uint8_t)g_chunkCount);
        }
    }

    if ((g_failAtChunk != 0U) && (g_chunkCount == g_failAtChunk)) {
        return g_failStatus;
    }

    return HAL_OK;
}

static void test_fill_write_buffer(void)
{
    uint32_t index = 0U;

    for (index = 0U; index < TEST_WRITE_BUFFER_SIZE_BYTES; index++) {
        g_writeBuffer[index] = (uint8_t)(index ^ 0x5AU);
    }
}

static void test_reset(void)
{
    uint32_t index = 0U;

    for (index = 0U; index < TEST_MAX_CHUNK_RECORDS; index++) {
        g_chunkRecords[index].size = 0U;
        g_chunkRecords[index].isReceive = 0U;
        g_chunkRecords[index].firstByte = 0U;
        g_chunkRecords[index].lastByte = 0U;
    }

    for (index = 0U; index < TEST_READ_BUFFER_SIZE_BYTES; index++) {
        g_readBuffer[index] = TEST_READ_BACK_FILL;
    }

    g_chunkCount = 0U;
    g_failAtChunk = 0U;
    g_failStatus = HAL_OK;
    g_lastTimeoutMs = 0U;
    g_lastHandle = NULL;
}

static void test_print_chunks(void)
{
    uint32_t index = 0U;
    uint32_t recordCount = (g_chunkCount < TEST_MAX_CHUNK_RECORDS) ?
                           g_chunkCount : TEST_MAX_CHUNK_RECORDS;

    (void)printf("       HAL calls=%u sizes=[", (unsigned int)g_chunkCount);
    for (index = 0U; index < recordCount; index++) {
        (void)printf("%s%u",
                     (index == 0U) ? "" : ",",
                     (unsigned int)g_chunkRecords[index].size);
    }
    (void)printf("]\n");
}

static void test_expect(int condition, const char *caseName)
{
    g_checkCount++;
    if (condition != 0) {
        (void)printf("[PASS] %s\n", caseName);
    } else {
        g_failureCount++;
        (void)printf("[FAIL] %s\n", caseName);
    }
}

/*校验发送侧 chunk 序列：全部为 Transmit，且按累计偏移覆盖源 Buffer。*/
static int test_verify_write_sequence(uint32_t expectedLength)
{
    uint32_t index = 0U;
    uint32_t offset = 0U;
    uint32_t recordCount = (g_chunkCount < TEST_MAX_CHUNK_RECORDS) ?
                           g_chunkCount : TEST_MAX_CHUNK_RECORDS;
    uint32_t size = 0U;

    if (g_chunkCount == 0U) {
        return 0;
    }

    for (index = 0U; index < recordCount; index++) {
        size = (uint32_t)g_chunkRecords[index].size;
        if ((g_chunkRecords[index].isReceive != 0U) || (size == 0U)) {
            return 0;
        }
        if (g_chunkRecords[index].firstByte != g_writeBuffer[offset]) {
            return 0;
        }
        if (g_chunkRecords[index].lastByte !=
            g_writeBuffer[offset + size - 1U]) {
            return 0;
        }
        offset += size;
    }

    return (offset == expectedLength) ? 1 : 0;
}

/*校验接收侧 chunk 序列：全部为 Receive，模式按 chunk 依次覆盖目标 Buffer。*/
static int test_verify_read_sequence(uint32_t expectedLength)
{
    uint32_t index = 0U;
    uint32_t offset = 0U;
    uint32_t byteIndex = 0U;
    uint32_t recordCount = (g_chunkCount < TEST_MAX_CHUNK_RECORDS) ?
                           g_chunkCount : TEST_MAX_CHUNK_RECORDS;
    uint32_t size = 0U;
    uint8_t expected = 0U;

    if (g_chunkCount == 0U) {
        return 0;
    }

    for (index = 0U; index < recordCount; index++) {
        size = (uint32_t)g_chunkRecords[index].size;
        if ((g_chunkRecords[index].isReceive == 0U) || (size == 0U)) {
            return 0;
        }

        expected = (uint8_t)(TEST_RX_PATTERN_BASE + (uint8_t)index + 1U);
        for (byteIndex = 0U; byteIndex < size; byteIndex++) {
            if (g_readBuffer[offset + byteIndex] != expected) {
                return 0;
            }
        }
        offset += size;
    }

    if (offset != expectedLength) {
        return 0;
    }

    /*长度之后的首字节必须仍是预填充值，证明没有多写一个 chunk。*/
    if (expectedLength < TEST_READ_BUFFER_SIZE_BYTES) {
        if (g_readBuffer[expectedLength] != TEST_READ_BACK_FILL) {
            return 0;
        }
    }

    return 1;
}

static void test_case_write_lengths(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_error_t result = PLATFORM_ERR_OK;

    bus.implContext = &g_testContext;

    test_reset();
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_ONE);
    test_expect((result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 1U) &&
                test_verify_write_sequence(TEST_LENGTH_ONE),
                "write length=1 -> 1 HAL chunk");
    test_print_chunks();

    test_reset();
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_HAL_MAX);
    test_expect((result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 1U) &&
                (g_chunkRecords[0].size == 0xFFFFU) &&
                test_verify_write_sequence(TEST_LENGTH_HAL_MAX),
                "write length=0xFFFF -> 1 HAL chunk of 0xFFFF");
    test_print_chunks();

    test_reset();
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_OVER_HAL_MAX);
    test_expect((result != PLATFORM_ERR_OVERFLOW) &&
                (result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 2U) &&
                (g_chunkRecords[0].size == 0xFFFFU) &&
                (g_chunkRecords[1].size == 1U) &&
                test_verify_write_sequence(TEST_LENGTH_OVER_HAL_MAX),
                "write length=0x10000 -> 0xFFFF + 1, no PLATFORM_ERR_OVERFLOW");
    test_print_chunks();
    test_expect((g_lastTimeoutMs == TEST_HAL_TIMEOUT_MS) &&
                (g_lastHandle == &hspi2),
                "write keeps per-chunk HAL timeout and bound HAL handle");

    test_reset();
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_MULTI_CHUNK);
    test_expect((result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 4U) &&
                (g_chunkRecords[0].size == 0xFFFFU) &&
                (g_chunkRecords[1].size == 0xFFFFU) &&
                (g_chunkRecords[2].size == 0xFFFFU) &&
                (g_chunkRecords[3].size == 3U) &&
                test_verify_write_sequence(TEST_LENGTH_MULTI_CHUNK),
                "write length=0x30000 -> 0xFFFF + 0xFFFF + 0xFFFF + 3");
    test_print_chunks();
}

static void test_case_read_lengths(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_error_t result = PLATFORM_ERR_OK;

    bus.implContext = &g_testContext;

    test_reset();
    result = stm32_spi_read(&bus, g_readBuffer, TEST_LENGTH_ONE);
    test_expect((result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 1U) &&
                test_verify_read_sequence(TEST_LENGTH_ONE),
                "read length=1 -> 1 HAL chunk");
    test_print_chunks();

    test_reset();
    result = stm32_spi_read(&bus, g_readBuffer, TEST_LENGTH_HAL_MAX);
    test_expect((result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 1U) &&
                (g_chunkRecords[0].size == 0xFFFFU) &&
                test_verify_read_sequence(TEST_LENGTH_HAL_MAX),
                "read length=0xFFFF -> 1 HAL chunk of 0xFFFF");
    test_print_chunks();

    test_reset();
    result = stm32_spi_read(&bus, g_readBuffer, TEST_LENGTH_OVER_HAL_MAX);
    test_expect((result != PLATFORM_ERR_OVERFLOW) &&
                (result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 2U) &&
                (g_chunkRecords[0].size == 0xFFFFU) &&
                (g_chunkRecords[1].size == 1U) &&
                test_verify_read_sequence(TEST_LENGTH_OVER_HAL_MAX),
                "read length=0x10000 -> 0xFFFF + 1, no PLATFORM_ERR_OVERFLOW");
    test_print_chunks();

    test_reset();
    result = stm32_spi_read(&bus, g_readBuffer, TEST_LENGTH_MULTI_CHUNK);
    test_expect((result == PLATFORM_ERR_OK) &&
                (g_chunkCount == 4U) &&
                (g_chunkRecords[3].size == 3U) &&
                test_verify_read_sequence(TEST_LENGTH_MULTI_CHUNK),
                "read length=0x30000 -> 0xFFFF + 0xFFFF + 0xFFFF + 3");
    test_print_chunks();
}

static void test_case_error_paths(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    platform_error_t result = PLATFORM_ERR_OK;

    bus.implContext = &g_testContext;

    test_reset();
    g_failAtChunk = 2U;
    g_failStatus = HAL_TIMEOUT;
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_OVER_HAL_MAX);
    test_expect((result == PLATFORM_ERR_TIMEOUT) && (g_chunkCount == 2U),
                "write 0x10000 timeout on chunk 2 -> PLATFORM_ERR_TIMEOUT and stop");
    test_print_chunks();

    test_reset();
    g_failAtChunk = 1U;
    g_failStatus = HAL_BUSY;
    result = stm32_spi_read(&bus, g_readBuffer, TEST_LENGTH_OVER_HAL_MAX);
    test_expect((result == PLATFORM_ERR_BUSY) && (g_chunkCount == 1U),
                "read 0x10000 busy on chunk 1 -> PLATFORM_ERR_BUSY and stop");
    test_print_chunks();

    test_reset();
    g_failAtChunk = 3U;
    g_failStatus = HAL_ERROR;
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_MULTI_CHUNK);
    test_expect((result == PLATFORM_ERR_IO) &&
                (g_chunkCount == 3U) &&
                (g_chunkRecords[2].size == 0xFFFFU),
                "write 0x30000 HAL_ERROR on chunk 3 -> PLATFORM_ERR_IO and stop");
    test_print_chunks();
}

static void test_case_param_checks(void)
{
    platform_spi_bus_t bus = PLATFORM_SPI_BUS_INITIALIZER;
    stm32_spi_impl_context_t emptyContext = { NULL };
    platform_error_t result = PLATFORM_ERR_OK;

    bus.implContext = &g_testContext;

    test_reset();
    result = stm32_spi_write(&bus, NULL, TEST_LENGTH_ONE);
    test_expect((result == PLATFORM_ERR_INVALID_PARAM) && (g_chunkCount == 0U),
                "write NULL -> PLATFORM_ERR_INVALID_PARAM without HAL call");

    result = stm32_spi_read(&bus, NULL, TEST_LENGTH_ONE);
    test_expect((result == PLATFORM_ERR_INVALID_PARAM) && (g_chunkCount == 0U),
                "read NULL -> PLATFORM_ERR_INVALID_PARAM without HAL call");

    result = stm32_spi_write(&bus, g_writeBuffer, 0U);
    test_expect((result == PLATFORM_ERR_INVALID_PARAM) && (g_chunkCount == 0U),
                "write length=0 -> PLATFORM_ERR_INVALID_PARAM without HAL call");

    result = stm32_spi_read(&bus, g_readBuffer, 0U);
    test_expect((result == PLATFORM_ERR_INVALID_PARAM) && (g_chunkCount == 0U),
                "read length=0 -> PLATFORM_ERR_INVALID_PARAM without HAL call");

    bus.implContext = NULL;
    result = stm32_spi_write(&bus, g_writeBuffer, TEST_LENGTH_OVER_HAL_MAX);
    test_expect((result == PLATFORM_ERR_INVALID_PARAM) && (g_chunkCount == 0U),
                "missing Impl context -> PLATFORM_ERR_INVALID_PARAM without HAL call");

    bus.implContext = &emptyContext;
    result = stm32_spi_read(&bus, g_readBuffer, TEST_LENGTH_OVER_HAL_MAX);
    test_expect((result == PLATFORM_ERR_NOT_INITIALIZED) && (g_chunkCount == 0U),
                "unbound HAL handle -> PLATFORM_ERR_NOT_INITIALIZED without HAL call");
}

int main(void)
{
    (void)printf("S02 SPI Impl chunking host test (HAL stub, PC only)\n");

    test_fill_write_buffer();

    test_case_write_lengths();
    test_case_read_lengths();
    test_case_error_paths();
    test_case_param_checks();

    if (g_failureCount == 0U) {
        (void)printf("RESULT: PASS (%u checks)\n", (unsigned int)g_checkCount);
        return 0;
    }

    (void)printf("RESULT: FAIL (%u checks, %u failed)\n",
                 (unsigned int)g_checkCount,
                 (unsigned int)g_failureCount);
    return 1;
}
//******************************** Private Functions *************************//
