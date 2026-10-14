# Logging dinput.dll proxy used to decide the loader design (docs/work/loader.md).
add_library(dinput_probe SHARED dinput_probe.cpp dinput_probe.def)
set_target_properties(dinput_probe PROPERTIES OUTPUT_NAME dinput)
