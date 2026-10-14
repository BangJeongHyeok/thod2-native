# Helpers for hooking and patching the original Hod2.exe.
# Linked statically into the loader and the core.
add_library(thod2_interop_lib STATIC iat_hook.cpp)
target_include_directories(thod2_interop_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
