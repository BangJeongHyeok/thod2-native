# Logging dinput.dll proxy used to decide the loader design (docs/work/loader.md).
add_library(dinput_probe SHARED dinput_probe.cpp dinput_probe.def)
# Built into bin/experiments so it does not overwrite the real loader (also named dinput.dll).
set_target_properties(dinput_probe PROPERTIES
  OUTPUT_NAME dinput
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/experiments"
)
