# SPDX-License-Identifier: GPL-3.0-or-later
set(stage "${CMAKE_CURRENT_BINARY_DIR}/cpu-reference-install-stage")
foreach(component IN ITEMS all Unspecified)
  file(REMOVE_RECURSE "${stage}")
  set(args --install "${CMAKE_CURRENT_BINARY_DIR}" --prefix "${stage}")
  if(component STREQUAL "Unspecified")
    list(APPEND args --component "${component}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" ${args}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(status STREQUAL "0" OR NOT error MATCHES "for tests and cannot be installed"
      OR EXISTS "${stage}")
    message(FATAL_ERROR "CPU reference install (${component}) did not refuse before staging files.\n${output}${error}")
  endif()
endforeach()
