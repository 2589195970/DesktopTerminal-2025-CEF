# CEF 109 测试环境使用指南

## 概述

本文档说明如何使用CEF 109测试环境来验证从CEF 75到CEF 109的迁移工作。测试环境支持并行构建，不会影响现有的CEF 75生产构建。

## 快速开始

### 1. 下载CEF 109

**Windows:**
```cmd
scripts\download-cef109.bat
```

**Linux/macOS:**
```bash
scripts/download-cef109.sh
```

### 2. 构建CEF 109测试版本

```bash
# 创建独立构建目录
mkdir build-cef109
cd build-cef109

# 配置CEF 109构建
cmake .. -DUSE_CEF109=ON -DCMAKE_BUILD_TYPE=Release

# 构建项目
cmake --build . --config Release
```

### 3. 运行测试

```bash
# Windows
.\bin\Release\DesktopTerminal-CEF_CEF109.exe

# Linux/macOS  
./bin/DesktopTerminal-CEF_CEF109
```

## 详细说明

### 目录结构

```
project/
├── third_party/
│   ├── cef/                    # CEF 75 (生产版本)
│   └── cef109/                 # CEF 109 (测试版本)
│       ├── downloads/          # 下载的CEF包
│       └── cef_binary_*/       # 解压的CEF文件
├── scripts/
│   ├── download-cef109.bat     # Windows下载脚本
│   └── download-cef109.sh      # Linux/macOS下载脚本
├── cmake/
│   ├── FindCEF.cmake          # CEF 75查找模块
│   └── FindCEF109.cmake       # CEF 109查找模块
└── CMakeLists.cef109.txt       # CEF 109构建配置
```

### CMake 构建选项

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `USE_CEF109` | `OFF` | 启用CEF 109测试模式 |
| `CMAKE_BUILD_TYPE` | `Debug` | 构建类型 (Debug/Release) |

### 构建目标

- **标准构建**: `DesktopTerminal-CEF` (使用CEF 75)
- **测试构建**: `DesktopTerminal-CEF_CEF109` (使用CEF 109)

### 编译定义

CEF 109测试构建会自动添加以下编译定义：

```cpp
#define CEF_VERSION_109 1
#define CEF_TESTING_MODE 1
#define CEF109_USE_SANDBOX 0
#define CEF109_TESTING 1
```

在代码中可以使用这些定义来区分CEF版本：

```cpp
#ifdef CEF_VERSION_109
    // CEF 109特定代码
    // 例如：新的IResourceRequestHandler实现
#else
    // CEF 75兼容代码
#endif
```

## 迁移测试步骤

### 阶段1：基础兼容性测试

1. **构建验证**
   ```bash
   cmake .. -DUSE_CEF109=ON
   cmake --build . --config Release
   ```

2. **基础功能测试**
   - 应用程序启动
   - 网页加载
   - 基本安全控制

### 阶段2：API兼容性测试

1. **接口迁移测试**
   - IResourceHandler接口变更
   - RequestHandler架构拆分
   - OnBeforeResourceLoad迁移

2. **功能回归测试**
   - URL访问控制
   - 键盘事件过滤
   - 安全退出机制

### 阶段3：新功能开发测试

1. **URL检测功能**
   - 模式匹配算法
   - 自动退出流程
   - 配置系统集成

2. **性能优化测试**
   - 内存使用对比
   - 启动时间测试
   - 运行时性能分析

## 故障排除

### 常见问题

**Q: CEF 109下载失败**
```
[ERROR] CEF109下载失败！
```
**A:** 检查网络连接，或手动下载CEF 109.1.18包：
- URL: `https://cef-builds.spotifycdn.com/cef_binary_109.1.18%2Bg97a8d9e%2Bchromium-109.0.5414.120_windows64.tar.bz2`
- 解压到: `third_party/cef109/`

**Q: CMake找不到CEF109**
```
CEF109未找到，但测试模式已启用
```
**A:** 确保CEF109已正确下载和解压：
```bash
ls -la third_party/cef109/cef_binary_*/include/cef_version.h
```

**Q: 编译错误**
```
error: use of undeclared identifier 'IResourceRequestHandler'
```
**A:** 这是正常的，表明需要实现CEF 109的新接口。检查代码中的`#ifdef CEF_VERSION_109`条件编译。

### 调试技巧

1. **启用详细日志**
   ```bash
   cmake .. -DUSE_CEF109=ON -DCMAKE_BUILD_TYPE=Debug
   ```

2. **检查CEF版本**
   在应用程序中添加版本检查：
   ```cpp
   #ifdef CEF_VERSION_109
   std::cout << "Using CEF 109 for testing" << std::endl;
   #else
   std::cout << "Using CEF 75 (production)" << std::endl;
   #endif
   ```

3. **对比构建输出**
   ```bash
   # CEF 75构建
   mkdir build-cef75 && cd build-cef75
   cmake .. && cmake --build .
   
   # CEF 109构建  
   mkdir build-cef109 && cd build-cef109
   cmake .. -DUSE_CEF109=ON && cmake --build .
   ```

## 注意事项

### 兼容性

- **Windows 7**: CEF 109是最后支持Windows 7的版本
- **硬件加速**: 在旧系统上可能需要禁用
- **内存使用**: CEF 109可能比CEF 75占用更多内存

### 开发建议

1. **保持并行开发**
   - 生产代码继续使用CEF 75
   - 测试代码使用CEF 109
   - 使用条件编译管理差异

2. **渐进式迁移**
   - 优先迁移核心接口
   - 逐步添加新功能
   - 保持向后兼容性

3. **充分测试**
   - 在多个平台测试
   - 验证性能影响
   - 确保功能完整性

## 相关文档

- [CEF迁移计划](cef-migration-plan.md)
- [Windows 7运行时指南](windows7-runtime-guide.md)
- [构建说明](../README.md)

---

**更新日期**: 2025-06-30  
**版本**: 1.0  
**状态**: 测试环境就绪