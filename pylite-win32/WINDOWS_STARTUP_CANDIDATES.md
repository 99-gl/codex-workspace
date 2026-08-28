# Windows 10 启动崩溃修复候选

## 当前判断

根因状态：**高可信推断，等待 Windows 10 真机 A/B 验证**。

Windows 侧已经确认原始 v1.0.0 在 `wWinMain` 之前以 `0xC0000409` 退出，调用栈经过 `USER32!UserClientDllInitialize` 和激活上下文初始化。原 manifest 同时包含以下无效声明：

```xml
<dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">PerMonitorV2</dpiAware>
```

微软的 DPI manifest 规范明确规定：2005 版 `dpiAware` 的每显示器值是 `true/pm`，不支持 `PerMonitorV2`；`PerMonitorV2` 属于 2016 版 `dpiAwareness`。修正为：

```xml
<dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
<dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2, PerMonitor</dpiAwareness>
```

官方参考：<https://learn.microsoft.com/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process>

## A/B/C 候选

三个文件均为 PE32+、Windows GUI、x86-64。导入表只包含 Windows 系统 DLL，包括 `ADVAPI32`、`COMCTL32`、`COMDLG32`、`GDI32`、`KERNEL32`、`MSVCRT`、`OLE32`、`SHELL32`、`SHLWAPI` 和 `USER32`。

| 文件 | 唯一变量 | 字节数 | SHA-256 |
|---|---|---:|---|
| `PyLite-A-original-manifest.exe` | 原始无效 DPI manifest；预期复现崩溃 | 3,932,771 | `d6168f9bd15edb19909cc87e90f304683dc786afa79ca28f7155660c2a7dcf8d` |
| `PyLite-B-fixed-manifest.exe` | 仅修正 DPI manifest；首选验证 | 3,932,771 | `0392420bdc76e3d5ad6fb315fce659b4d1eb59247486b324361fbc7629291bfe` |
| `PyLite-C-no-manifest.exe` | 完全移除 `RT_MANIFEST`；用于隔离 manifest 问题 | 3,931,235 | `fd93a10b73a8e5b176557ff746bfbc620807ec7e6de8c326a918bed432fc8b14` |

已用 `windres` 从三个 EXE 反向读取资源：A 含旧的 `dpiAware=PerMonitorV2`；B 含 `dpiAware=true/pm` 和 `dpiAwareness=PerMonitorV2, PerMonitor`；C 不含 `RT_MANIFEST`。

## Windows 10 验证顺序

先验证 B；若 B 仍异常，再验证 C。A 仅用于确认测试环境能复现旧故障，可不重复运行。

PowerShell：

```powershell
Get-FileHash .\PyLite-B-fixed-manifest.exe -Algorithm SHA256
$p = Start-Process .\PyLite-B-fixed-manifest.exe -PassThru
Start-Sleep -Seconds 3
if ($p.HasExited) { "EXIT=$($p.ExitCode)" } else { "RUNNING PID=$($p.Id)" }
```

若 B 能显示窗口并持续运行，即可把 DPI manifest 认定为已确认根因。若 B 仍以 `0xC0000409` 退出而 C 能运行，说明 manifest 中还有第二个不兼容项；若 B、C 都失败，再构建非全静态或 Win32 线程模型的 D 候选。

请记录每个候选的窗口是否出现、退出码，以及事件查看器中的故障模块和偏移。

## 构建信息

- `x86_64-w64-mingw32-g++-posix`：GCC 13，POSIX thread model
- GNU ld：2.41.90.20240122
- GNU windres：2.41.90.20240122
- 链接参数包含 `-static -static-libgcc -static-libstdc++`
- 构建入口：`scripts/build-win-x64.sh`
- manifest 校验：`tests/validate_manifest.py`
- 当前 Linux 环境没有 Wine，因此未声称完成 Windows GUI 运行验证

## 防回归

生产构建在资源编译前执行 manifest 静态校验。校验要求：

- 2005 版 `dpiAware` 必须且只能是 `true/pm`
- 2016 版 `dpiAwareness` 必须且只能是 `PerMonitorV2, PerMonitor`
- 节点必须各出现一次

校验失败会终止构建，不生成新的生产 EXE。
