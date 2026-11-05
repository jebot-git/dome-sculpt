# Current-main frontend-off renderer and fonts

Public `55b1c89` already contains runtime `a08c9d9`. Its CMake discovery gate is
`DOME_BUILD_UI OR DOME_BUILD_XR OR BUILD_TESTING`, so actual offscreen image and
palette tests retain their renderer/font targets when both frontends are off.
Line blame and ancestry confirm this; no duplicate CMake patch is needed.
Historical seal `26a0704` is not an ancestor, and its older pending-admission
wording does not describe the current runtime. The genuine missing-font-target
failure on the older native36/e77 component remains preserved separately.

A fresh optimized CPU build on exact current main completes198 tasks for the
actual core, renderer, support libraries and seven native consumers in461.90s.
All7 selected CTest commands pass in23.86s: compute, session appearance, Unicode
bitmap text, spatial rendering, source UV1, a real VRM producer and expression
palette. The actual renderer is Mesa llvmpipe; the five independent UV1 image
comparisons have error0, and83 expression-palette checks pass. This is real
windowless EGL rendering and native bitmap-font behavior, without a hardware,
physical headset or application/OpenXR launch claim.

All2142 tracked inputs,20 compiled/config artifacts and48 bitmap library names
remain exact through the test boundary. No borrowed archives, overlay objects,
runtime/header/test edits, Qt fallback, installation or active-session action
are involved. The receipt retains the initial orchestration assertion that
mistook XR control unit-test names for a frontend; exact application/library
target checks corrected that observer before any compilation or test execution.

Default FLTK/XR configuration still registers740 tests. A configuration-only
counterfactual with the old gate produces identical complete registration
commands/properties after path normalization; both options remain ON by default.
Frontend-off testing registers714 commands, retains real renderer/font targets
and native XR control unit tests, and creates no application or XR library.
With testing also OFF, core-only mode registers0 tests and discovers neither
renderer nor fonts. No configuration discovers Qt6. Full714/default740 execution,
a new frontend build and current-main sanitizer execution are not claimed.

With the current canonical bitmap runtime in `PKG_CONFIG_PATH` and
`LD_LIBRARY_PATH`, reproduce the selected frontend-off workflow:

```
cmake -S . -B build-headless -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DDOME_COMPUTE_BACKEND=CPU -DDOME_BUILD_UI=OFF -DDOME_BUILD_XR=OFF
cmake --build build-headless --parallel 2 --target \
  dome-sculpt-compute-backend-test dome-sculpt-appearance-test \
  dome-sculpt-palette-text-test dome-sculpt-render-test \
  dome-sculpt-source-uv1-render-test dome-sculpt-vrm1-background-fixture-test \
  dome-sculpt-vrm1-expression-palette-test
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 ctest --test-dir build-headless \
  -R '^(compute-backend|session-appearance|palette-unicode|spatial-gpu-render|source-uv1-render|vrm1-background-fixture|vrm1-expression-palette)$' \
  --output-on-failure --parallel 2
```

Read [the receipt](validation/CURRENT-HEADLESS-QUALIFICATION.json) for exact
source/history, canonical paths, build flags, graphs, binary/runtime hashes,
commands and retained logs. This milestone is documentation and current-main
qualification only. Private native36/canonical37 composition and all unfinished
authoring requirements remain separate.
