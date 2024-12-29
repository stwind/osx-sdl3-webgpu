cmake_minimum_required(VERSION 3.12.0)
project(sdl3hello LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET sdl3hello)

list(PREPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake/
)

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

include(wgpu)

add_executable(${TARGET} sdl3webgpu.h sdl3webgpu.mm main.cpp)

target_link_libraries(${TARGET} 
PRIVATE SDL3::SDL3 wgpu
"-framework QuartzCore"
"-framework Cocoa"
"-framework Metal"
)
