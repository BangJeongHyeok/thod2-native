# Reimplemented platform layer (input, later timing, audio, files).
# Works on views of the game's memory, so it builds and tests without the game.
add_library(thod2_platform_lib STATIC input/input.cpp)
target_include_directories(thod2_platform_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
