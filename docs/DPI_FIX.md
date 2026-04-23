# 网页变形问题修复说明

## 问题描述
在高DPI显示器上，加载的网页出现严重变形，表现为：
- 文字间距异常（如"在线 考试"之间有不正常的空格）
- 整体布局缩放不正确
- 页面元素位置错乱

## 根本原因
1. `CefEnableHighDPISupport()`在Windows上自动应用系统DPI缩放
2. CEF默认会根据系统DPI设置自动缩放网页内容
3. 没有强制设置设备缩放因子为1.0

## 修复方案

### 1. 禁用CEF高DPI支持 (src/main.cpp)
```cpp
#ifdef Q_OS_WIN
    CefMainArgs cefMainArgs(GetModuleHandle(nullptr));
    // 禁用CEF的高DPI支持以避免网页变形问题
    // CefEnableHighDPISupport();
#else
    CefMainArgs cefMainArgs(originalArgc, originalArgv);
#endif
```

### 2. 强制设备缩放因子为1.0 (src/cef/cef_app_impl.cpp)
在`CEFApp::applyCompatibilityFlags()`中添加：
```cpp
// 禁用DPI缩放以避免网页变形
command_line->AppendSwitchWithValue("--force-device-scale-factor", "1");
command_line->AppendSwitchWithValue("--high-dpi-support", "0");
command_line->AppendSwitch("--disable-gpu-driver-bug-workarounds");
```

### 3. 设置浏览器默认字体大小 (src/core/cef_manager.cpp)
在`CEFManager::createBrowser()`中添加：
```cpp
// 禁用图像加载缩放以避免网页变形
browserSettings.image_loading = STATE_ENABLED;

// 设置默认字体大小以确保正常显示
browserSettings.default_font_size = 16;
browserSettings.default_fixed_font_size = 13;
```

### 4. 命令行参数补充 (src/core/cef_manager.cpp)
在`CEFManager::buildCEFCommandLine()`中添加：
```cpp
// 禁用DPI缩放以避免网页变形
args << "--force-device-scale-factor=1";
args << "--high-dpi-support=0";
args << "--disable-gpu-driver-bug-workarounds";
```

## 验证方法
1. 重新编译项目
2. 在高DPI显示器上运行程序
3. 检查网页是否正常显示，文字间距是否正常
4. 验证页面布局是否符合预期

## 影响范围
- Windows平台：主要修复目标
- macOS平台：不受影响（未启用CefEnableHighDPISupport）
- Linux平台：不受影响（未启用CefEnableHighDPISupport）

## 注意事项
1. 此修复会禁用CEF的自动DPI缩放功能
2. 网页将以1:1的比例显示，不会根据系统DPI自动缩放
3. 如果需要支持高DPI显示，需要在网页层面使用CSS媒体查询处理

## 相关文件
- `src/main.cpp` - 禁用CefEnableHighDPISupport
- `src/cef/cef_app_impl.cpp` - 添加命令行参数
- `src/core/cef_manager.cpp` - 设置浏览器配置和命令行参数
