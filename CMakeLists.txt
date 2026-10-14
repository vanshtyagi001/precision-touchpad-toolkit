cmake_minimum_required(VERSION 3.16)
project(TouchpadTracker VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# Find required packages
find_package(OpenGL REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/external/imgui)
include_directories(${CMAKE_SOURCE_DIR}/external/imgui/backends)
include_directories(${CMAKE_SOURCE_DIR}/external)
include_directories(${CMAKE_SOURCE_DIR}/external/glad)

# GLAD sources - compile as separate static library
add_library(glad STATIC external/glad/glad.c)
target_compile_definitions(glad PRIVATE GLAD_GLAPI_EXPORT_BUILD)

# ImGui sources
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_win32.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# Main application sources
set(APP_SOURCES
    src/main.cpp
    src/TouchpadInput.cpp
    src/GestureRecognizer.cpp
    src/TouchpadGUI.cpp
    src/Logger.cpp
    src/DeviceEnumerator.cpp
)

# Create executable (temporarily as console app for debugging)
add_executable(TouchpadTracker ${APP_SOURCES} ${IMGUI_SOURCES})

# Link libraries
target_link_libraries(TouchpadTracker PRIVATE
    glad
    OpenGL::GL
    hid
    setupapi
    user32
    gdi32
    dwmapi
    imm32
)

# Test mode executable (console app without GUI)
add_executable(TouchpadTest
    src/test_mode.cpp
    src/TouchpadInput.cpp
    src/DeviceEnumerator.cpp
    src/Logger.cpp
)

target_link_libraries(TouchpadTest PRIVATE
    hid
    setupapi
    user32
)

# Device lister utility
add_executable(ListDevices
    src/list_devices.cpp
    src/DeviceEnumerator.cpp
)

target_link_libraries(ListDevices PRIVATE
    hid
    setupapi
)

# Set output directory
set_target_properties(TouchpadTracker TouchpadTest ListDevices PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy any required DLLs or resources
add_custom_command(TARGET TouchpadTracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete! Executables in ${CMAKE_BINARY_DIR}/bin"
)
