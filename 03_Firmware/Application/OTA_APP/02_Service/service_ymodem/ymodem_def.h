/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file ymodem_def.h
 * @brief YMODEM 协议固定定义
 * @author YaoQian Wang
 * @date 2026-09-14
 * @version V1.0
 *
 *****************************************************************************/

#ifndef YMODEM_DEF_H
#define YMODEM_DEF_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define YMODEM_SOH                            (0x01U)
#define YMODEM_STX                            (0x02U)
#define YMODEM_EOT                            (0x04U)
#define YMODEM_ACK                            (0x06U)
#define YMODEM_NAK                            (0x15U)
#define YMODEM_CAN                            (0x18U)
#define YMODEM_C                              (0x43U)

#define YMODEM_PACKET_DATA_SIZE_128           (128U)
#define YMODEM_PACKET_DATA_SIZE_1K            (1024U)
#define YMODEM_PACKET_MAX_SIZE                (1029U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/** @brief Parser 输出事件类型。 */
typedef enum
{
    YMODEM_PARSER_EVENT_NONE = 0, /**< 当前字节未形成协议事件。 */
    YMODEM_PARSER_EVENT_PACKET,    /**< 收到并验证通过一个完整 Packet。 */
    YMODEM_PARSER_EVENT_EOT,       /**< 收到 Packet 边界外的 EOT。 */
    YMODEM_PARSER_EVENT_CAN,       /**< 收到 Packet 边界外的 CAN。 */
    YMODEM_PARSER_EVENT_PACKET_ERROR /**< 完整 Packet 的序号或 CRC 校验失败。 */
} ymodem_parser_event_t;

/** @brief Parser 输出的单个已验证 YMODEM Packet。 */
typedef struct
{
    uint8_t blockNumber;       /**< 8-bit Packet 序号。 */
    const uint8_t *data;       /**< 指向 Parser 内部数据 Buffer。 */
    uint16_t dataSize;         /**< Packet 数据区长度，不含协议头和 CRC。 */
} ymodem_packet_t;
//******************************** Types ***********************************//

#endif
