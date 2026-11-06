# Explicit manual consumer: production runs must never enter the default CTest set.
if(BUILD_TESTING AND TARGET dome-sculpt-render)
  add_executable(dome-sculpt-sculpt-sustained EXCLUDE_FROM_ALL tests/sculpt-sustained.cpp)
  target_include_directories(dome-sculpt-sculpt-sustained PRIVATE src third_party/desktop)
  target_link_libraries(dome-sculpt-sculpt-sustained PRIVATE dome-sculpt-render)
  target_link_options(dome-sculpt-sculpt-sustained PRIVATE
    -Wl,--wrap=glBufferData -Wl,--wrap=glBufferSubData)
  # Use the same declarations as the complete consumer, avoiding a second ABI list.
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sculpt-sustained-metrics.hpp"
    dome_sustained_wrappers REGEX "^#define SUSTAINED_[A-Z]+ \"")
  list(LENGTH dome_sustained_wrappers dome_sustained_wrapper_count)
  if(NOT dome_sustained_wrapper_count EQUAL 7)
    message(FATAL_ERROR "Sustained consumer requires its seven complete metric wrappers")
  endif()
  foreach(dome_sustained_wrapper IN LISTS dome_sustained_wrappers)
    string(REGEX MATCH "\"([^\"]+)\"" dome_sustained_symbol "${dome_sustained_wrapper}")
    target_link_options(dome-sculpt-sculpt-sustained PRIVATE "-Wl,--wrap=${CMAKE_MATCH_1}")
  endforeach()
endif()
