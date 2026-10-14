add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE thod2_core_lib)
# core_tests also loads thod2_core.dll from its own folder.
add_dependencies(core_tests thod2_core)

add_test(NAME core_tests COMMAND core_tests)
