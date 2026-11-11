if(BUILD_TESTING AND TARGET dome-sculpt-render)
  # Explicit manual GPU/oracle gate; never substitutes the separate-actor537.
  add_executable(dome-sculpt-texture-set-source-render-test EXCLUDE_FROM_ALL
    tests/texture-set-source-render.cpp)
  target_include_directories(dome-sculpt-texture-set-source-render-test PRIVATE src)
  target_link_libraries(dome-sculpt-texture-set-source-render-test PRIVATE dome-sculpt-render)
endif()
