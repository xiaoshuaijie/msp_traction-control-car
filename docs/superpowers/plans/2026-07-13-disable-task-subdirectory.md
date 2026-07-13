# 暂时停用 task 子目录构建实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 移除尚未实现的 `task/` 子目录构建入口，使 CMake 不再因该目录缺少 `CMakeLists.txt` 而配置失败。

**Architecture:** 保留 `task/` 及其占位文件，只从根目录构建图中移除过早的子目录引用。使用 CMake 配置命令作为回归测试，先记录失败，再应用单行修改并重新配置、构建。

**Tech Stack:** CMake 3.28、Ninja、GNU Arm Embedded Toolchain

---

### Task 1: 移除未实现的 task 子目录构建入口

**Files:**
- Modify: `CMakeLists.txt:30`
- Test: CMake 配置与构建命令

- [ ] **Step 1: 运行失败用例并确认 RED**

Run:

```powershell
cmake -S . -B build
```

Expected: 命令退出码非 0，输出包含：

```text
The source directory
  E:/msp_exe/msp_traction-control-car/task
does not contain a CMakeLists.txt file.
```

- [ ] **Step 2: 应用最小修改**

从根目录 `CMakeLists.txt` 删除且只删除以下一行：

```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/task")
```

保留下列已有模块引用：

```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/module/GreySensor")
add_subdirectory("${CMAKE_SOURCE_DIR}/module/tracking")
add_subdirectory("${CMAKE_SOURCE_DIR}/module/NRF24L01")
add_subdirectory("${CMAKE_SOURCE_DIR}/module/MPU6050")
```

- [ ] **Step 3: 重新配置并确认 GREEN**

Run:

```powershell
cmake -S . -B build
```

Expected: 输出不再包含 `task does not contain a CMakeLists.txt file`。理想结果为退出码 0；若出现新的独立配置错误，记录完整错误并停止扩大修改范围。

- [ ] **Step 4: 构建现有目标**

仅在 Step 3 退出码为 0 时运行：

```powershell
cmake --build build
```

Expected: 构建退出码 0，并生成 `build/ti_mspm0_libxr_dev.elf`。若现有未提交代码导致独立编译错误，保留本次单行修复并单独报告该错误。

- [ ] **Step 5: 检查修改范围并同步暂存状态**

Run:

```powershell
git diff --check -- CMakeLists.txt
git diff -- CMakeLists.txt
```

Expected: 未暂存差异只包含删除 `add_subdirectory("${CMAKE_SOURCE_DIR}/task")`，且 `git diff --check` 无输出。

确认后运行：

```powershell
git add -- CMakeLists.txt
git diff --cached --check -- CMakeLists.txt
```

Expected: 暂存区保留用户原有的模块接入改动，但不再包含 `task/` 子目录引用。由于 `CMakeLists.txt` 已含用户预先暂存的其他改动，本任务不创建代码提交，避免把用户改动纳入代理提交。
