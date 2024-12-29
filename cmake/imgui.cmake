FetchContent_DeclareShallowGit(
    imgui
    GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
    GIT_TAG "v1.91.6"
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
${imgui_SOURCE_DIR}/imgui.cpp
${imgui_SOURCE_DIR}/imgui_draw.cpp
${imgui_SOURCE_DIR}/imgui_demo.cpp
${imgui_SOURCE_DIR}/imgui_tables.cpp
${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

set(IMGUI_INCLUDES 
${imgui_SOURCE_DIR}
${imgui_SOURCE_DIR}/backends
)