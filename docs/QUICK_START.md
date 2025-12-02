# CEF GitHub Release 快速开始

## 首次设置（仅需一次）

### 1. 运行准备脚本

```bash
cd scripts
./prepare-cef-release.sh
```

这将：
- 下载4个平台的CEF包（~974MB，需5-10分钟）
- 自动创建GitHub Release `cef-75.1.14`
- 上传所有文件到Release

### 2. 修改仓库名称

如果你的GitHub用户名不是`zhao`，需要修改：

**scripts/download-cef.bat** (第11行):
```batch
set "GITHUB_REPO=你的用户名/DesktopTerminal-2025-CEF"
```

**scripts/download-cef.sh** (第8行):
```bash
GITHUB_REPO="你的用户名/DesktopTerminal-2025-CEF"
```

**scripts/prepare-cef-release.sh** (第6-7行):
```bash
REPO_OWNER="你的用户名"
REPO_NAME="DesktopTerminal-2025-CEF"
```

## 验证设置

### 本地测试

```bash
# Windows
scripts\download-cef.bat /platform=windows64

# Linux/macOS
./scripts/download-cef.sh --platform=linux64
```

应该看到：
```
[INFO] 尝试从GitHub Release下载...
[SUCCESS] GitHub Release下载成功！
```

### CI测试

提交代码并推送：
```bash
git add .
git commit -m "feat: 使用GitHub Release加速CEF下载"
git push
```

观察GitHub Actions构建日志，应该看到：
- 首次构建：从GitHub Release下载（~2分钟）
- 后续构建：使用缓存（~10秒）

## 性能对比

| 场景 | 优化前 | 优化后 |
|------|--------|--------|
| 首次构建 | 5分钟 | **2分钟** |
| 缓存命中 | 5分钟（误判） | **10秒** |

## 故障排查

### 问题：GitHub Release下载失败

**现象**：
```
[WARNING] GitHub Release下载失败，回退到Spotify CDN...
```

**原因**：Release不存在或未公开

**解决**：
1. 检查Release是否创建：`gh release list`
2. 确认Release是public：`gh release view cef-75.1.14`
3. 手动创建：运行`prepare-cef-release.sh`

### 问题：脚本找不到gh命令

**解决**：
```bash
# macOS
brew install gh

# Linux
sudo apt install gh

# Windows
choco install gh
```

## 维护

### 更新CEF版本

1. 修改版本号（3个文件）
2. 重新运行`prepare-cef-release.sh`
3. 更新脚本中的Release标签

### 删除旧Release

```bash
gh release delete cef-75.1.14 --yes
```
