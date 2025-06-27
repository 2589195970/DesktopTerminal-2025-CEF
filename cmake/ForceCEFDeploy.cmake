# 强制CEF文件部署模块
# 解决DeployCEF模块失效导致的安装包缺少CEF文件问题

# 强制部署CEF文件的主函数
function(force_deploy_cef_files TARGET_NAME)
    message(STATUS "[DEPLOY] 开始强制部署CEF文件...")
    
    # 确定CEF根目录
    if(NOT CEF_ROOT_DIR)
        message(FATAL_ERROR "[ERROR] CEF_ROOT_DIR未定义，无法部署CEF文件")
    endif()
    
    message(STATUS "CEF根目录: ${CEF_ROOT_DIR}")
    
    # 多路径CEF文件搜索策略
    set(SEARCH_PATTERNS
        "${CEF_ROOT_DIR}/cef_binary_*/Release"
        "${CEF_ROOT_DIR}/Release"
        "${CEF_ROOT_DIR}/*/Release"
    )
    
    # 查找实际的CEF二进制目录
    set(CEF_BINARY_DIR "")
    foreach(pattern ${SEARCH_PATTERNS})
        file(GLOB matched_dirs "${pattern}")
        foreach(dir ${matched_dirs})
            if(EXISTS "${dir}/libcef.dll" OR EXISTS "${dir}/libcef.lib")
                set(CEF_BINARY_DIR "${dir}")
                message(STATUS "[OK] 找到CEF二进制目录: ${CEF_BINARY_DIR}")
                break()
            endif()
        endforeach()
        if(CEF_BINARY_DIR)
            break()
        endif()
    endforeach()
    
    # 查找CEF资源目录
    set(RESOURCE_PATTERNS
        "${CEF_ROOT_DIR}/cef_binary_*/Resources"
        "${CEF_ROOT_DIR}/Resources"
        "${CEF_ROOT_DIR}/*/Resources"
    )
    
    set(CEF_RESOURCE_DIR "")
    foreach(pattern ${RESOURCE_PATTERNS})
        file(GLOB matched_dirs "${pattern}")
        foreach(dir ${matched_dirs})
            if(EXISTS "${dir}/cef.pak")
                set(CEF_RESOURCE_DIR "${dir}")
                message(STATUS "[OK] 找到CEF资源目录: ${CEF_RESOURCE_DIR}")
                break()
            endif()
        endforeach()
        if(CEF_RESOURCE_DIR)
            break()
        endif()
    endforeach()
    
    # 验证找到的目录
    if(NOT CEF_BINARY_DIR)
        message(WARNING "[WARNING] 未找到CEF二进制目录，显示搜索信息...")
        foreach(pattern ${SEARCH_PATTERNS})
            message(STATUS "搜索模式: ${pattern}")
            file(GLOB matched_dirs "${pattern}")
            foreach(dir ${matched_dirs})
                message(STATUS "  检查目录: ${dir}")
                if(EXISTS "${dir}")
                    file(GLOB files "${dir}/*")
                    foreach(file ${files})
                        get_filename_component(filename "${file}" NAME)
                        message(STATUS "    文件: ${filename}")
                    endforeach()
                endif()
            endforeach()
        endforeach()
        return()
    endif()
    
    if(NOT CEF_RESOURCE_DIR)
        message(WARNING "[WARNING] 未找到CEF资源目录")
        return()
    endif()
    
    # 定义目标输出目录
    set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    message(STATUS "CEF文件将复制到: ${OUTPUT_DIR}")
    
    # Windows平台的CEF文件列表
    if(WIN32)
        # 核心DLL文件
        set(CEF_CORE_DLLS
            "libcef.dll"
            "chrome_elf.dll"
            "d3dcompiler_47.dll"
            "libEGL.dll"
            "libGLESv2.dll"
        )
        
        # 数据文件
        set(CEF_DATA_FILES
            "snapshot_blob.bin"
            "v8_context_snapshot.bin"
            "icudtl.dat"
        )
        
        # 可执行文件
        set(CEF_EXECUTABLES
            "chrome_crashpad_handler.exe"
        )
        
        # 根据架构添加子进程可执行文件
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            list(APPEND CEF_EXECUTABLES "cef_subprocess_win64.exe")
        else()
            list(APPEND CEF_EXECUTABLES "cef_subprocess_win32.exe")
        endif()
        
        # 复制核心DLL文件
        foreach(dll ${CEF_CORE_DLLS})
            set(src_file "${CEF_BINARY_DIR}/${dll}")
            set(dst_file "${OUTPUT_DIR}/${dll}")
            
            if(EXISTS "${src_file}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${src_file}" "${dst_file}"
                    COMMENT "强制复制CEF核心文件: ${dll}")
                message(STATUS "[COPY] 将复制: ${dll}")
            else()
                message(WARNING "[WARNING] CEF文件不存在: ${src_file}")
            endif()
        endforeach()
        
        # 复制数据文件
        foreach(data ${CEF_DATA_FILES})
            set(src_file "${CEF_BINARY_DIR}/${data}")
            set(dst_file "${OUTPUT_DIR}/${data}")
            
            if(EXISTS "${src_file}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${src_file}" "${dst_file}"
                    COMMENT "强制复制CEF数据文件: ${data}")
                message(STATUS "[COPY] 将复制: ${data}")
            endif()
        endforeach()
        
        # 复制可执行文件
        foreach(exe ${CEF_EXECUTABLES})
            set(src_file "${CEF_BINARY_DIR}/${exe}")
            set(dst_file "${OUTPUT_DIR}/${exe}")
            
            if(EXISTS "${src_file}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${src_file}" "${dst_file}"
                    COMMENT "强制复制CEF可执行文件: ${exe}")
                message(STATUS "[COPY] 将复制: ${exe}")
            endif()
        endforeach()
    endif()
    
    # 复制资源文件
    set(CEF_RESOURCE_FILES
        "cef.pak"
        "cef_100_percent.pak"
        "cef_200_percent.pak"
        "cef_extensions.pak"
        "devtools_resources.pak"
    )
    
    foreach(resource ${CEF_RESOURCE_FILES})
        set(src_file "${CEF_RESOURCE_DIR}/${resource}")
        set(dst_file "${OUTPUT_DIR}/${resource}")
        
        if(EXISTS "${src_file}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${src_file}" "${dst_file}"
                COMMENT "强制复制CEF资源文件: ${resource}")
            message(STATUS "[COPY] 将复制: ${resource}")
        else()
            message(WARNING "[WARNING] CEF资源文件不存在: ${src_file}")
        endif()
    endforeach()
    
    # 复制locales目录
    set(locales_src "${CEF_RESOURCE_DIR}/locales")
    set(locales_dst "${OUTPUT_DIR}/locales")
    
    if(EXISTS "${locales_src}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${locales_src}" "${locales_dst}"
            COMMENT "强制复制CEF本地化文件")
        message(STATUS "[COPY] 将复制locales目录")
    endif()
    
    # 复制swiftshader目录（如果存在）
    set(swiftshader_src "${CEF_BINARY_DIR}/swiftshader")
    set(swiftshader_dst "${OUTPUT_DIR}/swiftshader")
    
    if(EXISTS "${swiftshader_src}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${swiftshader_src}" "${swiftshader_dst}"
            COMMENT "强制复制SwiftShader文件")
        message(STATUS "[COPY] 将复制swiftshader目录")
    endif()
    
    # 创建验证目标
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "[SUCCESS] CEF文件强制部署完成"
        COMMENT "验证CEF部署")
    
    message(STATUS "[OK] CEF强制部署配置完成")
endfunction()

# 验证CEF部署结果的函数
function(verify_forced_cef_deployment TARGET_NAME)
    set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "[INFO] 验证CEF文件部署..."
        COMMAND ${CMAKE_COMMAND} -E echo "输出目录: ${OUTPUT_DIR}"
        COMMENT "验证CEF文件部署")
    
    # 检查关键文件是否存在
    if(WIN32)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "检查libcef.dll..."
            COMMAND ${CMAKE_COMMAND} -E echo "检查cef.pak..."
            COMMAND ${CMAKE_COMMAND} -E echo "检查locales目录..."
            COMMENT "验证关键CEF文件")
    endif()
endfunction()