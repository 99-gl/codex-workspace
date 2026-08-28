# PyLite

PyLite 是一个面向 Windows 10/11 x64 的轻量 Python 记事本。它是原生 Win32 C++ 便携程序，不包含 Python，也不安装或管理 Python 环境。

## 功能

- 单文件新建、打开、保存、另存为和未保存修改确认
- UTF-8、UTF-8 BOM 以及 PEP 263 `gbk`/`latin-1` 声明读取与原编码保存
- Python 关键字、字符串、注释、数字、函数、类和装饰器语法高亮
- Python 关键字、内置名称、函数、类、变量和 import 名称补全
- 通过独立 Python 子进程为 `json.`、`numpy as np`、`torch.nn.` 等上下文获取动态属性补全
- Python 解释器检测与选择，异步运行脚本，实时输出、退出码和进程树停止
- 懒加载文件树，隐藏 `.git` 和 `__pycache__`
- 可拖动左右栏和输出高度，输出区可折叠，布局写入 `%APPDATA%\PyLite\settings.json`
- 当前用户范围注册 `.py`/`.pyw` 的“打开方式”，不改变默认程序
- 支持 `PyLite.exe 文件.py`、`--register-open-with` 和 `--unregister-open-with`

## 使用

复制唯一的 `PyLite.exe` 到任意目录即可运行，无需安装 .NET、Visual Studio 或 C++ 运行库。第一次启动会尝试从 PATH 检测 `python.exe`/`python3.exe`，也可在工具菜单或顶部选择器中手动选择。按 `F5` 运行，`Shift+F5` 停止，`Ctrl+J` 折叠输出区。

补全列表在输入标识符时显示；方向键选择，Tab/Enter 接受，Esc 关闭。输入模块点号会启动所选 Python 的独立隐藏子进程。导入模块可能执行模块初始化代码，只应对可信 Python 环境使用动态补全。

栏间的浅灰分隔条可拖动；输出标题上方的横向分隔条可调整输出高度。工具菜单可注册或移除“打开方式”。

## 构建

构建环境为 Linux x86-64，使用 GCC 13 MinGW-w64 交叉工具链。当前成品用 Ubuntu 24.04 的以下稳定包构建：

- GCC/MinGW-w64 13.2.0（GPL-3.0-or-later with GCC Runtime Library Exception）
- MinGW-w64 11.0.1（ZPL-2.1 及各组件宽松许可证）
- GNU Binutils 2.41.90（GPL-3.0-or-later）
- Windows 系统 API；无应用运行时第三方 DLL

将解包后的工具链根目录通过 `PYLITE_MINGW_ROOT` 指定，或放在工作区 `.bootstrap/mingw`，然后：

```sh
cd pylite-win32
./scripts/build-win-x64.sh
```

脚本运行 Linux 核心测试、界面结构检查、资源编译、静态链接和单文件验证。成品位于 `artifacts/win-x64/PyLite.exe`。

单独测试：

```sh
g++ -std=c++17 -O2 -Wall -Wextra tests/core_tests.cpp -o build/core_tests
./build/core_tests
./tests/structure_tests.sh
```

## 快捷键

`Ctrl+N/O/Shift+O/S/Shift+S/Z/Y/A` 分别对应新建、打开文件、打开文件夹、保存、另存为、撤销、重做、全选；`F5` 运行，`Shift+F5` 停止，`Ctrl+J` 折叠输出区。

## 当前限制

- 只支持一个编辑文件，不含调试器、Git、终端、多标签、插件或环境管理。
- 不向运行中的脚本提供 stdin。
- Linux 无法原生启动 Windows GUI；本次验证覆盖核心逻辑、资源、PE 架构和静态依赖，未声称真实 Windows GUI 测试。
- 未做商业代码签名，Windows SmartScreen 可能提示“未知发布者”。

## 已验证成品

- 文件：`artifacts/win-x64/PyLite.exe`
- 大小：3,932,771 bytes（约 3.8 MiB）
- SHA-256：`1031528c0b749b23bd7def87b0ec579c28d4fedfde0b67f029fbaeea57e3bd97`
- 格式：PE32+ x86-64，Windows GUI 子系统
- 发布目录内容：仅 `PyLite.exe`
