# dinput.dll proxy that loads thod2_core.dll into Hod2.exe (docs/work/loader.md).
add_library(thod2_loader SHARED dinput_proxy.cpp dinput_proxy.def)
set_target_properties(thod2_loader PROPERTIES OUTPUT_NAME dinput)
target_link_libraries(thod2_loader PRIVATE thod2_interop_lib)
# The core DLL is loaded at run time, but it must be built next to the loader.
add_dependencies(thod2_loader thod2_core)
