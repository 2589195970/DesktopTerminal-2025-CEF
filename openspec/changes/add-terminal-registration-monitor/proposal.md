# Proposal

## Why

机房电脑开机后由 Windows 服务自动注册。现场人员需要在本机确认电脑是否已注册、是否还在等绑定，以及异常发生在哪一步。

## What Changes

- 安装「终端注册状态」托盘工具，支持登录自启和开始菜单入口。
- 显示注册、绑定、心跳和 Windows 服务状态，提供对应的现场排查提示。
- 提供运行日志、时间和异常筛选、诊断摘要复制、UTF-8 文本导出与自动保留。
- 扩充现有管理员诊断，收集 Windows 服务启动事件和近期服务日志。

## Capabilities

### New Capabilities

- `terminal-registration-monitor` 提供本机注册状态与日志诊断。

## Impact

涉及 Windows 注册服务、Qt 托盘工具、NSIS 安装器及诊断脚本。Windows 服务继续在用户登录前向学校服务器注册。托盘只读与电脑注册相关的本地信息。
