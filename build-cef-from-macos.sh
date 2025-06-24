#!/bin/bash

# ===============================================
# 从macOS快速触发CEF版本Windows构建的脚本
# ===============================================

set -e

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}智多分机考桌面端-CEF版 - Windows构建触发器${NC}"
echo "=============================================="

# 检查GitHub CLI
if ! command -v gh &> /dev/null; then
    echo -e "${YELLOW}[INFO]${NC} GitHub CLI未安装，正在安装..."
    if command -v brew &> /dev/null; then
        brew install gh
    else
        echo -e "${RED}[ERROR]${NC} 请手动安装GitHub CLI: https://cli.github.com/"
        exit 1
    fi
fi

# 检查认证
if ! gh auth status &> /dev/null; then
    echo -e "${YELLOW}[INFO]${NC} 需要GitHub认证..."
    gh auth login
fi

# 检查是否在CEF项目目录
if [[ ! -f "CMakeLists.txt" ]] || [[ ! -d "src/cef" ]]; then
    echo -e "${RED}[ERROR]${NC} 请在DesktopTerminal-2025-CEF项目根目录下运行此脚本"
    exit 1
fi

echo -e "${BLUE}[INFO]${NC} 触发CEF版本Windows构建..."

# 直接触发工作流，使用默认设置
gh workflow run "build.yml" || {
    echo -e "${RED}[ERROR]${NC} 触发失败，可能的原因："
    echo "  1. 网络连接问题"
    echo "  2. 仓库权限不足"
    echo "  3. GitHub Actions未启用"
    echo ""
    echo "请检查："
    echo "  - 当前仓库: $(gh repo view --json name -q .name 2>/dev/null || echo '未知')"
    echo "  - 工作流文件: .github/workflows/build.yml"
    exit 1
}

if [[ $? -eq 0 ]]; then
    echo -e "${GREEN}[SUCCESS]${NC} GitHub Actions工作流已触发！"
    echo ""
    echo "构建特性："
    echo "  ✅ Windows 32位 (CEF 75 - Windows 7兼容)"
    echo "  ✅ Windows 64位 (CEF 118 - 现代系统)"
    echo "  ✅ 自动生成安装包"
    echo "  ✅ CEF依赖自动下载和缓存"
    echo ""
    echo "监控构建进度:"
    echo "  网页版: $(gh repo view --web --json url -q .url)/actions"
    echo "  命令行: gh run list --workflow=build.yml"
    echo ""
    echo "预期产物："
    echo "  - DesktopTerminal-CEF-v1.0.0-x64-cef118-setup.exe (64位安装包)"
    echo "  - DesktopTerminal-CEF-v1.0.0-x86-cef75-win7-setup.exe (32位Windows 7兼容)"
    echo "  - 便携版压缩包"
    echo ""
    echo -e "${YELLOW}提示: 构建完成后，可在Actions页面下载CEF版本的Windows安装包${NC}"
else
    echo -e "${RED}[ERROR]${NC} 触发失败，请检查网络连接和仓库权限"
    exit 1
fi