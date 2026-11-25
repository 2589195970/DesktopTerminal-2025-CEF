cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(../../cmake/AppVersion.cmake)
    project(TerminalService VERSION "${APP_VERSION}" LANGUAGES CXX)
endif()

option(TERMINAL_SERVICE_TESTS "构建终端服务单元测试" OFF)

if(WIN32 OR TERMINAL_SERVICE_TESTS)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Network)
    add_library(terminal_service_core STATIC terminal_core.cpp seat_display_store.cpp registration_monitor.cpp)
    target_compile_features(terminal_service_core PUBLIC cxx_std_17)
    target_link_libraries(terminal_service_core PUBLIC Qt5::Core Qt5::Network)
    if(MSVC)
        target_compile_options(terminal_service_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
    endif()
endif()

if(WIN32)
    enable_language(RC)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Concurrent)
    configure_file(terminal_service_version.rc.in terminal_service_version.rc @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/terminal_status_manifest.rc"
        CONTENT "1 24 \"${CMAKE_CURRENT_SOURCE_DIR}/terminal_status.manifest\"\n")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/terminal_status_manifest.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/terminal_status.manifest")
    add_executable(DesktopTerminalService
        commands_win.cpp main_win.cpp network_win.cpp service_runtime.cpp monitor_windows.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/terminal_service_version.rc"
    )
    add_executable(DesktopTerminalStatus WIN32
        status_tray_win.cpp monitor_windows.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/terminal_service_version.rc"
        "${CMAKE_CURRENT_BINARY_DIR}/terminal_status_manifest.rc"
    )
    set_target_properties(DesktopTerminalService DesktopTerminalStatus PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    set_target_properties(DesktopTerminalService PROPERTIES WIN32_EXECUTABLE FALSE)
    target_compile_definitions(terminal_service_core PUBLIC
        NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601
    )
    target_link_libraries(DesktopTerminalService PRIVATE
        terminal_service_core wtsapi32 userenv iphlpapi ws2_32 advapi32 shell32
    )
    target_link_libraries(DesktopTerminalStatus PRIVATE terminal_service_core Qt5::Widgets Qt5::Concurrent advapi32 shell32)
    if(MSVC)
        target_link_options(DesktopTerminalStatus PRIVATE /MANIFEST:NO)
    endif()
    install(TARGETS DesktopTerminalService DesktopTerminalStatus RUNTIME DESTINATION bin)
endif()

if(TERMINAL_SERVICE_TESTS)
    enable_testing()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    add_executable(terminal_service_tests terminal_service_tests.cpp)
    set_target_properties(terminal_service_tests PROPERTIES AUTOMOC ON WIN32_EXECUTABLE FALSE)
    target_link_libraries(terminal_service_tests PRIVATE terminal_service_core Qt5::Test)
    add_test(NAME terminal_service_tests COMMAND terminal_service_tests)
endif()
