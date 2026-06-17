#!/bin/bash
# DesktopTerminal-CEF macOS 打包脚本
# 构建 .app -> macdeployqt -> 可选 .dmg
# Apple Silicon 优先使用 CEF 118 macosarm64；Intel Mac 使用 CEF 75 macosx64

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_ROOT="$PROJECT_ROOT/build"
PACKAGE_ROOT="$PROJECT_ROOT/package/macos"

CEF_VERSION_75="75.1.14+gc81164e+chromium-75.0.3770.100"
CEF_VERSION_118="118.7.1+g99817d2+chromium-118.0.5993.119"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TARGET="Release"
MAC_ARCH="auto"
APP_VERSION="1.0.0"
MAKE_DMG="false"
CLEAN_BUILD="false"
SKIP_BUILD="false"
QT_DIR=""

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

show_help() {
    cat <<EOF
DesktopTerminal-CEF macOS 打包脚本

用法: $0 [选项]

选项:
  -h, --help              显示帮助
  -t, --target TARGET     构建类型 (Release|Debug)，默认 Release
  -a, --arch ARCH         目标架构 (arm64|x86_64|auto)，默认 auto
  --version VERSION       应用版本号，用于输出文件名
  --qt-dir DIR            Qt5 安装目录 (含 bin/macdeployqt)
  --dmg                   额外生成 .dmg
  --clean                 清理对应架构的 build 目录后重新构建
  --skip-build            跳过编译，仅对已有 .app 执行打包

架构与 CEF 策略:
  arm64 (Apple Silicon) -> CEF 118 + macosarm64
  x86_64 (Intel Mac)    -> CEF 75  + macosx64

示例:
  $0                      # 自动检测本机架构并打包
  $0 -a arm64 --dmg       # M 系列 Mac 构建并生成 DMG
  $0 -a x86_64            # Intel Mac 构建
  $0 --skip-build --dmg     # 仅打包已有产物
EOF
}

resolve_mac_arch() {
    case "$MAC_ARCH" in
        auto)
            case "$(uname -m)" in
                arm64|aarch64) MAC_ARCH="arm64" ;;
                x86_64) MAC_ARCH="x86_64" ;;
                *)
                    log_error "无法识别 macOS 架构: $(uname -m)"
                    exit 1
                    ;;
            esac
            ;;
        arm64|x86_64) ;;
        *)
            log_error "不支持的架构: $MAC_ARCH (可选: arm64, x86_64, auto)"
            exit 1
            ;;
    esac

    if [[ "$MAC_ARCH" == "arm64" ]]; then
        CEF_PLATFORM="macosarm64"
        CEF_VERSION="$CEF_VERSION_118"
        ARCH_LABEL="arm64"
    else
        CEF_PLATFORM="macosx64"
        CEF_VERSION="$CEF_VERSION_75"
        ARCH_LABEL="x64"
    fi

    CEF_BINARY_NAME="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}"
    BUILD_DIR="$BUILD_ROOT/${TARGET}_${ARCH_LABEL}"
    APP_NAME="DesktopTerminal-CEF.app"
    APP_PATH="$BUILD_DIR/bin/$APP_NAME"

    log_info "目标架构: $MAC_ARCH (CEF $CEF_PLATFORM / ${CEF_VERSION%%+*})"
}

resolve_qt_dir() {
    if [[ -n "$QT_DIR" ]]; then
        return
    fi

    local candidates=(
        "/opt/homebrew/opt/qt@5"
        "/usr/local/opt/qt@5"
        "/opt/homebrew/opt/qt5"
        "/usr/local/opt/qt5"
    )

    for candidate in "${candidates[@]}"; do
        if [[ -x "$candidate/bin/macdeployqt" ]]; then
            QT_DIR="$candidate"
            return
        fi
    done

    log_error "未找到 macdeployqt，请安装 Qt5 并通过 --qt-dir 指定路径"
    log_error "例如: brew install qt@5"
    exit 1
}

verify_qt_architecture() {
    local qt_core="$QT_DIR/lib/QtCore.framework/QtCore"
    if [[ ! -f "$qt_core" ]]; then
        log_warning "无法检测 Qt 架构: $qt_core 不存在"
        return
    fi

    local qt_archs
    qt_archs="$(lipo -info "$qt_core" 2>/dev/null | sed 's/.*: //')"
    log_info "Qt 库架构: $qt_archs"

    if [[ "$MAC_ARCH" == "x86_64" && "$qt_archs" == *"arm64"* && "$qt_archs" != *"x86_64"* ]]; then
        log_error "当前 Qt 为 arm64，无法在 Apple Silicon 上交叉编译 x86_64 目标"
        log_error "请使用 -a arm64（需 CEF 118 源码适配），或在 Intel Mac 上执行 x86_64 打包"
        exit 1
    fi

    if [[ "$MAC_ARCH" == "arm64" && "$qt_archs" == *"x86_64"* && "$qt_archs" != *"arm64"* ]]; then
        log_error "当前 Qt 为 x86_64，无法编译 arm64 目标"
        exit 1
    fi
}

check_arm64_build_readiness() {
    if [[ "$MAC_ARCH" != "arm64" ]]; then
        return
    fi

    log_warning "Apple Silicon 原生构建使用 CEF 118，当前源码基于 CEF 75 API"
    log_warning "若编译失败，需先完成 CEF 118 接口适配；Intel Mac 可先用 -a x86_64 + CEF 75"
}

check_dependencies() {
    if [[ "$(uname -s)" != "Darwin" ]]; then
        log_error "此脚本仅支持在 macOS 上运行"
        exit 1
    fi

    for cmd in cmake clang tar lipo; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            log_error "缺少依赖: $cmd"
            exit 1
        fi
    done

    resolve_qt_dir
    log_info "Qt 目录: $QT_DIR"
}

download_cef_for_arch() {
    log_info "检查/下载 CEF..."
    if [[ "$MAC_ARCH" == "arm64" ]]; then
        "$SCRIPT_DIR/download-cef.sh" --platform=macosarm64 --version=118
    else
        "$SCRIPT_DIR/download-cef.sh" --platform=macosx64 --version=75
    fi
}

build_application() {
    if [[ "$SKIP_BUILD" == "true" ]]; then
        log_info "跳过构建 (--skip-build)"
        return
    fi

    if [[ "$CLEAN_BUILD" == "true" ]]; then
        log_info "清理构建目录: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi

    download_cef_for_arch

    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR" >/dev/null

    log_info "配置 CMake..."
    cmake \
        -DCMAKE_BUILD_TYPE="$TARGET" \
        -DCMAKE_OSX_ARCHITECTURES="$MAC_ARCH" \
        -DCEF_VERSION="$CEF_VERSION" \
        -DQt5_DIR="$QT_DIR/lib/cmake/Qt5" \
        "$PROJECT_ROOT"

    log_info "编译项目..."
    cmake --build . --config "$TARGET" -j "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

    popd >/dev/null
    log_success "编译完成: $APP_PATH"
}

ensure_app_exists() {
    if [[ ! -d "$APP_PATH" ]]; then
        log_error "未找到应用包: $APP_PATH"
        log_error "请先执行构建，或去掉 --skip-build"
        exit 1
    fi

    local executable="$APP_PATH/Contents/MacOS/DesktopTerminal-CEF"
    if [[ ! -x "$executable" ]]; then
        log_error "应用可执行文件不存在或不可执行: $executable"
        exit 1
    fi
}

bundle_cef_into_app() {
    local cef_root="$PROJECT_ROOT/third_party/cef/$CEF_BINARY_NAME"
    local release_dir="$cef_root/Release"
    local frameworks_dir="$APP_PATH/Contents/Frameworks"
    local macos_dir="$APP_PATH/Contents/MacOS"

    if [[ ! -d "$release_dir" ]]; then
        log_error "CEF Release 目录不存在: $release_dir"
        exit 1
    fi

    mkdir -p "$frameworks_dir"

    log_info "复制 CEF Framework..."
    rm -rf "$frameworks_dir/Chromium Embedded Framework.framework"
    cp -R "$release_dir/Chromium Embedded Framework.framework" "$frameworks_dir/"

    log_info "复制 CEF Helper 应用..."
    shopt -s nullglob
    for helper_app in "$release_dir"/*.app; do
        local helper_name
        helper_name="$(basename "$helper_app")"
        rm -rf "$frameworks_dir/$helper_name"
        cp -R "$helper_app" "$frameworks_dir/"
    done
    shopt -u nullglob

    log_info "同步 CEF 资源到 MacOS 目录..."
    local framework_resources="$frameworks_dir/Chromium Embedded Framework.framework/Resources"
    if [[ -d "$framework_resources" ]]; then
        for pak in "$framework_resources"/*.pak "$framework_resources"/*.dat; do
            [[ -e "$pak" ]] || continue
            cp -f "$pak" "$macos_dir/"
        done
        if [[ -d "$framework_resources/locales" ]]; then
            rm -rf "$macos_dir/locales"
            cp -R "$framework_resources/locales" "$macos_dir/"
        fi
    fi

    if [[ -d "$cef_root/Resources/locales" ]]; then
        rm -rf "$macos_dir/locales"
        cp -R "$cef_root/Resources/locales" "$macos_dir/"
    fi

    log_success "CEF 运行时已写入应用包"
}

run_macdeployqt() {
    log_info "运行 macdeployqt..."
    "$QT_DIR/bin/macdeployqt" "$APP_PATH" \
        -verbose=1 \
        -always-overwrite

    log_success "Qt 依赖已部署"
}

create_launcher_readme() {
    local output_dir="$PACKAGE_ROOT/DesktopTerminal-CEF-${APP_VERSION}-macos-${MAC_ARCH}"
    mkdir -p "$output_dir"

    rm -rf "$output_dir/$APP_NAME"
    cp -R "$APP_PATH" "$output_dir/"

    cat > "$output_dir/README.txt" <<EOF
DesktopTerminal-CEF macOS 版 v${APP_VERSION}
==========================================

架构: ${MAC_ARCH}
CEF: ${CEF_VERSION}
构建时间: $(date)

启动方式:
  双击 DesktopTerminal-CEF.app

若系统提示无法打开(未签名应用):
  xattr -cr DesktopTerminal-CEF.app

退出方式:
  F10 + 退出密码 (config.json 中 exitPassword)
  或 Web 页面跳转至含 /logout 的 URL (urlExitPattern)

配置文件:
  DesktopTerminal-CEF.app/Contents/MacOS/resources/config.json
EOF

    PACKAGE_DIR="$output_dir"
    log_success "打包目录: $PACKAGE_DIR"
}

create_zip_package() {
    local zip_name="$PACKAGE_ROOT/DesktopTerminal-CEF-${APP_VERSION}-macos-${MAC_ARCH}.zip"
    mkdir -p "$PACKAGE_ROOT"

    (
        cd "$PACKAGE_ROOT"
        rm -f "$(basename "$zip_name")"
        zip -r "$(basename "$zip_name")" "$(basename "$PACKAGE_DIR")"
    )

    log_success "ZIP: $zip_name"
    PACKAGE_ZIP="$zip_name"
}

create_dmg_package() {
    if ! command -v hdiutil >/dev/null 2>&1; then
        log_warning "未找到 hdiutil，跳过 DMG 生成"
        return
    fi

    local dmg_name="$PACKAGE_ROOT/DesktopTerminal-CEF-${APP_VERSION}-macos-${MAC_ARCH}.dmg"
    mkdir -p "$PACKAGE_ROOT"
    rm -f "$dmg_name"

    hdiutil create \
        -volname "DesktopTerminal-CEF" \
        -srcfolder "$PACKAGE_DIR" \
        -ov \
        -format UDZO \
        "$dmg_name"

    log_success "DMG: $dmg_name"
    PACKAGE_DMG="$dmg_name"
}

package_application() {
    ensure_app_exists
    run_macdeployqt
    bundle_cef_into_app
    create_launcher_readme
    create_zip_package

    if [[ "$MAKE_DMG" == "true" ]]; then
        create_dmg_package
    fi
}

show_summary() {
    log_success "=== macOS 打包完成 ==="
    log_info "架构: $MAC_ARCH"
    log_info "CEF: $CEF_BINARY_NAME"
    log_info "应用: $APP_PATH"
    log_info "分发目录: $PACKAGE_DIR"
    [[ -n "${PACKAGE_ZIP:-}" ]] && log_info "ZIP: $PACKAGE_ZIP"
    [[ -n "${PACKAGE_DMG:-}" ]] && log_info "DMG: $PACKAGE_DMG"
}

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
            MAC_ARCH="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
            shift 2
            ;;
        --qt-dir)
            QT_DIR="$2"
            shift 2
            ;;
        --dmg)
            MAKE_DMG="true"
            shift
            ;;
        --clean)
            CLEAN_BUILD="true"
            shift
            ;;
        --skip-build)
            SKIP_BUILD="true"
            shift
            ;;
        *)
            log_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

log_info "DesktopTerminal-CEF macOS 打包开始"
check_dependencies
resolve_mac_arch
verify_qt_architecture
check_arm64_build_readiness
build_application
package_application
show_summary
