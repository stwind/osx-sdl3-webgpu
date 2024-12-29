cmake_minimum_required(VERSION 3.12.0)
project(sdl3hello LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET ${PROJECT_NAME})
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

list(PREPEND CMAKE_MODULE_PATH ${ROOT}/cmake/)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
include(utils)

FetchContent_DeclareShallowGit(
    SDL3
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG "main"
)
FetchContent_MakeAvailable(SDL3)

FetchContent_DeclareShallowGit(
    imgui
    GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
    GIT_TAG "v1.91.6"
)
FetchContent_MakeAvailable(imgui)

include(wgpu)

set(IMGUI_SOURCES
${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
${imgui_SOURCE_DIR}/imgui.cpp
${imgui_SOURCE_DIR}/imgui_draw.cpp
${imgui_SOURCE_DIR}/imgui_demo.cpp
${imgui_SOURCE_DIR}/imgui_tables.cpp
${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

file(GLOB_RECURSE LIB_SOURCES "${ROOT}/lib/*")

add_executable(${TARGET} 
${IMGUI_SOURCES}
${LIB_SOURCES}
main.cpp
)

target_include_directories(${TARGET} PUBLIC
${imgui_SOURCE_DIR}
${imgui_SOURCE_DIR}/backends
${ROOT}/include()
)

target_compile_definitions(${TARGET} PUBLIC
"IMGUI_IMPL_WEBGPU_BACKEND_WGPU"
)

target_link_libraries(${TARGET} 
PRIVATE SDL3::SDL3 wgpu
"-framework QuartzCore"
"-framework Cocoa"
"-framework Metal"
)
