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

# re/*.csv format and their links to the code (hod2_addresses.h, "Reimplements:").
# Needs no game files. Skipped when no Python 3 is installed.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME re_tables COMMAND Python3::Interpreter -I ${PROJECT_SOURCE_DIR}/tools/re/check_re.py)
else()
  message(STATUS "Python 3 not found: re_tables test skipped")
endif()

add_executable(platform_tests platform_tests.cpp)
target_link_libraries(platform_tests PRIVATE thod2_platform_lib)
add_test(NAME platform_tests COMMAND platform_tests)
