#!/bin/bash

# DesktopTerminal-CEF 跨平台构建脚本
# 支持Windows 7 SP1 32位、64位系统以及macOS、Linux

set -e  # 遇到错误时退出

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
INSTALL_DIR="$PROJECT_ROOT/install"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 输出函数
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

# 显示帮助信息
show_help() {
    echo "DesktopTerminal-CEF 构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help              显示此帮助信息"
    echo "  -t, --target TARGET     构建目标 (Debug|Release|MinSizeRel|RelWithDebInfo)"
    echo "  -a, --arch ARCH         目标架构 (x86|x64|auto)"
    echo "  -p, --platform PLATFORM 目标平台 (windows|macos|linux|auto)"
    echo "  -j, --jobs JOBS         并行构建任务数 (默认: CPU核心数)"
    echo "  -c, --clean             清理构建目录"
    echo "  -i, --install           构建后安装"
    echo "  --cef-version VERSION   强制使用指定的CEF版本"
    echo "  --qt-dir DIR            Qt安装目录"
    echo "  --verbose               详细输出"
    echo ""
    echo "示例:"
    echo "  $0 -t Release -a x64                 # 构建64位Release版本"
    echo "  $0 -t Debug -a x86 --clean           # 清理并构建32位Debug版本"
    echo "  $0 --cef-version 75 -a x86           # 强制使用CEF 75构建32位版本"
    echo ""
}

# 检测系统信息
detect_system() {
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
        PLATFORM="windows"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PLATFORM="macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PLATFORM="linux"
    else
        PLATFORM="unknown"
    fi
    
    if [[ -z "$ARCH" || "$ARCH" == "auto" ]]; then
        if [[ "$(uname -m)" == "x86_64" ]]; then
            ARCH="x64"
        else
            ARCH="x86"
        fi
    fi
    
    log_info "检测到平台: $PLATFORM, 架构: $ARCH"
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."
    
    # 检查CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake未找到，请安装CMake 3.20或更高版本"
        exit 1
    fi
    
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    log_info "CMake版本: $CMAKE_VERSION"
    
    # 检查编译器
    if [[ "$PLATFORM" == "windows" ]]; then
        if ! command -v cl &> /dev/null && ! command -v gcc &> /dev/null; then
            log_error "未找到编译器（MSVC或GCC）"
            exit 1
        fi
    elif [[ "$PLATFORM" == "macos" ]]; then
        if ! command -v clang &> /dev/null; then
            log_error "未找到Clang编译器"
            exit 1
        fi
    else
        if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
            log_error "未找到GCC或Clang编译器"
            exit 1
        fi
    fi
    
    log_success "依赖检查完成"
}

# 下载CEF
download_cef() {
    log_info "检查CEF安装..."
    
    CEF_DIR="$PROJECT_ROOT/third_party/cef"
    
    if [[ ! -d "$CEF_DIR" ]]; then
        log_info "CEF未找到，开始下载..."
        
        if [[ "$PLATFORM" == "windows" ]]; then
            "$PROJECT_ROOT/scripts/download-cef.bat" "$ARCH" "$CEF_VERSION"
        else
            "$PROJECT_ROOT/scripts/download-cef.sh" "$ARCH" "$CEF_VERSION"
        fi
        
        if [[ $? -ne 0 ]]; then
            log_error "CEF下载失败"
            exit 1
        fi
    else
        log_info "CEF已存在: $CEF_DIR"
    fi
}

# 配置构建
configure_build() {
    log_info "配置构建..."
    
    # 创建构建目录
    BUILD_SUBDIR="$BUILD_DIR/${TARGET}_${ARCH}"
    mkdir -p "$BUILD_SUBDIR"
    cd "$BUILD_SUBDIR"
    
    # CMake参数
    CMAKE_ARGS=(
        "-DCMAKE_BUILD_TYPE=$TARGET"
        "-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR"
    )
    
    # 架构特定配置
    if [[ "$ARCH" == "x86" ]]; then
        if [[ "$PLATFORM" == "windows" ]]; then
            CMAKE_ARGS+=("-A" "Win32")
        else
            CMAKE_ARGS+=("-DCMAKE_CXX_FLAGS=-m32")
            CMAKE_ARGS+=("-DCMAKE_C_FLAGS=-m32")
        fi
    fi
    
    # Qt配置
    if [[ -n "$QT_DIR" ]]; then
        CMAKE_ARGS+=("-DQt5_DIR=$QT_DIR/lib/cmake/Qt5")
    fi
    
    # CEF版本配置
    if [[ -n "$CEF_VERSION" ]]; then
        CMAKE_ARGS+=("-DCEF_VERSION=$CEF_VERSION")
    fi
    
    # 详细输出
    if [[ "$VERBOSE" == "true" ]]; then
        CMAKE_ARGS+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi
    
    log_info "CMake参数: ${CMAKE_ARGS[*]}"
    
    # 运行CMake
    cmake "${CMAKE_ARGS[@]}" "$PROJECT_ROOT"
    
    if [[ $? -ne 0 ]]; then
        log_error "CMake配置失败"
        exit 1
    fi
    
    log_success "构建配置完成"
}

# 执行构建
build_project() {
    log_info "开始构建..."
    
    cd "$BUILD_SUBDIR"
    
    # 构建参数
    BUILD_ARGS=(
        "--build" "."
        "--config" "$TARGET"
    )
    
    if [[ -n "$JOBS" ]]; then
        BUILD_ARGS+=("-j" "$JOBS")
    fi
    
    if [[ "$VERBOSE" == "true" ]]; then
        BUILD_ARGS+=("--verbose")
    fi
    
    log_info "构建参数: ${BUILD_ARGS[*]}"
    
    # 执行构建
    cmake "${BUILD_ARGS[@]}"
    
    if [[ $? -ne 0 ]]; then
        log_error "构建失败"
        exit 1
    fi
    
    log_success "构建完成"
}

# 安装
install_project() {
    if [[ "$INSTALL" == "true" ]]; then
        log_info "开始安装..."
        
        cd "$BUILD_SUBDIR"
        cmake --install . --config "$TARGET"
        
        if [[ $? -ne 0 ]]; then
            log_error "安装失败"
            exit 1
        fi
        
        log_success "安装完成: $INSTALL_DIR"
    fi
}

# 清理构建目录
clean_build() {
    if [[ "$CLEAN" == "true" ]]; then
        log_info "清理构建目录..."
        rm -rf "$BUILD_DIR"
        log_success "构建目录已清理"
    fi
}

# 显示构建摘要
show_summary() {
    log_success "=== 构建摘要 ==="
    log_info "平台: $PLATFORM"
    log_info "架构: $ARCH"
    log_info "构建类型: $TARGET"
    log_info "构建目录: $BUILD_SUBDIR"
    
    if [[ "$INSTALL" == "true" ]]; then
        log_info "安装目录: $INSTALL_DIR"
    fi
    
    if [[ -n "$CEF_VERSION" ]]; then
        log_info "CEF版本: $CEF_VERSION"
    fi
    
    log_success "构建完成！"
}

# 默认参数
TARGET="Release"
ARCH="auto"
PLATFORM="auto"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")
CLEAN="false"
INSTALL="false"
VERBOSE="false"
CEF_VERSION=""
QT_DIR=""

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -t|--target)
            TARGET="$2"
            shift 2
            ;;
        -a|--arch)
            ARCH="$2"
            shift 2
            ;;
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN="true"
            shift
            ;;
        -i|--install)
            INSTALL="true"
            shift
            ;;
        --cef-version)
            CEF_VERSION="$2"
            shift 2
            ;;
        --qt-dir)
            QT_DIR="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE="true"
            shift
            ;;
        *)
            log_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

# 主流程
main() {
    log_info "DesktopTerminal-CEF 构建脚本启动"
    
    detect_system
    check_dependencies
    clean_build
    download_cef
    configure_build
    build_project
    install_project
    show_summary
}

# 运行主流程
main "$@"