#!/bin/bash

# ===============================================
# macOS下触发Windows构建的脚本
# ===============================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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
智多分机考桌面端 - Windows构建触发脚本

用法: $0 [选项]

选项:
  -t, --type TYPE          构建类型 (Release|Debug, 默认: Release)
  -i, --installer         生成安装包 (默认: true)
  -u, --upload           上传构建产物 (默认: true)
  -r, --release VERSION   创建发布版本 (例如: v1.0.0)
  -b, --branch BRANCH     推送到指定分支触发构建
  -h, --help             显示此帮助信息

示例:
  $0                                    # 使用默认设置触发构建
  $0 -t Debug                          # 触发Debug构建
  $0 -r v1.0.0                        # 创建发布版本
  $0 -b release/v1.0.0 -i -u          # 推送到发布分支并生成安装包

前置要求:
  - GitHub CLI (gh) 已安装并已认证
  - 当前目录为项目根目录
  - 有推送权限到远程仓库

EOF
}

# 默认值
BUILD_TYPE="Release"
CREATE_INSTALLER="true"
UPLOAD_ARTIFACTS="true"
RELEASE_VERSION=""
TARGET_BRANCH=""

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -i|--installer)
            CREATE_INSTALLER="true"
            shift
            ;;
        --no-installer)
            CREATE_INSTALLER="false"
            shift
            ;;
        -u|--upload)
            UPLOAD_ARTIFACTS="true"
            shift
            ;;
        --no-upload)
            UPLOAD_ARTIFACTS="false"
            shift
            ;;
        -r|--release)
            RELEASE_VERSION="$2"
            shift 2
            ;;
        -b|--branch)
            TARGET_BRANCH="$2"
            shift 2
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

# 验证构建类型
if [[ "$BUILD_TYPE" != "Release" && "$BUILD_TYPE" != "Debug" ]]; then
    print_error "无效的构建类型: $BUILD_TYPE (应该是 Release 或 Debug)"
    exit 1
fi

print_info "智多分机考桌面端 - Windows构建触发器"
echo "========================================"

# 检查环境
print_info "检查环境..."

# 检查是否在Git仓库中
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    print_error "当前目录不是Git仓库"
    exit 1
fi

# 检查GitHub CLI
if ! command -v gh &> /dev/null; then
    print_error "GitHub CLI (gh) 未安装"
    print_info "请访问 https://cli.github.com/ 安装"
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
    ".github/workflows/build-windows.yml"
    "zdf-exam-desktop/CMakeLists.txt"
    "installer.nsi"
)

for file in "${required_files[@]}"; do
    if [[ ! -f "$file" ]]; then
        print_error "缺少必需文件: $file"
        exit 1
    fi
done

print_success "环境检查通过"

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
print_info "构建配置:"
echo "  构建类型: $BUILD_TYPE"
echo "  生成安装包: $CREATE_INSTALLER"
echo "  上传产物: $UPLOAD_ARTIFACTS"
if [[ -n "$RELEASE_VERSION" ]]; then
    echo "  发布版本: $RELEASE_VERSION"
fi
if [[ -n "$TARGET_BRANCH" ]]; then
    echo "  目标分支: $TARGET_BRANCH"
fi
echo

# 如果是发布版本，创建并推送标签
if [[ -n "$RELEASE_VERSION" ]]; then
    print_info "创建发布版本: $RELEASE_VERSION"
    
    # 检查标签是否已存在
    if git tag -l "$RELEASE_VERSION" | grep -q "$RELEASE_VERSION"; then
        print_error "标签 $RELEASE_VERSION 已存在"
        exit 1
    fi
    
    # 创建标签
    git tag -a "$RELEASE_VERSION" -m "Release $RELEASE_VERSION"
    
    # 推送标签
    print_info "推送标签到远程仓库..."
    git push origin "$RELEASE_VERSION"
    
    print_success "标签 $RELEASE_VERSION 已创建并推送"
    print_info "GitHub Actions将自动触发构建..."
    
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
    print_info "GitHub Actions将自动触发构建..."
    
else
    # 手动触发workflow
    print_info "手动触发GitHub Actions工作流..."
    
    gh workflow run "build-windows.yml" \
        --field build_type="$BUILD_TYPE" \
        --field create_installer="$CREATE_INSTALLER" \
        --field upload_artifacts="$UPLOAD_ARTIFACTS"
    
    if [[ $? -eq 0 ]]; then
        print_success "GitHub Actions工作流已触发"
    else
        print_error "触发GitHub Actions工作流失败"
        exit 1
    fi
fi

# 显示监控信息
echo
print_info "监控构建进度:"
echo "  GitHub网页: $(gh repo view --web --json url -q .url)/actions"
echo "  命令行监控: gh run list --workflow=build-windows.yml"
echo "  查看详情: gh run view --web"

# 等待并监控构建（可选）
echo
read -p "是否监控构建进度? (y/N): " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    print_info "监控构建进度..."
    
    # 等待工作流开始
    sleep 5
    
    # 获取最新的运行ID
    run_id=$(gh run list --workflow=build-windows.yml --limit=1 --json databaseId -q '.[0].databaseId')
    
    if [[ -n "$run_id" ]]; then
        print_info "构建运行ID: $run_id"
        print_info "正在监控构建状态..."
        
        # 监控构建状态
        while true; do
            status=$(gh run view "$run_id" --json status -q '.status')
            conclusion=$(gh run view "$run_id" --json conclusion -q '.conclusion')
            
            case "$status" in
                "completed")
                    if [[ "$conclusion" == "success" ]]; then
                        print_success "构建成功完成！"
                        
                        # 显示下载链接
                        print_info "下载构建产物:"
                        gh run view "$run_id" --json artifacts -q '.artifacts[] | "  - \(.name): \(.downloadUrl)"'
                        
                    elif [[ "$conclusion" == "failure" ]]; then
                        print_error "构建失败"
                        print_info "查看详细日志: gh run view $run_id --log"
                    else
                        print_warning "构建结束，状态: $conclusion"
                    fi
                    break
                    ;;
                "in_progress")
                    print_info "构建进行中..."
                    ;;
                "queued")
                    print_info "构建排队中..."
                    ;;
                *)
                    print_info "构建状态: $status"
                    ;;
            esac
            
            sleep 30
        done
    else
        print_warning "无法获取构建运行ID，请手动检查构建状态"
    fi
fi

print_success "脚本执行完成"