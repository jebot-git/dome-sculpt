# Explicit production workflow: full-size fields must not enter default CTest.
if(BUILD_TESTING)
  add_executable(dome-sculpt-sculpt-independent-workflow EXCLUDE_FROM_ALL tests/sculpt-independent-workflow.cpp)
  target_include_directories(dome-sculpt-sculpt-independent-workflow PRIVATE src third_party/nlohmann)
  target_link_libraries(dome-sculpt-sculpt-independent-workflow PRIVATE dome-sculpt-core)
endif()
