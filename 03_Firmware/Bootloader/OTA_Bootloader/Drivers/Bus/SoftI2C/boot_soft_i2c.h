/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_soft_i2c.h
 * @brief Bootloader Soft-I2C 最小同步接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_SOFT_I2C_H
#define BOOT_SOFT_I2C_H

//******************************** Includes *********************************//
#include "boot_driver_status.h"
#include "main.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief Bootloader Soft-I2C 两线总线绑定。
 * @note SCL/SDA GPIO 已由 CubeMX 配置为开漏输出；总线只操作线路电平。
 */
typedef struct
{
    /** SCL GPIO 端口。 */
    GPIO_TypeDef *sclPort;
    /** SCL GPIO 引脚。 */
    uint16_t sclPin;
    /** SDA GPIO 端口。 */
    GPIO_TypeDef *sdaPort;
    /** SDA GPIO 引脚。 */
    uint16_t sdaPin;
    /** 非零表示总线绑定完成。 */
    uint8_t initialized;
} boot_soft_i2c_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 绑定并释放 Soft-I2C 总线。
 * @param[out] i2c : 总线对象；不得为空。
 * @param[in] sclPort : SCL GPIO 端口。
 * @param[in] sclPin : SCL GPIO 引脚。
 * @param[in] sdaPort : SDA GPIO 端口。
 * @param[in] sdaPin : SDA GPIO 引脚。
 * @return 驱动状态；若线路无法回到 Idle 则返回 BUSY。
 */
boot_driver_status_t boot_soft_i2c_init(
    boot_soft_i2c_t *i2c,
    GPIO_TypeDef *sclPort,
    uint16_t sclPin,
    GPIO_TypeDef *sdaPort,
    uint16_t sdaPin);
/**
 * @brief 探测一个 7-bit I2C 从设备地址。
 * @param[in] i2c : 已初始化总线。
 * @param[in] address : 7-bit 从设备地址。
 * @return ACK 返回 OK，NACK 返回 NOT_FOUND。
 */
boot_driver_status_t boot_soft_i2c_probe(
    boot_soft_i2c_t *i2c,
    uint8_t address);
/**
 * @brief 向 7-bit 从设备写入连续字节。
 * @param[in] i2c : 已初始化总线。
 * @param[in] address : 7-bit 从设备地址。
 * @param[in] data : 发送 Buffer；不得为空。
 * @param[in] length : 发送字节数，必须大于 0。
 * @return 驱动状态；失败时尝试发送 STOP 释放总线。
 */
boot_driver_status_t boot_soft_i2c_write(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    const uint8_t *data,
    uint16_t length);
/**
 * @brief 从 7-bit 从设备连续读取字节。
 * @param[in] i2c : 已初始化总线。
 * @param[in] address : 7-bit 从设备地址。
 * @param[out] data : 接收 Buffer；不得为空。
 * @param[in] length : 接收字节数，必须大于 0。
 * @return 驱动状态。
 */
boot_driver_status_t boot_soft_i2c_read(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    uint8_t *data,
    uint16_t length);
/**
 * @brief 执行写地址后 Repeated START 读取事务。
 * @param[in] i2c : 已初始化总线。
 * @param[in] address : 7-bit 从设备地址。
 * @param[in] txData : 写阶段 Buffer；不得为空。
 * @param[in] txLength : 写阶段字节数，必须大于 0。
 * @param[out] rxData : 读阶段 Buffer；不得为空。
 * @param[in] rxLength : 读阶段字节数，必须大于 0。
 * @return 驱动状态。
 */
boot_driver_status_t boot_soft_i2c_write_read(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    const uint8_t *txData,
    uint16_t txLength,
    uint8_t *rxData,
    uint16_t rxLength);
//******************************** Functions ********************************//

#endif
