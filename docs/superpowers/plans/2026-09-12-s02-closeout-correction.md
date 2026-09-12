# S02 Closeout Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 收口 S02 返工后的工程状态、交接证据和 machine-local Keil 配置跟踪，使阶段停留在 `READY_FOR_VERIFICATION`，并保留真实硬件回归为 `PENDING`。

**Architecture:** 不改动已经完成的 SPI Impl、W25Q64 Driver、Platform SPI 公共接口、Host Test 或 Review 历史结论。只同步阶段入口/状态/交接/验证报告，并从 Git 索引移除 `toolchain.local.bat`，保留本机文件和仓库模板。

**Tech Stack:** Git、PowerShell、Markdown、现有 S02 Host Test、Keil 统一构建脚本。

**Spec:** 用户提供的“S02 收口修正”任务；阶段依据 `00_Project/WORKFLOW.md` 与 `00_Project/03_Stages/S02_External_Flash_Driver/design.md`。

## Global Constraints

- Active Stage 必须保持 `S02_External_Flash_Driver`。
- Status 和 Current Role 必须统一为 `READY_FOR_VERIFICATION` / `Verification`。
- Rework Commit 必须记录为 `42c02b891d7f32b728857c44024c3c91a15ea604`。
- `review.md` 保持现有 `CHANGES_REQUESTED`，`implementation_plan.md` 作为冻结计划不修改。
- `impl_platform_spi.c`、W25Q64 Driver 和 Vendor 文件不修改。
- `toolchain.local.bat` 本地内容保留但不再由 Git 跟踪；`toolchain.local.example.bat` 保留。
- Original S02 Hardware Verification 保持 `PASS`；Finding 1 后的 Rework Hardware Regression 保持 `PENDING`。
- 不重写 Host Test，不新增测试框架，不重新设计 SPI 功能。

---

### Task 1: Reconcile S02 status and handoff documents

**Files:**
- Modify: `PROJECT_CONTEXT.md`
- Modify: `00_Project/05_Status/current_status.md`
- Modify: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`

**Interfaces:**
- Consumes: `00_Project/WORKFLOW.md`, current S02 review/verification evidence, existing Rework Commit.
- Produces: synchronized `READY_FOR_VERIFICATION` status, Verification role, exact Rework Commit, and minimal Project Owner next action.

- [ ] **Step 1: Update metadata and goal wording**

Set the three files' active status to `READY_FOR_VERIFICATION`, role to `Verification`, and Rework Commit to `42c02b891d7f32b728857c44024c3c91a15ea604`. State that SPI long-length rework plus Host/Build verification is complete and only the minimum real hardware regression remains.

- [ ] **Step 2: Preserve review history and narrow next action**

Keep `review.md` unchanged and continue to describe it as `CHANGES_REQUESTED`. In the three synchronized documents, require only W25Q64 Init, JEDEC `EF 40 17`, an ordinary Read, and at least one real Read Back/Compare case; do not require the complete destructive suite unless this regression fails.

- [ ] **Step 3: Verify document-level consistency**

Search the three files and confirm they contain the same active stage, status, role, Rework Commit, and pending regression wording, while no `review.md` or `implementation_plan.md` change is introduced.

### Task 2: Correct verification report semantics

**Files:**
- Modify: `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`

**Interfaces:**
- Consumes: existing original S02 hardware evidence and Finding 1 rework evidence.
- Produces: a report that distinguishes original hardware PASS from rework regression PENDING.

- [ ] **Step 1: Reconcile the report status block**

Mark the report as awaiting verification, label the original S02 hardware result separately as `PASS`, and retain `Rework Code Verification: PASS`, `Keil Normal Build: PASS`, `Keil Clean/Rebuild: PASS`, and `Rework Hardware Regression: PENDING`.

- [ ] **Step 2: Record exact Rework Commit and local-config wording**

Replace every rework placeholder in the report with `42c02b891d7f32b728857c44024c3c91a15ea604`. Describe `toolchain.local.bat` as machine-local, currently untracked by Git, with only `toolchain.local.example.bat` retained in the repository; do not claim it never existed in Git history.

- [ ] **Step 3: Verify report does not claim rework hardware PASS**

Search the report for all hardware status lines and confirm the only Finding 1 regression result is `PENDING`; keep the earlier JEDEC/Erase/Program/Cross-page/Reset evidence clearly under Original S02 Hardware Verification.

### Task 3: Stop tracking the machine-local Keil configuration

**Files:**
- Keep local file: `05_Tools/Config/toolchain.local.bat`
- Keep tracked template: `05_Tools/Config/toolchain.local.example.bat`
- Update Git index: `05_Tools/Config/toolchain.local.bat`

**Interfaces:**
- Consumes: existing `.gitignore` rule and current tracked local configuration.
- Produces: local configuration present on disk but absent from `git ls-files` and the new commit.

- [ ] **Step 1: Remove only the local file from the index**

Run `git rm --cached -- 05_Tools/Config/toolchain.local.bat` after confirming the exact target. Do not delete or rewrite its local contents and do not alter `.gitignore`.

- [ ] **Step 2: Verify index and files**

Confirm `Test-Path` is true for both local and example files, `git ls-files --error-unmatch 05_Tools/Config/toolchain.local.bat` fails because it is no longer tracked, and the example remains tracked.

### Task 4: Run bounded verification and commit the closeout

**Files:**
- Verify: existing `04_Test/Host/S02_External_Flash_Driver/` tests
- Verify: `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.c`
- Verify: `review.md`, `implementation_plan.md`, `.gitignore`, template/local config

**Interfaces:**
- Consumes: Tasks 1-3 working-tree changes.
- Produces: fresh diff/status evidence and one closeout commit.

- [ ] **Step 1: Run existing Host Test without expanding it**

Use the command documented by `04_Test/Host/S02_External_Flash_Driver/README.md`; confirm coverage for `1`, `0xFFFF`, `0x10000`, `0x30000`, `HAL_TIMEOUT`, `HAL_BUSY`, `HAL_ERROR`, `NULL`, and zero length.

- [ ] **Step 2: Run repository checks**

Run `git diff --check` and `git status`, inspect the diff, and confirm no production SPI, W25Q64 Driver, Vendor, Review, or frozen plan file changed. Run `05_Tools\\Scripts\\build_app.bat` if the local toolchain configuration is usable.

- [ ] **Step 3: Commit one single-purpose closeout change**

Stage the plan, four requested documentation files, and the index deletion, then commit with:

```text
chore: correct S02 rework handoff state
```

- [ ] **Step 4: Verify the committed result**

Read the new commit SHA, inspect `git status`, and re-check that the local Keil configuration remains on disk but is not tracked, the active status is `READY_FOR_VERIFICATION`, and Rework Hardware Regression remains `PENDING`.

---

## Plan Self-check

This plan covers the requested state correction, exact Rework Commit recording, machine-local configuration untracking, bounded Host Test/build verification, preservation of Review history, and final commit evidence. It does not change SPI behavior or expand S02 functionality.
