cmake_minimum_required(VERSION 3.12.0)
project(sdl3hello)

set(TARGET sdl3hello)

list(PREPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake/
)

include(FetchContent)
include(utils)

FetchContent_DeclareShallowGit(
    SDL3
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG "main"
)
FetchContent_MakeAvailable(SDL3)

add_executable(${TARGET})
target_sources(${TARGET} PRIVATE main.cpp)
target_compile_features(${TARGET} PRIVATE cxx_std_20)

target_link_libraries(${TARGET} PRIVATE SDL3::SDL3)