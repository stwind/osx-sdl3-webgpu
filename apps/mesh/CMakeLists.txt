cmake_minimum_required(VERSION 3.24.0)
project(app LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
list(PREPEND CMAKE_MODULE_PATH ${ROOT}/cmake/)

cmake_policy(SET CMP0135 NEW)

include(CPM)
include(utils)
include(sdl3)
include(wgpu)
include(imgui)
include(eigen)

set(TARGET ${PROJECT_NAME})

file(GLOB_RECURSE LIB_SOURCES "${ROOT}/lib/*")

add_executable(${TARGET} 
${IMGUI_SOURCES}
${LIB_SOURCES}
main.cpp
)

target_include_directories(${TARGET} PUBLIC
${IMGUI_INCLUDES}
${ROOT}/include
)

target_compile_definitions(${TARGET} PUBLIC
"IMGUI_IMPL_WEBGPU_BACKEND_WGPU"
)
target_link_libraries(${TARGET} 
PRIVATE SDL3::SDL3 wgpu Eigen
"-framework QuartzCore"
"-framework Cocoa"
"-framework Metal"
)
