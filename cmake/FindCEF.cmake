# FindCEF.cmake - CEF库查找模块
# 全平台统一使用CEF 75以获得最大兼容性

# 统一CEF版本 - 所有平台使用CEF 75
set(CEF_VERSION_DEFAULT "75.1.14+gc81164e+chromium-75.0.3770.100")

# 根据平台确定CEF平台后缀
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CEF_PLATFORM_SUFFIX "windows64")
    else()
        set(CEF_PLATFORM_SUFFIX "windows32")
    endif()
endif()

# macOS平台检测
if(APPLE)
    set(CEF_PLATFORM_SUFFIX "macosx64")
endif()

# Linux平台检测
if(UNIX AND NOT APPLE)
    set(CEF_PLATFORM_SUFFIX "linux64")
endif()

# 使用传入的版本或默认版本
if(NOT CEF_VERSION)
    set(CEF_VERSION ${CEF_VERSION_DEFAULT})
endif()

# 构建CEF二进制包名称
set(CEF_BINARY_NAME "cef_binary_${CEF_VERSION}_${CEF_PLATFORM_SUFFIX}")

# CEF根目录查找路径
set(CEF_ROOT_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/${CEF_BINARY_NAME}"
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/cef"
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/cef/${CEF_BINARY_NAME}"
    "$ENV{CEF_ROOT}"
)

# 查找CEF根目录 - 支持多种目录结构
find_path(CEF_ROOT_DIR
    NAMES include/cef_version.h cef_version.h
    PATHS ${CEF_ROOT_PATHS}
    NO_DEFAULT_PATH
)

# 如果找不到，尝试在CEF二进制目录中查找
if(NOT CEF_ROOT_DIR)
    find_path(CEF_ROOT_DIR
        NAMES include/cef_version.h
        PATHS 
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/${CEF_BINARY_NAME}"
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/cef/${CEF_BINARY_NAME}"
        NO_DEFAULT_PATH
    )
endif()

if(NOT CEF_ROOT_DIR)
    message(STATUS "CEF未找到，将在构建时自动下载")
    set(CEF_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/${CEF_BINARY_NAME}")
endif()

# 设置CEF路径
set(CEF_INCLUDE_PATH "${CEF_ROOT_DIR}")
set(CEF_BINARY_DIR "${CEF_ROOT_DIR}")

# 平台特定的库和资源路径
if(WIN32)
    set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    # Windows可能有独立的Resources目录或在Release中
    if(EXISTS "${CEF_ROOT_DIR}/Resources")
        set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Resources")
    else()
        set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Release")
    endif()
elseif(APPLE)
    set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    # macOS的Resources在Framework内部
    set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Release/Chromium Embedded Framework.framework/Resources")
    # macOS使用Framework路径
    set(CEF_FRAMEWORK_PATH "${CEF_ROOT_DIR}/Release/Chromium Embedded Framework.framework")
else() # Linux
    set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    # Linux检查Resources目录是否存在
    if(EXISTS "${CEF_ROOT_DIR}/Resources")
        set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Resources")
    else()
        set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Release")
    endif()
endif()

# 查找CEF库文件 - 增强调试信息
if(WIN32)
    message(STATUS "查找CEF库文件在目录: ${CEF_LIBRARY_DIR}")
    
    # 检查库目录是否存在
    if(EXISTS "${CEF_LIBRARY_DIR}")
        message(STATUS "CEF库目录存在，列出内容:")
        file(GLOB CEF_LIB_FILES "${CEF_LIBRARY_DIR}/*.lib")
        foreach(lib_file ${CEF_LIB_FILES})
            message(STATUS "  找到库文件: ${lib_file}")
        endforeach()
    else()
        message(WARNING "CEF库目录不存在: ${CEF_LIBRARY_DIR}")
    endif()
    
    # 首先尝试标准名称和路径 - 增强Windows搜索
    message(STATUS "Windows CEF库搜索 - CEF_LIBRARY_DIR: ${CEF_LIBRARY_DIR}")
    message(STATUS "Windows CEF库搜索 - CEF_ROOT_DIR: ${CEF_ROOT_DIR}")
    find_library(CEF_LIBRARY
        NAMES libcef cef
        PATHS 
            ${CEF_LIBRARY_DIR}
            "${CEF_ROOT_DIR}/Release"
            "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/Release"
        NO_DEFAULT_PATH
    )
    
    if(CEF_LIBRARY)
        message(STATUS "✓ 找到CEF主库: ${CEF_LIBRARY}")
    else()
        message(WARNING "✗ CEF主库未找到，搜索路径: ${CEF_LIBRARY_DIR}")
        message(STATUS "尝试扩展搜索...")
        message(STATUS "CEF_BINARY_NAME: ${CEF_BINARY_NAME}")
        
        # 尝试其他可能的路径和名称 - 优先搜索具体二进制目录
        find_library(CEF_LIBRARY
            NAMES libcef cef
            PATHS 
                "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/Release"
                "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/Debug"
                "${CEF_ROOT_DIR}/Release"
                "${CEF_ROOT_DIR}/Debug"
                "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/lib"
                "${CEF_ROOT_DIR}/lib"
            NO_DEFAULT_PATH
        )
        
        # 如果仍未找到，列出实际存在的库文件用于诊断
        if(NOT CEF_LIBRARY)
            message(STATUS "=== CEF库诊断信息 ===")
            file(GLOB_RECURSE ALL_CEF_LIBS "${CEF_ROOT_DIR}/**/*.lib")
            foreach(lib_file ${ALL_CEF_LIBS})
                message(STATUS "发现库文件: ${lib_file}")
            endforeach()
        endif()
        if(CEF_LIBRARY)
            message(STATUS "✓ 在备用路径找到CEF主库: ${CEF_LIBRARY}")
        else()
            # 最后尝试: 手动查找所有.lib文件
            message(STATUS "执行深度搜索CEF库文件...")
            file(GLOB_RECURSE ALL_CEF_LIBS 
                "${CEF_ROOT_DIR}/*.lib"
                "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/*.lib"
            )
            foreach(lib_file ${ALL_CEF_LIBS})
                get_filename_component(lib_name "${lib_file}" NAME_WE)
                message(STATUS "  发现库文件: ${lib_name} -> ${lib_file}")
                if(lib_name MATCHES "cef" OR lib_name MATCHES "libcef")
                    set(CEF_LIBRARY "${lib_file}")
                    message(STATUS "✓ 选择CEF库文件: ${CEF_LIBRARY}")
                    break()
                endif()
            endforeach()
        endif()
    endif()
    
    find_library(CEF_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper libcef_dll_wrapper
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    if(CEF_WRAPPER_LIBRARY)
        message(STATUS "✓ 找到CEF Wrapper库: ${CEF_WRAPPER_LIBRARY}")
    else()
        message(WARNING "✗ CEF Wrapper库未找到，将从源码编译")
        
        # Windows平台也需要查找wrapper源码目录
        if(EXISTS "${CEF_ROOT_DIR}/libcef_dll")
            set(CEF_WRAPPER_SOURCE_DIR "${CEF_ROOT_DIR}/libcef_dll")
            message(STATUS "找到CEF Wrapper源码: ${CEF_WRAPPER_SOURCE_DIR}")
        endif()
    endif()
elseif(APPLE)
    # macOS使用Framework路径
    if(EXISTS "${CEF_FRAMEWORK_PATH}")
        set(CEF_LIBRARY "${CEF_FRAMEWORK_PATH}")
        message(STATUS "找到CEF Framework: ${CEF_LIBRARY}")
    else()
        find_library(CEF_LIBRARY
            NAMES "Chromium Embedded Framework"
            PATHS ${CEF_LIBRARY_DIR}
            NO_DEFAULT_PATH
        )
    endif()
    
    # 查找编译好的wrapper库或静态库
    find_library(CEF_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper
        PATHS ${CEF_LIBRARY_DIR} "${CEF_ROOT_DIR}/build/libcef_dll_wrapper"
        NO_DEFAULT_PATH
    )
    
    # 如果没找到编译好的，查找源码目录
    if(NOT CEF_WRAPPER_LIBRARY)
        if(EXISTS "${CEF_ROOT_DIR}/libcef_dll")
            set(CEF_WRAPPER_SOURCE_DIR "${CEF_ROOT_DIR}/libcef_dll")
            message(STATUS "找到CEF Wrapper源码: ${CEF_WRAPPER_SOURCE_DIR}")
        endif()
    endif()
else() # Linux
    find_library(CEF_LIBRARY
        NAMES cef libcef
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(CEF_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper
        PATHS ${CEF_LIBRARY_DIR} "${CEF_ROOT_DIR}/build/libcef_dll_wrapper"
        NO_DEFAULT_PATH
    )
    
    # 如果没找到编译好的，查找源码目录
    if(NOT CEF_WRAPPER_LIBRARY)
        if(EXISTS "${CEF_ROOT_DIR}/libcef_dll")
            set(CEF_WRAPPER_SOURCE_DIR "${CEF_ROOT_DIR}/libcef_dll")
            message(STATUS "找到CEF Wrapper源码: ${CEF_WRAPPER_SOURCE_DIR}")
        endif()
    endif()
endif()

# 设置CEF库列表
set(CEF_LIBRARIES)
if(CEF_LIBRARY)
    list(APPEND CEF_LIBRARIES ${CEF_LIBRARY})
endif()
if(CEF_WRAPPER_LIBRARY)
    list(APPEND CEF_LIBRARIES ${CEF_WRAPPER_LIBRARY})
endif()

# 查找额外的CEF库文件
if(APPLE OR UNIX)
    # 查找sandbox库（可选）
    find_library(CEF_SANDBOX_LIBRARY
        NAMES cef_sandbox
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    if(CEF_SANDBOX_LIBRARY)
        list(APPEND CEF_LIBRARIES ${CEF_SANDBOX_LIBRARY})
        message(STATUS "找到CEF Sandbox库: ${CEF_SANDBOX_LIBRARY}")
    endif()
endif()

# 如果没有找到wrapper库但有源码，记录信息
if(NOT CEF_WRAPPER_LIBRARY AND CEF_WRAPPER_SOURCE_DIR)
    message(STATUS "CEF Wrapper库未编译，源码目录: ${CEF_WRAPPER_SOURCE_DIR}")
    message(STATUS "将在构建时编译CEF Wrapper")
endif()

# 检查是否找到了必要的组件
include(FindPackageHandleStandardArgs)

# CI环境中的特殊处理 - 确保CEF确实存在才通过验证
if(DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{CI})
    message(STATUS "CI环境检测：验证CEF文件完整性")
    
    # 支持多种路径结构的CEF头文件验证
    set(CEF_HEADER_NAMES "cef_version.h" "cef_app.h" "cef_client.h")
    set(CEF_HEADERS_FOUND 0)
    
    foreach(header_name ${CEF_HEADER_NAMES})
        set(HEADER_FOUND FALSE)
        
        # 检查多个可能的路径
        set(POSSIBLE_PATHS
            "${CEF_INCLUDE_PATH}/include/${header_name}"
            "${CEF_ROOT_DIR}/include/${header_name}"
            "${CEF_ROOT_DIR}/${header_name}"
        )
        
        foreach(path ${POSSIBLE_PATHS})
            if(NOT HEADER_FOUND AND EXISTS "${path}")
                message(STATUS "✓ 找到CEF头文件: ${header_name} at ${path}")
                set(HEADER_FOUND TRUE)
                math(EXPR CEF_HEADERS_FOUND "${CEF_HEADERS_FOUND} + 1")
            endif()
        endforeach()
        
        if(NOT HEADER_FOUND)
            message(WARNING "CEF关键头文件缺失: ${header_name}")
        endif()
    endforeach()
    
    # 至少需要找到2个头文件才认为CEF完整
    if(CEF_HEADERS_FOUND LESS 2)
        message(STATUS "CEF头文件不完整 (${CEF_HEADERS_FOUND}/3)，将触发下载流程")
        # 清空库列表，让find_package_handle_standard_args失败
        set(CEF_LIBRARIES)
    else()
        message(STATUS "CEF头文件验证通过 (${CEF_HEADERS_FOUND}/3)")
    endif()
endif()

find_package_handle_standard_args(CEF
    REQUIRED_VARS CEF_INCLUDE_PATH CEF_LIBRARIES CEF_BINARY_DIR CEF_RESOURCE_DIR
    VERSION_VAR CEF_VERSION
)

if(CEF_FOUND)
    # 确保CEF_ROOT变量被正确设置，供其他脚本使用
    set(CEF_ROOT "${CEF_ROOT_DIR}")
    
    message(STATUS "找到CEF: ${CEF_ROOT_DIR}")
    message(STATUS "CEF版本: ${CEF_VERSION}")
    message(STATUS "CEF平台: ${CEF_PLATFORM_SUFFIX}")
    message(STATUS "CEF库: ${CEF_LIBRARIES}")
    
    # 设置CEF编译定义
    add_definitions(-DCEF_USE_SANDBOX=0)
    
    # 32位Windows特殊设置
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_definitions(-DCEF_32BIT_BUILD=1)
        message(STATUS "启用32位CEF构建优化")
    endif()
else()
    message(FATAL_ERROR "
    ❌ CEF未找到或不完整！
    
    📋 解决方案：
    1. 自动下载 (推荐)：
       Windows: scripts/download-cef.bat
       Linux/macOS: scripts/download-cef.sh
    
    2. 手动安装：
       - 下载CEF版本: ${CEF_VERSION}
       - 平台: ${CEF_PLATFORM_SUFFIX}
       - 解压到: ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/
    
    3. 在CI环境中：
       确保CEF下载步骤正确执行且文件完整
    
    🔍 当前配置：
    - CEF根目录: ${CEF_ROOT_DIR}
    - 预期头文件: ${CEF_INCLUDE_PATH}/cef_version.h
    - 预期库目录: ${CEF_LIBRARY_DIR}
    
    请运行相应的下载脚本后重新构建项目。
    ")
endif()

# 标记变量为高级变量（在cmake-gui中隐藏）
mark_as_advanced(
    CEF_ROOT_DIR
    CEF_INCLUDE_PATH
    CEF_LIBRARY_DIR
    CEF_BINARY_DIR
    CEF_RESOURCE_DIR
    CEF_LIBRARY
    CEF_WRAPPER_LIBRARY
)