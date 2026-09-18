/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_at24c02.h
 * @brief Bootloader AT24C02 读写驱动接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_AT24C02_H
#define BOOT_AT24C02_H

//******************************** Includes *********************************//
#include "boot_soft_i2c.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_AT24C02_TOTAL_SIZE_BYTES (256U)
#define BOOT_AT24C02_PAGE_SIZE_BYTES  (8U)
#define BOOT_AT24C02_ADDRESS          (0x50U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/**
 * @brief AT24C02 设备绑定。
 * @note I2C 总线生命周期由调用者拥有，设备只保存引用。
 */
typedef struct
{
    /** 已初始化的 Soft-I2C 总线。 */
    boot_soft_i2c_t *i2c;
    /** 7-bit 设备地址，通常为 0x50。 */
    uint8_t address;
    /** 非零表示地址探测已通过。 */
    uint8_t initialized;
} boot_at24c02_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 绑定并探测 AT24C02。
 * @param[out] eeprom : AT24C02 对象；不得为空。
 * @param[in] i2c : 已初始化 Soft-I2C 总线；驱动不重复初始化。
 * @param[in] address : 7-bit 设备地址，范围 0x50~0x57。
 * @return 驱动状态。
 */
boot_driver_status_t boot_at24c02_init(
    boot_at24c02_t *eeprom,
    boot_soft_i2c_t *i2c,
    uint8_t address);
/**
 * @brief 从 AT24C02 读取连续字节。
 * @param[in] eeprom : 已初始化设备。
 * @param[in] address : EEPROM 字节地址，范围 0x00~0xFF。
 * @param[out] data : 接收 Buffer；不得为空。
 * @param[in] length : 读取字节数，不得越过 256 Byte 容量。
 * @return 驱动状态。
 */
boot_driver_status_t boot_at24c02_read(
    boot_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    uint16_t length);
/**
 * @brief 向 AT24C02 写入连续字节并执行 ACK polling。
 * @param[in] eeprom : 已初始化设备。
 * @param[in] address : EEPROM 字节地址，范围 0x00~0xFF。
 * @param[in] data : 发送 Buffer；不得为空。
 * @param[in] length : 写入字节数，不得越过 256 Byte 容量。
 * @return 驱动状态；内部按 8 Byte Page 拆分。
 */
boot_driver_status_t boot_at24c02_write(
    boot_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    uint16_t length);
//******************************** Functions ********************************//

#endif
