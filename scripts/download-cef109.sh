#!/bin/bash
# ======================================================================
# CEF 109.1.18 下载脚本 (Linux/macOS)  
# 用于CEF 75->109迁移测试
# ======================================================================

set -e  # 遇到错误立即退出

echo "[INFO] CEF 109.1.18 下载脚本启动..."
echo "[INFO] 目标版本: 109.1.18+g97a8d9e+chromium-109.0.5414.120"
echo

# 设置CEF版本
CEF109_VERSION="109.1.18+g97a8d9e+chromium-109.0.5414.120"

# 检测系统平台
if [[ "$OSTYPE" == "darwin"* ]]; then
    CEF109_PLATFORM="macosx64"
    echo "[INFO] 检测到macOS系统"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    CEF109_PLATFORM="linux64"
    echo "[INFO] 检测到Linux系统"
else
    echo "[ERROR] 不支持的系统类型: $OSTYPE"
    exit 1
fi

# URL编码版本号（+号需要编码为%2B）
CEF109_VERSION_ENCODED=$(echo "$CEF109_VERSION" | sed 's/+/%2B/g')

# 构建下载文件名和URL
CEF109_FILENAME="cef_binary_${CEF109_VERSION}_${CEF109_PLATFORM}.tar.bz2"
CEF109_URL="https://cef-builds.spotifycdn.com/cef_binary_${CEF109_VERSION_ENCODED}_${CEF109_PLATFORM}.tar.bz2"

# 设置目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CEF109_DIR="$PROJECT_ROOT/third_party/cef109"
DOWNLOAD_DIR="$CEF109_DIR/downloads"
EXTRACT_DIR="$CEF109_DIR"

echo "[INFO] 项目根目录: $PROJECT_ROOT"
echo "[INFO] CEF109目标目录: $CEF109_DIR"
echo "[INFO] 下载URL: $CEF109_URL"
echo

# 创建目录
echo "[STEP] 创建CEF109目录结构..."
mkdir -p "$CEF109_DIR"
mkdir -p "$DOWNLOAD_DIR"

# 检查是否已经下载
DOWNLOAD_FILE="$DOWNLOAD_DIR/$CEF109_FILENAME"
if [[ -f "$DOWNLOAD_FILE" ]]; then
    echo "[INFO] CEF109文件已存在: $DOWNLOAD_FILE"
    echo "[INFO] 跳过下载，直接解压..."
else
    # 下载CEF109
    echo "[STEP] 正在下载CEF 109.1.18..."
    echo "[INFO] 这可能需要几分钟，CEF109文件约200-400MB"
    echo

    # 检查下载工具
    if command -v curl >/dev/null 2>&1; then
        echo "[INFO] 使用curl下载..."
        curl -L --progress-bar --fail \
             -H "User-Agent: CEF-Downloader/1.0" \
             -o "$DOWNLOAD_FILE" \
             "$CEF109_URL"
    elif command -v wget >/dev/null 2>&1; then
        echo "[INFO] 使用wget下载..."
        wget --progress=bar:force \
             --user-agent="CEF-Downloader/1.0" \
             -O "$DOWNLOAD_FILE" \
             "$CEF109_URL"
    else
        echo "[ERROR] 未找到curl或wget下载工具"
        echo "[INFO] 请安装curl或wget后重试"
        exit 1
    fi

    if [[ $? -ne 0 ]]; then
        echo "[ERROR] CEF109下载失败！"
        echo "[INFO] 可能的原因："
        echo "  - 网络连接问题"
        echo "  - CEF版本URL无效: $CEF109_URL"
        echo "  - 磁盘空间不足"
        rm -f "$DOWNLOAD_FILE"  # 清理不完整的下载文件
        exit 1
    fi

    echo "[SUCCESS] CEF109下载完成"
fi

# 检查下载文件
if [[ ! -f "$DOWNLOAD_FILE" ]]; then
    echo "[ERROR] 下载文件不存在: $DOWNLOAD_FILE"
    exit 1
fi

# 验证文件大小（应该 > 100MB）
FILE_SIZE=$(stat -f%z "$DOWNLOAD_FILE" 2>/dev/null || stat -c%s "$DOWNLOAD_FILE" 2>/dev/null || echo "0")
if [[ $FILE_SIZE -lt 104857600 ]]; then  # 100MB
    echo "[WARNING] 下载文件可能不完整 (大小: ${FILE_SIZE} bytes)"
    echo "[INFO] 建议删除文件并重新下载"
fi

echo "[STEP] 正在解压CEF109..."

# 切换到解压目录
cd "$EXTRACT_DIR"

# 解压tar.bz2文件
if command -v tar >/dev/null 2>&1; then
    echo "[INFO] 使用tar命令解压..."
    tar -xjf "$DOWNLOAD_FILE" --no-same-owner
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] tar解压失败"
        exit 1
    fi
else
    echo "[ERROR] 未找到tar命令"
    echo "[INFO] 请安装tar工具后重试"
    exit 1
fi

# 验证解压结果
echo "[STEP] 验证CEF109解压结果..."

# 查找解压后的CEF目录
CEF109_BINARY_DIR=""
for dir in "$EXTRACT_DIR"/cef_binary_*_"$CEF109_PLATFORM"; do
    if [[ -d "$dir" ]]; then
        CEF109_BINARY_DIR="$dir"
        break
    fi
done

if [[ -z "$CEF109_BINARY_DIR" ]]; then
    echo "[ERROR] 未找到解压后的CEF109目录"
    echo "[INFO] 解压目录内容:"
    ls -la "$EXTRACT_DIR"
    exit 1
fi

echo "[INFO] 找到CEF109目录: $CEF109_BINARY_DIR"

# 验证关键文件
CEF109_INCLUDE_DIR="$CEF109_BINARY_DIR/include"
CEF109_RELEASE_DIR="$CEF109_BINARY_DIR/Release"

if [[ ! -f "$CEF109_INCLUDE_DIR/cef_version.h" ]]; then
    echo "[ERROR] 关键头文件缺失: cef_version.h"
    echo "[INFO] 包含目录内容:"
    ls -la "$CEF109_INCLUDE_DIR" 2>/dev/null || echo "目录不存在"
    exit 1
fi

# 平台特定的库文件检查
if [[ "$CEF109_PLATFORM" == "macosx64" ]]; then
    CEF_FRAMEWORK="$CEF109_RELEASE_DIR/Chromium Embedded Framework.framework"
    if [[ ! -d "$CEF_FRAMEWORK" ]]; then
        echo "[WARNING] CEF Framework未找到: $CEF_FRAMEWORK"
    fi
else
    # Linux
    if [[ ! -f "$CEF109_RELEASE_DIR/libcef.so" ]]; then
        echo "[WARNING] libcef.so未找到，可能需要重新下载"
    fi
fi

# 检查wrapper源码
if [[ -d "$CEF109_BINARY_DIR/libcef_dll" ]]; then
    WRAPPER_FILES=$(find "$CEF109_BINARY_DIR/libcef_dll" -name "*.cc" | wc -l)
    echo "[INFO] 找到CEF Wrapper源码文件: $WRAPPER_FILES 个"
else
    echo "[WARNING] CEF Wrapper源码目录未找到"
fi

echo
echo "[SUCCESS] CEF 109.1.18 安装完成！"
echo
echo "[INFO] 安装摘要:"
echo "  - CEF版本: $CEF109_VERSION"
echo "  - 平台: $CEF109_PLATFORM"
echo "  - 安装路径: $CEF109_BINARY_DIR"
echo "  - 头文件: $CEF109_INCLUDE_DIR"
echo "  - 库文件: $CEF109_RELEASE_DIR"
echo "  - 下载文件: $DOWNLOAD_FILE"
echo
echo "[INFO] 使用CEF109测试构建:"
echo "  mkdir build-cef109 && cd build-cef109"
echo "  cmake .. -DUSE_CEF109=ON"
echo "  cmake --build . --config Release"
echo
echo "[INFO] 清理下载文件（可选）:"
echo "  rm '$DOWNLOAD_FILE'"
echo

echo "[INFO] CEF109下载脚本完成"