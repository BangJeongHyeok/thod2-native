# Internal implementation, linked into the DLL and into the tests.
add_library(thod2_core_lib STATIC
  config.cpp
  core.cpp
  exe_id.cpp
  log.cpp
  patches/cursor_map.cpp
  patches/ddraw_trace.cpp
  patches/device_dialog.cpp
  patches/dgvoodoo_preset.cpp
  patches/file_redirect.cpp
  patches/file_trace.cpp
  patches/frame_interp.cpp
  patches/frame_wait.cpp
  patches/input_update.cpp
  patches/profiler.cpp
  patches/texture_replace.cpp
  patches/widescreen.cpp
  sha256.cpp
  util.cpp
)
target_include_directories(thod2_core_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(thod2_core_lib PUBLIC THOD2_VERSION="${PROJECT_VERSION}")
target_link_libraries(thod2_core_lib PUBLIC bcrypt dxguid ole32 shell32 windowscodecs thod2_interop_lib
  thod2_platform_lib)

# The DLL that gets loaded into Hod2.exe. Only the C API in include/thod2/core.h is exported.
add_library(thod2_core SHARED dllmain.cpp)
target_compile_definitions(thod2_core PRIVATE THOD2_CORE_BUILD)
target_link_libraries(thod2_core PRIVATE thod2_core_lib)
