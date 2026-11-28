# SPDX-License-Identifier: GPL-3.0-or-later
set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/application-missing-gpu")
file(REMOVE_RECURSE "${output_dir}")
file(MAKE_DIRECTORY "${output_dir}")

function(expect_gpu_refusal mode)
  execute_process(COMMAND "${APP}" ${ARGN}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
  if(NOT status STREQUAL "1" OR NOT error MATCHES "Dome Sculpt requires an available ${BACKEND} GPU")
    message(FATAL_ERROR "${mode} did not report the unavailable compute GPU (exit ${status}).\n${output}${error}")
  endif()
endfunction()

expect_gpu_refusal("Desktop" --smoke-test "${output_dir}/desktop.ppm")
if(WITH_XR)
  expect_gpu_refusal("OpenXR" --xr --frames 1 --save "${output_dir}/xr.dsculpt")
endif()
expect_gpu_refusal("Demo export" --export-demo "${output_dir}/demo.glb")
expect_gpu_refusal("Project export" --export-project "${output_dir}/input.dsculpt" "${output_dir}/project.glb")
file(GLOB outputs "${output_dir}/*")
if(outputs)
  message(FATAL_ERROR "Dome Sculpt published output without an available compute GPU: ${outputs}")
endif()
