set(CORE_SOURCES
    serial_engine.cpp
    log_buffer.cpp
    log_parser.cpp
    config_manager.cpp
    log_exporter.cpp
    ipc_protocol.cpp
    pty_process_win.cpp
    pty_process_unix.cpp
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

add_library(serialmonitor_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(serialmonitor_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/terminal
    ${RAPIDJSON_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(serialmonitor_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::SerialPort
    fmt::fmt
)
