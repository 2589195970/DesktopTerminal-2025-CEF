# FindCEF.cmake - CEF库查找模块
# 支持Windows 7 SP1 32位和现代64位系统

# 根据架构确定CEF版本和平台
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CEF_PLATFORM_SUFFIX "windows64")
    set(CEF_VERSION_DEFAULT "118.6.8+g1e19f4c+chromium-118.0.5993.119")
else()
    set(CEF_PLATFORM_SUFFIX "windows32")
    set(CEF_VERSION_DEFAULT "75.1.16+g16a67c4+chromium-75.0.3770.100")
endif()

# macOS平台检测
if(APPLE)
    set(CEF_PLATFORM_SUFFIX "macosx64")
    set(CEF_VERSION_DEFAULT "118.6.8+g1e19f4c+chromium-118.0.5993.119")
endif()

# Linux平台检测
if(UNIX AND NOT APPLE)
    set(CEF_PLATFORM_SUFFIX "linux64")
    set(CEF_VERSION_DEFAULT "118.6.8+g1e19f4c+chromium-118.0.5993.119")
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

# 查找CEF根目录
find_path(CEF_ROOT_DIR
    NAMES include/cef_version.h
    PATHS ${CEF_ROOT_PATHS}
    NO_DEFAULT_PATH
)

if(NOT CEF_ROOT_DIR)
    message(STATUS "CEF未找到，将在构建时自动下载")
    set(CEF_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/${CEF_BINARY_NAME}")
endif()

# 设置CEF路径
set(CEF_INCLUDE_PATH "${CEF_ROOT_DIR}/include")
set(CEF_BINARY_DIR "${CEF_ROOT_DIR}")

# 平台特定的库和资源路径
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
        set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    else()
        set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
        set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    endif()
    set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Resources")
elseif(APPLE)
    set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Resources")
else() # Linux
    set(CEF_LIBRARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_BINARY_DIR "${CEF_ROOT_DIR}/Release")
    set(CEF_RESOURCE_DIR "${CEF_ROOT_DIR}/Resources")
endif()

# 查找CEF库文件
if(WIN32)
    find_library(CEF_LIBRARY
        NAMES cef libcef
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(CEF_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper libcef_dll_wrapper
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
elseif(APPLE)
    find_library(CEF_LIBRARY
        NAMES "Chromium Embedded Framework"
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(CEF_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
else() # Linux
    find_library(CEF_LIBRARY
        NAMES cef
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(CEF_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper
        PATHS ${CEF_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
endif()

# 设置CEF库列表
set(CEF_LIBRARIES)
if(CEF_LIBRARY)
    list(APPEND CEF_LIBRARIES ${CEF_LIBRARY})
endif()
if(CEF_WRAPPER_LIBRARY)
    list(APPEND CEF_LIBRARIES ${CEF_WRAPPER_LIBRARY})
endif()

# 检查是否找到了必要的组件
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CEF
    REQUIRED_VARS CEF_INCLUDE_PATH CEF_LIBRARIES CEF_BINARY_DIR CEF_RESOURCE_DIR
    VERSION_VAR CEF_VERSION
)

if(CEF_FOUND)
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
    message(WARNING "CEF未找到，请运行下载脚本或手动安装CEF")
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