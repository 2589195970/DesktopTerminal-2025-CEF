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
    
    # 定义目标输出目录 - 修复生成器表达式路径问题
    # 确保CEF文件与主程序在同一目录（Release/Debug）
    if(CMAKE_BUILD_TYPE)
        # Single-config generators (Unix Makefiles, Ninja)
        set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
        message(STATUS "CEF文件将复制到: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
    else()
        # Multi-config generators (Visual Studio, Xcode) - 默认使用Release
        set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
        message(STATUS "CEF文件将复制到: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
    endif()
    
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
        
        # 库文件 - CEF运行时需要的库文件
        set(CEF_LIBRARY_FILES
            "cef_sandbox.lib"
            "libcef.lib"
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
        
        # 复制可执行文件 - 增强错误检查确保CEF子进程文件存在
        foreach(exe ${CEF_EXECUTABLES})
            set(src_file "${CEF_BINARY_DIR}/${exe}")
            set(dst_file "${OUTPUT_DIR}/${exe}")
            
            if(EXISTS "${src_file}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${src_file}" "${dst_file}"
                    COMMENT "强制复制CEF可执行文件: ${exe}")
                message(STATUS "[COPY] 将复制: ${exe}")
            else()
                # 对于关键的CEF子进程文件，缺失时必须报错
                if(exe STREQUAL "chrome_crashpad_handler.exe" OR 
                   exe STREQUAL "cef_subprocess_win32.exe" OR 
                   exe STREQUAL "cef_subprocess_win64.exe")
                    message(FATAL_ERROR "[CRITICAL ERROR] 关键CEF子进程文件缺失: ${exe}
                    
文件路径: ${src_file}
这将导致CEF初始化失败，程序无法运行！

可能的原因：
1. CEF下载不完整 - 请重新运行CEF下载脚本
2. CEF版本不匹配 - 请检查CEF版本配置
3. CEF目录结构异常 - 请检查third_party/cef目录

解决方案：
1. 删除 third_party/cef 目录
2. 重新运行: scripts/download-cef.bat x86 75 (Windows)
3. 确保下载完整后重新构建

CEF目录应包含以下结构：
${CEF_BINARY_DIR}/
  ├── chrome_crashpad_handler.exe
  ├── cef_subprocess_win32.exe (32位)
  ├── libcef.dll
  └── 其他CEF文件")
                else()
                    message(WARNING "[WARNING] CEF可选文件不存在: ${src_file}")
                endif()
            endif()
        endforeach()
        
        # 复制库文件 - CEF运行时需要的库文件
        foreach(lib ${CEF_LIBRARY_FILES})
            set(src_file "${CEF_BINARY_DIR}/${lib}")
            set(dst_file "${OUTPUT_DIR}/${lib}")
            
            if(EXISTS "${src_file}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${src_file}" "${dst_file}"
                    COMMENT "强制复制CEF库文件: ${lib}")
                message(STATUS "[COPY] 将复制CEF库文件: ${lib}")
            else()
                # 对于关键的CEF库文件，缺失时必须报错
                if(lib STREQUAL "cef_sandbox.lib")
                    message(FATAL_ERROR "[CRITICAL ERROR] 关键CEF库文件缺失: ${lib}
                    
文件路径: ${src_file}
这将导致CEF初始化失败，程序运行时找不到所需的库文件！

可能的原因：
1. CEF下载不完整 - cef_sandbox.lib未下载
2. CEF版本问题 - 某些版本可能没有此文件
3. CEF目录结构异常 - 文件位置不正确

解决方案：
1. 检查 ${CEF_BINARY_DIR} 目录中是否存在 cef_sandbox.lib
2. 重新下载CEF包: scripts/download-cef.bat x86 75
3. 确认CEF 75版本包含沙盒库文件

CEF库文件位置应为：
${CEF_BINARY_DIR}/cef_sandbox.lib")
                else()
                    message(WARNING "[WARNING] CEF可选库文件不存在: ${src_file}")
                endif()
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