# DesktopTerminal-2025-CEF

基于Qt5 + CEF（Chromium Embedded Framework）的安全桌面终端应用程序，专为考试环境设计。

## 项目特性

### 🎯 核心功能
- **安全桌面环境**: 全屏锁定、键盘拦截、URL访问控制
- **CEF浏览器引擎**: 替代WebEngine，提供更好的兼容性和控制
- **跨平台支持**: Windows、macOS、Linux
- **32位兼容**: 特别优化支持Windows 7 SP1 32位系统

### 🔒 安全特性
- 强制全屏模式，防止窗口切换
- 键盘组合键拦截（Alt+Tab、Ctrl+Alt+Del等）
- URL白名单/黑名单访问控制
- 安全退出机制（F10或反斜杠键+密码）
- 右键菜单禁用
- 开发者工具阻止

### 🏗️ 技术架构
- **Qt 5.15+**: 跨平台GUI框架
- **CEF 109**: 统一使用CEF 109版本，支持所有平台和架构
- **QHotkey**: 全局热键支持
- **CMake**: 跨平台构建系统

## 系统要求

### Windows
- **Windows 7 SP1** (32位/64位) 或更高版本
- **内存**: 至少512MB可用内存（32位系统），1GB（64位系统）  
- **磁盘**: 200MB可用空间
- **运行时**: Visual C++ Redistributable（详见Windows 7特殊说明）

#### Windows 7 特殊说明
- 专门优化支持Windows 7 SP1系统
- 使用CEF 109版本提供现代化支持
- 如遇到 `api-ms-win-crt-runtime-l1-1-0.dll` 错误，请参考：
  - [Windows 7运行时安装指南](docs/windows7-runtime-guide.md)
  - 需要安装特定版本的VC++ Redistributable

### macOS
- **macOS 10.12** (Sierra) 或更高版本
- **内存**: 至少1GB可用内存
- **磁盘**: 200MB可用空间

### Linux
- **Ubuntu 18.04** 或等效发行版
- **内存**: 至少1GB可用内存
- **磁盘**: 200MB可用空间

## 快速开始

### 1. 克隆项目
```bash
git clone https://github.com/2589195970/DesktopTerminal-2025-CEF.git
cd DesktopTerminal-2025-CEF
```

### 2. 构建项目

#### Linux/macOS
```bash
# 构建64位Release版本
./scripts/build.sh -t Release -a x64

# 构建32位Debug版本（支持Windows 7 SP1）
./scripts/build.sh -t Debug -a x86 --cef-version 75
```

#### Windows
```cmd
REM 构建64位Release版本
scripts\build.bat -t Release -a x64

REM 构建32位版本（Windows 7 SP1支持）
scripts\build.bat -t Debug -a x86 --cef-version 75
```

### 3. 运行程序
```bash
# Linux/macOS
./build/Release_x64/bin/DesktopTerminal-CEF

# Windows
build\Release_x64\bin\DesktopTerminal-CEF.exe
```

## 构建选项

### 架构选择
- `x86`: 32位版本，支持Windows 7 SP1
- `x64`: 64位版本，推荐用于现代系统

### CEF版本
- **CEF 75**: Windows 7 SP1 32位系统专用
- **CEF 118**: 现代系统推荐版本

### 构建类型
- `Debug`: 调试版本，包含详细日志
- `Release`: 发布版本，优化性能
- `MinSizeRel`: 最小体积版本
- `RelWithDebInfo`: 带调试信息的发布版本

## 配置说明

### 配置文件位置
1. `./config.json` (应用程序目录)
2. `~/.config/DesktopTerminal-CEF/config.json` (用户配置目录)
3. `/etc/zdf-exam-desktop/config.json` (系统配置目录，仅Linux)

### 配置选项
```json
{
    "url": "http://stu.sdzdf.com/",
    "exitPassword": "sdzdf@2025",
    "appName": "智多分机考桌面端",
    "strictSecurityMode": true,
    "keyboardFilterEnabled": true,
    "contextMenuEnabled": false,
    "cefLogLevel": "WARNING",
    "cefSingleProcessMode": false,
    "autoArchDetection": true,
    "forceWindows7CompatMode": false
}
```

## 开发指南

### 项目结构
```
DesktopTerminal-2025-CEF/
├── src/                     # 源代码
│   ├── core/               # 核心模块
│   ├── cef/                # CEF集成
│   ├── security/           # 安全控制
│   ├── config/             # 配置管理
│   └── logging/            # 日志系统
├── third_party/            # 第三方库
│   ├── cef/               # CEF库
│   └── QHotkey/           # QHotkey库
├── scripts/                # 构建脚本
├── cmake/                  # CMake模块
└── resources/              # 资源文件
```

### 添加新功能
1. 在相应模块目录创建.h/.cpp文件
2. 更新CMakeLists.txt中的源文件列表
3. 确保遵循现有的代码风格和安全原则

### 安全注意事项
- 任何新功能都必须符合安全考试环境要求
- 不得添加可能被用于绕过安全控制的功能
- 所有用户输入必须经过严格验证

## 部署打包

### 创建分发包
```bash
# 创建ZIP包
./scripts/package.sh -t Release -a x64 -f zip

# 创建Windows安装包
./scripts/package.sh -t Release -a x86 -f nsis --version 1.0.0

# 创建macOS DMG
./scripts/package.sh -t Release -a x64 -f dmg
```

### 部署检查清单
- [ ] CEF运行时文件已包含
- [ ] Qt库文件已包含
- [ ] 配置文件已创建
- [ ] 启动脚本已配置
- [ ] 权限设置正确（Linux chrome-sandbox）

## 故障排除

### 常见问题

#### 1. CEF初始化失败
- 检查CEF文件是否完整下载
- 确认系统架构匹配（32位/64位）
- 检查权限设置（Linux需要chrome-sandbox权限）

#### 2. Qt库缺失
- 确保Qt5已正确安装
- 检查LD_LIBRARY_PATH或PATH环境变量
- 验证Qt版本兼容性

#### 3. 全局热键无效
- 检查QHotkey库是否正确编译
- 确认系统权限允许全局热键注册
- 查看应用程序日志获取详细错误信息

### 日志文件
- `logs/app.log`: 应用程序主日志
- `logs/security.log`: 安全事件日志
- `logs/keyboard.log`: 键盘事件日志
- `logs/window.log`: 窗口管理日志

## 常见问题与故障排除

### Windows 7 运行时问题

**问题**: 程序启动时提示 `api-ms-win-crt-runtime-l1-1-0.dll` 缺失
```
无法启动此程序，因为计算机中丢失 api-ms-win-crt-runtime-l1-1-0.dll
```

**解决方案**:
1. **推荐方案**: 安装Windows 7兼容的VC++ Redistributable
   - 详见: [Windows 7运行时安装指南](docs/windows7-runtime-guide.md)
   - 按顺序安装: VC++ 2013 → VC++ 2015 Update 3

2. **快速方案**: 
   ```
   步骤1：安装Windows 7关键更新
   - 下载KB2999226: https://support.microsoft.com/zh-cn/help/2999226
   
   步骤2：安装兼容的VC++ Redistributable
   - 下载VC++ 2013: https://aka.ms/highdpimfc2013x86enu
   - 下载VC++ 2015: https://www.microsoft.com/zh-cn/download/details.aspx?id=53840
   
   步骤3：以管理员身份安装，重启系统
   ```

**问题**: Visual C++ 2015-2022 安装失败，错误代码 0x80240017

**解决方案**: 这是Windows 7的已知兼容性问题
- 不要使用VC++ 2015-2022版本
- 使用Windows 7兼容版本（见上述推荐方案）

### 其他常见问题

**程序无法启动或黑屏**:
- 检查GPU驱动兼容性
- 在程序目录创建 `disable-gpu.txt` 空文件强制软件渲染

**快捷键无效**:
- 确保以管理员身份运行
- 检查杀毒软件是否阻止QHotkey库

**网页显示异常**:
- 验证CEF资源文件完整性
- 检查网络连接和URL配置

## 技术支持

### 联系信息
- **网站**: http://www.sdzdf.com
- **邮箱**: support@sdzdf.com
- **技术文档**: [项目Wiki](https://github.com/2589195970/DesktopTerminal-2025-CEF/wiki)

### 问题报告
请在GitHub Issues中报告问题，并包含以下信息：
- 操作系统和版本
- 系统架构（32位/64位）
- 错误信息和日志文件
- 重现步骤

## 许可证

本项目采用专有许可证，仅供山东智多分信息科技有限公司内部使用。

## 更新日志

### v1.0.0 (2024-06-24)
- 初始版本发布
- 完整的CEF集成和安全控制功能
- 支持Windows 7 SP1 32位系统
- 跨平台构建和部署支持

---

**注意**: 本软件专为考试环境设计，包含严格的安全控制功能。请确保在适当的环境中使用，并遵循相关的法律法规。