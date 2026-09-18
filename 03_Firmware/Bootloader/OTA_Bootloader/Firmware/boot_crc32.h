/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_crc32.h
 * @brief Bootloader 最小 CRC-32/ISO-HDLC 接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_CRC32_H
#define BOOT_CRC32_H

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Types ***********************************//
typedef struct
{
    uint32_t value;
} boot_crc32_context_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 初始化 CRC-32/ISO-HDLC 流式计算上下文。
 * @param[out] context : CRC 上下文；为空时不执行操作。
 */
void boot_crc32_init(boot_crc32_context_t *context);
/**
 * @brief 向 CRC-32/ISO-HDLC 上下文追加数据。
 * @param[in,out] context : 已初始化上下文；为空时不执行操作。
 * @param[in] data : 数据缓冲区；length 非零时不得为空。
 * @param[in] length : 数据字节数。
 */
void boot_crc32_update(boot_crc32_context_t *context, const uint8_t *data, uint32_t length);
/**
 * @brief 获取当前 CRC-32/ISO-HDLC 结果。
 * @param[in] context : CRC 上下文；为空时返回 0。
 * @return 已执行 final XOR 的 CRC-32 结果。
 */
uint32_t boot_crc32_finalize(const boot_crc32_context_t *context);
/**
 * @brief 一次性计算 CRC-32/ISO-HDLC。
 * @param[in] data : 数据缓冲区；length 非零时不得为空。
 * @param[in] length : 数据字节数。
 * @return CRC-32 结果。
 */
uint32_t boot_crc32_calculate(const uint8_t *data, uint32_t length);
//******************************** Functions ********************************//

#endif
