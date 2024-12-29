FetchContent_Declare(
  wgpu
  URL https://github.com/gfx-rs/wgpu-native/releases/download/v22.1.0.5/wgpu-macos-aarch64-release.zip
)
FetchContent_MakeAvailable(wgpu)
add_library(wgpu SHARED IMPORTED)
set_target_properties(wgpu PROPERTIES
    IMPORTED_LOCATION ${wgpu_SOURCE_DIR}/lib/libwgpu_native.dylib
)
target_include_directories(wgpu INTERFACE
    ${wgpu_SOURCE_DIR}/include/
    ${wgpu_SOURCE_DIR}/include/webgpu
    ${wgpu_SOURCE_DIR}/include/wgpu
)
