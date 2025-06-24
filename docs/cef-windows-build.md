# CEF版本Windows构建指南

## 概述

DesktopTerminal-CEF版本使用Chromium Embedded Framework (CEF)作为Web引擎，相比Qt WebEngine版本具有更好的兼容性和更丰富的Web功能。本文档说明如何从macOS环境触发Windows自动构建。

## CEF版本特性

### 🔧 **技术架构**
- **Web引擎**: CEF (Chromium Embedded Framework)
- **UI框架**: Qt5 + CEF集成
- **构建系统**: CMake + GitHub Actions
- **安全控制**: QHotkey + 系统级键盘拦截

### 📋 **版本兼容性**
| 架构 | CEF版本 | 支持系统 | 特性 |
|------|---------|----------|------|
| x86 (32位) | CEF 75 | Windows 7 SP1+ | 老系统兼容 |
| x64 (64位) | CEF 118 | Windows 10+ | 现代Web功能 |

### 🆚 **与Qt WebEngine版本对比**

| 特性 | CEF版本 | Qt WebEngine版本 |
|------|---------|------------------|
| Windows 7支持 | ✅ 完整支持 | ❌ 有限支持 |
| Web标准支持 | ✅ 最新Chromium | ✅ 较新但滞后 |
| 文件大小 | 📦 较大(~150MB) | 📦 大(~200MB) |
| 更新频率 | 🔄 频繁 | 🔄 跟随Qt发布 |
| 自定义能力 | ⚙️ 高度可定制 | ⚙️ 中等 |

## 快速构建

### 方法1: 一键构建（推荐）

```bash
# 在CEF项目根目录执行
cd DesktopTerminal-2025-CEF
./build-cef-from-macos.sh
```

### 方法2: GitHub CLI直接触发

```bash
# 手动触发CEF构建工作流
gh workflow run "build.yml"
```

### 方法3: Git标签发布

```bash
# 创建发布版本
git tag v1.0.0-cef
git push origin v1.0.0-cef
```

## 构建流程详解

### 🏗 **自动化构建流程**

1. **环境准备**
   - Windows 2019 Server
   - Qt 5.15.2 安装
   - Visual Studio Build Tools
   - NSIS安装包生成器

2. **CEF下载与缓存**
   - 自动检测构建架构
   - 32位: 下载CEF 75 (Windows 7兼容)
   - 64位: 下载CEF 118 (现代功能)
   - GitHub Actions缓存加速

3. **编译步骤**
   - CMake配置CEF路径
   - 多核并行编译
   - 依赖自动链接

4. **安装包生成**
   - NSIS脚本适配CEF结构
   - 自动包含CEF运行时
   - 版本和架构标识

### 📦 **构建产物说明**

#### 安装包命名规则
```
DesktopTerminal-CEF-v{版本}-{架构}-cef{CEF版本}-{标识}-setup.exe
```

**示例**:
- `DesktopTerminal-CEF-v1.0.0-x64-cef118-setup.exe` (64位标准版)
- `DesktopTerminal-CEF-v1.0.0-x86-cef75-win7-setup.exe` (32位Windows 7版)

#### 文件结构
```
安装后目录结构:
├── DesktopTerminal-CEF.exe      # 主程序
├── libcef.dll                   # CEF核心库
├── chrome_elf.dll              # Chrome ELF库
├── *.pak                       # CEF资源包
├── locales/                    # 本地化文件
├── swiftshader/               # 软件渲染器
├── config.json                # 应用配置
└── resources/                 # 应用资源
```

## CEF特有配置

### 配置文件增强

CEF版本的`config.json`包含额外的CEF特定配置：

```json
{
  "url": "https://example.com",
  "exitPassword": "admin123",
  "appName": "智多分机考桌面端-CEF版",
  "cefVersion": "118",
  "windowSettings": {
    "fullscreen": true,
    "alwaysOnTop": true,
    "showTaskbar": false
  },
  "security": {
    "blockKeyboard": true,
    "blockContextMenu": true,
    "allowedKeys": ["F5", "Ctrl+R"],
    "blockDevTools": true
  },
  "cefSettings": {
    "enableGPU": true,
    "enableWebSecurity": true,
    "allowFileAccess": false,
    "enablePlugins": false
  }
}
```

### 系统要求

#### Windows 32位版本
- **系统**: Windows 7 SP1 或更高版本
- **架构**: x86 (32位)
- **内存**: 最少2GB RAM
- **CEF版本**: 75 (Chromium 75)
- **特点**: 最大兼容性

#### Windows 64位版本
- **系统**: Windows 10 或更高版本
- **架构**: x64 (64位)
- **内存**: 推荐4GB+ RAM
- **CEF版本**: 118 (Chromium 118)
- **特点**: 最新Web功能

## 监控和下载

### 构建状态监控

```bash
# 查看最近的CEF构建
gh run list --workflow=build.yml

# 监控特定构建
gh run watch [RUN_ID]

# 查看构建日志
gh run view [RUN_ID] --log
```

### 下载构建产物

```bash
# 下载安装包
gh run download [RUN_ID] --name "DesktopTerminal-CEF-installer-x64"

# 下载便携版
gh run download [RUN_ID] --name "DesktopTerminal-CEF-windows-x64"
```

### 产物分类

1. **安装包** (`DesktopTerminal-CEF-installer-*`)
   - Windows exe安装程序
   - 自动安装、配置、创建快捷方式
   - 包含卸载程序

2. **便携版** (`DesktopTerminal-CEF-windows-*`)
   - 免安装压缩包
   - 解压即用
   - 适合受限环境

## 故障排除

### 常见问题

#### 1. CEF下载失败
```bash
# 症状: "CEF download failed"
# 原因: 网络连接或缓存问题
# 解决: 清除GitHub Actions缓存，重新触发构建
```

#### 2. 构建时CEF库链接错误
```bash
# 症状: "libcef.lib not found"
# 原因: CEF路径配置错误
# 解决: 检查CMakeLists.txt中CEF路径设置
```

#### 3. 安装包生成失败
```bash
# 症状: "NSIS script error"
# 原因: installer.nsi路径错误
# 解决: 检查NSIS脚本中的文件路径配置
```

#### 4. Windows 7运行错误
```bash
# 症状: 程序无法在Windows 7启动
# 原因: 可能使用了64位版本或缺少运行时
# 解决: 确保使用32位CEF75版本并安装VC++运行时
```

### 调试技巧

#### 检查CEF集成
```cpp
// 在应用启动时输出CEF版本信息
#include "include/cef_version.h"
qDebug() << "CEF Version:" << CEF_VERSION;
```

#### 验证依赖文件
```bash
# 检查CEF依赖是否完整
dumpbin /dependents DesktopTerminal-CEF.exe
```

## 开发指南

### 本地开发环境

如果需要在本地Windows环境开发CEF版本：

```bash
# 1. 克隆项目
git clone [repository]
cd DesktopTerminal-2025-CEF

# 2. 下载CEF
scripts/download-cef.bat x64

# 3. 配置和构建
mkdir build && cd build
cmake .. -A x64
cmake --build . --config Release
```

### 自定义CEF配置

修改`src/cef/cef_app.cpp`以自定义CEF行为：

```cpp
void CefApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
    // 禁用Web安全（仅开发环境）
    command_line->AppendSwitch("--disable-web-security");
    
    // 启用硬件加速
    command_line->AppendSwitch("--enable-gpu");
    
    // 禁用扩展
    command_line->AppendSwitch("--disable-extensions");
}
```

## 版本发布流程

### 自动发布

```bash
# 1. 更新版本号（CMakeLists.txt, installer.nsi）
# 2. 创建发布标签
git tag v1.0.0-cef-release
git push origin v1.0.0-cef-release

# 3. GitHub Actions自动构建并创建Release
# 4. 安装包自动上传到Release页面
```

### 手动发布验证

发布前验证清单：
- [ ] 32位版本在Windows 7测试通过
- [ ] 64位版本在Windows 10/11测试通过
- [ ] 安装包可正常安装/卸载
- [ ] 应用程序功能完整
- [ ] 安全控制功能正常
- [ ] 配置文件读取正确
- [ ] CEF Web功能正常

## 最佳实践

### 🔒 **安全建议**
1. 使用官方CEF二进制版本
2. 定期更新CEF版本修复安全漏洞
3. 禁用不必要的Web功能
4. 验证所有外部依赖

### ⚡ **性能优化**
1. 启用CEF缓存机制
2. 合理配置GPU加速
3. 优化CEF进程数量
4. 使用合适的CEF版本

### 🛠 **维护提示**
1. 监控CEF版本更新
2. 定期测试兼容性
3. 备份稳定版本配置
4. 文档同步更新

---
*基于CEF的现代Web技术，为考试环境提供最佳的安全和兼容性*