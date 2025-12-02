#!/bin/bash
# CEF Release准备脚本 - 下载并上传到GitHub Release

set -e

REPO_OWNER="zhao"  # 修改为你的GitHub用户名
REPO_NAME="DesktopTerminal-2025-CEF"
CEF_VERSION="75.1.14"
RELEASE_TAG="cef-${CEF_VERSION}"

echo "=== CEF GitHub Release 准备工具 ==="
echo "CEF版本: ${CEF_VERSION}"
echo "Release标签: ${RELEASE_TAG}"
echo ""

# 检查gh CLI
if ! command -v gh &> /dev/null; then
    echo "错误: 需要安装GitHub CLI (gh)"
    echo "安装: brew install gh"
    exit 1
fi

# 创建临时目录
TEMP_DIR=$(mktemp -d)
echo "临时目录: ${TEMP_DIR}"
cd "${TEMP_DIR}"

# 下载CEF包
echo ""
echo "=== 下载CEF二进制包 ==="

platforms=("windows32:178" "windows64:228" "linux64:412" "macosx64:156")

for platform_info in "${platforms[@]}"; do
    IFS=':' read -r platform size <<< "$platform_info"

    echo ""
    echo "下载 ${platform} (预计 ${size}MB)..."

    url="https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_${platform}.tar.bz2"
    output="cef-${CEF_VERSION}-${platform}.tar.bz2"

    if curl -L -f -o "${output}" "${url}"; then
        actual_size=$(du -m "${output}" | cut -f1)
        echo "✓ 下载成功: ${output} (${actual_size}MB)"
    else
        echo "✗ 下载失败: ${platform}"
        exit 1
    fi
done

echo ""
echo "=== 所有文件下载完成 ==="
ls -lh cef-*.tar.bz2

# 创建Release
echo ""
echo "=== 创建GitHub Release ==="

gh release create "${RELEASE_TAG}" \
    --repo "${REPO_OWNER}/${REPO_NAME}" \
    --title "CEF ${CEF_VERSION} Binary Distribution" \
    --notes "预编译的CEF二进制包，用于加速CI/CD构建

**包含平台：**
- Windows 32位 (178MB)
- Windows 64位 (228MB)
- Linux 64位 (412MB)
- macOS 64位 (156MB)

**CEF版本：** 75.1.14+gc81164e+chromium-75.0.3770.100
**Chromium版本：** 75.0.3770.100
**上传日期：** $(date -u +%Y-%m-%d)

**使用方法：**
\`\`\`bash
gh release download ${RELEASE_TAG} --pattern 'cef-*.tar.bz2'
\`\`\`" \
    cef-${CEF_VERSION}-*.tar.bz2

echo ""
echo "✓ Release创建成功！"
echo "查看: https://github.com/${REPO_OWNER}/${REPO_NAME}/releases/tag/${RELEASE_TAG}"

# 清理
cd -
rm -rf "${TEMP_DIR}"
echo ""
echo "=== 完成 ==="
