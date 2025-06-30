# Windows 7 运行时安装指南

## 问题说明

在Windows 7系统上运行DesktopTerminal-CEF时，可能会遇到以下错误：
- `api-ms-win-crt-runtime-l1-1-0.dll` 缺失
- Visual C++ 2015-2022 Redistributable安装失败（错误代码0x80240017）

这是因为新版本的VC++ Redistributable对Windows 7的支持有限，需要使用专门的解决方案。

## 快速解决方案

### 方案1：安装Windows 7兼容的VC++ Redistributable（推荐）

**步骤1：卸载失败的安装包**
1. 打开"控制面板" → "程序和功能"
2. 找到并卸载任何失败的"Microsoft Visual C++ 2015-2022 Redistributable"
3. 重启计算机

**步骤2：按顺序安装兼容版本**

1. **安装VC++ 2013 Redistributable (x86)**
   - 下载地址：https://aka.ms/highdpimfc2013x86enu
   - 文件名：`vcredist_x86.exe`
   - 以管理员身份运行并安装

2. **安装VC++ 2015 Update 3 Redistributable (x86)**
   - 下载地址：https://www.microsoft.com/zh-cn/download/details.aspx?id=53840
   - 选择：`vc_redist.x86.exe` (适用于32位CEF版本)
   - 以管理员身份运行并安装

**步骤3：验证安装**
1. 重启计算机
2. 运行DesktopTerminal-CEF.exe
3. 如果仍有问题，继续使用方案2

### 方案2：安装关键Windows更新（推荐用于顽固问题）

如果方案1仍然失败，问题通常是缺少关键的Windows 7系统更新：

**重要：必须安装以下更新才能支持现代C++运行时**

1. **安装通用C运行时更新 (KB2999226)** - 关键步骤
   - 这是支持Visual C++ 2015+的核心更新
   - 下载地址：https://support.microsoft.com/zh-cn/help/2999226
   - 选择对应您系统的版本：
     - Windows 7 SP1 x86 (32位)
     - Windows 7 SP1 x64 (64位)

2. **安装平台更新 (KB2670838)**
   - 改善对现代应用程序的支持
   - 下载地址：https://www.microsoft.com/zh-cn/download/details.aspx?id=36805

**安装步骤：**
1. 确认您的Windows 7已安装SP1
2. 下载并安装KB2999226（通用C运行时）
3. 下载并安装KB2670838（平台更新）
4. 重启计算机
5. 重新安装VC++ 2015 Redistributable
6. 验证程序运行

### 方案3：手动DLL复制（紧急临时方案，不推荐）

**重要警告：此方案存在技术风险，仅用于紧急情况**

**为什么不推荐：**
- DLL版本不匹配可能导致应用崩溃 (错误代码0xc0000142)
- 与Windows 7系统组件冲突风险
- Microsoft官方不推荐手动复制UCRT文件
- 可能影响其他应用程序的稳定性

**如果必须使用此方案：**
```
所需文件（仅限Windows 10相同架构系统）：
api-ms-win-crt-runtime-l1-1-0.dll
api-ms-win-crt-heap-l1-1-0.dll  
api-ms-win-crt-string-l1-1-0.dll
api-ms-win-crt-stdio-l1-1-0.dll
api-ms-win-crt-math-l1-1-0.dll
ucrtbase.dll
vcruntime140.dll
msvcp140.dll
```

**复制步骤：**
1. 从Windows 10计算机的 `C:\Windows\System32\` 目录复制上述文件
2. 将这些文件复制到DesktopTerminal-CEF.exe所在目录
3. 运行程序验证
4. 如果出现错误0xc0000142，立即删除这些文件并使用方案2

### 方案3：升级Windows 7系统（长期解决方案）

**安装必要的Windows更新：**
1. **安装 Windows 7 SP1**
   - 确保系统已安装Service Pack 1

2. **安装平台更新 (KB2670838)**
   - 下载地址：https://www.microsoft.com/zh-cn/download/details.aspx?id=36805
   - 这个更新改善了对现代应用程序的支持

3. **安装通用C运行时更新 (KB2999226)**
   - 下载地址：https://support.microsoft.com/zh-cn/kb/2999226
   - 这是支持新版VC++ Redistributable的关键更新

## 系统要求确认

### 如何查看Windows系统版本

在开始解决问题前，请先确认您的系统版本：

**快速检查方法（推荐）：**
```
1. 按 Windows键 + R
2. 输入：winver
3. 按回车键
```
会显示类似这样的信息：
- Windows 7 Service Pack 1（内部版本 7601）
- Windows 7（内部版本 7600）- 需要升级到SP1

**其他查看方法：**
- **系统信息**：Windows键 + R → 输入 `msinfo32`
- **命令行**：Windows键 + R → 输入 `cmd` → 输入 `ver`
- **控制面板**：控制面板 → 系统和安全 → 系统

### DesktopTerminal-CEF系统要求

- **Windows 7 SP1** (32位或64位) - 必须是Service Pack 1
- 已安装所有重要Windows更新
- 至少1GB可用内存
- DirectX 9.0c或更高版本

**重要提醒：**
- 如果显示"Windows 7（内部版本 7600）"，请先升级到SP1
- 内部版本应该是 7601 或更高

## 故障排除

### 常见错误及解决方法

**错误1：KB2999226显示"此更新不适用于您的计算机"**
- 原因：系统是Windows 7 RTM版本（内部版本7600），需要先升级到SP1
- 症状：winver显示"版本 6.1 (内部版本 7600)"
- 解决：
  ```
  1. 下载Windows 7 SP1：https://www.microsoft.com/zh-cn/download/details.aspx?id=5842
  2. 32位系统选择：windows6.1-KB976932-X86.exe
  3. 64位系统选择：windows6.1-KB976932-X64.exe
  4. 安装后重启，确认版本变为7601
  5. 然后再安装KB2999226
  ```

**错误2：0x80240017 - 未指定的错误**
- 原因：Windows Update服务问题
- 解决：使用方案1的兼容版本安装

**错误3：程序启动黑屏**
- 原因：GPU驱动兼容性问题
- 解决：在程序目录下创建 `disable-gpu.txt` 空文件

**错误4：快捷键无效**
- 原因：权限不足
- 解决：右键程序图标选择"以管理员身份运行"

### 验证安装成功

运行以下检查确认安装成功：

1. **检查DLL文件**
   ```cmd
   dir "%SystemRoot%\System32\api-ms-win-crt*"
   ```
   应该显示多个api-ms-win-crt开头的文件

2. **检查注册表**
   ```cmd
   reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86"
   ```
   应该显示VC++ 2015的安装信息

3. **运行程序**
   - 双击DesktopTerminal-CEF.exe
   - 如果能看到程序界面，说明安装成功

## 技术说明

### 为什么Windows 7需要特殊处理？

1. **API兼容性**：Windows 7缺少某些现代API
2. **运行时依赖**：新版VC++ Redistributable使用了Windows 7不支持的特性
3. **安全更新**：需要特定的Windows更新来支持现代C++运行时

### CEF 75兼容性优势

项目选择CEF 75.1.14版本的原因：
- ✅ 原生支持Windows 7 SP1
- ✅ 较小的运行时依赖
- ✅ 经过广泛测试的稳定版本
- ✅ 与Windows 7的C++运行时兼容

## 联系支持

如果以上方案都无法解决问题，请提供以下信息：

1. Windows 7版本信息 (`winver`命令结果)
2. 已安装的VC++ Redistributable列表
3. 具体错误截图
4. 系统事件日志中的相关错误

---

**相关文档：**
- [CEF构建指南](cef-build-guide.md)
- [主要README](../README.md)
- [配置文件说明](../resources/config.json)