# 暂时停用 task 子目录构建设计

## 背景

根目录 `CMakeLists.txt` 当前调用 `add_subdirectory("${CMAKE_SOURCE_DIR}/task")`，但 `task/` 下没有 `CMakeLists.txt`，其中四个源文件也都是空的占位文件。这会让 CMake 配置在进入实际编译前失败。

## 设计

- 从根目录 `CMakeLists.txt` 移除 `add_subdirectory("${CMAKE_SOURCE_DIR}/task")`。
- 保留 `task/` 目录及其中所有占位文件，不删除、不改写。
- 不调整其他模块的构建方式，也不为尚未实现的任务代码创建空目标。
- 后续任务代码具备实际实现和构建需求时，再新增 `task/CMakeLists.txt` 并恢复子目录引用。

## 验证

- 修改前，`cmake -S . -B build` 应因 `task/` 缺少 `CMakeLists.txt` 而失败。
- 修改后，重新运行 `cmake -S . -B build`，确认该错误消失。
- 配置成功后运行 `cmake --build build`，确认现有目标仍可构建。
- 如果出现与本次修改无关的新错误，单独报告并重新定位，不扩大本次修改范围。

## 范围边界

本次只处理 `task/` 被过早加入构建的问题，不实现 `NRF24L01_jie` 或 `pid_jie`，也不修改其他模块。
