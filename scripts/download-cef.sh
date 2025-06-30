#!/bin/bash
# CEF自动下载脚本 - 支持32位Windows 7 SP1和现代64位系统
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CEF_DIR="$PROJECT_ROOT/third_party/cef"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检测系统架构和平台
detect_platform() {
    OS="unknown"
    ARCH="unknown"
    
    # 检测操作系统
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macosx"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        OS="windows"
    else
        log_warning "未知操作系统: $OSTYPE，默认使用linux"
        OS="linux"
    fi
    
    # 检测架构
    MACHINE_TYPE=$(uname -m)
    case $MACHINE_TYPE in
        x86_64|amd64)
            ARCH="64"
            ;;
        i386|i686|x86)
            ARCH="32"
            ;;
        arm64|aarch64)
            if [[ "$OS" == "macosx" ]]; then
                ARCH="64"  # macOS ARM使用64位版本
            else
                log_error "不支持的ARM架构: $MACHINE_TYPE"
                exit 1
            fi
            ;;
        *)
            log_warning "未知架构: $MACHINE_TYPE，默认使用64位"
            ARCH="64"
            ;;
    esac
    
    # 构建平台字符串
    PLATFORM="${OS}${ARCH}"
    
    log_info "检测到平台: $OS ($MACHINE_TYPE) -> $PLATFORM"
}

# 全平台统一使用CEF 109版本
select_cef_version() {
    # 统一CEF版本
    CEF_VERSION="109.1.18+gf1c41e4+chromium-109.0.5414.120"
    
    case $PLATFORM in
        "windows32")
            CEF_PLATFORM="windows32"
            log_info "选择CEF 109.1.18版本 - Windows 32位统一版本"
            ;;
        "windows64")
            CEF_PLATFORM="windows64"
            log_info "选择CEF 109.1.18版本 - Windows 64位统一版本"
            ;;
        "macosx64")
            CEF_PLATFORM="macosx64"
            log_info "选择CEF 109.1.18版本 - macOS 64位统一版本"
            ;;
        "linux64")
            CEF_PLATFORM="linux64"
            log_info "选择CEF 109.1.18版本 - Linux 64位统一版本"
            ;;
        *)
            log_error "不支持的平台: $PLATFORM"
            exit 1
            ;;
    esac
    
    CEF_BINARY_NAME="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}"
    CEF_ARCHIVE_NAME="${CEF_BINARY_NAME}.tar.bz2"
}

# 构建下载URL
build_download_url() {
    # 所有CEF版本统一使用Spotify CDN下载，需要URL编码特殊字符
    CEF_ARCHIVE_NAME_ENCODED="$CEF_ARCHIVE_NAME"
    
    # 对CEF版本号中的+号进行URL编码（+变为%2B）
    CEF_ARCHIVE_NAME_ENCODED="${CEF_ARCHIVE_NAME_ENCODED//+/%2B}"
    
    # 统一使用Spotify CDN
    DOWNLOAD_URL="https://cef-builds.spotifycdn.com/${CEF_ARCHIVE_NAME_ENCODED}"
    
    log_info "下载URL: $DOWNLOAD_URL"
}

# 检查CEF是否已存在
check_existing_cef() {
    CEF_INSTALL_DIR="$CEF_DIR/$CEF_BINARY_NAME"
    
    # 检查多种可能的目录结构
    local CEF_INCLUDE_CHECK1="$CEF_INSTALL_DIR/include/cef_version.h"
    local CEF_INCLUDE_CHECK2="$CEF_DIR/include/cef_version.h"
    local CEF_INCLUDE_CHECK3="$CEF_INSTALL_DIR/cef_version.h"
    
    if [[ -d "$CEF_INSTALL_DIR" ]] && ([[ -f "$CEF_INCLUDE_CHECK1" ]] || [[ -f "$CEF_INCLUDE_CHECK2" ]] || [[ -f "$CEF_INCLUDE_CHECK3" ]]); then
        log_success "CEF已存在: $CEF_INSTALL_DIR"
        log_info "如需重新下载，请删除该目录后重新运行脚本"
        return 0
    fi
    
    return 1
}

# 下载CEF
download_cef() {
    log_info "开始下载CEF $CEF_VERSION..."
    
    # 创建临时目录
    TEMP_DIR=$(mktemp -d)
    trap "rm -rf $TEMP_DIR" EXIT
    
    # 下载文件
    log_info "下载 $CEF_ARCHIVE_NAME..."
    
    # 尝试使用curl，如果失败则使用wget
    if command -v curl >/dev/null 2>&1; then
        if ! curl -L "$DOWNLOAD_URL" -o "$TEMP_DIR/$CEF_ARCHIVE_NAME"; then
            log_error "curl下载失败"
            return 1
        fi
    elif command -v wget >/dev/null 2>&1; then
        if ! wget "$DOWNLOAD_URL" -O "$TEMP_DIR/$CEF_ARCHIVE_NAME"; then
            log_error "wget下载失败"
            return 1
        fi
    else
        log_error "未找到curl或wget，无法下载文件"
        return 1
    fi
    
    log_success "下载完成"
    
    # 验证下载的文件
    if [[ ! -f "$TEMP_DIR/$CEF_ARCHIVE_NAME" ]]; then
        log_error "下载的文件不存在"
        return 1
    fi
    
    FILE_SIZE=$(stat -f%z "$TEMP_DIR/$CEF_ARCHIVE_NAME" 2>/dev/null || stat -c%s "$TEMP_DIR/$CEF_ARCHIVE_NAME" 2>/dev/null || echo "0")
    if [[ "$FILE_SIZE" -lt 10000000 ]]; then  # 小于10MB可能是错误页面
        log_error "下载的文件太小，可能下载失败"
        return 1
    fi
    
    log_info "文件大小: $(echo $FILE_SIZE | awk '{printf "%.1fMB", $1/1024/1024}')"
    
    # 解压文件
    log_info "解压CEF二进制包..."
    mkdir -p "$CEF_DIR"
    
    if ! tar -xjf "$TEMP_DIR/$CEF_ARCHIVE_NAME" -C "$CEF_DIR"; then
        log_error "解压失败"
        return 1
    fi
    
    log_success "CEF解压完成: $CEF_DIR/$CEF_BINARY_NAME"
    
    # 验证解压结果 - 多路径验证策略，支持不同的CEF目录结构
    log_info "验证CEF解压结果..."
    
    local CEF_FOUND=false
    local CEF_VALID_PATH=""
    
    # 可能的CEF头文件路径（按优先级排序）
    local CEF_CHECK_PATHS=(
        "$CEF_DIR/$CEF_BINARY_NAME/include/cef_version.h"
        "$CEF_DIR/include/cef_version.h"
        "$CEF_DIR/$CEF_BINARY_NAME/cef_version.h"
    )
    
    # 遍历所有可能的路径
    for CHECK_PATH in "${CEF_CHECK_PATHS[@]}"; do
        if [[ "$CEF_FOUND" == "false" ]] && [[ -f "$CHECK_PATH" ]]; then
            CEF_FOUND=true
            CEF_VALID_PATH="$CHECK_PATH"
            log_success "CEF头文件验证成功: $CHECK_PATH"
            break
        fi
    done
    
    # 如果标准路径都找不到，进行深度搜索
    if [[ "$CEF_FOUND" == "false" ]]; then
        log_info "标准路径未找到，进行深度搜索..."
        
        if [[ -d "$CEF_DIR" ]]; then
            log_info "搜索CEF目录中的所有cef_version.h文件..."
            
            # 使用find进行递归搜索
            local FOUND_FILES=($(find "$CEF_DIR" -name "cef_version.h" -type f 2>/dev/null))
            
            if [[ ${#FOUND_FILES[@]} -gt 0 ]]; then
                CEF_FOUND=true
                CEF_VALID_PATH="${FOUND_FILES[0]}"
                log_success "在深度搜索中找到CEF头文件: $CEF_VALID_PATH"
            else
                # 如果还是找不到，显示详细的目录结构用于诊断
                log_warning "显示CEF目录结构用于诊断:"
                
                # 查找相关CEF头文件
                local CEF_HEADERS=($(find "$CEF_DIR" -name "*.h" -type f 2>/dev/null | grep -i cef | head -5))
                if [[ ${#CEF_HEADERS[@]} -gt 0 ]]; then
                    log_info "找到相关CEF头文件，但不是cef_version.h:"
                    for header in "${CEF_HEADERS[@]}"; do
                        log_info "  - $header"
                    done
                fi
                
                # 查找include目录
                local INCLUDE_DIRS=($(find "$CEF_DIR" -name "include" -type d 2>/dev/null))
                if [[ ${#INCLUDE_DIRS[@]} -gt 0 ]]; then
                    log_info "找到include目录:"
                    for dir in "${INCLUDE_DIRS[@]}"; do
                        log_info "  - $dir"
                    done
                else
                    log_warning "未找到include目录"
                fi
            fi
        else
            log_error "CEF目录不存在: $CEF_DIR"
        fi
    fi
    
    # 最终验证结果
    if [[ "$CEF_FOUND" == "true" ]]; then
        log_success "CEF验证成功！有效路径: $CEF_VALID_PATH"
        return 0
    else
        log_error "CEF验证失败：未找到必需的cef_version.h文件"
        log_error "可能的原因："
        log_error "1. CEF解压不完整"
        log_error "2. CEF版本的目录结构与预期不符"
        log_error "3. 下载的CEF包损坏"
        return 1
    fi
    
    return 0
}

# 显示使用帮助
show_help() {
    echo "CEF下载脚本"
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -f, --force     强制重新下载，即使CEF已存在"
    echo "  -h, --help      显示此帮助信息"
    echo "  --platform=PLATFORM  指定平台 (windows32, windows64, macosx64, linux64)"
    echo "  --version=VERSION     指定CEF版本"
    echo ""
    echo "示例:"
    echo "  $0                    # 自动检测平台并下载对应版本"
    echo "  $0 --force           # 强制重新下载"
    echo "  $0 --platform=windows32  # 下载Windows 32位版本"
}

# 主函数
main() {
    local FORCE_DOWNLOAD=false
    local CUSTOM_PLATFORM=""
    local CUSTOM_VERSION=""
    
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -f|--force)
                FORCE_DOWNLOAD=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            --platform=*)
                CUSTOM_PLATFORM="${1#*=}"
                shift
                ;;
            --version=*)
                CUSTOM_VERSION="${1#*=}"
                shift
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    log_info "开始CEF下载流程..."
    
    # 检测或使用自定义平台
    if [[ -n "$CUSTOM_PLATFORM" ]]; then
        PLATFORM="$CUSTOM_PLATFORM"
        log_info "使用指定平台: $PLATFORM"
    else
        detect_platform
    fi
    
    # 选择CEF版本
    if [[ -n "$CUSTOM_VERSION" ]]; then
        CEF_VERSION="$CUSTOM_VERSION"
        log_info "使用指定版本: $CEF_VERSION"
    else
        select_cef_version
    fi
    
    # 构建下载URL
    build_download_url
    
    # 检查是否已存在
    if [[ "$FORCE_DOWNLOAD" == "false" ]] && check_existing_cef; then
        exit 0
    fi
    
    # 下载CEF
    if download_cef; then
        log_success "CEF下载和安装成功！"
        log_info "CEF路径: $CEF_DIR/$CEF_BINARY_NAME"
        log_info "现在可以运行CMake构建项目了"
    else
        log_error "CEF下载失败"
        exit 1
    fi
}

# 运行主函数
main "$@"