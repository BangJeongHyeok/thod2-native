add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE thod2_core_lib)
# core_tests also loads thod2_core.dll from its own folder.
add_dependencies(core_tests thod2_core)
add_test(NAME core_tests COMMAND core_tests)

add_executable(loader_tests loader_tests.cpp)
target_link_libraries(loader_tests PRIVATE thod2_interop_lib dxguid)
# loader_tests loads dinput.dll (the loader), which loads thod2_core.dll.
add_dependencies(loader_tests thod2_loader thod2_core)
add_test(NAME loader_tests COMMAND loader_tests)

# Both tests write thod2-native.log in the output folder.
set_tests_properties(core_tests loader_tests PROPERTIES RUN_SERIAL TRUE)
