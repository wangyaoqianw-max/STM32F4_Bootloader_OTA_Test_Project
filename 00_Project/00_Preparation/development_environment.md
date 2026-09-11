# Development Environment

本文件记录项目可复现所需的开发、编译、烧录、调试和脚本环境基线。

## 1. Hardware Baseline

| Item | Value | Status | Notes |
| --- | --- | --- | --- |
| MCU |  |  |  |
| Board / PCB Revision |  |  |  |
| Debug Probe |  |  |  |
| Host OS |  |  |  |

## 2. Firmware Toolchain

| Tool | Version | Purpose | Source / Install Note | Status |
| --- | --- | --- | --- | --- |
| Keil MDK |  | IDE / Build |  |  |
| ARM Compiler |  | Compiler |  |  |
| STM32CubeMX |  | Configuration / Code Generation |  |  |
| STM32CubeF4 Package |  | HAL / CMSIS Package |  |  |
| CMSIS |  | Cortex-M Support |  |  |
| FreeRTOS |  | RTOS |  |  |

## 3. Debug / Flash Tools

| Tool | Version | Purpose | Status | Notes |
| --- | --- | --- | --- | --- |
| J-Link Software |  | Flash / Debug |  |  |
| SEGGER RTT |  | Runtime Logging |  |  |
| EasyLogger |  | Logging |  |  |
| Logic Analyzer Software |  | Signal Verification |  |  |

## 4. Host / Script Environment

| Tool | Version | Purpose | Status | Notes |
| --- | --- | --- | --- | --- |
| Python |  | Packaging / Host Tools / Test |  |  |
| Git |  | Version Control |  |  |
| Other |  |  |  |  |

## 5. Build Entry

### Application

- Project File:
- Build Command / IDE Action:
- Output Directory:
- Main Output:

### Bootloader

- Project File:
- Build Command / IDE Action:
- Output Directory:
- Main Output:

## 6. Flash / Debug Entry

- Flash Method:
- Debug Method:
- SWD Clock:
- RTT Channel / Viewer:
- Reset Strategy:

## 7. Environment Verification

| Check | Expected Result | Actual Result | Status | Evidence |
| --- | --- | --- | --- | --- |
| Application builds successfully |  |  |  |  |
| Bootloader builds successfully |  |  |  |  |
| J-Link connects to target |  |  |  |  |
| Firmware can be flashed |  |  |  |  |
| RTT output is visible |  |  |  |  |

## 8. Notes

- 关键工具升级后，应更新版本并重新验证构建与调试入口。
- 不要只记录“最新版”等模糊描述，应填写可复现的具体版本。
