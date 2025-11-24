#!/bin/bash

# DesktopTerminal-CEF 打包脚本
# 创建可分发的软件包，包含所有必需的文件和依赖

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
PACKAGE_DIR="$PROJECT_ROOT/package"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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
    echo "DesktopTerminal-CEF 打包脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help              显示此帮助信息"
    echo "  -t, --target TARGET     构建目标 (Debug|Release|MinSizeRel|RelWithDebInfo)"
    echo "  -a, --arch ARCH         目标架构 (x86|x64)"
    echo "  -p, --platform PLATFORM 目标平台 (windows|macos|linux)"
    echo "  -f, --format FORMAT     打包格式 (zip|tar.gz|deb|rpm|dmg|nsis)"
    echo "  --version VERSION       软件版本号"
    echo "  --clean                 清理打包目录"
    echo ""
}

# 检测系统信息
detect_system() {
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
        PLATFORM="windows"
        DEFAULT_FORMAT="zip"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PLATFORM="macos"
        DEFAULT_FORMAT="dmg"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PLATFORM="linux"
        DEFAULT_FORMAT="tar.gz"
    else
        PLATFORM="unknown"
        DEFAULT_FORMAT="zip"
    fi
    
    if [[ -z "$ARCH" ]]; then
        if [[ "$(uname -m)" == "x86_64" ]]; then
            ARCH="x64"
        else
            ARCH="x86"
        fi
    fi
}

# 创建打包目录结构
create_package_structure() {
    log_info "创建打包目录结构..."
    
    PACKAGE_NAME="DesktopTerminal-CEF-${VERSION}-${PLATFORM}-${ARCH}"
    PACKAGE_PATH="$PACKAGE_DIR/$PACKAGE_NAME"
    
    rm -rf "$PACKAGE_PATH"
    mkdir -p "$PACKAGE_PATH"
    
    # 创建子目录
    mkdir -p "$PACKAGE_PATH/bin"
    mkdir -p "$PACKAGE_PATH/lib"
    mkdir -p "$PACKAGE_PATH/resources"
    mkdir -p "$PACKAGE_PATH/config"
    mkdir -p "$PACKAGE_PATH/logs"
    mkdir -p "$PACKAGE_PATH/docs"
    
    log_success "打包目录结构创建完成: $PACKAGE_PATH"
}

# 复制主程序文件
copy_application() {
    log_info "复制主程序文件..."
    
    BUILD_SUBDIR="$BUILD_DIR/${TARGET}_${ARCH}"
    
    if [[ ! -d "$BUILD_SUBDIR" ]]; then
        log_error "构建目录不存在: $BUILD_SUBDIR"
        log_error "请先运行构建脚本"
        exit 1
    fi
    
    # 复制主执行文件
    if [[ "$PLATFORM" == "windows" ]]; then
        cp "$BUILD_SUBDIR/bin/DesktopTerminal-CEF.exe" "$PACKAGE_PATH/bin/"
    else
        cp "$BUILD_SUBDIR/bin/DesktopTerminal-CEF" "$PACKAGE_PATH/bin/"
        chmod +x "$PACKAGE_PATH/bin/DesktopTerminal-CEF"
    fi
    
    log_success "主程序文件复制完成"
}

# 复制CEF文件
copy_cef_files() {
    log_info "复制CEF运行时文件..."
    
    CEF_DIR="$PROJECT_ROOT/third_party/cef"
    
    if [[ ! -d "$CEF_DIR" ]]; then
        log_error "CEF目录不存在: $CEF_DIR"
        exit 1
    fi
    
    # 复制CEF二进制文件
    if [[ "$PLATFORM" == "windows" ]]; then
        # Windows CEF文件
        cp "$CEF_DIR"/Release/*.dll "$PACKAGE_PATH/bin/" 2>/dev/null || true
        cp "$CEF_DIR"/Release/*.exe "$PACKAGE_PATH/bin/" 2>/dev/null || true
        cp "$CEF_DIR"/Release/*.bin "$PACKAGE_PATH/bin/" 2>/dev/null || true
    elif [[ "$PLATFORM" == "macos" ]]; then
        # macOS CEF框架 - 从构建目录复制
        BUILD_CEF_DIR="$PROJECT_ROOT/build/Release_$ARCH/bin/Release"
        if [[ -d "$BUILD_CEF_DIR" ]]; then
            cp -R "$BUILD_CEF_DIR"/*.framework "$PACKAGE_PATH/bin/" 2>/dev/null || true
            cp "$BUILD_CEF_DIR"/*.pak "$PACKAGE_PATH/resources/" 2>/dev/null || true
        fi
    else
        # Linux CEF库
        cp "$CEF_DIR"/Release/*.so* "$PACKAGE_PATH/lib/" 2>/dev/null || true
        cp "$CEF_DIR"/Release/chrome-sandbox "$PACKAGE_PATH/bin/" 2>/dev/null || true
        chmod 4755 "$PACKAGE_PATH/bin/chrome-sandbox" 2>/dev/null || true
    fi
    
    # 复制CEF资源文件
    cp "$CEF_DIR"/Resources/*.pak "$PACKAGE_PATH/resources/" 2>/dev/null || true
    cp "$CEF_DIR"/Resources/*.dat "$PACKAGE_PATH/resources/" 2>/dev/null || true
    cp -R "$CEF_DIR"/Resources/locales "$PACKAGE_PATH/resources/" 2>/dev/null || true
    
    log_success "CEF文件复制完成"
}

# 复制Qt库
copy_qt_libraries() {
    log_info "复制Qt运行时库..."
    
    if [[ "$PLATFORM" == "windows" ]]; then
        # Windows Qt DLL
        QT_DLLS=("Qt5Core.dll" "Qt5Gui.dll" "Qt5Widgets.dll")
        for dll in "${QT_DLLS[@]}"; do
            find /c/Qt* -name "$dll" -exec cp {} "$PACKAGE_PATH/bin/" \; 2>/dev/null || true
        done
        
        # Qt平台插件
        mkdir -p "$PACKAGE_PATH/bin/platforms"
        find /c/Qt* -name "qwindows.dll" -exec cp {} "$PACKAGE_PATH/bin/platforms/" \; 2>/dev/null || true
        
    elif [[ "$PLATFORM" == "macos" ]]; then
        # macOS Qt框架
        QT_FRAMEWORKS=("QtCore" "QtGui" "QtWidgets" "QtNetwork")
        QT_PATHS=("/opt/homebrew/opt/qt@5/lib" "/usr/local/opt/qt5/lib" "/usr/local/opt/qt@5/lib")

        for framework in "${QT_FRAMEWORKS[@]}"; do
            for qt_path in "${QT_PATHS[@]}"; do
                if [[ -d "$qt_path/${framework}.framework" ]]; then
                    cp -R "$qt_path/${framework}.framework" "$PACKAGE_PATH/lib/"
                    break
                fi
            done
        done
        
    else
        # Linux Qt库
        QT_LIBS=("libQt5Core.so.5" "libQt5Gui.so.5" "libQt5Widgets.so.5")
        for lib in "${QT_LIBS[@]}"; do
            find /usr -name "$lib*" -exec cp {} "$PACKAGE_PATH/lib/" \; 2>/dev/null || true
        done
    fi
    
    log_success "Qt库复制完成"
}

# 复制配置文件
copy_config_files() {
    log_info "复制配置文件..."

    # 复制资源文件
    if [[ -f "$PROJECT_ROOT/resources/logo.svg" ]]; then
        cp "$PROJECT_ROOT/resources/logo.svg" "$PACKAGE_PATH/resources/"
    fi

    log_success "配置文件复制完成"
}

# 复制文档
copy_documentation() {
    log_info "复制文档文件..."
    
    # 创建README
    cat > "$PACKAGE_PATH/docs/README.txt" << EOF
DesktopTerminal-CEF v${VERSION}
==============================

这是基于CEF（Chromium Embedded Framework）的安全桌面终端应用程序。

系统要求:
- 操作系统: Windows 7 SP1及以上 / macOS 10.12及以上 / Linux (Ubuntu 18.04及以上)
- 架构: ${ARCH}
- 内存: 至少512MB可用内存

安装说明:
1. 解压所有文件到目标目录
2. 确保config/config.json配置正确
3. 运行bin/DesktopTerminal-CEF启动程序

安全功能:
- 全屏锁定模式
- 键盘操作拦截
- URL访问控制
- 安全退出机制（F10或反斜杠键+密码）

技术支持:
- 网站: http://www.sdzdf.com
- 邮箱: support@sdzdf.com

版本信息:
- 构建日期: $(date)
- CEF版本: ${CEF_VERSION:-自动检测}
- 目标平台: ${PLATFORM}
- 目标架构: ${ARCH}
EOF
    
    # 复制许可证
    if [[ -f "$PROJECT_ROOT/LICENSE" ]]; then
        cp "$PROJECT_ROOT/LICENSE" "$PACKAGE_PATH/docs/"
    fi
    
    log_success "文档复制完成"
}

# 创建启动脚本
create_launcher() {
    log_info "创建启动脚本..."
    
    if [[ "$PLATFORM" == "windows" ]]; then
        # Windows批处理文件
        cat > "$PACKAGE_PATH/DesktopTerminal-CEF.bat" << 'EOF'
@echo off
cd /d "%~dp0"
set PATH=%PATH%;%~dp0bin;%~dp0lib
start "" "%~dp0bin\DesktopTerminal-CEF.exe"
EOF
        
    else
        # Unix shell脚本
        cat > "$PACKAGE_PATH/DesktopTerminal-CEF.sh" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [[ "$OSTYPE" == "darwin"* ]]; then
    export DYLD_FRAMEWORK_PATH="$SCRIPT_DIR/lib:$SCRIPT_DIR/bin:$DYLD_FRAMEWORK_PATH"
else
    export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
fi

export PATH="$SCRIPT_DIR/bin:$PATH"
exec "$SCRIPT_DIR/bin/DesktopTerminal-CEF" "$@"
EOF
        chmod +x "$PACKAGE_PATH/DesktopTerminal-CEF.sh"
    fi
    
    log_success "启动脚本创建完成"
}

# 修复macOS动态库路径
fix_macos_dylib_paths() {
    if [[ "$PLATFORM" != "macos" ]]; then
        return
    fi

    log_info "修复macOS动态库路径..."

    local binary="$PACKAGE_PATH/bin/DesktopTerminal-CEF"
    local frameworks=("QtCore" "QtGui" "QtWidgets" "QtNetwork")

    for framework in "${frameworks[@]}"; do
        install_name_tool -change \
            "/opt/homebrew/opt/qt@5/lib/${framework}.framework/Versions/5/${framework}" \
            "@executable_path/../lib/${framework}.framework/Versions/5/${framework}" \
            "$binary" 2>/dev/null || true
    done

    log_success "动态库路径修复完成"
}

# 创建DMG包
create_dmg_package() {
    if ! command -v hdiutil &> /dev/null; then
        log_error "hdiutil未找到，无法创建DMG"
        return 1
    fi

    local dmg_name="${PACKAGE_NAME}.dmg"
    local temp_dmg="${PACKAGE_NAME}-temp.dmg"

    hdiutil create -volname "$PACKAGE_NAME" -srcfolder "$PACKAGE_NAME" -ov -format UDZO "$dmg_name"
    log_success "DMG包创建完成: $dmg_name"
}

# 创建软件包
create_package() {
    log_info "创建软件包..."

    cd "$PACKAGE_DIR"
    
    case "$FORMAT" in
        "zip")
            zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
            log_success "ZIP包创建完成: ${PACKAGE_NAME}.zip"
            ;;
        "tar.gz")
            tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME"
            log_success "TAR.GZ包创建完成: ${PACKAGE_NAME}.tar.gz"
            ;;
        "deb")
            create_deb_package
            ;;
        "rpm")
            create_rpm_package
            ;;
        "dmg")
            create_dmg_package
            ;;
        "nsis")
            create_nsis_package
            ;;
        *)
            log_warning "未知的打包格式: $FORMAT，使用ZIP格式"
            zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
            ;;
    esac
}

# 清理打包目录
clean_package() {
    if [[ "$CLEAN" == "true" ]]; then
        log_info "清理打包目录..."
        rm -rf "$PACKAGE_DIR"
        log_success "打包目录已清理"
    fi
}

# 默认参数
TARGET="Release"
ARCH=""
PLATFORM=""
FORMAT=""
VERSION="1.0.0"
CLEAN="false"

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
        -f|--format)
            FORMAT="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --clean)
            CLEAN="true"
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
    log_info "DesktopTerminal-CEF 打包脚本启动"
    
    detect_system
    
    if [[ -z "$FORMAT" ]]; then
        FORMAT="$DEFAULT_FORMAT"
    fi
    
    log_info "打包参数: 平台=$PLATFORM, 架构=$ARCH, 格式=$FORMAT, 版本=$VERSION"
    
    clean_package
    create_package_structure
    copy_application
    copy_cef_files
    copy_qt_libraries
    copy_config_files
    copy_documentation
    create_launcher
    fix_macos_dylib_paths
    create_package
    
    log_success "打包完成！"
}

# 运行主流程
main "$@"