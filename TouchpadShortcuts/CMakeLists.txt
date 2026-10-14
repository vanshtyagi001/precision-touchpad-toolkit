cmake_minimum_required(VERSION 3.16)
project(TouchpadShortcuts VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)

# Main executable - Windows GUI app (no console window)
add_executable(TouchpadShortcuts WIN32
    src/main.cpp
    src/TouchpadZones.cpp
    src/TouchpadZones.h
    src/SystemActions.cpp
    src/SystemActions.h
    src/TrayIcon.cpp
    src/TrayIcon.h
    src/Config.h
)

# Link required libraries
target_link_libraries(TouchpadShortcuts PRIVATE
    hid
    setupapi
    user32
    shell32
    ole32
)

# Add manifest for DPI awareness
target_sources(TouchpadShortcuts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app.manifest
)

# Post-build message
add_custom_command(TARGET TouchpadShortcuts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete! TouchpadShortcuts.exe in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
