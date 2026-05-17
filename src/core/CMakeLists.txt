set(CORE_SOURCES
    serial_engine.cpp
    log_buffer.cpp
    log_parser.cpp
    config_manager.cpp
    log_exporter.cpp
    ipc_protocol.cpp
)

set(CORE_HEADERS
    serial_engine.h
    log_buffer.h
    log_parser.h
    config_manager.h
    log_exporter.h
    ipc_protocol.h
)

add_library(serialmonitor_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(serialmonitor_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(serialmonitor_core PUBLIC
    Qt5::Core
    Qt5::Gui
    Qt5::SerialPort
    fmt::fmt
)
