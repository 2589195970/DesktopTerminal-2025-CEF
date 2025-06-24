#!/bin/bash

# ===============================================
# 从macOS快速触发Windows构建的简化脚本
# ===============================================

set -e

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}智多分机考桌面端-CEF - Windows构建触发器${NC}"
echo "=========================================="

# 检查GitHub CLI
if ! command -v gh &> /dev/null; then
    echo -e "${YELLOW}[INFO]${NC} GitHub CLI未安装，正在安装..."
    if command -v brew &> /dev/null; then
        brew install gh
    else
        echo "请手动安装GitHub CLI: https://cli.github.com/"
        exit 1
    fi
fi

# 检查认证
if ! gh auth status &> /dev/null; then
    echo -e "${YELLOW}[INFO]${NC} 需要GitHub认证..."
    gh auth login
fi

echo -e "${BLUE}[INFO]${NC} 触发Windows构建..."

# 直接触发CEF构建工作流
gh workflow run "build.yml"

if [[ $? -eq 0 ]]; then
    echo -e "${GREEN}[SUCCESS]${NC} GitHub Actions工作流已触发！"
    echo ""
    echo "监控构建进度:"
    echo "  网页版: $(gh repo view --web --json url -q .url)/actions"
    echo "  命令行: gh run list --workflow=build.yml"
    echo ""
    echo "提示: 构建完成后，可在Actions页面下载Windows安装包"
else
    echo "触发失败，请检查网络连接和仓库权限"
    exit 1
fi