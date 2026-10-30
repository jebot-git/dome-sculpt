# Sculpt protection and symmetry — source20c132d

Full production objective remains active. Exact product source hashes from the
frozen candidate artifacts/verify-sculpt-ui-79fe match20c132d, including all179
src/include/third_party/CMake product files. The candidate excludes unrelated
primary normal-frame/creator drafts. Logs below are under artifacts/.

- Optimized CPU native73/73,173.67s: tests-sculpt-integrated-native-full.log.
- CPU real-loader synthetic OpenXR140/140, complete=true/failed0/exit0:
  tests-sculpt-ui-real-loader-full.log; candidate/build/xr-mask-full/report.json.
- Complete ONEAPI native FLTK/OpenXR application built with pinned DPC++7.2;
  actual Arc A770 brush compute +software EGL passes the same140/140 XR cases:
  tests-sculpt-integrated-oneapi-xr.log;
  candidate/build-oneapi/xr-sculpt-full/report.json. This separates real compute
  execution from graphics transport; it is not physical HMD evidence.
- Actual Arc native12/12,6.15s: tests-sculpt-integrated-oneapi-native.log.
  Includes184 independent endpoints,224 CPU-reference batches/445088 samples,
  maxerror8.9407e-08,2000007-sample chunking/concurrency, mask/controller/document
  workflows and explicit missing-GPU failure without host sample mutation.
  The selected native scope excludes the separate INSTRUMENTED fault backend.
- Whole-core C/C++ ASan/UBSan/leaks17/17,186.49s:
  tests-sculpt-integrated-sanitize.log; build-sculpt-integrated-sanitize.log.
  Mikk and xatlas are instrumented. Includes mask arithmetic/transactions,
  actual Document/controller, deterministic fsync cancellation/stale generation,
  global files/recovery, SourceUV adoption, Scene compatibility, worker shutdown,
  glTF and atlas. This is whole-core coverage, unlike earlier kernel-only proof.
- Native FLTK/DBusMenu controls17/17: tests-sculpt-native-menu.log; compact
 2xBDF5/5: tests-sculpt-native-menu-compact-all-fields.log. Actual pressure paints
  visible protection,25mm numeric input preserves untouched float coordinates,
  no-op Apply creates no history, worker Fill and complete Undo/Redo persist.
  Normal and compact beginning/middle/end screenshots inspected.
- Existing general menus53/53, Skin11/11, Unicode input/history/save3/3:
  tests-sculpt-{menu,skin-menu,unicode-menu}-regression.log. Exact source scripts
  copied into sculpt-ui-regression-root; build symlink targets the candidate.
- Actual canonical DomeD native Wayland mask9/9 and existing8/8:
  tests-sculpt-mask-dome-wayland-popup.log and
  tests-sculpt-dome-wayland-regression.log. No active compositor/session restart.
- Actual Arc and llvmpipe mask/mirror GPU38 assertions each:
  tests-sculpt-mask-render-{arc,software}.log. Independent linear pixel oracles,
  zero/sentinel exactness, PBR/MToon coverage, picking, weights, invalid data and
  object-local reflected cursor transforms. Existing MToon75 +visibility25
  assertions pass on both: tests-sculpt-mask-material-regression-{arc,software}.log.

The140 XR checks retain all117 previous cases and add23 mask cases: actual
joystick/ray/pressure edits, independently known mirrored positions, four-row
origins/visibility, Cancel/focus rollback, Fill publication/reopening and exact
Undo. Runtime-only fixture errors were corrected without changing product code
or weakening oracles: a second released sample rearms the source lease; accepted
Fill rebinds and closes the menu, so the test follows the real reopen route.
Initial failed focused logs remain. Temporary GL4.5 debug introspection was removed.

The new Wayland harness initially assumed a fixed heading order and ordinary
desktop rectangle for a native modal. Canonical export regrouping and spatial
popup placement require current headings plus parent anchors/work-area clamp.
Only test navigation/coordinates changed. Failed logs remain
 tests-sculpt-mask-dome-wayland{,-headings,-fields}.log; final9 checks preserve
all original assertions and existing8-case regression passes.

Canonical FLTK282467e Release headers/libs and matching compositor were used.
Explicit FLTK_DIR corrected a cached /opt selection before final UI verification.
No system installation, physical test relaunch or active session restart occurred.
Physical rudimentary confirmation remains scoped only to8f4d67d. AMD/CUDA execution,
production-scale performance and the remaining requirement rows are not complete.
Prior integration/history: VALIDATION-2c89e8d.md and its referenced archives.
