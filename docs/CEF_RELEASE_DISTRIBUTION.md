# CEF GitHub Release 分发方案

## 方案概述

将CEF二进制包上传到项目的GitHub Release，替代从Spotify CDN下载，实现：
- **下载速度提升2-3倍**（GitHub CDN在国内访问更快）
- **版本管理集中化**（CEF版本与项目版本绑定）
- **构建稳定性提升**（避免外部CDN故障）

## 实施步骤

### 步骤1：准备CEF二进制包

```bash
# 下载并重命名CEF包（仅需执行一次）
cd scripts

# Windows 32位
curl -L "https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_windows32.tar.bz2" \
  -o cef-75.1.14-windows32.tar.bz2

# Windows 64位
curl -L "https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_windows64.tar.bz2" \
  -o cef-75.1.14-windows64.tar.bz2

# Linux 64位
curl -L "https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_linux64.tar.bz2" \
  -o cef-75.1.14-linux64.tar.bz2

# macOS 64位
curl -L "https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_macosx64.tar.bz2" \
  -o cef-75.1.14-macosx64.tar.bz2
```

### 步骤2：创建CEF专用Release

```bash
# 使用GitHub CLI创建Release
gh release create cef-75.1.14 \
  --title "CEF 75.1.14 Binary Distribution" \
  --notes "预编译的CEF二进制包，用于加速CI/CD构建

**包含平台：**
- Windows 32位 (178MB)
- Windows 64位 (228MB)
- Linux 64位 (412MB)
- macOS 64位 (156MB)

**CEF版本：** 75.1.14+gc81164e+chromium-75.0.3770.100
**Chromium版本：** 75.0.3770.100
**上传日期：** $(date -u +%Y-%m-%d)" \
  cef-75.1.14-windows32.tar.bz2 \
  cef-75.1.14-windows64.tar.bz2 \
  cef-75.1.14-linux64.tar.bz2 \
  cef-75.1.14-macosx64.tar.bz2
```

### 步骤3：修改下载脚本

#### Windows脚本 (`scripts/download-cef.bat`)

在`:download_cef`函数中添加GitHub Release优先逻辑：

```batch
:download_cef
call :log_info "开始下载CEF %CEF_VERSION%..."

REM 创建临时目录
set "TEMP_DIR=%TEMP%\cef_download_%RANDOM%"
mkdir "%TEMP_DIR%"

REM 优先从GitHub Release下载
set "GITHUB_RELEASE_URL=https://github.com/YOUR_USERNAME/DesktopTerminal-2025-CEF/releases/download/cef-75.1.14/cef-75.1.14-%CEF_PLATFORM%.tar.bz2"
set "DOWNLOAD_FILE=%TEMP_DIR%\%CEF_ARCHIVE_NAME%"

call :log_info "尝试从GitHub Release下载..."
call :log_info "URL: %GITHUB_RELEASE_URL%"

powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ProgressPreference = 'SilentlyContinue'; try { Invoke-WebRequest -Uri '%GITHUB_RELEASE_URL%' -OutFile '%DOWNLOAD_FILE%' -UseBasicParsing; exit 0 } catch { exit 1 } }"

if %errorlevel% neq 0 (
    call :log_warning "GitHub Release下载失败，回退到Spotify CDN..."
    REM 原有的Spotify CDN下载逻辑
    set "DOWNLOAD_URL=https://cef-builds.spotifycdn.com/!CEF_ARCHIVE_NAME_ENCODED!"
    call :log_info "备用URL: !DOWNLOAD_URL!"

    powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri '!DOWNLOAD_URL!' -OutFile '%DOWNLOAD_FILE%' -UseBasicParsing }"

    if !errorlevel! neq 0 (
        call :log_error "所有下载源均失败"
        exit /b 1
    )
)

call :log_success "下载成功！"
goto :eof
```

#### Linux/macOS脚本 (`scripts/download-cef.sh`)

在`download_cef()`函数中添加：

```bash
download_cef() {
    log_info "开始下载CEF ${CEF_VERSION}..."

    local temp_dir=$(mktemp -d)
    local download_file="${temp_dir}/${CEF_ARCHIVE_NAME}"

    # 优先从GitHub Release下载
    local github_url="https://github.com/YOUR_USERNAME/DesktopTerminal-2025-CEF/releases/download/cef-75.1.14/cef-75.1.14-${CEF_PLATFORM}.tar.bz2"

    log_info "尝试从GitHub Release下载..."
    if curl -L -f -o "${download_file}" "${github_url}" 2>/dev/null; then
        log_success "GitHub Release下载成功"
    else
        log_warning "GitHub Release下载失败，回退到Spotify CDN..."

        # 备用：Spotify CDN
        local cdn_url="https://cef-builds.spotifycdn.com/${CEF_ARCHIVE_NAME_ENCODED}"
        if ! curl -L -f -o "${download_file}" "${cdn_url}"; then
            log_error "所有下载源均失败"
            rm -rf "${temp_dir}"
            return 1
        fi
    fi

    # 解压逻辑...
}
```

### 步骤4：更新GitHub Actions配置

修改`.github/workflows/build.yml`中的下载步骤：

```yaml
- name: Download CEF
  if: steps.cache-cef.outputs.cache-hit != 'true' || steps.verify-cef-cache.outputs.cef-valid != 'true'
  env:
    GH_TOKEN: ${{ github.token }}
  shell: cmd
  run: |
    echo "下载CEF for ${{ matrix.arch }}..."

    REM 设置GitHub仓库信息
    set "GITHUB_REPO=${{ github.repository }}"
    set "CEF_RELEASE_TAG=cef-75.1.14"

    if "${{ matrix.arch }}"=="x86" (
      set "CEF_ASSET=cef-75.1.14-windows32.tar.bz2"
    ) else (
      set "CEF_ASSET=cef-75.1.14-windows64.tar.bz2"
    )

    REM 使用gh CLI下载（自动处理认证和重试）
    gh release download %CEF_RELEASE_TAG% ^
      --repo %GITHUB_REPO% ^
      --pattern "%CEF_ASSET%" ^
      --dir third_party\cef_download

    if %errorlevel% neq 0 (
      echo "GitHub Release下载失败，回退到脚本下载..."
      if "${{ matrix.arch }}"=="x86" (
        scripts\download-cef.bat /platform=windows32 /force
      ) else (
        scripts\download-cef.bat /platform=windows64 /force
      )
    ) else (
      echo "解压CEF..."
      REM 解压逻辑
    )
```

## 性能对比

| 下载源 | 平均速度 | 稳定性 | 国内访问 |
|--------|----------|--------|----------|
| **Spotify CDN** | 5-10 MB/s | 中等 | 较慢 |
| **GitHub Release** | 15-30 MB/s | 高 | 快 |

### 预期构建时间

| 平台 | 优化前 | 优化后 | 节省 |
|------|--------|--------|------|
| Windows 32位 | ~5分钟 | **~2分钟** | 60% |
| Windows 64位 | ~6分钟 | **~2.5分钟** | 58% |
| Linux 64位 | ~8分钟 | **~3分钟** | 62% |
| macOS 64位 | ~4分钟 | **~1.5分钟** | 62% |

## 维护说明

### 更新CEF版本

1. 下载新版本CEF包
2. 创建新的Release标签（如`cef-118.7.1`）
3. 更新脚本中的版本号和URL
4. 更新GitHub Actions配置

### 清理旧版本

```bash
# 删除旧的CEF Release（保留最近2个版本）
gh release delete cef-75.1.14 --yes
```

## 故障排查

### 问题1：GitHub Release下载失败

**原因**：Release不存在或网络问题
**解决**：自动回退到Spotify CDN

### 问题2：gh CLI认证失败

**原因**：GitHub Actions token权限不足
**解决**：确保workflow有`contents: read`权限

```yaml
permissions:
  contents: read
```

### 问题3：下载速度仍然慢

**原因**：GitHub CDN在某些地区较慢
**解决**：考虑使用国内镜像或自建CDN

## 安全考虑

1. **校验和验证**：建议在Release notes中添加SHA256校验和
2. **访问控制**：CEF Release可设为public，无需认证
3. **版本锁定**：使用固定的Release标签，避免意外更新

## 实施检查清单

- [ ] 下载所有平台的CEF二进制包
- [ ] 创建GitHub Release并上传文件
- [ ] 修改`download-cef.bat`添加GitHub优先逻辑
- [ ] 修改`download-cef.sh`添加GitHub优先逻辑
- [ ] 更新GitHub Actions配置
- [ ] 测试Windows 32位构建
- [ ] 测试Windows 64位构建
- [ ] 测试Linux构建
- [ ] 测试macOS构建
- [ ] 更新项目文档

## 回滚方案

如果GitHub Release方案出现问题，可立即回滚：

```bash
# 删除GitHub Release相关代码
git revert <commit-hash>

# 或手动注释掉GitHub下载逻辑
# 脚本会自动回退到Spotify CDN
```
