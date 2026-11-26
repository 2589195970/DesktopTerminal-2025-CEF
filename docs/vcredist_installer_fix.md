# Windows 安装与终端服务诊断

## VC++ 运行库

安装器在复制程序和启动终端服务之前，按照安装包架构检查 Windows 系统目录中的 VC++ DLL。

| 安装包 | 最低版本 | 检查文件 |
| --- | --- | --- |
| x86（Win7 SP1 兼容包） | 14.29.30157.0 | `msvcp140.dll`、`vcruntime140.dll` |
| x64 | 14.44.35211.0 | `msvcp140.dll`、`vcruntime140.dll`、`vcruntime140_1.dll` |

DLL 缺失或版本过低时，安装器静默运行随包提供的 Microsoft 离线安装程序。完成后复查 DLL，失败时结束安装并记录原因。若运行库要求重启，安装器完成服务配置，服务随 Windows 下次启动。静默安装以退出码 `3010` 表示需要重启。

安装目录中的 `install.log` 记录检查版本、运行库安装结果和服务启动结果。`vc-redist-install.log` 和 `vc-redist-repair.log` 记录各次运行库操作详情。

## 手动启动、停止和重启终端服务

右键点击「终端注册状态」托盘图标，选择「启动服务」「停止服务」或「重启服务」，并通过 Windows 管理员授权。

没有状态托盘时，在安装目录的 `终端注册` 文件夹里右键单击对应的 BAT，选择「以管理员身份运行」。

- `start-terminal-service.bat` 启动终端服务，成功时显示 `RUNNING`。
- `stop-terminal-service.bat` 停止终端服务，成功时显示 `STOPPED`。
- `restart-terminal-service.bat` 重启终端服务，成功时显示 `RUNNING`。

脚本退出前会保留窗口，方便查看状态和错误。这些 BAT 也可以复制到旧版安装目录使用。

## 手动启动、停止和重启状态托盘

右键点击托盘图标，选择「重启托盘」或「退出托盘」。托盘未运行时，从开始菜单打开「终端注册状态」，或在安装目录的「终端注册」文件夹里双击对应的 BAT。脚本只操作当前登录会话中的状态托盘。

- `start-terminal-status.bat` 启动状态托盘，成功时显示 `RUNNING`。
- `stop-terminal-status.bat` 停止状态托盘，成功时显示 `STOPPED`。
- `restart-terminal-status.bat` 重启状态托盘，成功时显示 `RUNNING`。

## 终端服务诊断

在安装目录里右键单击 `diagnose-terminal-service.cmd`，选择「以管理员身份运行」。窗口会显示报告的位置。报告包含 Windows 服务状态、`DesktopTerminalService.exe` 的文件版本、VC++ DLL 版本、终端服务日志和近期崩溃事件。服务配置文件与状态文件只检查是否存在。

检查已装旧版的电脑时，可以把 `diagnose-terminal-service.cmd` 和 `diagnose-terminal-service.ps1` 一起拷贝到该电脑的安装目录，再运行诊断。
