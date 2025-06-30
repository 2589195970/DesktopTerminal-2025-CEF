# FindCEF109.cmake - CEF 109库查找模块（测试版本）
# 用于测试CEF 109.1.18+g97a8d9e+chromium-109.0.5414.120迁移

# CEF 109版本配置
set(CEF109_VERSION_DEFAULT "109.1.18+g97a8d9e+chromium-109.0.5414.120")

# 根据平台确定CEF平台后缀
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CEF109_PLATFORM_SUFFIX "windows64")
    else()
        set(CEF109_PLATFORM_SUFFIX "windows32")
    endif()
endif()

# macOS平台检测
if(APPLE)
    set(CEF109_PLATFORM_SUFFIX "macosx64")
endif()

# Linux平台检测
if(UNIX AND NOT APPLE)
    set(CEF109_PLATFORM_SUFFIX "linux64")
endif()

# 使用传入的版本或默认版本
if(NOT CEF109_VERSION)
    set(CEF109_VERSION ${CEF109_VERSION_DEFAULT})
endif()

# 构建CEF109二进制包名称
set(CEF109_BINARY_NAME "cef_binary_${CEF109_VERSION}_${CEF109_PLATFORM_SUFFIX}")

# CEF109根目录查找路径（独立于CEF75）
set(CEF109_ROOT_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef109"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef109/${CEF109_BINARY_NAME}"
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/cef109"
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/cef109/${CEF109_BINARY_NAME}"
    "$ENV{CEF109_ROOT}"
)

# 查找CEF109根目录 - 支持多种目录结构
find_path(CEF109_ROOT_DIR
    NAMES include/cef_version.h cef_version.h
    PATHS ${CEF109_ROOT_PATHS}
    NO_DEFAULT_PATH
)

# 如果找不到，设置预期路径
if(NOT CEF109_ROOT_DIR)
    message(STATUS "CEF109未找到，将在构建时自动下载")
    set(CEF109_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef109/${CEF109_BINARY_NAME}")
endif()

# 设置CEF109路径
set(CEF109_INCLUDE_PATH "${CEF109_ROOT_DIR}")
set(CEF109_BINARY_DIR "${CEF109_ROOT_DIR}")

# 平台特定的库和资源路径
if(WIN32)
    set(CEF109_LIBRARY_DIR "${CEF109_ROOT_DIR}/Release")
    set(CEF109_BINARY_DIR "${CEF109_ROOT_DIR}/Release")
    if(EXISTS "${CEF109_ROOT_DIR}/Resources")
        set(CEF109_RESOURCE_DIR "${CEF109_ROOT_DIR}/Resources")
    else()
        set(CEF109_RESOURCE_DIR "${CEF109_ROOT_DIR}/Release")
    endif()
elseif(APPLE)
    set(CEF109_LIBRARY_DIR "${CEF109_ROOT_DIR}/Release")
    set(CEF109_BINARY_DIR "${CEF109_ROOT_DIR}/Release")
    set(CEF109_RESOURCE_DIR "${CEF109_ROOT_DIR}/Release/Chromium Embedded Framework.framework/Resources")
    set(CEF109_FRAMEWORK_PATH "${CEF109_ROOT_DIR}/Release/Chromium Embedded Framework.framework")
else() # Linux
    set(CEF109_LIBRARY_DIR "${CEF109_ROOT_DIR}/Release")
    set(CEF109_BINARY_DIR "${CEF109_ROOT_DIR}/Release")
    if(EXISTS "${CEF109_ROOT_DIR}/Resources")
        set(CEF109_RESOURCE_DIR "${CEF109_ROOT_DIR}/Resources")
    else()
        set(CEF109_RESOURCE_DIR "${CEF109_ROOT_DIR}/Release")
    endif()
endif()

# 查找CEF109库文件
if(WIN32)
    message(STATUS "查找CEF109库文件在目录: ${CEF109_LIBRARY_DIR}")
    
    find_library(CEF109_LIBRARY
        NAMES libcef cef
        PATHS ${CEF109_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(CEF109_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper libcef_dll_wrapper
        PATHS ${CEF109_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    # 如果没找到wrapper库，查找源码目录
    if(NOT CEF109_WRAPPER_LIBRARY)
        if(EXISTS "${CEF109_ROOT_DIR}/libcef_dll")
            set(CEF109_WRAPPER_SOURCE_DIR "${CEF109_ROOT_DIR}/libcef_dll")
            message(STATUS "找到CEF109 Wrapper源码: ${CEF109_WRAPPER_SOURCE_DIR}")
        endif()
    endif()
elseif(APPLE)
    # macOS使用Framework路径
    if(EXISTS "${CEF109_FRAMEWORK_PATH}")
        set(CEF109_LIBRARY "${CEF109_FRAMEWORK_PATH}")
        message(STATUS "找到CEF109 Framework: ${CEF109_LIBRARY}")
    endif()
    
    # 查找wrapper库或源码
    find_library(CEF109_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper
        PATHS ${CEF109_LIBRARY_DIR} "${CEF109_ROOT_DIR}/build/libcef_dll_wrapper"
        NO_DEFAULT_PATH
    )
    
    if(NOT CEF109_WRAPPER_LIBRARY AND EXISTS "${CEF109_ROOT_DIR}/libcef_dll")
        set(CEF109_WRAPPER_SOURCE_DIR "${CEF109_ROOT_DIR}/libcef_dll")
    endif()
else() # Linux
    find_library(CEF109_LIBRARY
        NAMES cef libcef
        PATHS ${CEF109_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(CEF109_WRAPPER_LIBRARY
        NAMES cef_dll_wrapper
        PATHS ${CEF109_LIBRARY_DIR} "${CEF109_ROOT_DIR}/build/libcef_dll_wrapper"
        NO_DEFAULT_PATH
    )
    
    if(NOT CEF109_WRAPPER_LIBRARY AND EXISTS "${CEF109_ROOT_DIR}/libcef_dll")
        set(CEF109_WRAPPER_SOURCE_DIR "${CEF109_ROOT_DIR}/libcef_dll")
    endif()
endif()

# 设置CEF109库列表
set(CEF109_LIBRARIES)
if(CEF109_LIBRARY)
    list(APPEND CEF109_LIBRARIES ${CEF109_LIBRARY})
endif()
if(CEF109_WRAPPER_LIBRARY)
    list(APPEND CEF109_LIBRARIES ${CEF109_WRAPPER_LIBRARY})
endif()

# 检查是否找到了必要的组件
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(CEF109
    REQUIRED_VARS CEF109_INCLUDE_PATH CEF109_BINARY_DIR CEF109_RESOURCE_DIR
    VERSION_VAR CEF109_VERSION
)

if(CEF109_FOUND)
    set(CEF109_ROOT "${CEF109_ROOT_DIR}")
    
    message(STATUS "找到CEF109: ${CEF109_ROOT_DIR}")
    message(STATUS "CEF109版本: ${CEF109_VERSION}")
    message(STATUS "CEF109平台: ${CEF109_PLATFORM_SUFFIX}")
    message(STATUS "CEF109库: ${CEF109_LIBRARIES}")
    
    # 设置CEF109编译定义
    add_definitions(-DCEF109_USE_SANDBOX=0)
    add_definitions(-DCEF109_TESTING=1)
    
    # 32位Windows特殊设置
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_definitions(-DCEF109_32BIT_BUILD=1)
        message(STATUS "启用32位CEF109构建优化")
    endif()
else()
    message(STATUS "CEF109未找到 - 这是正常的，因为这是测试分支")
    message(STATUS "CEF109将在需要时下载和配置")
endif()

# 标记变量为高级变量
mark_as_advanced(
    CEF109_ROOT_DIR
    CEF109_INCLUDE_PATH
    CEF109_LIBRARY_DIR
    CEF109_BINARY_DIR
    CEF109_RESOURCE_DIR
    CEF109_LIBRARY
    CEF109_WRAPPER_LIBRARY
)