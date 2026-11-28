# SPDX-License-Identifier: GPL-3.0-or-later
function(expect_cpu_refusal mode reference testing ui xr)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}" -B "${CMAKE_CURRENT_BINARY_DIR}/cpu-production-config/${mode}"
    -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${BUILD_PROGRAM}"
    "-DCMAKE_C_COMPILER=${C_COMPILER}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    -DCMAKE_BUILD_TYPE=Release -DDOME_COMPUTE_BACKEND=CPU
    "-DDOME_CPU_REFERENCE_TESTS=${reference}" "-DBUILD_TESTING=${testing}"
    "-DDOME_BUILD_UI=${ui}" "-DDOME_BUILD_XR=${xr}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
  if(NOT status STREQUAL "1" OR NOT error MATCHES "Dome Sculpt requires a GPU backend")
    message(FATAL_ERROR "CPU configuration ${mode} missed the GPU policy gate (exit ${status}).\n${output}${error}")
  endif()
endfunction()

expect_cpu_refusal(default-app OFF ON ON ON)
expect_cpu_refusal(desktop-with-reference ON ON ON OFF)
expect_cpu_refusal(xr-with-reference ON ON OFF ON)
expect_cpu_refusal(headless-without-opt-in OFF ON OFF OFF)
expect_cpu_refusal(headless-without-tests ON OFF OFF OFF)
