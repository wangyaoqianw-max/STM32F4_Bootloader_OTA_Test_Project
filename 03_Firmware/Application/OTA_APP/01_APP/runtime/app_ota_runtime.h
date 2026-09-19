/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ota_runtime.h
 * @brief OTA Application 运行时资源绑定接口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_OTA_RUNTIME_H
#define APP_OTA_RUNTIME_H

//******************************** Includes *********************************//
#include "platform_error.h"
#include "platform_types.h"
#include "platform_os.h"
#include "service_ota.h"
#include "service_uart.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 初始化 OTA Application 使用的 Storage、UART 与 OTA Service
 * @param[in] ownerThread : OTA Worker 所属的 RTOS Thread
 * @return platform_error_t : 初始化结果
 * @note 本模块只负责已有资源的板级绑定，不实现 OTA 业务状态机。
 */
platform_error_t app_ota_runtime_init(platform_thread_t *ownerThread);

/**
 * @brief 开始一次 OTA UART Session
 * @return platform_error_t : 启动结果
 * @note 业务目标 Slot 与 Metadata 事务由 service_ota 处理。
 */
platform_error_t app_ota_runtime_start_session(void);

/**
 * @brief 停止当前 OTA UART Session
 * @return platform_error_t : 停止结果
 */
platform_error_t app_ota_runtime_stop_session(void);

/**
 * @brief 判断 OTA Service 是否仍处于接收状态
 * @return PLATFORM_TRUE 表示接收仍在进行；否则返回 PLATFORM_FALSE
 */
platform_bool_t app_ota_runtime_session_is_active(void);

/**
 * @brief 检查 UART Service 是否可以继续被 OTA Worker 消费
 * @return platform_error_t : UART 状态结果
 */
platform_error_t app_ota_runtime_check_uart(void);

/**
 * @brief 获取 UART RingBuffer 当前可读字节数
 * @param[out] readableSize : 可读字节数
 * @return platform_error_t : 查询结果
 */
platform_error_t app_ota_runtime_get_readable_size(
    platform_size_t *readableSize);

/**
 * @brief 从 OTA UART RingBuffer 读取数据
 * @param[out] buffer : 接收 Buffer
 * @param[in] bufferSize : Buffer 容量，单位为 Byte
 * @param[out] readLength : 实际读取长度
 * @return platform_error_t : 读取结果
 */
platform_error_t app_ota_runtime_read(
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength);

/**
 * @brief 在 OTA Worker 上下文执行严格 Trial Confirm
 * @return platform_error_t : Lifecycle 事务结果
 * @note 不向其他 Task 暴露 Firmware Storage 或底层 Driver。
 */
platform_error_t app_ota_runtime_confirm_trial(void);

/**
 * @brief 获取 OTA Worker 初始化时读取到的 Firmware Lifecycle 状态。
 * @param[out] trial : PLATFORM_TRUE 表示当前 Application 处于 Trial
 * @return platform_error_t : 查询结果
 * @note 本接口只返回缓存的状态，不执行 Storage I/O。
 */
platform_error_t app_ota_runtime_get_trial_status(platform_bool_t *trial);

/**
 * @brief 获取 OTA Service 对象
 * @return OTA Service 对象指针；运行时未初始化时返回 NULL
 */
service_ota_t *app_ota_runtime_get_service(void);

/**
 * @brief 获取 UART Service 统计信息
 * @param[out] statistics : UART 统计信息输出
 * @return platform_error_t : 查询结果
 */
platform_error_t app_ota_runtime_get_uart_statistics(
    service_uart_statistics_t *statistics);
//******************************** Functions ********************************//

#endif
