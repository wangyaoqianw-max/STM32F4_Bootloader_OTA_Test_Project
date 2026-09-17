/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_ota_worker.h
 * @brief S06 otaWorker 生命周期接口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_OTA_WORKER_H
#define APP_OTA_WORKER_H

//******************************** Includes *********************************//
#include "app_runtime_contract.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 创建并启动 otaWorker。
 * @param[in] displayQueue : displayTask 创建并拥有的 Display Queue 句柄
 * @return platform_error_t : 创建结果
 * @note S06 仅冻结启动入口，不提前暴露 S07 OTA 业务控制接口。
 */
platform_error_t app_ota_worker_start(platform_queue_t *displayQueue);
//******************************** Functions ********************************//

#endif
