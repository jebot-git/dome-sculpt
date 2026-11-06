# Explicit manual native gate. P1 is opt-in argv, never default CTest work.
if(BUILD_TESTING)
  add_executable(dome-sculpt-sculpt-placed-workspace-test EXCLUDE_FROM_ALL tests/sculpt-placed-workspace.cpp)
  target_include_directories(dome-sculpt-sculpt-placed-workspace-test PRIVATE src)
  target_link_libraries(dome-sculpt-sculpt-placed-workspace-test PRIVATE dome-sculpt-core)
  target_link_options(dome-sculpt-sculpt-placed-workspace-test PRIVATE
    "-Wl,--wrap=_ZN4dome6detail17SculptSceneChunks7captureERKNS_5SceneENS0_23SculptSceneChunkContextERKSt10shared_ptrIKS1_ENS0_22SculptSceneChunkLimitsESt8functionIFbvEE")
endif()
