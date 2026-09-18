/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.h
 * @brief Application System Composition / Bootstrap 接口。
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 在 defaultTask 上完成 Application 系统装配和启动裁决。
 * @return PLATFORM_ERR_OK 表示进入 RUNNING 或 DEGRADED；其他值表示启动基础设施失败。
 */
platform_error_t app_system_bootstrap(void);
//******************************** Functions ********************************//

#endif
