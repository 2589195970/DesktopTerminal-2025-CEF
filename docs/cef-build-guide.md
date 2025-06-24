# DesktopTerminal-CEF 构建指南

## 概述

DesktopTerminal-CEF 是基于 Chromium Embedded Framework (CEF) 构建的安全考试浏览器，提供更好的网页兼容性和性能。

## 与Qt版本的区别

| 特性 | Qt WebEngine版本 | CEF版本 |
|------|------------------|---------|
| 浏览器内核 | Qt集成的Chromium | 原生CEF |
| 文件大小 | ~50MB | ~100-150MB |
| 网页兼容性 | 良好 | 优秀 |
| 性能 | 良好 | 更好 |
| Windows 7支持 | 有限 | 原生支持 |
| 自定义能力 | 中等 | 强大 |

## 从macOS触发Windows构建

### 快速开始

```bash
# 一键触发构建
./build-windows-from-macos.sh
```

### 高级选项

```bash
# 使用高级触发脚本
./scripts/trigger-windows-build.sh -t Release -r v1.0.0
```

### 监控构建

```bash
# 查看构建状态
gh run list --workflow=build.yml

# 实时监控
gh run watch
```

## 构建产物说明

### Windows构建产物

1. **便携版**: `DesktopTerminal-CEF-windows-{arch}.zip`
   - 解压即用
   - 包含所有CEF依赖
   - 适合临时使用

2. **安装包**: `DesktopTerminal-CEF-v{version}-{arch}-cef{cef_ver}-setup.exe`
   - NSIS制作的Windows安装包
   - 自动配置系统
   - 推荐分发方式

### 支持的架构

- **x86 (32位)**: 支持Windows 7 SP1+，使用CEF 75
- **x64 (64位)**: 支持Windows 10+，使用CEF 118

## CEF版本说明

### CEF 75 (32位Windows)
- 专为Windows 7 SP1兼容性设计
- 基于Chromium 75
- 文件较小，兼容性最好

### CEF 118 (64位Windows)
- 现代Chromium特性
- 更好的性能和安全性
- 支持最新Web标准

## 构建流程详解

### 1. 环境准备
- Windows Server 2019 (GitHub Actions)
- Qt 5.15.2
- MSVC 2019编译器
- NSIS安装包生成器

### 2. CEF下载
- 自动缓存机制
- 版本特定下载
- 完整性验证

### 3. 编译构建
- CMake配置
- 多架构并行构建
- 依赖自动解析

### 4. 安装包生成
- NSIS脚本自动化
- 版本信息嵌入
- 数字签名准备

## 配置文件特性

### CEF专用配置

```json
{
  "appName": "智多分机考桌面端-CEF",
  "cefSettings": {
    "enableGPU": true,
    "enableWebSecurity": true,
    "enableJavaScript": true,
    "debugPort": 0,
    "multiThreadedMessageLoop": true
  }
}
```

### 配置选项说明

- `enableGPU`: 启用GPU加速
- `enableWebSecurity`: Web安全策略
- `enableJavaScript`: JavaScript支持
- `debugPort`: 调试端口（0=禁用）
- `multiThreadedMessageLoop`: 多线程消息循环

## 故障排除

### 构建失败

1. **CEF下载失败**
   ```bash
   # 检查网络连接
   # 清理CEF缓存
   ```

2. **编译错误**
   ```bash
   # 检查Qt版本
   # 验证MSVC环境
   ```

3. **安装包生成失败**
   ```bash
   # 检查NSIS安装
   # 验证文件路径
   ```

### 运行时问题

1. **程序无法启动**
   - 检查Visual C++ Redistributable
   - 验证CEF文件完整性

2. **网页显示异常**
   - 检查CEF资源文件
   - 验证GPU驱动

3. **快捷键失效**
   - 确认QHotkey库加载
   - 检查系统权限

## 开发建议

### 本地开发

1. **环境搭建**
   ```bash
   # 安装Qt 5.15.2
   # 配置MSVC环境
   # 下载CEF SDK
   ```

2. **代码修改**
   ```bash
   # 修改源代码
   # 本地测试
   # 提交到GitHub
   ```

3. **云端构建**
   ```bash
   # 触发GitHub Actions
   # 下载测试版本
   # 验证功能
   ```

### 发布流程

1. **版本准备**
   ```bash
   # 更新版本号
   # 更新文档
   # 创建Git标签
   ```

2. **自动构建**
   ```bash
   git tag v1.0.0
   git push --tags
   ```

3. **质量检查**
   ```bash
   # 下载所有平台版本
   # 功能测试
   # 安全扫描
   ```

## 性能优化

### 构建优化

- **缓存利用**: CEF版本缓存
- **并行构建**: 多架构同时进行
- **增量编译**: 仅构建变更部分

### 运行时优化

- **内存管理**: CEF进程模型
- **GPU加速**: 硬件渲染优化
- **网络优化**: 连接池管理

## 安全考虑

### 构建安全

- **依赖验证**: CEF文件校验
- **代码扫描**: 安全漏洞检测
- **权限控制**: 最小权限原则

### 运行时安全

- **沙箱隔离**: CEF进程隔离
- **网络过滤**: URL白名单
- **文件保护**: 配置文件加密

---

**相关文档**:
- [GitHub Actions使用指南](../README.md#github-actions)
- [配置文件参考](../resources/config.json)
- [故障排除FAQ](troubleshooting.md)