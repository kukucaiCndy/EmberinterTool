set(GUI_SOURCES
    main.cpp
    ipc_server.cpp
    qml/app_core.cpp
    qml/serial_tab_page.cpp
    qml/terminal_tab_page.cpp
    qml/debug_page.cpp
    qml/terminal/terminal_model.cpp
    qml/terminal/terminal_renderer.cpp
    qml/terminal/terminal_view.cpp
    qml/terminal/glyph_atlas.cpp
)

set(GUI_HEADERS
    ipc_server.h
    qml/tab_page.h
    qml/app_core.h
    qml/serial_tab_page.h
    qml/terminal_tab_page.h
    qml/debug_page.h
    qml/terminal/terminal_model.h
    qml/terminal/terminal_renderer.h
    qml/terminal/terminal_view.h
    qml/terminal/glyph_atlas.h
)

add_executable(serial-monitor
    ${GUI_SOURCES}
    ${GUI_HEADERS}
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)

target_include_directories(serial-monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/terminal
    ${CMAKE_CURRENT_SOURCE_DIR}/qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/terminal
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(serial-monitor PRIVATE
    serialmonitor_core
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
    Qt6::OpenGL
)

set_target_properties(serial-monitor PROPERTIES
    WIN32_EXECUTABLE TRUE
)
