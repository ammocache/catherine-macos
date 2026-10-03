# catherine - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(catherine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(CATHERINE_SOURCES
    src/main.cpp
    src/settings_menu.cpp
    src/setup_screen.cpp
)

if(WIN32)
    add_executable(catherine WIN32 ${CATHERINE_SOURCES})
else()
    add_executable(catherine ${CATHERINE_SOURCES})
endif()

rexglue_setup_target(catherine GPU_PLUGINS xenos)

# Workaround: the host target compiles SDK UI code that includes <imgui.h>,
# but the SDK's source-tree mode does not pass the imgui folder on to it.
target_include_directories(catherine PRIVATE "${REXSDK_DIR}/thirdparty/imgui")

# Fonts and other assets for the settings menu (found next to the source tree in dev builds,
# and in Catherine.app/Contents/Resources in the packaged app).
target_compile_definitions(catherine PRIVATE CATHERINE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
