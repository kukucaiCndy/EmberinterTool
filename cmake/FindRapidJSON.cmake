find_path(RAPIDJSON_INCLUDE_DIR rapidjson/rapidjson.h
    HINTS
    ${CMAKE_SOURCE_DIR}/third_party/rapidjson/include
    /usr/include
    /usr/local/include
    /mingw64/include
    /opt/homebrew/include
)

if(RAPIDJSON_INCLUDE_DIR)
    set(RAPIDJSON_FOUND TRUE)
    message(STATUS "Found rapidjson: ${RAPIDJSON_INCLUDE_DIR}")
else()
    if(APPLE)
        message(FATAL_ERROR "rapidjson not found. Install via: brew install rapidjson")
    elseif(WIN32)
        message(FATAL_ERROR "rapidjson not found. Install via: pacman -S mingw-w64-x86_64-rapidjson")
    else()
        message(FATAL_ERROR "rapidjson not found. Install via your package manager")
    endif()
endif()
