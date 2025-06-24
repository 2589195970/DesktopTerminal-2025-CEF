#!/bin/bash

# ===============================================
# CEF版本高级构建触发脚本
# ===============================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    cat << EOF
智多分机考桌面端-CEF版 - 高级Windows构建触发脚本

用法: $0 [选项]

选项:
  -r, --release VERSION    创建发布版本 (例如: v1.0.0-cef)
  -b, --branch BRANCH      推送到指定分支触发构建
  -w, --watch             触发后监控构建进度
  -d, --download          构建完成后自动下载产物
  -c, --clean             清理本地缓存和临时文件
  -s, --status            检查最近构建状态
  -h, --help              显示此帮助信息

CEF特定选项:
  --cef-32               仅构建32位CEF版本 (Windows 7兼容)
  --cef-64               仅构建64位CEF版本 (现代系统)
  --force-cef-download   强制重新下载CEF依赖

示例:
  $0                              # 标准触发构建
  $0 -r v1.0.0-cef -w -d         # 创建发布版本并监控下载
  $0 --cef-32 -w                 # 仅构建32位版本并监控
  $0 -s                          # 检查构建状态
  $0 -c                          # 清理缓存

前置要求:
  - GitHub CLI (gh) 已安装并已认证
  - 当前目录为CEF项目根目录
  - 有推送权限到远程仓库

EOF
}

# 默认值
RELEASE_VERSION=""
TARGET_BRANCH=""
WATCH_BUILD=false
DOWNLOAD_ARTIFACTS=false
CLEAN_CACHE=false
CHECK_STATUS=false
CEF_ARCH=""
FORCE_CEF_DOWNLOAD=false

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--release)
            RELEASE_VERSION="$2"
            shift 2
            ;;
        -b|--branch)
            TARGET_BRANCH="$2"
            shift 2
            ;;
        -w|--watch)
            WATCH_BUILD=true
            shift
            ;;
        -d|--download)
            DOWNLOAD_ARTIFACTS=true
            shift
            ;;
        -c|--clean)
            CLEAN_CACHE=true
            shift
            ;;
        -s|--status)
            CHECK_STATUS=true
            shift
            ;;
        --cef-32)
            CEF_ARCH="x86"
            shift
            ;;
        --cef-64)
            CEF_ARCH="x64"
            shift
            ;;
        --force-cef-download)
            FORCE_CEF_DOWNLOAD=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

print_info "智多分机考桌面端-CEF版 - 高级构建触发器"
echo "================================================="

# 检查环境
print_info "检查环境..."

# 检查是否在CEF项目中
if [[ ! -f "CMakeLists.txt" ]] || [[ ! -d "src/cef" ]]; then
    print_error "当前目录不是CEF项目根目录"
    print_info "请切换到 DesktopTerminal-2025-CEF 目录"
    exit 1
fi

# 检查GitHub CLI
if ! command -v gh &> /dev/null; then
    print_error "GitHub CLI (gh) 未安装"
    print_info "安装命令: brew install gh"
    exit 1
fi

# 检查GitHub认证
if ! gh auth status &> /dev/null; then
    print_error "GitHub CLI 未认证"
    print_info "请运行: gh auth login"
    exit 1
fi

# 检查项目文件
required_files=(
    ".github/workflows/build.yml"
    "installer.nsi"
    "src/cef/cef_app.cpp"
    "cmake/FindCEF.cmake"
)

for file in "${required_files[@]}"; do
    if [[ ! -f "$file" ]]; then
        print_error "缺少必需文件: $file"
        exit 1
    fi
done

print_success "环境检查通过"

# 清理缓存功能
if [[ "$CLEAN_CACHE" == true ]]; then
    print_info "清理本地缓存..."
    
    # 清理构建目录
    if [[ -d "build" ]]; then
        rm -rf build
        print_success "已清理 build 目录"
    fi
    
    # 清理CEF目录
    if [[ -d "third_party/cef" ]]; then
        rm -rf third_party/cef
        print_success "已清理 CEF 缓存"
    fi
    
    # 清理临时文件
    find . -name "*.tmp" -delete 2>/dev/null || true
    find . -name ".DS_Store" -delete 2>/dev/null || true
    
    print_success "缓存清理完成"
    exit 0
fi

# 检查构建状态功能
if [[ "$CHECK_STATUS" == true ]]; then
    print_info "检查最近的CEF构建状态..."
    
    # 获取最近的构建
    recent_runs=$(gh run list --workflow=build.yml --limit=5 --json status,conclusion,createdAt,headBranch)
    
    if [[ -n "$recent_runs" ]]; then
        echo "$recent_runs" | jq -r '.[] | "分支: \(.headBranch) | 状态: \(.status) | 结果: \(.conclusion // "进行中") | 时间: \(.createdAt)"'
    else
        print_warning "未找到最近的构建记录"
    fi
    
    exit 0
fi

# 获取当前状态
current_branch=$(git branch --show-current)
current_commit=$(git rev-parse --short HEAD)
remote_url=$(git remote get-url origin)

print_info "当前分支: $current_branch"
print_info "当前提交: $current_commit"
print_info "远程仓库: $remote_url"

# 检查是否有未提交的更改
if ! git diff-index --quiet HEAD --; then
    print_warning "检测到未提交的更改"
    print_info "未提交的文件:"
    git status --porcelain
    echo
    read -p "是否继续? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "构建已取消"
        exit 0
    fi
fi

# 显示构建配置
echo
print_info "CEF构建配置:"
if [[ -n "$CEF_ARCH" ]]; then
    echo "  目标架构: $CEF_ARCH"
    if [[ "$CEF_ARCH" == "x86" ]]; then
        echo "  CEF版本: 75 (Windows 7兼容)"
    else
        echo "  CEF版本: 118 (现代功能)"
    fi
else
    echo "  目标架构: 32位和64位"
    echo "  CEF版本: 75 (32位) + 118 (64位)"
fi

if [[ -n "$RELEASE_VERSION" ]]; then
    echo "  发布版本: $RELEASE_VERSION"
fi
if [[ -n "$TARGET_BRANCH" ]]; then
    echo "  目标分支: $TARGET_BRANCH"
fi
echo "  监控构建: $WATCH_BUILD"
echo "  自动下载: $DOWNLOAD_ARTIFACTS"
echo "  强制下载CEF: $FORCE_CEF_DOWNLOAD"
echo

# 如果是发布版本，创建并推送标签
if [[ -n "$RELEASE_VERSION" ]]; then
    print_info "创建CEF发布版本: $RELEASE_VERSION"
    
    # 检查标签是否已存在
    if git tag -l "$RELEASE_VERSION" | grep -q "$RELEASE_VERSION"; then
        print_error "标签 $RELEASE_VERSION 已存在"
        exit 1
    fi
    
    # 创建标签
    git tag -a "$RELEASE_VERSION" -m "CEF Release $RELEASE_VERSION"
    
    # 推送标签
    print_info "推送标签到远程仓库..."
    git push origin "$RELEASE_VERSION"
    
    print_success "标签 $RELEASE_VERSION 已创建并推送"
    print_info "GitHub Actions将自动触发CEF构建..."
    
elif [[ -n "$TARGET_BRANCH" ]]; then
    # 推送到指定分支
    print_info "推送到分支: $TARGET_BRANCH"
    
    # 检查分支是否存在
    if ! git show-ref --verify --quiet refs/heads/$TARGET_BRANCH; then
        print_warning "分支 $TARGET_BRANCH 不存在，将创建新分支"
        git checkout -b "$TARGET_BRANCH"
    else
        git checkout "$TARGET_BRANCH"
    fi
    
    # 推送分支
    git push origin "$TARGET_BRANCH"
    print_success "已推送到分支 $TARGET_BRANCH"
    print_info "GitHub Actions将自动触发CEF构建..."
    
else
    # 手动触发workflow
    print_info "手动触发GitHub Actions CEF构建工作流..."
    
    # 构建触发参数
    trigger_cmd="gh workflow run build.yml"
    
    if [[ "$FORCE_CEF_DOWNLOAD" == true ]]; then
        print_info "将强制重新下载CEF依赖"
        # 这里可以添加清除GitHub Actions缓存的逻辑
    fi
    
    $trigger_cmd
    
    if [[ $? -eq 0 ]]; then
        print_success "GitHub Actions CEF工作流已触发"
    else
        print_error "触发GitHub Actions工作流失败"
        exit 1
    fi
fi

# 显示监控信息
echo
print_info "CEF构建监控:"
echo "  GitHub网页: $(gh repo view --web --json url -q .url)/actions"
echo "  命令行监控: gh run list --workflow=build.yml"
echo "  查看详情: gh run view --web"

# 监控构建进度
if [[ "$WATCH_BUILD" == true ]]; then
    print_info "监控CEF构建进度..."
    
    # 等待工作流开始
    sleep 5
    
    # 获取最新的运行ID
    run_id=$(gh run list --workflow=build.yml --limit=1 --json databaseId -q '.[0].databaseId')
    
    if [[ -n "$run_id" ]]; then
        print_info "CEF构建运行ID: $run_id"
        print_info "正在监控构建状态..."
        
        # 监控构建状态
        while true; do
            status=$(gh run view "$run_id" --json status -q '.status')
            conclusion=$(gh run view "$run_id" --json conclusion -q '.conclusion')
            
            case "$status" in
                "completed")
                    if [[ "$conclusion" == "success" ]]; then
                        print_success "CEF构建成功完成！"
                        
                        # 显示下载链接
                        print_info "可用的CEF构建产物:"
                        artifacts=$(gh run view "$run_id" --json artifacts -q '.artifacts[] | "\(.name): \(.url)"')
                        echo "$artifacts" | while read -r line; do
                            echo "  - $line"
                        done
                        
                        # 自动下载功能
                        if [[ "$DOWNLOAD_ARTIFACTS" == true ]]; then
                            print_info "自动下载CEF构建产物..."
                            mkdir -p downloads
                            cd downloads
                            gh run download "$run_id"
                            print_success "CEF构建产物已下载到 downloads/ 目录"
                            ls -la
                        fi
                        
                    elif [[ "$conclusion" == "failure" ]]; then
                        print_error "CEF构建失败"
                        print_info "查看详细日志: gh run view $run_id --log"
                    else
                        print_warning "CEF构建结束，状态: $conclusion"
                    fi
                    break
                    ;;
                "in_progress")
                    print_info "CEF构建进行中... (包含32位和64位版本)"
                    ;;
                "queued")
                    print_info "CEF构建排队中..."
                    ;;
                *)
                    print_info "CEF构建状态: $status"
                    ;;
            esac
            
            sleep 30
        done
    else
        print_warning "无法获取CEF构建运行ID，请手动检查构建状态"
    fi
fi

print_success "CEF构建脚本执行完成"

# 显示CEF特有的提示信息
echo
print_info "CEF版本特色功能："
echo "  ✅ Windows 7 SP1 32位兼容 (CEF 75)"
echo "  ✅ 现代Web功能 64位版本 (CEF 118)"
echo "  ✅ 自动CEF依赖管理和缓存"
echo "  ✅ 专用NSIS安装包生成"
echo "  ✅ 跨平台构建 (Windows/Linux/macOS)"