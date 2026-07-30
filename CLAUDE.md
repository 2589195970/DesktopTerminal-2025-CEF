# CLAUDE.md
# Repository Guidelines

## 必须遵循的规范
1.必须中文回答问题
2.禁止各类icon,emoji,表情插入到代码和注释当中来
3.必须使用UTF-8字符集来回答

# AI Coding Assistant Guide v6.0 - CoT-AoT Workflow Edition

## 🎯 核心认知模型：CoT-AoT 双阶段工作流

### **阶段1: CoT (Chain of Thought) - 问题定义**

> 💡 **核心原则**: 我的首要任务不是解决问题，而是**定义问题**


graph LR
A[用户输入] --> B[问题识别]
B --> C[边界定义]
C --> D[约束提取]
D --> E[理解验证]
E --> F[问题定义完成]


### **阶段2: AoT (Algorithm of Thoughts) - 算法化求解**

> 🔧 **核心原则**: 一旦问题被清晰定义，我将问题视为一个算法挑战，系统性地分解、执行和验证


graph TD
A[定义的问题] --> B[算法分解]
B --> C[子任务生成]
C --> D[约束验证]
D --> E{满足约束?}
E -->|是| F[执行任务]
E -->|否| G[回溯优化]
G --> B
F --> H[验证结果]


## 📐 三大核心原则

### **1️⃣ Why-How-Done 哲学原则**


Why (为什么做):
- 功能完整性优先
- 用户价值导向
- 问题本质理解
  How (如何做):
- 80%实用主义
- 渐进式交付
- 最简有效方案
  Done (完成标准):
- 客观可验证
- 满足验收标准
- 可复现结果


### **2️⃣ 工程原则优先级**

1. **YAGNI** (You Aren't Gonna Need It)

    - 不写当前不需要的代码

    - 避免过度设计

2. **KISS** (Keep It Simple, Stupid)

    - 选择最简单直接的方案

    - 降低复杂度

3. **DRY** (Don't Repeat Yourself)

    - 单一权威知识源

    - 消除重复


### **3️⃣ 执行原则**

- **原子任务原则**: 任务分解到不可再分

- **持续验证原则**: 每步都要验证

- **工具增强原则**: 充分利用工具能力


## 🔄 CoT-AoT 驱动的工作流程

### **Phase 1: CoT 问题定义阶段**


class CoT_ProblemDefinition:

    链式思考：深度理解并定义问题
    重点：定义问题，而非解决问题

    def execute(self, user_input):
        # Step 1: 识别核心问题
        core_problem = self.identify_core_problem(user_input)
        # Step 2: 提取约束和边界
        constraints = self.extract_constraints(user_input)
        boundaries = self.define_boundaries(user_input)
        # Step 3: 工具增强理解
        enhanced_context = self.enhance_with_tools(core_problem)
        # Step 4: 验证理解
        problem_definition = {
            "problem": core_problem,
            "constraints": constraints,
            "boundaries": boundaries,
            "context": enhanced_context,
            "assumptions": self.list_assumptions(),
            "success_criteria": self.define_success_criteria()
        }
        return self.validate_definition(problem_definition)

    def enhance_with_tools(self, problem):
        使用工具增强问题理解
        context = {}
        # 使用 code-reasoning 规划任务
        context['plan'] = code_reasoning.plan_and_decompose(problem)
        # 使用 Context7 获取相关文档
        context['docs'] = self.get_documentation(problem)
        # 使用 Serena 分析现有代码
        context['code'] = self.analyze_existing_code(problem)
        return context


### **Phase 2: AoT 算法化分解阶段**


class AoT_AlgorithmicDecomposition:

    算法思考：将定义的问题作为算法挑战
    系统性分解、执行和验证

    def execute(self, problem_definition):
        # 初始化算法树
        algorithm_tree = self.initialize_tree(problem_definition)
        # DFS 分解策略
        atomic_tasks = self.dfs_decompose(algorithm_tree.root)
        # 优化任务序列
        optimized_sequence = self.optimize_sequence(atomic_tasks)
        # 验证可执行性
        return self.verify_executability(optimized_sequence)

    def dfs_decompose(self, node, depth=0):
        深度优先搜索分解
        # 终止条件：原子任务
        if self.is_atomic(node):
            return [self.create_atomic_task(node)]

        # 应用启发式规则
        heuristics = {
            "h1": "prefer_simple_solution",
            "h2": "minimize_dependencies",
            "h3": "maximize_testability"
        }

        # 分解当前节点
        sub_problems = self.apply_heuristics(node, heuristics)

        # 递归分解
        tasks = []
        for sub in sub_problems:
            if self.satisfies_constraints(sub):
                tasks.extend(self.dfs_decompose(sub, depth + 1))
            else:
                # 回溯机制
                self.backtrack_and_refine(sub)
        return tasks


### **Phase 3: 执行与验证阶段**


class ExecutionWithTools:
工具增强的执行阶段
def execute_atomic_task(self, task):
result = {
"task_id": task.id,
"status": "INITIATED",
"changes": []
}

        # 1. 使用 code-reasoning 制定执行计划
        plan = self.create_execution_plan(task)
        # 2. 使用 Filesystem 工具读取必要文件
        files_content = self.read_required_files(task)
        # 3. 使用 Serena 进行代码修改
        code_changes = self.apply_code_changes(task, files_content)
        # 4. 验证修改
        validation = self.validate_changes(code_changes)

        result["status"] = "COMPLETED" if validation.passed else "FAILED"
        result["changes"] = code_changes
        return result


## 🛠️ 工具使用策略

### **1. Code-Reasoning 工具**


用途: 规划与分解复杂任务，形成可执行计划与里程碑
触发时机:
- CoT阶段: 理解问题复杂度
- AoT阶段: 生成分解策略
- 执行阶段: 制定执行计划
  使用模式:
- 最多3轮迭代
- 每轮聚焦特定方面
- 输出必须包含里程碑


### **2. Filesystem 工具**


read_file:
用途: 读取本地文件或URL内容
参数:
- path: 文件路径或URL
- offset: 起始行（支持负数）
- length: 读取行数
  read_multiple_files:
  用途: 同时读取多个文件
  场景:
- 分析相关模块
- 对比实现方案
- 收集上下文信息


### **3. Context7 文档聚合**


触发条件:
- 查询SDK/API/框架文档
- 需要官方示例
- 参数规范确认
  工作流程:
1. resolve-library-id: 确认相关库
2. get-library-docs: 获取文档
3. 筛选和提炼: 聚焦相关内容
   输出要求:
- 精炼答案 + 引用链接
- 标注库ID/版本
- 关键片段摘要
  限制:
- tokens默认5000
- 避免大段复制
- 遵守引用规范


### **4. Serena 语义检索/编辑**


核心能力:
- 基于LSP的符号级检索
- 跨文件引用分析
- 精准代码编辑
  常用工具集:
  查找类:
    - find_symbol
    - find_referencing_symbols
    - get_symbols_overview
    - search_for_pattern
      编辑类:
    - insert_before_symbol
    - insert_after_symbol
    - replace_symbol_body
    - replace_regex
      文件操作:
    - find_file
    - read_file
    - create_text_file
    - write_file
      使用策略:
- 优先小范围精准操作
- 单轮单工具原则
- 输出带符号定位和理由


## 📋 任务文档模板

### **CoT-AoT 输出文档**


# 任务: ${TASK_NAME}

## 📍 CoT: 问题定义

### 核心问题
- **原始需求**: [用户输入重述]
- **问题本质**: [识别的根本问题]
- **成功标准**: [明确的验收条件]

### 约束与边界
- **硬约束**: [不可违反的约束]
- **软约束**: [可协商的约束]
- **边界条件**: [范围定义]

### 工具增强上下文
- **code-reasoning分析**: [复杂度评估]
- **Context7文档**: [相关API/框架知识]
- **Serena代码分析**: [现有实现情况]

## 🔄 AoT: 算法分解

### 分解树


根问题

├── 子问题1 [使用工具: code-reasoning]

│ ├── 原子任务1.1 [使用工具: Serena]

│ └── 原子任务1.2 [使用工具: Filesystem]

└── 子问题2 [使用工具: Context7]

└── 原子任务2.1 [使用工具: Serena]



### 执行序列
| 序号 | 任务ID | 描述 | 工具 | 依赖 |
|---|---|---|---|---|
| 1 | AT-1.1 | [描述] | Serena | None |
| 2 | AT-1.2 | [描述] | Filesystem | AT-1.1 |
| 3 | AT-2.1 | [描述] | Serena | AT-1.2 |

### 启发式决策
- **H1(简单性)**: [选择理由]
- **H2(依赖性)**: [优化策略]
- **H3(可测试性)**: [验证方案]


### **执行跟踪文档**


# 执行任务: ${TASK_ID}

## 状态追踪
- [ ] TODO
- [ ] DOING
- [x] DONE

## 工具使用记录

### Code-Reasoning
- **调用时机**: [问题分析阶段]
- **输出**: [计划和里程碑]

### Filesystem
- **读取文件**:
    - `path/to/file1.kt` (lines 20-50)
    - `path/to/file2.kt` (全文)

### Context7
- **查询**: "React hooks best practices"
- **结果**: [文档摘要和链接]

### Serena
- **操作**: replace_symbol_body
- **目标**: ClassName.methodName
- **变更**: [具体修改内容]

## 验证清单
- [x] 代码编译通过
- [x] 单元测试通过
- [x] 满足约束条件
- [x] 性能基准达标


## 🔁 执行循环实现


def main_workflow(user_input):

    CoT-AoT 主工作流
    强调：先定义问题，再算法化求解

    # Phase 1: CoT - 定义问题（不是解决问题）
    cot = CoT_ProblemDefinition()
    problem_definition = cot.execute(user_input)

    # 使用工具验证问题定义
    validation = code_reasoning.validate_problem_definition(problem_definition)
    if not validation.is_valid:
        return cot.refine_definition(problem_definition, validation.feedback)

    # Phase 2: AoT - 算法化分解
    aot = AoT_AlgorithmicDecomposition()
    atomic_tasks = aot.execute(problem_definition)

    # Phase 3: 工具增强执行
    executor = ExecutionWithTools()
    results = []
    for task in atomic_tasks:
        max_attempts = 3
        for attempt in range(max_attempts):
            try:
                result = executor.execute_atomic_task(task)
                if result["status"] == "COMPLETED":
                    results.append(result)
                    break
            except Exception as e:
                if attempt == max_attempts - 1:
                    # 回溯到AoT重新分解
                    atomic_tasks = aot.refine_decomposition(
                        problem_definition, failed_task=task, error=e
                    )
                    break
    return compile_results(results)


## 📊 监控与反馈


性能指标:
- CoT阶段耗时: <30s
- AoT分解深度: ≤5层
- 工具调用次数: 优化最小
- 回溯频率: <20%
  质量指标:
- 问题定义准确率: >95%
- 分解合理性: 满足MECE原则
- 执行成功率: >90%
- 约束满足度: 100%


**🎯 核心记忆点**:

1. **CoT = 定义问题**，不是解决问题

2. **AoT = 算法挑战**，系统性分解和验证

3. **工具 = 能力增强**，每个阶段都要充分利用


4. **三大原则贯穿始终**：Why-How-Done + YAGNI/KISS/DRY + 原子/验证/工具

## Project Overview

This is a Qt-based educational exam browser (机考霸屏桌面端) designed as a secure kiosk-style application using Qt WebEngine. It restricts user access to only specified educational websites and provides secure exit mechanisms for exam environments.

## Build Commands

### Prerequisites
- **Qt 5.15.2** with WebEngine module
- **CMake 3.14+**
- Platform-specific toolchain (MSVC for Windows, GCC for Linux, Clang for macOS)

### Quick Build
```bash
# Windows
build-windows.bat

# Linux (Kylin)
cd qt-shell && ./build-kylin.sh

# macOS
cd qt-shell && ./build-macos.sh
```

### Manual CMake Build
```bash
mkdir build && cd build
cmake ../qt-shell -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Testing
Build QHotkey test application:
```bash
cmake ../qt-shell -DQHOTKEY_EXAMPLES=ON
cmake --build . --target HotkeyTest
```

## Core Architecture

### Single-File Application Structure
The entire application logic is contained in `qt-shell/main.cpp` with three main classes:

1. **ConfigManager** (Singleton)
   - Loads configuration from `resources/config.json`
   - Supports multiple path fallbacks
   - Creates default config if none exists
   - Manages URL, exit password, and app name

2. **Logger** (Singleton)
   - UTF-8 encoded logging to `exit.log`
   - Categories: application events, hotkey attempts
   - UI helper methods for consistent encoding

3. **ShellBrowser** (QWebEngineView)
   - Fullscreen, frameless, always-on-top window
   - Global hotkey registration (F10, \ key)
   - Aggressive focus management with 500ms timer
   - Blocks all system shortcuts and right-click menus

### Key Security Features
- Restricts access to configured URL only
- Intercepts and blocks system shortcuts (Cmd+Tab, Cmd+Opt+Esc)
- Password-protected exit via global hotkeys
- Force-quit prevention through event filtering
- Automatic focus stealing every 500ms

### Configuration System
Config file location priority:
1. Provided path
2. Relative to application directory
3. Parent directory (for build environments)
4. Absolute path

Required config fields: `url`, `exitPassword`, `appName`

### Dependencies
- **Qt WebEngine**: Chromium-based web rendering
- **QHotkey**: Cross-platform global hotkey library (embedded in `qt-shell/QHotkey/`)
- Platform-specific hotkey backends (Win32, Carbon, X11)

## Development Notes

### Chinese Language Support
- UTF-8 encoding enforced throughout
- Chinese comments and UI text
- Locale-specific text codec setup in main()

### Build Artifacts
- Windows: Creates installer via NSIS (`installer.nsi`)
- macOS: Generates .app bundle and DMG
- Linux: Produces standalone executable

### Debugging
Application outputs current working directory and attempts multiple config paths with detailed logging to help with deployment issues.

## ⚠️ 关键配置信息 - CEF版本管理

### 已验证可用的CEF版本 (HTTP 200)

**重要：以下版本号已通过URL验证，禁止随意修改！**

| 平台 | CEF版本 | 下载URL | 文件大小 | 用途 |
|------|---------|---------|----------|------|
| Windows 32位 | `75.1.14+gc81164e+chromium-75.0.3770.100` | `https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_windows32.tar.bz2` | 178MB | 全平台统一版本 - 最大兼容性 |
| Windows 64位 | `75.1.14+gc81164e+chromium-75.0.3770.100` | `https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_windows64.tar.bz2` | 228MB | 全平台统一版本 - 最大兼容性 |
| Linux 64位 | `75.1.14+gc81164e+chromium-75.0.3770.100` | `https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_linux64.tar.bz2` | 412MB | 全平台统一版本 - 最大兼容性 |
| macOS 64位 | `75.1.14+gc81164e+chromium-75.0.3770.100` | `https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_macosx64.tar.bz2` | 156MB | 全平台统一版本 - 最大兼容性 |

### 配置文件一致性检查清单

**修改CEF版本时，必须同步更新以下所有文件：**

1. **`.github/workflows/build.yml`** - GitHub Actions构建配置
   ```yaml
   cef_version: "75.1.14+gc81164e+chromium-75.0.3770.100"  # 全平台统一版本
   ```

2. **`scripts/download-cef.bat`** - Windows下载脚本
   ```batch
   set "CEF_VERSION=75.1.14+gc81164e+chromium-75.0.3770.100"  # 全平台统一版本
   ```

3. **`scripts/download-cef.sh`** - Linux/macOS下载脚本
   ```bash
   CEF_VERSION="75.1.14+gc81164e+chromium-75.0.3770.100"  # 全平台统一版本
   ```

4. **`CMakeLists.txt`** - 主CMake配置
   ```cmake
   set(CEF_VERSION "75.1.14+gc81164e+chromium-75.0.3770.100")  # 全平台统一版本
   ```

5. **`cmake/FindCEF.cmake`** - ⚠️ 关键模块 - 覆盖主配置
   ```cmake
   set(CEF_VERSION_DEFAULT "75.1.14+gc81164e+chromium-75.0.3770.100")  # 全平台统一版本
   ```

### F12 DevTools 白屏问题

#### 问题描述
按F12输入密码后，DevTools窗口打开但显示白屏，无法正常使用。

#### 根本原因
这是CEF 75在单进程模式下的已知限制，DevTools窗口渲染需要时间。

#### 快速解决方案

**方法1：使用远程调试接口（推荐，100%有效）**
1. 启动应用后，在系统浏览器访问：`http://localhost:9222`
2. 点击显示的调试目标
3. 完整DevTools在浏览器中打开

**方法2：等待加载**
1. 按F12输入密码后保持窗口打开
2. 等待10-15秒，内容会逐渐显示

**方法3：使用诊断工具**
- Windows: `scripts\test-devtools.bat`
- Linux/macOS: `./scripts/test-devtools.sh`

#### 详细文档
- 用户指南：`docs/DEVTOOLS_GUIDE.md`
- 技术文档：`docs/DEVTOOLS_WHITESACREEN_FIX.md`

### 常见CEF构建问题解决方案

#### 问题1: CEF下载404错误
**症状**: `The remote server returned an error: (404) Not Found`
**原因**: CEF版本号错误或URL编码问题
**解决**: 
1. 使用上述已验证版本号
2. 确保URL编码：`+` → `%2B`
3. 验证URL: `curl -I "https://cef-builds.spotifycdn.com/cef_binary_[VERSION].tar.bz2"`

#### 问题2: CMake找不到CEF
**症状**: `Could NOT find CEF (missing: CEF_LIBRARIES)`
**原因**: FindCEF.cmake中版本号与实际不符，或验证路径不匹配实际目录结构
**解决**: 
1. 检查并同步所有配置文件中的版本号
2. 确保FindCEF.cmake支持多种CEF目录结构
3. 验证CEF解压后的实际目录结构

#### 问题3: Qt安装超时
**症状**: `The operation was canceled`
**原因**: qtwebengine模块下载过大
**解决**: 移除qtwebengine模块依赖（项目使用CEF替代）

#### 问题4: GitHub Actions构建失败
**症状**: artifact actions或Qt架构错误
**解决**: 
- 使用 `actions/upload-artifact@v4`
- 使用 `win32_msvc2019` 架构
- 移除 `modules: 'qtwebengine'`

#### 问题5: CEF解压后文件验证失败
**症状**: `CEF解压后文件不完整` 或 `CEF解压后未找到cef_version.h文件`
**原因**: 硬编码的验证路径不匹配CEF实际的目录结构
**解决**: 
1. 使用多路径验证逻辑支持不同的CEF目录结构
2. 添加详细的目录结构检查和错误诊断
3. 确保所有下载脚本(Windows/Linux/macOS)使用一致的验证逻辑

### URL编码规则
CEF下载需要URL编码特殊字符：
- `+` → `%2B`
- 示例: `75.1.14+gc81164e` → `75.1.14%2Bgc81164e`

### 版本验证命令
```bash
# 验证Windows 32位
curl -I "https://cef-builds.spotifycdn.com/cef_binary_75.1.14%2Bgc81164e%2Bchromium-75.0.3770.100_windows32.tar.bz2"

# 验证Windows 64位  
curl -I "https://cef-builds.spotifycdn.com/cef_binary_118.7.1%2Bg99817d2%2Bchromium-118.0.5993.119_windows64.tar.bz2"

# 预期结果: HTTP/2 200
```