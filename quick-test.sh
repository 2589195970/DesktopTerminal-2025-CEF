#!/bin/bash
# macOS快速构建和打包验证脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🚀 开始快速验证..."

# 1. 下载CEF（如果不存在）
if [ ! -d "third_party/cef" ]; then
    echo "📦 下载CEF..."
    ./scripts/download-cef.sh
fi

# 2. 构建项目
echo "🔨 构建项目..."
./scripts/build.sh -t Release

# 3. 打包
echo "📦 打包应用..."
./scripts/package.sh

echo "✅ 验证完成！"
echo ""
echo "构建产物: build/Release_x64/bin/DesktopTerminal-CEF"
echo "打包目录: package/"
