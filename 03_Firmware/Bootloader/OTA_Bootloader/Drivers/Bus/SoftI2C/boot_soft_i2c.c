/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_soft_i2c.c
 * @brief Bootloader Soft-I2C 最小同步实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_soft_i2c.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_I2C_SCL_TIMEOUT_US (1000U)
#define BOOT_I2C_HALF_PERIOD_US (2U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static void boot_soft_i2c_delay_us(uint32_t microseconds)
{
    uint32_t cyclesPerUs;
    uint32_t start;

    if (microseconds == 0U) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    cyclesPerUs = SystemCoreClock / 1000000U;
    if (cyclesPerUs == 0U) {
        return;
    }
    start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < (microseconds * cyclesPerUs)) {
    }
}

static boot_driver_status_t boot_soft_i2c_validate(
    const boot_soft_i2c_t *i2c)
{
    if (i2c == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if ((i2c->initialized == 0U) || (i2c->sclPort == NULL) ||
        (i2c->sdaPort == NULL)) {
        return BOOT_DRIVER_ERR_NOT_INITIALIZED;
    }

    return BOOT_DRIVER_OK;
}

static void boot_soft_i2c_scl_low(boot_soft_i2c_t *i2c)
{
    HAL_GPIO_WritePin(i2c->sclPort, i2c->sclPin, GPIO_PIN_RESET);
}

static void boot_soft_i2c_scl_release(boot_soft_i2c_t *i2c)
{
    HAL_GPIO_WritePin(i2c->sclPort, i2c->sclPin, GPIO_PIN_SET);
}

static void boot_soft_i2c_sda_low(boot_soft_i2c_t *i2c)
{
    HAL_GPIO_WritePin(i2c->sdaPort, i2c->sdaPin, GPIO_PIN_RESET);
}

static void boot_soft_i2c_sda_release(boot_soft_i2c_t *i2c)
{
    HAL_GPIO_WritePin(i2c->sdaPort, i2c->sdaPin, GPIO_PIN_SET);
}

static boot_driver_status_t boot_soft_i2c_wait_scl_high(boot_soft_i2c_t *i2c)
{
    uint32_t waitedUs = 0U;

    /* 释放 SCL 后读取物理电平，支持从设备 clock stretching。 */
    boot_soft_i2c_scl_release(i2c);
    while (waitedUs < BOOT_I2C_SCL_TIMEOUT_US) {
        if (HAL_GPIO_ReadPin(i2c->sclPort, i2c->sclPin) == GPIO_PIN_SET) {
            return BOOT_DRIVER_OK;
        }
        boot_soft_i2c_delay_us(1U);
        waitedUs++;
    }

    return BOOT_DRIVER_ERR_TIMEOUT;
}

static boot_driver_status_t boot_soft_i2c_start(boot_soft_i2c_t *i2c)
{
    boot_driver_status_t result;

    /* SCL 为高时拉低 SDA，形成 START 或 Repeated START。 */
    boot_soft_i2c_sda_release(i2c);
    result = boot_soft_i2c_wait_scl_high(i2c);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    boot_soft_i2c_sda_low(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    boot_soft_i2c_scl_low(i2c);
    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_soft_i2c_stop(boot_soft_i2c_t *i2c)
{
    boot_driver_status_t result;

    /* SCL 为高时释放 SDA，形成 STOP 并把总线交还 Idle。 */
    boot_soft_i2c_sda_low(i2c);
    result = boot_soft_i2c_wait_scl_high(i2c);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    boot_soft_i2c_sda_release(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_soft_i2c_write_bit(
    boot_soft_i2c_t *i2c,
    GPIO_PinState level)
{
    boot_driver_status_t result;

    boot_soft_i2c_scl_low(i2c);
    if (level == GPIO_PIN_SET) {
        boot_soft_i2c_sda_release(i2c);
    } else {
        boot_soft_i2c_sda_low(i2c);
    }
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    result = boot_soft_i2c_wait_scl_high(i2c);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    boot_soft_i2c_scl_low(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_soft_i2c_read_bit(
    boot_soft_i2c_t *i2c,
    GPIO_PinState *level)
{
    boot_driver_status_t result;

    boot_soft_i2c_scl_low(i2c);
    boot_soft_i2c_sda_release(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    result = boot_soft_i2c_wait_scl_high(i2c);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    *level = HAL_GPIO_ReadPin(i2c->sdaPort, i2c->sdaPin);
    boot_soft_i2c_scl_low(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_soft_i2c_write_byte(
    boot_soft_i2c_t *i2c,
    uint8_t value)
{
    uint8_t mask = 0x80U;
    GPIO_PinState level;
    boot_driver_status_t result;

    /* 每个字节发送后单独采样 ACK，NACK 立即终止当前事务。 */
    while (mask != 0U) {
        level = ((value & mask) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        result = boot_soft_i2c_write_bit(i2c, level);
        if (result != BOOT_DRIVER_OK) {
            return result;
        }
        mask >>= 1U;
    }

    boot_soft_i2c_sda_release(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    result = boot_soft_i2c_wait_scl_high(i2c);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    result = (HAL_GPIO_ReadPin(i2c->sdaPort, i2c->sdaPin) == GPIO_PIN_RESET) ?
             BOOT_DRIVER_OK : BOOT_DRIVER_ERR_NOT_FOUND;
    boot_soft_i2c_scl_low(i2c);
    boot_soft_i2c_delay_us(BOOT_I2C_HALF_PERIOD_US);
    return result;
}

static boot_driver_status_t boot_soft_i2c_read_byte(
    boot_soft_i2c_t *i2c,
    uint8_t *value,
    uint8_t acknowledge)
{
    uint8_t bitIndex;
    GPIO_PinState level;
    boot_driver_status_t result;

    /* 最后一个字节发送 NACK，通知从设备读取事务结束。 */
    *value = 0U;
    for (bitIndex = 0U; bitIndex < 8U; bitIndex++) {
        result = boot_soft_i2c_read_bit(i2c, &level);
        if (result != BOOT_DRIVER_OK) {
            return result;
        }
        *value <<= 1U;
        if (level == GPIO_PIN_SET) {
            *value |= 1U;
        }
    }

    result = boot_soft_i2c_write_bit(
        i2c,
        (acknowledge != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return result;
}

static boot_driver_status_t boot_soft_i2c_begin(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    uint8_t read)
{
    boot_driver_status_t result = boot_soft_i2c_start(i2c);

    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    result = boot_soft_i2c_write_byte(i2c, (uint8_t)((address << 1U) | read));
    if (result != BOOT_DRIVER_OK) {
        (void)boot_soft_i2c_stop(i2c);
    }
    return result;
}

static boot_driver_status_t boot_soft_i2c_end(
    boot_soft_i2c_t *i2c,
    boot_driver_status_t operationResult)
{
    boot_driver_status_t stopResult = boot_soft_i2c_stop(i2c);

    return (operationResult != BOOT_DRIVER_OK) ? operationResult : stopResult;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_soft_i2c_init(
    boot_soft_i2c_t *i2c,
    GPIO_TypeDef *sclPort,
    uint16_t sclPin,
    GPIO_TypeDef *sdaPort,
    uint16_t sdaPin)
{
    if ((i2c == NULL) || (sclPort == NULL) || (sdaPort == NULL)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (i2c->initialized != 0U) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    /* GPIO 已由 CubeMX 配置为开漏输出，本模块只释放线路并检查 Idle。 */
    i2c->sclPort = sclPort;
    i2c->sclPin = sclPin;
    i2c->sdaPort = sdaPort;
    i2c->sdaPin = sdaPin;
    i2c->initialized = 1U;
    boot_soft_i2c_scl_release(i2c);
    boot_soft_i2c_sda_release(i2c);

    if ((HAL_GPIO_ReadPin(sclPort, sclPin) != GPIO_PIN_SET) ||
        (HAL_GPIO_ReadPin(sdaPort, sdaPin) != GPIO_PIN_SET)) {
        i2c->initialized = 0U;
        return BOOT_DRIVER_ERR_BUSY;
    }

    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_soft_i2c_probe(
    boot_soft_i2c_t *i2c,
    uint8_t address)
{
    boot_driver_status_t result = boot_soft_i2c_validate(i2c);

    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if (address > 0x7FU) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    result = boot_soft_i2c_begin(i2c, address, 0U);
    return (result == BOOT_DRIVER_OK) ? boot_soft_i2c_end(i2c, result) : result;
}

boot_driver_status_t boot_soft_i2c_write(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    const uint8_t *data,
    uint16_t length)
{
    uint16_t index;
    boot_driver_status_t result = boot_soft_i2c_validate(i2c);

    if ((data == NULL) && (length != 0U)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if ((address > 0x7FU) || (length == 0U)) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    result = boot_soft_i2c_begin(i2c, address, 0U);
    for (index = 0U; (result == BOOT_DRIVER_OK) && (index < length); index++) {
        result = boot_soft_i2c_write_byte(i2c, data[index]);
    }
    return boot_soft_i2c_end(i2c, result);
}

boot_driver_status_t boot_soft_i2c_read(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    uint8_t *data,
    uint16_t length)
{
    uint16_t index;
    boot_driver_status_t result = boot_soft_i2c_validate(i2c);

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if ((address > 0x7FU) || (length == 0U)) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    result = boot_soft_i2c_begin(i2c, address, 1U);
    for (index = 0U; (result == BOOT_DRIVER_OK) && (index < length); index++) {
        result = boot_soft_i2c_read_byte(i2c, &data[index], (index + 1U < length) ? 1U : 0U);
    }
    return boot_soft_i2c_end(i2c, result);
}

boot_driver_status_t boot_soft_i2c_write_read(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    const uint8_t *txData,
    uint16_t txLength,
    uint8_t *rxData,
    uint16_t rxLength)
{
    uint16_t index;
    boot_driver_status_t result = boot_soft_i2c_validate(i2c);

    if ((txData == NULL) || (rxData == NULL)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if ((address > 0x7FU) || (txLength == 0U) || (rxLength == 0U)) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    result = boot_soft_i2c_begin(i2c, address, 0U);
    for (index = 0U; (result == BOOT_DRIVER_OK) && (index < txLength); index++) {
        result = boot_soft_i2c_write_byte(i2c, txData[index]);
    }
    if (result == BOOT_DRIVER_OK) {
        result = boot_soft_i2c_start(i2c);
    }
    if (result == BOOT_DRIVER_OK) {
        result = boot_soft_i2c_write_byte(i2c, (uint8_t)((address << 1U) | 1U));
    }
    for (index = 0U; (result == BOOT_DRIVER_OK) && (index < rxLength); index++) {
        result = boot_soft_i2c_read_byte(i2c, &rxData[index], (index + 1U < rxLength) ? 1U : 0U);
    }
    return boot_soft_i2c_end(i2c, result);
}
