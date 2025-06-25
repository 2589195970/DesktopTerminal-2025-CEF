cmake_minimum_required(VERSION 3.20)
project(DesktopTerminal-CEF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 检测架构和平台
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CEF_PLATFORM "windows64")
    set(CMAKE_ARCH "x64")
    message(STATUS "64位架构检测")
else()
    set(CEF_PLATFORM "windows32")
    set(CMAKE_ARCH "Win32")
    message(STATUS "32位架构检测")
endif()

# 全平台统一使用CEF 75以获得最大兼容性
set(CEF_VERSION "75.1.14+gc81164e+chromium-75.0.3770.100")
message(STATUS "使用CEF 75.1.14版本 - 全平台最大兼容性模式")

# 为所有平台设置CEF 75兼容性优化
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(STATUS "Windows 32位 - CEF 75原生支持")
else()
    message(STATUS "其他平台 - CEF 75统一兼容性模式")
endif()

# Windows平台特殊设置
if(WIN32)
    # 设置Windows应用程序属性（不显示控制台窗口）
    set(CMAKE_WIN32_EXECUTABLE TRUE)
    # 确保使用UTF-8编码
    add_compile_options("/utf-8")
    # 32位系统内存优化
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_compile_definitions(CEF_32BIT_BUILD)
    endif()
endif()

# 添加cmake模块路径
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含CEF部署模块
include(DeployCEF)

# 查找Qt5
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# 设置Qt5的MOC、UIC、RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找CEF - 启用详细调试
message(STATUS "=== CEF查找过程开始 ===")
message(STATUS "项目根目录: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "构建目录: ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "CMAKE模块路径: ${CMAKE_MODULE_PATH}")

# 检查third_party/cef目录
set(CEF_CHECK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef")
if(EXISTS "${CEF_CHECK_DIR}")
    message(STATUS "✓ third_party/cef目录存在")
    file(GLOB CEF_SUBDIRS "${CEF_CHECK_DIR}/*")
    foreach(subdir ${CEF_SUBDIRS})
        if(IS_DIRECTORY "${subdir}")
            get_filename_component(dirname "${subdir}" NAME)
            message(STATUS "  发现子目录: ${dirname}")
        endif()
    endforeach()
else()
    message(WARNING "✗ third_party/cef目录不存在！")
endif()

find_package(CEF REQUIRED)

# 如果CEF wrapper库不存在但有源码，则编译wrapper库
if(CEF_FOUND AND CEF_WRAPPER_SOURCE_DIR AND NOT CEF_WRAPPER_LIBRARY)
    message(STATUS "编译CEF Wrapper库从源码: ${CEF_WRAPPER_SOURCE_DIR}")
    
    # 收集wrapper库源文件
    file(GLOB_RECURSE CEF_WRAPPER_SOURCES
        "${CEF_WRAPPER_SOURCE_DIR}/*.cc"
        "${CEF_WRAPPER_SOURCE_DIR}/*.cpp"
    )
    
    if(CEF_WRAPPER_SOURCES)
        # 创建wrapper静态库
        add_library(cef_dll_wrapper STATIC ${CEF_WRAPPER_SOURCES})
        
        # 设置wrapper库的include目录
        target_include_directories(cef_dll_wrapper PRIVATE
            ${CEF_INCLUDE_PATH}
            ${CEF_INCLUDE_PATH}/include
        )
        
        # 设置编译定义 - CEF wrapper需要的所有定义
        target_compile_definitions(cef_dll_wrapper PRIVATE
            -DUSING_CEF_SHARED
            -DNOMINMAX
            -DWIN32_LEAN_AND_MEAN
            -D_CRT_SECURE_NO_WARNINGS
            -D_SCL_SECURE_NO_WARNINGS
            -DWRAPPING_CEF_SHARED
        )
        
        # 平台特定设置
        if(WIN32)
            target_compile_definitions(cef_dll_wrapper PRIVATE
                -DWIN32
                -D_WINDOWS
                -DUNICODE
                -D_UNICODE
            )
            
            # 32位系统特殊处理
            if(CMAKE_SIZEOF_VOID_P EQUAL 4)
                target_compile_definitions(cef_dll_wrapper PRIVATE
                    -DCEF_32BIT_BUILD
                )
            endif()
        elseif(APPLE)
            target_compile_definitions(cef_dll_wrapper PRIVATE
                -DMACOSX
            )
        elseif(UNIX)
            target_compile_definitions(cef_dll_wrapper PRIVATE
                -DLINUX
            )
        endif()
        
        # 将编译的wrapper库添加到CEF_LIBRARIES
        list(APPEND CEF_LIBRARIES cef_dll_wrapper)
        
        message(STATUS "CEF Wrapper库编译配置完成")
    else()
        message(WARNING "CEF Wrapper源码目录为空: ${CEF_WRAPPER_SOURCE_DIR}")
    endif()
endif()

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CEF_INCLUDE_PATH}
)

# 源文件列表
set(SOURCES
    src/main.cpp
    src/core/application.cpp
    src/core/cef_manager.cpp
    src/core/secure_browser.cpp
    src/core/window_manager.cpp
    src/cef/cef_client_impl.cpp
    src/cef/cef_app_impl.cpp
    src/config/config_manager.cpp
    src/logging/logger.cpp
    src/security/security_controller.cpp
    src/security/keyboard_filter.cpp
)

# 头文件列表
set(HEADERS
    src/core/application.h
    src/core/cef_manager.h
    src/core/secure_browser.h
    src/core/window_manager.h
    src/cef/cef_client_impl.h
    src/cef/cef_app_impl.h
    src/config/config_manager.h
    src/logging/logger.h
    src/security/security_controller.h
    src/security/keyboard_filter.h
)

# QHotkey子项目
add_subdirectory(third_party/QHotkey)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 验证CEF库并配置链接
message(STATUS "=== CEF库链接配置 ===")
message(STATUS "CEF_LIBRARIES: ${CEF_LIBRARIES}")

# 确保CEF库存在
if(NOT CEF_LIBRARIES)
    message(FATAL_ERROR "CEF_LIBRARIES为空！CEF库未找到。")
endif()

# 链接库 - 注意顺序很重要
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    qhotkey
)

# 单独链接CEF库以更好地控制顺序
foreach(cef_lib ${CEF_LIBRARIES})
    if(EXISTS "${cef_lib}")
        message(STATUS "✓ 链接CEF库: ${cef_lib}")
        target_link_libraries(${PROJECT_NAME} "${cef_lib}")
    else()
        message(WARNING "✗ CEF库文件不存在: ${cef_lib}")
    endif()
endforeach()

# 如果没有找到CEF库，尝试手动添加
if(WIN32 AND NOT CEF_LIBRARY)
    message(STATUS "尝试手动查找并链接CEF库...")
    set(MANUAL_CEF_PATHS
        "${CEF_ROOT_DIR}/Release"
        "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/Release"
        "${CEF_ROOT_DIR}/Debug"
        "${CEF_ROOT_DIR}/${CEF_BINARY_NAME}/Debug"
    )
    
    foreach(manual_path ${MANUAL_CEF_PATHS})
        if(EXISTS "${manual_path}/libcef.lib")
            message(STATUS "✓ 手动找到CEF库: ${manual_path}/libcef.lib")
            target_link_libraries(${PROJECT_NAME} "${manual_path}/libcef.lib")
            break()
        endif()
    endforeach()
endif()

# CEF库特殊处理
if(CEF_FOUND)
    # 确保CEF include路径被正确设置
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CEF_INCLUDE_PATH}
        ${CEF_INCLUDE_PATH}/include
    )
    
    # macOS Framework特殊处理
    if(APPLE AND CEF_FRAMEWORK_PATH)
        message(STATUS "链接CEF Framework: ${CEF_FRAMEWORK_PATH}")
        target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
        target_link_libraries(${PROJECT_NAME} "-framework ApplicationServices")
    endif()
endif()

# Windows特定链接库 - CEF需要的所有系统库
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        # 基础系统库
        comctl32.lib
        rpcrt4.lib
        shlwapi.lib
        ws2_32.lib
        
        # CEF特定的Windows库
        advapi32.lib
        dbghelp.lib
        dnsapi.lib
        gdi32.lib
        psapi.lib
        user32.lib
        version.lib
        winmm.lib
        winspool.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        shell32.lib
        kernel32.lib
    )
    
    # CEF特定的链接选项
    if(MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE
            /SUBSYSTEM:WINDOWS
            /DELAYLOAD:libcef.dll
        )
    endif()
endif()

# 编译器特定设置
if(MSVC)
    # 禁用MSVC警告
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    
    # 32位系统优化
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
    endif()
endif()

# 设置目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "DesktopTerminal-CEF"
    DEBUG_POSTFIX "_d"
)

# 部署CEF文件
if(CEF_FOUND)
    # 确保有CEF_ROOT变量用于部署
    if(NOT CEF_ROOT)
        set(CEF_ROOT "${CEF_ROOT_DIR}")
    endif()
    
    # 检查DeployCEF模块是否存在
    if(COMMAND deploy_cef_files)
        deploy_cef_files(${PROJECT_NAME}
            CEF_ROOT "${CEF_ROOT}"
            BINARY_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            RESOURCES_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        )
        
        # 配置CEF运行环境
        if(COMMAND configure_cef_environment)
            configure_cef_environment(${PROJECT_NAME})
        endif()
        
        # 验证CEF部署
        if(COMMAND verify_cef_deployment)
            verify_cef_deployment(${PROJECT_NAME} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        endif()
    else()
        message(STATUS "DeployCEF模块不可用，跳过CEF文件部署")
        message(STATUS "请确保CEF二进制文件和资源在运行时可用")
        message(STATUS "CEF根目录: ${CEF_ROOT}")
        message(STATUS "CEF二进制目录: ${CEF_BINARY_DIR}")
        message(STATUS "CEF资源目录: ${CEF_RESOURCE_DIR}")
    endif()
endif()

# 安装配置
install(TARGETS ${PROJECT_NAME}
    DESTINATION bin
)

# CEF资源文件安装
if(CEF_FOUND)
    install(DIRECTORY ${CEF_RESOURCE_DIR}/
        DESTINATION bin
        FILES_MATCHING 
        PATTERN "*.pak"
        PATTERN "*.dat"
        PATTERN "*.bin"
    )
    
    install(DIRECTORY ${CEF_BINARY_DIR}/
        DESTINATION bin
        FILES_MATCHING 
        PATTERN "*.dll"
        PATTERN "*.exe"
        PATTERN "*.so"
        PATTERN "*.dylib"
    )
endif()