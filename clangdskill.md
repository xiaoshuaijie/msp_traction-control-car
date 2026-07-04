---
name: clangd-embedded-navigation-fix
description: Use when VS Code reports "The clangd language server is not installed", Ctrl+click/go-to-definition fails in embedded C/C++ CMake projects, clangd is installed outside PATH, or clangd cannot parse ARM/GCC compile_commands.json.
---

# Clangd Embedded Navigation Fix

## 适用场景

当用户遇到这些症状时使用本 skill：

- VS Code 提示 `The clangd language server is not installed`
- C/C++ 代码无法 `Ctrl + 鼠标左键` 跳转
- 已安装 VS Code clangd 扩展，但 `clangd --version` 提示命令不存在
- 项目是 CMake + ARM GCC/交叉编译工具链，已有或应生成 `compile_commands.json`
- clangd 能启动但报 GCC 专用参数错误，例如 `unknown argument: '-fconcepts'`

## 核心判断

不要把 “clangd 扩展已安装” 等同于 “clangd 语言服务器可用”。VS Code 的 `llvm-vs-code-extensions.vscode-clangd` 只是扩展；真正需要的是可执行文件 `clangd.exe`。

本项目这次的根因是：

- `clangd --version` 在 PowerShell 中失败，说明 `clangd.exe` 不在 PATH
- 用户提供了可用 clangd：`E:/RM/RM/clangd_22.1.0/bin/clangd.exe`
- 项目已有 `build/compile_commands.json`
- clangd 读取编译数据库后又被 GCC 参数 `-fconcepts` 卡住

## 诊断步骤

先做只读诊断，避免猜测：

```powershell
clangd --version
code --list-extensions
Get-ChildItem -Path '.' -Recurse -Filter 'compile_commands.json' -File -ErrorAction SilentlyContinue
Get-Content -Path 'CMakeLists.txt' -Raw -Encoding UTF8
```

判断标准：

- `clangd --version` 失败：需要定位或指定 `clangd.exe`
- `compile_commands.json` 不存在：先重新配置 CMake，确保 `CMAKE_EXPORT_COMPILE_COMMANDS ON`
- `compile_commands.json` 存在但 clangd 报错：用 `.clangd` 过滤 clangd 不支持的编译参数

## 本项目推荐修复

在 `.vscode/settings.json` 中固定 clangd 路径和编译数据库：

```json
{
  "clangd.path": "E:/RM/RM/clangd_22.1.0/bin/clangd.exe",
  "clangd.arguments": [
    "--compile-commands-dir=E:/msp_exe/MSPM0G3507_LibXR_Template/build",
    "--query-driver=E:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-*",
    "--background-index"
  ],
  "C_Cpp.intelliSenseEngine": "disabled"
}
```

说明：

- `clangd.path` 解决 clangd 不在 PATH 的问题
- `--compile-commands-dir` 让 clangd 使用 CMake 生成的真实编译参数
- `--query-driver` 允许 clangd 调用 ARM GCC 提取系统 include 路径和 target
- `C_Cpp.intelliSenseEngine: disabled` 避免 Microsoft C/C++ IntelliSense 和 clangd 抢同一批 C/C++ 文件

## `.clangd` 处理 GCC 专用参数

如果 clangd 报：

```text
unknown argument: '-fconcepts'
```

不要改 CMake 构建参数；只在 `.clangd` 里让 clangd 忽略该参数。

本项目原本已有 `.clangd` 配置，必须保留原内容，只在 `CompileFlags` 下追加：

```yaml
CompileFlags:
  Remove: [-fconcepts]
```

如果 `CompileFlags` 已有 `Add`，就放在同级：

```yaml
CompileFlags:
  Add: [...]
  Remove: [-fconcepts]
```

## 验证命令

完成后必须运行：

```powershell
& 'E:/RM/RM/clangd_22.1.0/bin/clangd.exe' --compile-commands-dir='E:/msp_exe/MSPM0G3507_LibXR_Template/build' --query-driver='E:/DevEnv/GNU-tools-for-STM32/bin/arm-none-eabi-*' --check='E:/msp_exe/MSPM0G3507_LibXR_Template/src/app_main.cpp'
```

通过标准：

```text
All checks completed, 0 errors
```

如果仍失败，先读完整错误，不要继续叠加配置。重点看：

- 是否加载到正确的 `compile_commands.json`
- 是否成功执行 `arm-none-eabi-g++.exe`
- 是否仍有 `unknown argument`
- 是否缺少系统头文件或项目 include

## VS Code 操作

配置完成并验证通过后，让用户执行其中一个：

- `Developer: Reload Window`
- `clangd: Restart language server`

然后再测试 `Ctrl + 鼠标左键` 或 `Go to Definition`。

## 常见误区

- 不要只安装 clangd 扩展后就认为语言服务器可用；必须有 `clangd.exe`
- 不要在 PATH 问题未确认前改大量 includePath
- 不要用 `c_cpp_properties.json` 替代 `compile_commands.json` 作为首选方案
- 不要覆盖已有 `.clangd`，只做最小增量修改
- 不要为了 clangd 修改真实构建参数；clangd 专用兼容项放 `.clangd`
- 不要在未运行 `clangd --check` 前声称跳转已修复
