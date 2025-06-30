# DesktopTerminal-2025-CEF 项目架构说明与打包流程指南

## 项目概述

DesktopTerminal-2025-CEF 是一个基于 Qt5 和 CEF (Chromium Embedded Framework) 的安全桌面终端应用程序，专为教育考试环境设计。该应用提供严格的安全控制，包括全屏锁定、键盘拦截、URL访问控制等功能。

### 核心特性
- 🎯 **安全桌面环境**: 全屏锁定、键盘拦截、URL访问控制
- 🌐 **CEF浏览器引擎**: 替代WebEngine，提供更好的兼容性和控制
- 🔧 **跨平台支持**: Windows、macOS、Linux
- 🖥️ **32位兼容**: 特别优化支持Windows 7 SP1 32位系统
- 🔒 **严格安全控制**: 防止窗口切换、右键菜单禁用、开发者工具阻止

## 技术架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                       │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   Main UI   │  │ Config Mgr  │  │   Security Ctrl     │  │
│  │ (Qt5 GUI)   │  │             │  │ (Keyboard Filter)   │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                     Core Layer                              │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │CEF Manager  │  │SecureBrowser│  │   System Detector   │  │
│  │             │  │             │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                   Framework Layer                           │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │    Qt5      │  │ CEF 109/109  │  │      QHotkey        │  │
│  │ Framework   │  │             │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    System Layer                             │
├─────────────────────────────────────────────────────────────┤
│  Windows 7 SP1 / Windows 10+ / macOS 10.12+ / Linux       │
└─────────────────────────────────────────────────────────────┘
```

### 技术栈

| 组件分类 | 技术选型 | 版本要求 | 说明 |
|---------|---------|---------|------|
| **GUI框架** | Qt5 | 5.15+ | 跨平台GUI开发框架 |
| **Web引擎** | CEF | 75/109 | Chromium嵌入式框架 |
| **全局热键** | QHotkey | Latest | 跨平台全局热键支持 |
| **构建系统** | CMake | 3.20+ | 跨平台构建管理 |
| **编译器** | MSVC 2019/GCC/Clang | - | 平台特定编译器 |

## 系统架构

### 跨平台支持策略

#### 平台架构支持矩阵

| 平台 | 32位支持 | 64位支持 | CEF版本推荐 | 特殊说明 |
|------|---------|---------|------------|----------|
| **Windows 7 SP1** | ✅ | ✅ | CEF 109 | 现代化支持优化 |
| **Windows 10+** | ✅ | ✅ | CEF 109 | 现代化功能支持 |
| **macOS 10.12+** | ❌ | ✅ | CEF 109 | 仅64位支持 |
| **Linux** | ❌ | ✅ | CEF 109 | 现代发行版 |

#### CEF版本管理策略

```cpp
// CMakeLists.txt 中的版本切换逻辑
option(USE_CEF109 "使用CEF 109进行构建（迁移模式）" OFF)

if(USE_CEF109)
    set(CEF_VERSION "109.1.18+g97a8d9e+chromium-109.0.5414.120")
    add_definitions(-DCEF_VERSION_109=1)
else()
    set(CEF_VERSION "109.1.18+gf1c41e4+chromium-109.0.5414.120")
    // 默认生产模式
endif()
```

### 安全架构设计

```
┌─────────────────────────────────────────────┐
│              Security Layer                  │
├─────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────────┐   │
│  │ Keyboard    │  │   Window Manager    │   │
│  │ Filter      │  │   (Focus Control)   │   │
│  │             │  │                     │   │
│  │ • Block     │  │ • Force Fullscreen  │   │
│  │   Alt+Tab   │  │ • Prevent Minimize  │   │
│  │ • Block     │  │ • Always On Top     │   │
│  │   Ctrl+Alt  │  │                     │   │
│  │   +Del      │  │                     │   │
│  └─────────────┘  └─────────────────────┘   │
├─────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────────┐   │
│  │ URL Access  │  │   Privilege Mgr     │   │
│  │ Control     │  │   (Windows Only)    │   │
│  │             │  │                     │   │
│  │ • Whitelist │  │ • Admin Rights      │   │
│  │ • Blacklist │  │ • UAC Handling      │   │
│  │ • Validate  │  │                     │   │
│  └─────────────┘  └─────────────────────┘   │
└─────────────────────────────────────────────┘
```

## 模块架构详解

### 1. Core 模块

#### Application 类 (`src/core/application.h`)

```cpp
class Application : public QApplication {
    Q_OBJECT

public:
    enum class ArchType { Unknown, X86_32, X86_64, ARM64 };
    enum class PlatformType { Unknown, Windows, MacOS, Linux };
    enum class CompatibilityLevel { Unknown, LegacySystem, ModernSystem, OptimalSystem };

    // 系统检测和兼容性管理
    static ArchType getSystemArchitecture();
    static PlatformType getSystemPlatform();
    static CompatibilityLevel getCompatibilityLevel();
    static bool isWindows7SP1();
    static bool checkSystemRequirements();
};
```

**职责**:
- 应用程序生命周期管理
- 系统兼容性检测和优化
- 跨平台初始化逻辑
- Windows 7 SP1 特殊优化

#### CEFManager 类 (`src/core/cef_manager.h`)

```cpp
class CEFManager : public QObject {
    Q_OBJECT

public:
    enum class ProcessMode { SingleProcess, MultiProcess };
    enum class MemoryProfile { Minimal, Balanced, Performance };

    bool initialize();
    void shutdown();
    int createBrowser(void* parentWidget, const QString& url);
    
    static ProcessMode selectOptimalProcessMode();
    static MemoryProfile selectOptimalMemoryProfile();
};
```

**职责**:
- CEF 初始化和配置管理
- 进程模式选择（单进程适合32位系统）
- 内存优化配置
- 跨平台CEF设置

#### SecureBrowser 类 (`src/core/secure_browser.h`)

**职责**:
- 主浏览器窗口管理
- 全屏强制和焦点控制
- 与CEF的交互接口
- 安全事件处理

### 2. CEF 集成模块

#### CEF App 实现 (`src/cef/cef_app_impl.h`)

```cpp
class CefAppImpl : public CefApp,
                   public CefBrowserProcessHandler,
                   public CefRenderProcessHandler {
public:
    // CefApp interface
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
    
    // 进程间通信
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override;
};
```

#### CEF Client 实现 (`src/cef/cef_client_impl.h`)

```cpp
class CefClientImpl : public CefClient,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefDisplayHandler,
                      public CefRequestHandler {
public:
    // 浏览器生命周期
    bool OnBeforePopup(...) override;    // 阻止弹窗
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    
    // 请求拦截
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(...) override;
};
```

### 3. 安全控制模块

#### SecurityController (`src/security/security_controller.h`)

**功能**:
- 统一安全策略管理
- 安全事件日志记录
- 违规行为检测和响应

#### KeyboardFilter (`src/security/keyboard_filter.h`)

**功能**:
- 全局键盘事件拦截
- 危险快捷键阻止 (Alt+Tab, Ctrl+Alt+Del等)
- 安全退出热键处理 (F10 + 密码)

```cpp
class KeyboardFilter : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray &eventType, 
                          void *message, 
                          long *result) override;
private:
    void blockDangerousKeys(MSG* msg);
    bool handleSecureExit(int keyCode);
};
```

### 4. 配置管理模块

#### ConfigManager (`src/config/config_manager.h`)

**配置层次结构**:
```
1. 命令行参数 (最高优先级)
2. 环境变量
3. 用户配置文件 (~/.config/DesktopTerminal-CEF/config.json)
4. 系统配置文件 (/etc/zdf-exam-desktop/config.json)
5. 默认配置 (最低优先级)
```

**配置示例**:
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

### 5. 日志系统模块

#### Logger (`src/logging/logger.h`)

**日志分类**:
- `app.log` - 应用程序主日志
- `security.log` - 安全事件专用日志
- `keyboard.log` - 键盘事件日志
- `cef_debug.log` - CEF调试日志
- `startup.log` - 启动过程日志
- `exit.log` - 退出事件日志

## 构建系统详解

### CMake 配置架构

#### 主配置文件 (`CMakeLists.txt`)

```cmake
# 核心配置
cmake_minimum_required(VERSION 3.20)
project(DesktopTerminal-CEF)
set(CMAKE_CXX_STANDARD 17)

# CEF版本切换
option(USE_CEF109 "使用CEF 109进行构建（迁移模式）" OFF)

if(USE_CEF109)
    set(CEF_VERSION "109.1.18+g97a8d9e+chromium-109.0.5414.120")
    add_definitions(-DCEF_VERSION_109=1)
else()
    set(CEF_VERSION "109.1.18+gf1c41e4+chromium-109.0.5414.120")
endif()

# 平台检测
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CEF_PLATFORM "windows64")
    set(CMAKE_ARCH "x64")
else()
    set(CEF_PLATFORM "windows32") 
    set(CMAKE_ARCH "Win32")
endif()
```

#### CMake 模块

| 模块文件 | 功能 | 说明 |
|---------|------|------|
| `cmake/FindCEF.cmake` | CEF库发现和配置 | 支持75/109版本切换 |
| `cmake/DeployCEF.cmake` | CEF文件部署 | 资源文件复制和验证 |
| `cmake/ForceCEFDeploy.cmake` | 强制CEF部署 | 解决DeployCEF失效问题 |

### 依赖管理

#### 第三方库管理

```
third_party/
├── QHotkey/                 # 全局热键库 (Git Submodule)
│   ├── CMakeLists.txt
│   ├── QHotkey/
│   └── HotkeyTest/         # 测试程序
├── cef/                    # CEF 109 库目录
│   └── cef_binary_75.../   # 自动下载的CEF包
└── cef109/                 # CEF 109 库目录 (可选)
    └── cef_binary_109.../  # 自动下载的CEF包
```

#### CEF 自动下载机制

```bash
# Windows 平台
scripts/download-cef.bat
  ├── 检测系统架构 (32位/64位)
  ├── 选择合适的CEF版本
  ├── 从Spotify CDN下载
  ├── 解压并验证
  └── 设置正确的文件权限

# Linux/macOS 平台  
scripts/download-cef.sh
  ├── 平台检测 (Linux64/macOS64)
  ├── CEF版本选择
  ├── 下载和解压
  └── chmod 设置可执行权限
```

## 打包流程完整指南

### 1. 环境准备

#### Windows 环境
```cmd
# 必需软件
- Visual Studio 2019/2022 (MSVC编译器)
- Qt 5.15.2 或更高版本
- CMake 3.20+
- Git (用于克隆QHotkey子模块)
- NSIS 3.0+ (用于创建安装包)

# 设置环境变量
set QT_DIR=C:\Qt\5.15.2\msvc2019_64
set PATH=%QT_DIR%\bin;%PATH%
```

#### Linux 环境
```bash
# Ubuntu/Debian 系统
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    qt5-default \
    qtbase5-dev \
    libqt5webkit5-dev \
    git \
    curl \
    libnss3-dev \
    libxss1 \
    libgconf-2-4

# CentOS/RHEL 系统
sudo yum groupinstall "Development Tools"
sudo yum install cmake qt5-qtbase-devel qt5-qtwebkit-devel
```

#### macOS 环境
```bash
# 安装 Xcode Command Line Tools
xcode-select --install

# 安装 Qt (使用 Homebrew)
brew install qt@5
brew install cmake

# 设置环境变量
export QT_DIR=/usr/local/opt/qt@5
export PATH=$QT_DIR/bin:$PATH
```

### 2. 构建流程

#### 2.1 克隆项目和子模块

```bash
# 克隆主项目
git clone https://github.com/2589195970/DesktopTerminal-2025-CEF.git
cd DesktopTerminal-2025-CEF

# 初始化并更新子模块
git submodule init
git submodule update --recursive

# 或者一步完成
git clone --recursive https://github.com/2589195970/DesktopTerminal-2025-CEF.git
```

#### 2.2 CEF 依赖下载

```bash
# Windows (32位系统 - Windows 7 SP1 兼容)
scripts\download-cef.bat

# Windows (64位系统 - 现代版本)
scripts\download-cef.bat --arch x64

# Linux/macOS
./scripts/download-cef.sh

# 手动指定CEF版本
./scripts/download-cef.sh --version 109  # 使用CEF 109
```

#### 2.3 构建配置

##### Windows 构建

```cmd
# 32位 Release 构建 (Windows 7 SP1)
scripts\build.bat -t Release -a x86 --cef-version 75

# 64位 Release 构建 (现代Windows)
scripts\build.bat -t Release -a x64 --cef-version 109

# Debug 构建
scripts\build.bat -t Debug -a x64 --verbose
```

##### Linux 构建

```bash
# 标准 Release 构建
./scripts/build.sh -t Release -a x64

# 使用特定 Qt 版本
./scripts/build.sh -t Release -a x64 --qt-dir /opt/qt5.15.2

# 详细输出模式
./scripts/build.sh -t Debug -a x64 --verbose
```

##### macOS 构建

```bash
# 标准构建
./scripts/build.sh -t Release -a x64

# 创建应用包
./scripts/build.sh -t Release -a x64 --create-app-bundle

# 代码签名构建
./scripts/build.sh -t Release -a x64 --sign-app
```

#### 2.4 手动 CMake 构建

```bash
# 创建构建目录
mkdir build && cd build

# 配置 - Windows 32位 (CEF 109)
cmake .. -G "Visual Studio 16 2019" -A Win32 \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_CEF109=OFF

# 配置 - Windows 64位 (CEF 109)  
cmake .. -G "Visual Studio 16 2019" -A x64 \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_CEF109=ON

# 配置 - Linux/macOS
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DUSE_CEF109=ON \
    -DCMAKE_PREFIX_PATH=$QT_DIR

# 编译
cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%

# 安装 (可选)
cmake --install . --prefix ../install
```

### 3. 打包和分发

#### 3.1 Windows 打包

##### NSIS 安装包生成

```cmd
# 自动打包 (推荐)
scripts\package.bat -t Release -a x64 -f nsis --version 1.0.0

# 手动 NSIS 打包
cd build/Release
makensis /DARCH=x64 /DCEF_VERSION=109 ..\..\installer.nsi
```

**生成的安装包**:
- `Output/智多分机考桌面端-setup-cef109.exe` (64位)
- `Output/智多分机考桌面端-setup-x86-cef75.exe` (32位)

##### 便携版 ZIP 包

```cmd
# 创建便携版
scripts\package.bat -t Release -a x64 -f zip

# 生成文件
- DesktopTerminal-CEF-v1.0.0-windows-x64.zip
- DesktopTerminal-CEF-v1.0.0-windows-x86.zip
```

#### 3.2 macOS 打包

##### 应用包 (.app) 创建

```bash
# 自动创建 .app 包
./scripts/package.sh -t Release -a x64 -f app

# 手动使用 macdeployqt
$QT_DIR/bin/macdeployqt build/Release/DesktopTerminal-CEF.app \
    -verbose=2 -always-overwrite
```

##### DMG 镜像创建

```bash
# 创建 DMG 分发镜像
./scripts/package.sh -t Release -a x64 -f dmg

# 代码签名的 DMG
./scripts/package.sh -t Release -a x64 -f dmg --sign \
    --identity "Developer ID Application: Company Name"
```

#### 3.3 Linux 打包

##### AppImage 包创建

```bash
# 创建 AppImage
./scripts/package.sh -t Release -a x64 -f appimage

# 生成文件: DesktopTerminal-CEF-x86_64.AppImage
```

##### DEB/RPM 包创建

```bash
# Debian/Ubuntu DEB 包
./scripts/package.sh -t Release -a x64 -f deb

# CentOS/RHEL RPM 包  
./scripts/package.sh -t Release -a x64 -f rpm
```

### 4. 自动化构建 (CI/CD)

#### GitHub Actions 配置

项目包含完整的 GitHub Actions 配置 (`.github/workflows/build.yml`):

```yaml
name: 多平台构建和发布

on:
  push:
    branches: [ master, develop ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ master ]

jobs:
  # Windows 构建 (32位 + 64位)
  build-windows:
    runs-on: windows-latest
    strategy:
      matrix:
        arch: [x86, x64]
        cef_version: [75, 109]
        
  # macOS 构建
  build-macos:
    runs-on: macos-latest
    
  # Linux 构建
  build-linux:
    runs-on: ubuntu-latest
```

#### 构建产物

**每次构建自动生成**:
- Windows: `.exe` 安装包 + `.zip` 便携版
- macOS: `.dmg` 镜像文件
- Linux: `.AppImage` + `.deb` + `.rpm` 包

**发布版本时额外生成**:
- 完整的源码包
- 各平台 SHA256 校验文件
- 发布说明和更新日志

### 5. 部署配置

#### 5.1 配置文件部署

**配置文件层次**:
```
1. ./config.json                    (应用目录 - 最高优先级)
2. ~/.config/DesktopTerminal-CEF/    (用户配置目录)
3. /etc/zdf-exam-desktop/           (系统配置目录 - 仅Linux)
4. 内置默认配置                     (代码中定义 - 最低优先级)
```

**生产环境配置示例**:
```json
{
    "url": "https://exam.school.edu.cn/",
    "exitPassword": "SecurePassword123!",
    "appName": "考试专用浏览器",
    "strictSecurityMode": true,
    "keyboardFilterEnabled": true,
    "contextMenuEnabled": false,
    "cefLogLevel": "ERROR",
    "cefSingleProcessMode": false,
    "autoArchDetection": true,
    "allowedDomains": [
        "exam.school.edu.cn",
        "cdn.school.edu.cn"
    ],
    "blockedKeyboardShortcuts": [
        "Alt+Tab", "Ctrl+Alt+Del", "Win+R",
        "Ctrl+Shift+Esc", "Alt+F4"
    ]
}
```

#### 5.2 权限和安全设置

##### Windows 部署

```cmd
# 管理员权限安装 (推荐)
智多分机考桌面端-setup-cef109.exe /S /AdminInstall

# 注册表设置 (可选 - 增强安全性)
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\System" /v DisableCMD /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableTaskMgr /t REG_DWORD /d 1 /f
```

##### Linux 部署

```bash
# 系统级安装
sudo dpkg -i DesktopTerminal-CEF_1.0.0_amd64.deb

# 设置 chrome-sandbox 权限 (必需)
sudo chown root:root /opt/DesktopTerminal-CEF/chrome-sandbox
sudo chmod 4755 /opt/DesktopTerminal-CEF/chrome-sandbox

# 创建系统服务 (可选)
sudo systemctl enable desktopterminal-cef
sudo systemctl start desktopterminal-cef
```

##### macOS 部署

```bash
# 拖拽安装到 Applications
cp -R DesktopTerminal-CEF.app /Applications/

# 首次运行可能需要安全授权
sudo spctl --add /Applications/DesktopTerminal-CEF.app
sudo xattr -d com.apple.quarantine /Applications/DesktopTerminal-CEF.app
```

### 6. 故障排除

#### 6.1 常见构建问题

**CEF 下载失败**:
```bash
# 问题: CEF 下载 404 错误
# 解决: 检查版本号和 URL 编码
curl -I "https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_windows64.tar.bz2"

# 问题: CEF 解压失败
# 解决: 手动下载并解压到 third_party/cef/
```

**Qt 相关错误**:
```bash
# 问题: Qt5 找不到
# 解决: 设置 CMAKE_PREFIX_PATH
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt5

# 问题: MOC 文件生成失败
# 解决: 确保 CMAKE_AUTOMOC=ON
```

**链接错误 (LNK2019)**:
```bash
# 问题: CEF Wrapper 库链接失败
# 解决: 确保 CEF Wrapper 正确编译
# 检查 CMakeLists.txt 中的 cef_dll_wrapper 目标
```

#### 6.2 运行时问题

**Windows 7 运行时错误**:
```cmd
REM 问题: api-ms-win-crt-runtime-l1-1-0.dll 缺失
REM 解决: 安装 Visual C++ Redistributable
REM 下载: https://aka.ms/highdpimfc2013x86enu (32位)
REM 下载: https://www.microsoft.com/download/details.aspx?id=53840 (64位)
```

**Linux 权限问题**:
```bash
# 问题: chrome-sandbox 权限不足
sudo chmod 4755 /path/to/chrome-sandbox
sudo chown root:root /path/to/chrome-sandbox

# 问题: 显示相关错误
export DISPLAY=:0
xhost +local:
```

**macOS 安全策略**:
```bash
# 问题: 应用被阻止运行
sudo spctl --master-disable  # 临时禁用 Gatekeeper
# 或者添加例外
sudo spctl --add /Applications/DesktopTerminal-CEF.app
```

## 版本发布流程

### 1. 版本号管理

**语义化版本控制 (SemVer)**:
- `MAJOR.MINOR.PATCH` 格式
- `1.0.0` - 初始发布版本  
- `1.1.0` - 新功能添加
- `1.0.1` - 错误修复

### 2. 发布检查清单

**代码质量检查**:
- [ ] 所有平台构建成功
- [ ] 单元测试通过
- [ ] 安全功能验证
- [ ] 性能基准测试
- [ ] 内存泄漏检查

**文档更新**:
- [ ] 更新 `README.md`
- [ ] 更新 `CHANGELOG.md`
- [ ] 更新配置文档
- [ ] 更新部署指南

**打包验证**:
- [ ] Windows 32位/64位安装包
- [ ] macOS DMG 镜像
- [ ] Linux 各发行版包
- [ ] 便携版 ZIP 包

### 3. 发布步骤

```bash
# 1. 创建发布分支
git checkout -b release/v1.1.0

# 2. 更新版本信息
# 编辑 CMakeLists.txt 中的版本号
# 编辑 installer.nsi 中的版本号

# 3. 创建发布标签
git tag -a v1.1.0 -m "Release version 1.1.0"
git push origin v1.1.0

# 4. GitHub Actions 自动构建
# 等待 CI 完成所有平台构建

# 5. 创建 GitHub Release
# 上传构建产物和发布说明
```

## 总结

DesktopTerminal-2025-CEF 项目采用了模块化的架构设计，通过 Qt5 和 CEF 的结合提供了强大的跨平台支持。项目特别关注了Windows 7 SP1 32位系统的兼容性，同时提供了完整的自动化构建和部署流程。

**关键优势**:
- 🎯 **安全第一**: 严格的安全控制确保考试环境的完整性
- 🔧 **跨平台**: 支持主流操作系统和架构
- 📦 **自动化**: 完整的 CI/CD 流程和打包方案
- 🔄 **版本管理**: 灵活的 CEF 版本切换机制
- 📚 **文档完善**: 详细的架构说明和部署指南

该架构设计确保了项目的可维护性、可扩展性和部署的便利性，为教育考试环境提供了可靠的技术解决方案。