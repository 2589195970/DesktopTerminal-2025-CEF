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

# 根据架构选择CEF版本
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # Windows 32位使用CEF 75.1.14（确认可用的Windows 7 SP1支持版本）
    set(CEF_VERSION "75.1.14+gc81164e+chromium-75.0.3770.100")
    message(STATUS "使用CEF 75.1.14版本 - Windows 7 SP1 32位兼容")
else()
    # 其他平台使用较新版本
    set(CEF_VERSION "118.7.1+g99817d2+chromium-118.0.5993.119")
    message(STATUS "使用CEF 118.7.1版本")
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

# 查找CEF
find_package(CEF REQUIRED)

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
    src/cef/cef_client.cpp
    src/cef/cef_app.cpp
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
    src/cef/cef_client.h
    src/cef/cef_app.h
    src/config/config_manager.h
    src/logging/logger.h
    src/security/security_controller.h
    src/security/keyboard_filter.h
)

# QHotkey子项目
add_subdirectory(third_party/QHotkey)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    ${CEF_LIBRARIES}
    qhotkey
)

# Windows特定链接库
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        comctl32.lib
        rpcrt4.lib
        shlwapi.lib
        ws2_32.lib
    )
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
    deploy_cef_files(${PROJECT_NAME}
        CEF_ROOT "${CEF_ROOT}"
        BINARY_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        RESOURCES_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
    
    # 配置CEF运行环境
    configure_cef_environment(${PROJECT_NAME})
    
    # 验证CEF部署
    verify_cef_deployment(${PROJECT_NAME} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
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