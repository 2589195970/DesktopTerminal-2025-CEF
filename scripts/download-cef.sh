#!/bin/bash
# CEF自动下载脚本 - 支持32位Windows 7 SP1和现代64位系统
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CEF_DIR="$PROJECT_ROOT/third_party/cef"
GITHUB_REPO="zhao/DesktopTerminal-2025-CEF"

CEF_VERSION_75="75.1.14+gc81164e+chromium-75.0.3770.100"
CEF_VERSION_118="118.7.1+g99817d2+chromium-118.0.5993.119"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

resolve_cef_version_string() {
    local version_input="$1"

    case "$version_input" in
        75|cef75|CEF75)
            CEF_VERSION="$CEF_VERSION_75"
            ;;
        118|118.*|cef118|CEF118)
            CEF_VERSION="$CEF_VERSION_118"
            ;;
        75.*)
            CEF_VERSION="$CEF_VERSION_75"
            ;;
        *)
            CEF_VERSION="$version_input"
            ;;
    esac

    log_info "使用CEF版本: $CEF_VERSION"
}

# 检测系统架构和平台
detect_platform() {
    OS="unknown"
    HOST_MACHINE="$(uname -m)"

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

    case "$HOST_MACHINE" in
        x86_64|amd64)
            ARCH="64"
            ;;
        i386|i686|x86)
            ARCH="32"
            ;;
        arm64|aarch64)
            if [[ "$OS" != "macosx" ]]; then
                log_error "不支持的ARM架构: $HOST_MACHINE"
                exit 1
            fi
            ARCH="arm64"
            ;;
        *)
            log_warning "未知架构: $HOST_MACHINE，默认使用64位"
            ARCH="64"
            ;;
    esac

    case "$OS" in
        windows)
            if [[ "$ARCH" == "32" ]]; then
                PLATFORM="windows32"
            else
                PLATFORM="windows64"
            fi
            ;;
        macosx)
            if [[ "$HOST_MACHINE" == "arm64" || "$HOST_MACHINE" == "aarch64" ]]; then
                PLATFORM="macosarm64"
            else
                PLATFORM="macosx64"
            fi
            ;;
        linux)
            PLATFORM="linux64"
            ;;
        *)
            log_error "不支持的操作系统: $OS"
            exit 1
            ;;
    esac

    log_info "检测到平台: $OS ($HOST_MACHINE) -> $PLATFORM"
}

select_cef_version() {
    case $PLATFORM in
        windows32)
            CEF_PLATFORM="windows32"
            CEF_VERSION="$CEF_VERSION_75"
            log_info "选择CEF 75 - Windows 32位"
            ;;
        windows64)
            CEF_PLATFORM="windows64"
            CEF_VERSION="$CEF_VERSION_75"
            log_info "选择CEF 75 - Windows 64位"
            ;;
        macosarm64)
            CEF_PLATFORM="macosarm64"
            CEF_VERSION="$CEF_VERSION_118"
            log_info "选择CEF 118 - macOS Apple Silicon (macosarm64)"
            ;;
        macosx64)
            CEF_PLATFORM="macosx64"
            CEF_VERSION="$CEF_VERSION_75"
            log_info "选择CEF 75 - macOS Intel (macosx64)"
            ;;
        linux64)
            CEF_PLATFORM="linux64"
            CEF_VERSION="$CEF_VERSION_75"
            log_info "选择CEF 75 - Linux 64位"
            ;;
        *)
            log_error "不支持的平台: $PLATFORM"
            exit 1
            ;;
    esac

    CEF_BINARY_NAME="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}"
    CEF_ARCHIVE_NAME="${CEF_BINARY_NAME}.tar.bz2"
}

apply_platform_version_defaults() {
    if [[ -z "$CEF_PLATFORM" ]]; then
        CEF_PLATFORM="$PLATFORM"
    fi

    if [[ -z "$CEF_VERSION" ]]; then
        select_cef_version
        return
    fi

    CEF_BINARY_NAME="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}"
    CEF_ARCHIVE_NAME="${CEF_BINARY_NAME}.tar.bz2"
}

build_download_url() {
    CEF_ARCHIVE_NAME_ENCODED="$CEF_ARCHIVE_NAME"
    CEF_ARCHIVE_NAME_ENCODED="${CEF_ARCHIVE_NAME_ENCODED//+/%2B}"
    DOWNLOAD_URL="https://cef-builds.spotifycdn.com/${CEF_ARCHIVE_NAME_ENCODED}"
    log_info "下载URL: $DOWNLOAD_URL"
}

check_existing_cef() {
    CEF_INSTALL_DIR="$CEF_DIR/$CEF_BINARY_NAME"

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

verify_downloaded_archive() {
    if [[ ! -f "$TEMP_DIR/$CEF_ARCHIVE_NAME" ]]; then
        log_error "下载的文件不存在"
        return 1
    fi

    FILE_SIZE=$(stat -f%z "$TEMP_DIR/$CEF_ARCHIVE_NAME" 2>/dev/null || stat -c%s "$TEMP_DIR/$CEF_ARCHIVE_NAME" 2>/dev/null || echo "0")
    if [[ "$FILE_SIZE" -lt 10000000 ]]; then
        log_error "下载的文件太小($FILE_SIZE bytes)，可能下载失败"
        return 1
    fi

    log_info "文件大小: $(echo "$FILE_SIZE" | awk '{printf "%.1fMB", $1/1024/1024}')"
    return 0
}

extract_and_verify_cef() {
    log_info "解压CEF二进制包..."
    mkdir -p "$CEF_DIR"

    if ! tar -xjf "$TEMP_DIR/$CEF_ARCHIVE_NAME" -C "$CEF_DIR"; then
        log_error "解压失败"
        return 1
    fi

    log_success "CEF解压完成: $CEF_DIR/$CEF_BINARY_NAME"

    log_info "验证CEF解压结果..."

    local CEF_FOUND=false
    local CEF_VALID_PATH=""
    local CEF_CHECK_PATHS=(
        "$CEF_DIR/$CEF_BINARY_NAME/include/cef_version.h"
        "$CEF_DIR/include/cef_version.h"
        "$CEF_DIR/$CEF_BINARY_NAME/cef_version.h"
    )

    for CHECK_PATH in "${CEF_CHECK_PATHS[@]}"; do
        if [[ "$CEF_FOUND" == "false" ]] && [[ -f "$CHECK_PATH" ]]; then
            CEF_FOUND=true
            CEF_VALID_PATH="$CHECK_PATH"
            log_success "CEF头文件验证成功: $CHECK_PATH"
            break
        fi
    done

    if [[ "$CEF_FOUND" == "false" ]]; then
        local FOUND_FILES=()
        while IFS= read -r line; do
            FOUND_FILES+=("$line")
        done < <(find "$CEF_DIR" -name "cef_version.h" -type f 2>/dev/null)

        if [[ ${#FOUND_FILES[@]} -gt 0 ]]; then
            CEF_FOUND=true
            CEF_VALID_PATH="${FOUND_FILES[0]}"
            log_success "在深度搜索中找到CEF头文件: $CEF_VALID_PATH"
        fi
    fi

    if [[ "$CEF_FOUND" == "true" ]]; then
        log_success "CEF验证成功！有效路径: $CEF_VALID_PATH"
        return 0
    fi

    log_error "CEF验证失败：未找到必需的cef_version.h文件"
    return 1
}

download_cef() {
    log_info "开始下载CEF $CEF_VERSION ($CEF_PLATFORM)..."

    TEMP_DIR=$(mktemp -d)
    trap 'rm -rf "$TEMP_DIR"' EXIT

    if [[ "$CEF_VERSION" == "$CEF_VERSION_75" ]]; then
        GITHUB_RELEASE_URL="https://github.com/${GITHUB_REPO}/releases/download/cef-75.1.14/cef-75.1.14-${CEF_PLATFORM}.tar.bz2"
        log_info "尝试从GitHub Release下载..."

        if command -v curl >/dev/null 2>&1; then
            if curl -L -f "$GITHUB_RELEASE_URL" -o "$TEMP_DIR/$CEF_ARCHIVE_NAME" 2>/dev/null; then
                log_success "GitHub Release下载成功"
                if verify_downloaded_archive && extract_and_verify_cef; then
                    return 0
                fi
                log_warning "GitHub Release包验证失败，回退到Spotify CDN..."
            fi
        fi
    fi

    log_info "从Spotify CDN下载 $CEF_ARCHIVE_NAME..."
    build_download_url

    if command -v curl >/dev/null 2>&1; then
        if ! curl -fL "$DOWNLOAD_URL" -o "$TEMP_DIR/$CEF_ARCHIVE_NAME"; then
            log_error "curl下载失败"
            return 1
        fi
    elif command -v wget >/dev/null 2>&1; then
        if ! wget "$DOWNLOAD_URL" -O "$TEMP_DIR/$CEF_ARCHIVE_NAME"; then
            log_error "wget下载失败"
            return 1
        fi
    else
        log_error "未找到curl或wget"
        return 1
    fi

    log_success "下载完成"

    if ! verify_downloaded_archive; then
        return 1
    fi

    extract_and_verify_cef
}

show_help() {
    echo "CEF下载脚本"
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -f, --force              强制重新下载，即使CEF已存在"
    echo "  -h, --help               显示此帮助信息"
    echo "  --platform=PLATFORM      指定平台 (windows32, windows64, macosx64, macosarm64, linux64)"
    echo "  --version=VERSION        指定CEF版本 (75, 118 或完整版本号)"
    echo ""
    echo "macOS 默认策略:"
    echo "  arm64 (Apple Silicon) -> CEF 118 macosarm64"
    echo "  x86_64 (Intel Mac)    -> CEF 75  macosx64"
    echo ""
    echo "示例:"
    echo "  $0"
    echo "  $0 --platform=macosarm64"
    echo "  $0 --platform=macosx64 --version=75"
    echo "  $0 --force --platform=macosarm64 --version=118"
}

main() {
    local FORCE_DOWNLOAD=false
    local CUSTOM_PLATFORM=""
    local CUSTOM_VERSION=""

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

    if [[ -n "$CUSTOM_PLATFORM" ]]; then
        PLATFORM="$CUSTOM_PLATFORM"
        log_info "使用指定平台: $PLATFORM"
    else
        detect_platform
    fi

    if [[ -n "$CUSTOM_VERSION" ]]; then
        resolve_cef_version_string "$CUSTOM_VERSION"
    fi

    apply_platform_version_defaults

    build_download_url

    if [[ "$FORCE_DOWNLOAD" == "false" ]] && check_existing_cef; then
        exit 0
    fi

    if download_cef; then
        log_success "CEF下载和安装成功！"
        log_info "CEF路径: $CEF_DIR/$CEF_BINARY_NAME"
        log_info "现在可以运行CMake构建项目了"
    else
        log_error "CEF下载失败"
        exit 1
    fi
}

main "$@"
