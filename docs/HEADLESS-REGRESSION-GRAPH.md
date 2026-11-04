# Optional frontends retain real offscreen regression dependencies

The UI-off/XR-off testing configuration previously failed generation because
the expression palette linked the missing `PkgConfig::RENDERFONTS` target. Image
tests also need the actual offscreen renderer. CMake now includes that renderer
and bitmap-font discovery whenever BUILD_TESTING is enabled. It does not drop
image tests or enable an application frontend implicitly. FLTK remains the
default frontend; UI-off/XR-off with testing disabled still configures without
renderer or font discovery.

The corrected configuration registers663 tests. A fresh optimized191-task build
of actual offscreen palette/background, compute and appearance consumers passes,
followed by all4 selected CTest commands. All tracked inputs and dependencies
remain exact. Default UI/XR-on configuration retains all689 commands and
properties after source/build-path normalization and removal of backtrace-only
metadata. Full663 execution and a new full frontend build are not claimed.

See validation/HEADLESS-REGRESSION-GRAPH.json for commands, original negative,
log hashes and precise scope. No renderer/core/math/public schema changes,
system installation, headset launch or active-session action are included.
