# CEF资源文件部署配置
# 根据目标架构和平台自动部署相应的CEF文件

include(CMakeParseArguments)

# 部署CEF文件的主函数
function(deploy_cef_files TARGET_NAME)
    cmake_parse_arguments(
        DEPLOY_CEF
        ""
        "CEF_ROOT;BINARY_DIR;RESOURCES_DIR"
        ""
        ${ARGN}
    )
    
    if(NOT DEPLOY_CEF_CEF_ROOT)
        set(DEPLOY_CEF_CEF_ROOT "${CEF_ROOT}")
    endif()
    
    if(NOT DEPLOY_CEF_BINARY_DIR)
        set(DEPLOY_CEF_BINARY_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
    
    if(NOT DEPLOY_CEF_RESOURCES_DIR)
        set(DEPLOY_CEF_RESOURCES_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
    
    message(STATUS "部署CEF文件到: ${DEPLOY_CEF_BINARY_DIR}")
    message(STATUS "CEF根目录: ${DEPLOY_CEF_CEF_ROOT}")
    
    # 根据平台部署不同的文件
    if(WIN32)
        deploy_cef_windows(${TARGET_NAME} 
            "${DEPLOY_CEF_CEF_ROOT}" 
            "${DEPLOY_CEF_BINARY_DIR}" 
            "${DEPLOY_CEF_RESOURCES_DIR}")
    elseif(APPLE)
        deploy_cef_macos(${TARGET_NAME} 
            "${DEPLOY_CEF_CEF_ROOT}" 
            "${DEPLOY_CEF_BINARY_DIR}" 
            "${DEPLOY_CEF_RESOURCES_DIR}")
    else()
        deploy_cef_linux(${TARGET_NAME} 
            "${DEPLOY_CEF_CEF_ROOT}" 
            "${DEPLOY_CEF_BINARY_DIR}" 
            "${DEPLOY_CEF_RESOURCES_DIR}")
    endif()
endfunction()

# Windows平台CEF文件部署
function(deploy_cef_windows TARGET_NAME CEF_ROOT BINARY_DIR RESOURCES_DIR)
    message(STATUS "部署Windows CEF文件")
    
    # 检测CEF版本和架构
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CEF_PLATFORM "win32")
        set(CEF_CONFIG "Release")
    else()
        set(CEF_PLATFORM "win64") 
        set(CEF_CONFIG "Release")
    endif()
    
    # CEF二进制文件路径
    set(CEF_BINARY_PATH "${CEF_ROOT}/${CEF_CONFIG}")
    set(CEF_RESOURCE_PATH "${CEF_ROOT}/Resources")
    
    message(STATUS "CEF二进制路径: ${CEF_BINARY_PATH}")
    message(STATUS "CEF资源路径: ${CEF_RESOURCE_PATH}")
    
    # 核心CEF DLL文件
    set(CEF_DLLS
        "libcef.dll"
        "chrome_elf.dll"
        "d3dcompiler_47.dll"
        "libEGL.dll"
        "libGLESv2.dll"
        "snapshot_blob.bin"
        "v8_context_snapshot.bin"
    )
    
    # 可选的CEF DLL（根据版本可能不存在）
    set(CEF_OPTIONAL_DLLS
        "icudtl.dat"
        "natives_blob.bin"
        "widevinecdmadapter.dll"
    )
    
    # CEF子进程可执行文件
    set(CEF_EXECUTABLES
        "chrome_crashpad_handler.exe"
    )
    
    # 根据架构选择子进程文件
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        list(APPEND CEF_EXECUTABLES "cef_subprocess_win32.exe")
    else()
        list(APPEND CEF_EXECUTABLES "cef_subprocess_win64.exe")
    endif()
    
    # 复制核心DLL文件
    foreach(dll ${CEF_DLLS})
        if(EXISTS "${CEF_BINARY_PATH}/${dll}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_PATH}/${dll}"
                "${BINARY_DIR}/${dll}"
                COMMENT "复制CEF DLL: ${dll}")
        else()
            message(WARNING "CEF DLL文件不存在: ${CEF_BINARY_PATH}/${dll}")
        endif()
    endforeach()
    
    # 复制可选DLL文件
    foreach(dll ${CEF_OPTIONAL_DLLS})
        if(EXISTS "${CEF_BINARY_PATH}/${dll}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_PATH}/${dll}"
                "${BINARY_DIR}/${dll}"
                COMMENT "复制可选CEF文件: ${dll}")
        endif()
    endforeach()
    
    # 复制可执行文件
    foreach(exe ${CEF_EXECUTABLES})
        if(EXISTS "${CEF_BINARY_PATH}/${exe}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_PATH}/${exe}"
                "${BINARY_DIR}/${exe}"
                COMMENT "复制CEF可执行文件: ${exe}")
        endif()
    endforeach()
    
    # 复制资源文件
    deploy_cef_resources(${TARGET_NAME} "${CEF_RESOURCE_PATH}" "${RESOURCES_DIR}")
    
    # 安装规则
    install(FILES ${CEF_DLLS} ${CEF_OPTIONAL_DLLS}
        DESTINATION bin
        OPTIONAL)
    
    install(FILES ${CEF_EXECUTABLES}
        DESTINATION bin
        OPTIONAL)
endfunction()

# macOS平台CEF文件部署
function(deploy_cef_macos TARGET_NAME CEF_ROOT BINARY_DIR RESOURCES_DIR)
    message(STATUS "部署macOS CEF文件")
    
    set(CEF_BINARY_PATH "${CEF_ROOT}/Release")
    set(CEF_RESOURCE_PATH "${CEF_ROOT}/Resources")
    
    # CEF框架
    set(CEF_FRAMEWORK "Chromium Embedded Framework.framework")
    
    if(EXISTS "${CEF_BINARY_PATH}/${CEF_FRAMEWORK}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_BINARY_PATH}/${CEF_FRAMEWORK}"
            "${BINARY_DIR}/${CEF_FRAMEWORK}"
            COMMENT "复制CEF框架")
    endif()
    
    # CEF Helper应用程序
    set(CEF_HELPERS
        "cef_subprocess.app"
        "cef_subprocess (GPU).app"
        "cef_subprocess (Plugin).app"
        "cef_subprocess (Renderer).app"
    )
    
    foreach(helper ${CEF_HELPERS})
        if(EXISTS "${CEF_BINARY_PATH}/${helper}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_BINARY_PATH}/${helper}"
                "${BINARY_DIR}/${helper}"
                COMMENT "复制CEF Helper: ${helper}")
        endif()
    endforeach()
    
    # 复制资源文件
    deploy_cef_resources(${TARGET_NAME} "${CEF_RESOURCE_PATH}" "${RESOURCES_DIR}")
endfunction()

# Linux平台CEF文件部署
function(deploy_cef_linux TARGET_NAME CEF_ROOT BINARY_DIR RESOURCES_DIR)
    message(STATUS "部署Linux CEF文件")
    
    set(CEF_BINARY_PATH "${CEF_ROOT}/Release")
    set(CEF_RESOURCE_PATH "${CEF_ROOT}/Resources")
    
    # CEF共享库
    set(CEF_LIBRARIES
        "libcef.so"
        "libEGL.so"
        "libGLESv2.so"
        "swiftshader/libEGL.so"
        "swiftshader/libGLESv2.so"
    )
    
    # CEF可执行文件
    set(CEF_EXECUTABLES
        "chrome-sandbox"
        "cef_subprocess"
        "chrome_crashpad_handler"
    )
    
    # CEF数据文件
    set(CEF_DATA_FILES
        "icudtl.dat"
        "snapshot_blob.bin"
        "v8_context_snapshot.bin"
    )
    
    # 复制库文件
    foreach(lib ${CEF_LIBRARIES})
        if(EXISTS "${CEF_BINARY_PATH}/${lib}")
            get_filename_component(lib_dir "${lib}" DIRECTORY)
            if(lib_dir)
                file(MAKE_DIRECTORY "${BINARY_DIR}/${lib_dir}")
            endif()
            
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_PATH}/${lib}"
                "${BINARY_DIR}/${lib}"
                COMMENT "复制CEF库: ${lib}")
        endif()
    endforeach()
    
    # 复制可执行文件
    foreach(exe ${CEF_EXECUTABLES})
        if(EXISTS "${CEF_BINARY_PATH}/${exe}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_PATH}/${exe}"
                "${BINARY_DIR}/${exe}"
                COMMENT "复制CEF可执行文件: ${exe}")
            
            # 设置chrome-sandbox的特殊权限
            if(exe STREQUAL "chrome-sandbox")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND chmod 4755 "${BINARY_DIR}/${exe}"
                    COMMENT "设置chrome-sandbox权限")
            endif()
        endif()
    endforeach()
    
    # 复制数据文件
    foreach(data ${CEF_DATA_FILES})
        if(EXISTS "${CEF_BINARY_PATH}/${data}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_PATH}/${data}"
                "${BINARY_DIR}/${data}"
                COMMENT "复制CEF数据文件: ${data}")
        endif()
    endforeach()
    
    # 复制资源文件
    deploy_cef_resources(${TARGET_NAME} "${CEF_RESOURCE_PATH}" "${RESOURCES_DIR}")
endfunction()

# 部署CEF资源文件（所有平台通用）
function(deploy_cef_resources TARGET_NAME RESOURCE_PATH RESOURCES_DIR)
    message(STATUS "部署CEF资源文件")
    
    # CEF资源文件
    set(CEF_RESOURCE_FILES
        "cef.pak"
        "cef_100_percent.pak"
        "cef_200_percent.pak"
        "cef_extensions.pak"
        "devtools_resources.pak"
    )
    
    # 复制资源文件
    foreach(resource ${CEF_RESOURCE_FILES})
        if(EXISTS "${RESOURCE_PATH}/${resource}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RESOURCE_PATH}/${resource}"
                "${RESOURCES_DIR}/${resource}"
                COMMENT "复制CEF资源: ${resource}")
        endif()
    endforeach()
    
    # 复制本地化文件夹
    if(EXISTS "${RESOURCE_PATH}/locales")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${RESOURCE_PATH}/locales"
            "${RESOURCES_DIR}/locales"
            COMMENT "复制CEF本地化文件")
    endif()
    
    # 安装资源文件
    install(FILES ${CEF_RESOURCE_FILES}
        DESTINATION bin
        OPTIONAL)
    
    install(DIRECTORY "${RESOURCE_PATH}/locales"
        DESTINATION bin
        OPTIONAL)
endfunction()

# 验证CEF文件完整性
function(verify_cef_deployment TARGET_NAME BINARY_DIR)
    message(STATUS "Verifying CEF deployment integrity")
    
    # Create verification script
    set(VERIFY_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/verify_cef.cmake")
    
    file(WRITE "${VERIFY_SCRIPT}" "
        # CEF deployment verification script
        set(BINARY_DIR \"${BINARY_DIR}\")
        set(MISSING_FILES \"\")
        
        # Check required files
        if(WIN32)
            set(REQUIRED_FILES \"libcef.dll\" \"cef.pak\")
        elseif(APPLE)
            set(REQUIRED_FILES \"Chromium Embedded Framework.framework\" \"cef.pak\")
        else()
            set(REQUIRED_FILES \"libcef.so\" \"cef.pak\")
        endif()
        
        foreach(file \${REQUIRED_FILES})
            if(NOT EXISTS \"\${BINARY_DIR}/\${file}\")
                list(APPEND MISSING_FILES \"\${file}\")
            endif()
        endforeach()
        
        if(MISSING_FILES)
            message(FATAL_ERROR \"CEF required files missing: \${MISSING_FILES}\")
        else()
            message(STATUS \"CEF files verification passed\")
        endif()
    ")
    
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${VERIFY_SCRIPT}"
        COMMENT "Verifying CEF file deployment")
endfunction()

# 创建CEF环境配置
function(configure_cef_environment TARGET_NAME)
    message(STATUS "配置CEF运行环境")
    
    # Windows特定配置
    if(WIN32)
        # 设置DLL搜索路径
        set_target_properties(${TARGET_NAME} PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY};$ENV{PATH}")
    endif()
    
    # 设置运行时库路径
    if(UNIX AND NOT APPLE)
        set_target_properties(${TARGET_NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN:$ORIGIN/lib")
    endif()
endfunction()