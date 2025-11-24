# 配置文件优先级修复验证步骤

## 修改内容

修改了 [src/config/config_manager.cpp](src/config/config_manager.cpp#L28-L44) 中的配置文件加载顺序：

**修改前**：嵌入资源优先
```
1. :/resources/config.json (嵌入资源)
2. exe目录/config.json
3. 系统配置目录
```

**修改后**：外部文件优先
```
1. exe目录/config.json (最高优先级)
2. 系统配置目录
3. :/resources/config.json (后备)
```

## Windows环境验证步骤

### 1. 重新编译程序

```batch
cd build\Release_x86
cmake --build . --config Release
```

或使用完整构建脚本：
```batch
build-windows.bat
```

### 2. 确认配置文件存在

检查 `build\Release_x86\bin\config.json` 内容：
```json
{
  "url": "http://test.sdzdf.com:8011/stu?Client='ExamTerminal'",
  ...
}
```

### 3. 运行程序

```batch
cd build\Release_x86\bin
DesktopTerminal-CEF.exe
```

### 4. 检查日志输出

查看日志文件中的关键行：

**期望看到**：
```
配置文件加载成功: C:/path/to/build/Release_x86/bin/config.json
目标URL: http://test.sdzdf.com:8011/stu?Client='ExamTerminal'
```

**不应该看到**：
```
配置文件加载成功: :/resources/config.json
目标URL: http://stu.sdzdf.com?Client='ExamTerminal'
```

### 5. 验证浏览器加载的URL

日志中应该显示：
```
开始加载页面: http://test.sdzdf.com:8011/stu/...
主框架地址变更: http://test.sdzdf.com:8011/stu/...
```

## 验证成功标准

- ✅ 日志显示加载的配置文件路径是外部文件（不是 `:/resources/config.json`）
- ✅ 日志显示目标URL是 `http://test.sdzdf.com:8011`
- ✅ 浏览器实际访问的是 `http://test.sdzdf.com:8011`

## 客户部署验证

在客户机器上，只需修改 `config.json` 文件即可：

```batch
# 客户机器上的配置文件位置
C:\Program Files\山东智多分\智多分机考桌面端\config.json
```

修改后重启程序，无需重新编译。

## 故障排查

如果仍然加载旧URL：

1. 确认已重新编译（检查exe文件修改时间）
2. 确认config.json文件在正确位置
3. 检查config.json文件编码是UTF-8
4. 查看完整日志，确认配置文件搜索路径
