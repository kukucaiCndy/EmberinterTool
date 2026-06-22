set(CORE_SOURCES
    serial_engine.cpp
    log_buffer.cpp
    log_parser.cpp
    config_manager.cpp
    log_exporter.cpp
    ipc_protocol.cpp
    terminal/terminal_cell.cpp
    terminal/terminal_buffer.cpp
    terminal/vt_parser.cpp
    terminal/terminal_input.cpp
)

set(CORE_HEADERS
    tab_type.h
    serial_engine.h
    log_buffer.h
    log_parser.h
    config_manager.h
    log_exporter.h
    ipc_protocol.h
    pty_process.h
    terminal/terminal_cell.h
    terminal/terminal_buffer.h
    terminal/vt_parser.h
    terminal/terminal_input.h
)

# 平台条件编译 PTY 实现
if(WIN32)
    list(APPEND CORE_SOURCES pty_process_win.cpp)
else()
    list(APPEND CORE_SOURCES pty_process_unix.cpp)
endif()

add_library(serialmonitor_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(serialmonitor_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/terminal
    ${RAPIDJSON_INCLUDE_DIR}
)

# spdlog 链接（优先使用 find_package，回退到 header-only）
if(TARGET spdlog::spdlog)
    target_link_libraries(serialmonitor_core PUBLIC spdlog::spdlog)
else()
    target_include_directories(serialmonitor_core PUBLIC ${SPDLOG_INCLUDE_DIR})
    # header-only 模式需要定义此宏
    target_compile_definitions(serialmonitor_core PUBLIC SPDLOG_HEADER_ONLY)
endif()

target_link_libraries(serialmonitor_core PUBLIC
    Qt6::Core
    fmt::fmt
)

# SerialPort/Gui 仅 core 自身需要，避免通过 PUBLIC 传递给 CLI
if(TARGET Qt6::SerialPort)
    target_link_libraries(serialmonitor_core PRIVATE Qt6::SerialPort)
endif()
if(TARGET Qt6::Gui)
    target_link_libraries(serialmonitor_core PRIVATE Qt6::Gui)
endif()
