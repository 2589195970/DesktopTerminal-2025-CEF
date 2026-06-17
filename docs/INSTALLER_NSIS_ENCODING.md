# NSIS 安装器与 config.json 编码问题

## 问题记录（2026-06-17）

### 现象

程序启动时报错：

```
配置文件加载失败: JSON解析失败
路径: C:/Program Files/山东智多分/智多分机考桌面端/resources/config.json
错误: invalid UTF8 string
位置: 112
```

### 根本原因

`installer.nsi` 曾用 NSIS `FileWrite` 在安装阶段**手写整份** `config.json`，并先用 `FileWriteByte` 写入 UTF-8 BOM（`EF BB BF`），再用 `FileWrite` 写入含中文的字段（如 `appName: "智多分机考桌面端-CEF"`）。

在 Windows 中文环境下，NSIS `FileWrite` 对非 ASCII 字符往往按**系统 ANSI/GBK**写入，而不是 UTF-8。结果是：

- 文件头：UTF-8 BOM
- ASCII 字段：单字节，与 UTF-8 兼容
- 中文字段：GBK 双字节

形成**混编码**文件。Qt `QJsonDocument::fromJson` 按 UTF-8 解析，在 `appName` 附近（约偏移 112）遇到非法 UTF-8 序列即失败。

### 错误做法（禁止）

```nsis
FileOpen $0 "$INSTDIR\resources\config.json" w
FileWriteByte $0 0xEF
FileWriteByte $0 0xBB
FileWriteByte $0 0xBF
FileWrite $0 '  "appName": "智多分机考桌面端-CEF",$\r$\n'
; ... 其他 FileWrite 行 ...
```

**禁止**在安装脚本里用 `FileWrite` / `FileWriteByte` 拼接含中文或需严格 UTF-8 的 JSON、XML、YAML 等文本。

### 正确做法

1. **模板落盘**：使用仓库内已验证的 UTF-8 文件（如 `resources/config.json`），或通过构建产物 `artifacts\...\resources\config.json` 复制，保证全文 UTF-8。
2. **字段补丁**：仅对用户可改字段（`url`、`exitPassword`、`apiBaseUrl` 等）调用 PowerShell 脚本更新：
   - 脚本：`scripts/patch-install-config.ps1`
   - 安装器将用户输入写入 `$PLUGINSDIR\dtcef-install-params.txt`（逐行纯文本，避免命令行转义问题），再调用补丁脚本。
   - 安装器中的调用见 `installer.nsi` 中「更新配置文件」段落。
3. **输出编码**：补丁脚本使用 `UTF8Encoding(false)` 写出，**无 BOM**（与仓库模板一致）；程序侧已支持跳过 BOM 读取。
4. **程序侧兜底**：`ConfigManager::loadConfig` 在 Windows 上若 UTF-8 解析失败，会尝试 GB18030 解码（兼容历史错误安装包生成的文件）。

### 修改安装器配置时的检查清单

- [ ] 是否新增/修改了 `FileWrite` 写入含中文或 JSON 的逻辑？若是，改为「UTF-8 模板 + 补丁脚本」。
- [ ] `resources/config.json` 是否保持 UTF-8（无 BOM 或有 BOM 均可，程序会处理）？
- [ ] 补丁脚本是否覆盖所有安装页可配置项？
- [ ] 本地或 CI 打安装包后，是否用十六进制编辑器或 `file` 命令确认 `config.json` 中文段为 UTF-8 三字节序列（非 GBK 双字节）？
- [ ] 安装后启动程序，确认不再出现 `invalid UTF8 string`。

### 已安装环境的临时修复

无需重装时，可将 `resources\config.json` 用编辑器以 **UTF-8（无 BOM）** 重新保存，或替换为仓库 `resources/config.json` 内容后再改 URL 和密码。

使用含 GB18030 回退的新版程序时，旧的 GBK 混编文件也可能被自动识别。

### 相关文件

| 文件 | 说明 |
|------|------|
| `installer.nsi` | 安装流程；配置更新逻辑 |
| `scripts/patch-install-config.ps1` | 安装时 UTF-8 安全更新 config.json |
| `resources/config.json` | UTF-8 配置模板 |
| `src/config/config_manager.cpp` | 加载配置；BOM 跳过与 GB18030 回退 |

### 类似风险场景

任何通过 NSIS `FileWrite` 生成的含非 ASCII 文本（日志模板、多语言字符串文件、带中文路径的配置等）都可能出现相同问题。统一原则：**二进制/模板文件用 `File` 复制，文本用 UTF-8 源文件 + 外部脚本补丁，勿在 NSIS 内拼接多语言文本。**

## 关联问题：Qt HTTPS 与 TLS initialization failed（2026-06-17）

### 现象

日志中出现 `获取前端Config.json失败: TLS initialization failed`，网络检测用 HTTP 可能成功，但桌面端认证 HTTPS 失败。

### 原因

`QNetworkAccessManager` 在 Windows 上需要 OpenSSL 运行时（libssl/libcrypto DLL）与主程序同目录；仅部署 Qt5Network.dll 不够。CEF 自带 SSL，与 Qt 网络栈无关。

### 修复

- CI：`scripts/deploy-openssl-windows.ps1` 复制 OpenSSL DLL 到产物。
- 安装包：`installer.nsi` 安装上述 DLL。
- 参数传递：用户输入写入 `dtcef-install-params.txt` 再调用补丁脚本，避免命令行转义导致 URL 未写入；失败时 MessageBox 提示。日志：`resources/install-config-patch.log`。
