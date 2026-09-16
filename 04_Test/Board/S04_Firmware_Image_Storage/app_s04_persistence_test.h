/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s04_persistence_test.h
 * @brief S04 Reset / Power-cycle Persistence 只读板测入口
 * @author YaoQian Wang
 * @date 2026-09-16
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_S04_PERSISTENCE_TEST_H
#define APP_S04_PERSISTENCE_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/** @brief 供 RTT 日志和 GDB 读取的 S04 持久化快照。 */
typedef struct
{
    uint32_t imageValidation;
    uint32_t imageSize;
    uint32_t imageCrc;
    uint32_t metadataAValid;
    uint32_t metadataBValid;
    uint32_t selectedCopy;
    uint32_t sequence;
    uint32_t activeSlot;
    uint32_t confirmedSlot;
    uint32_t slotAState;
    uint32_t slotBState;
    uint16_t confirmedVersionMajor;
    uint16_t confirmedVersionMinor;
    uint16_t confirmedVersionPatch;
} s04_persistence_snapshot_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
/** @brief 最近一次完整读取的快照，供 GDB 在复位后读取。 */
extern volatile s04_persistence_snapshot_t g_s04PersistenceSnapshot;
//******************************** Variables ********************************//

//******************************** Functions ********************************//
/**
 * @brief 执行 S04 持久化只读板测并周期性输出快照
 * @return PLATFORM_ERR_OK 表示初始化完成且测试持续运行；其他值表示初始化或读取失败
 * @note 本入口只读取 W25Q64 和 AT24C02，不执行擦除、写入或 Metadata 提交。
 * @warning 该入口必须只通过临时测试开关接入，不得作为正式 Application 启动路径。
 */
platform_error_t app_s04_persistence_test_run(void);
//******************************** Functions ********************************//

#endif
