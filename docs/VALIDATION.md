# Authenticated local meshing/GPU/plane-cut verification — 2026-10-30

Application source `cf89a81c45d36c1ed538a3f24bbab56869d25e2f` combines source
`42b2ff3` and remote `a8af3f8` with both histories retained. Test-only `91ceb04`
changes two cut-fixture offsets; test-only `8b8dc60` adds native Unicode input
acknowledgement. Product code/headers remain identical to cf89a81. The full
production goal remains incomplete. Exact incoming proof is archived in
VALIDATION-a8af3f8.md, with prior scopes retained in their original archives.

Local logs and ignored binary/source/report manifest:
`artifacts/local-cut-gpu-final-manifest-8b8dc60.json`. They do not accompany a
fresh clone. The branch worktree is artifacts/local-cut-gpu-integration; logs
listed below are in the parent artifacts directory. Native CPU build, full
OneAPI build and whole-core sanitizer build use separate coherent directories.

## Toolchain and source identity

- Current DomeD HEAD rechecked2026-10-30:
  282467e727df78c8e7f2452aa05a7747bac854a6. Its canonical22 patches build optimized
  FLTK1.4.5 at artifacts/toolkit-282467e/**lib64**. CMake uses explicit FLTK_DIR
  ending share/fltk. Saved ldd reports verify all three FLTK libraries there;
  no Qt/GTK/Pango frontend library or unresolved dependency is linked.
- Local CPU Intel i7-12700, GCC16.2.1; Release uses-O3/-DNDEBUG. OneAPI uses the
  pinned official DPC++7.2 compiler5680988bb38bd45f13081394fba43588c5f5e395 and
  archive385586a25e75ac727883f0be5e442dec0ee81b3c20f9a29037a79921ad2f1728.
  Existing separate libumf compatibility/provenance is unchanged. Runtime uses
  ONEAPI_DEVICE_SELECTOR=level_zero:0 and actual Arc A770; no CPU substitution.
- Whole-core C/C++ sanitizer archives and renderer/test consumers were rebuilt
  coherently with-O1/-g1/ASan/UBSan/frame-pointers, leak detection and halt-on-error.
  UI is disabled for this build; direct native EGL render consumers are included.
  An initial target-list typo stopped after existing targets; the follow-up
  completed the correctly named mask/render/native targets before testing.

## CPU Release and synthetic XR

All80 non-XR native suites pass in484.25s (serial run alongside other isolated
validation). Log tests-local-cut-gpu-native.log and retained-detail.log. This is
regression-suite wall time, not a stroke/frame performance measurement. It
includes cut authoring/cancellation, source paint/bake/UV/material/skin/morph/VRM,
scene/Volume compatibility, exact allocation-failure transactions, six-worker
shutdown, local extraction/journal ownership, tiny geometry and actual GL checks.

CPU real-loader synthetic report build/xr-local-cut-gpu/report.json passes
150/150, complete=true, status=passed, failed=0; log tests-local-cut-gpu-xr.log.
It retains all140 earlier checks and10 cut checks. Tracking is synthetic and
EGL uses llvmpipe, while real GL textures/eye ownership, joystick/ray/pressure,
focus/rebase/restart, accepted/unconfirmed shutdown, cancellation/source identity
and native saved geometry are exercised. Original per-client40s limits remain.
No physical HMD/controller/stylus/device performance claim follows.

## Actual Arc OneAPI and rendering

Fresh complete native FLTK/OpenXR OneAPI build passes17 selected native suites
in7.80s. Logs tests-local-cut-gpu-oneapi-native.log and -native-detail.log. Exact
selected suites are recorded in the log, including actual sculpt/controller/
mask kernels, local surfaces, cuts, file/geometry/shutdown regressions and the
explicit missing-GPU failure. The compute oracle reports actual Intel Arc A770,
184 independent endpoints,224 CPU-reference batches/445088 samples, maximum
error8.9407e-08, bounded2M chunk/concurrency and no silent CPU fallback.

The complete OneAPI real-loader run also passes150/150, complete=true/failed=0:
build-oneapi/xr-local-cut-gpu/report.json and tests-local-cut-gpu-oneapi-xr.log.
Its tracking/GL transport is synthetic/software, while eligible brush computation
executes on Arc. This distinction is preserved in the manifest.

Separate direct EGL fixtures require hardware and print actual Mesa Intel Arc
A770/DG2. They all pass:

| Fixture | Assertions | Log suffix after tests-local-cut-gpu-oneapi- |
| --- | ---: | --- |
| Actual Document → GL buffers |22| document-arc.log |
| Buffer traffic/UV1/cache/error recovery |24| buffers-arc.log |
| PBR/MToon/linear/outline/clipping |81| mtoon-arc.log |
| Protection/mirrors/scalar overlay |38| masks-arc.log |
| Material visibility/picking |25| visibility-arc.log |

The actual Document fixture wraps real glBufferData and checks independent
pixels. Mask/material/texture-only edits retain geometry, a real local Carve
invalidates it, object transforms reuse extraction but update world buffers,
cap geometry gets a unique key, the second eye reuses it, cap-free planes update
uniforms without geometry uploads and Cancel restores exact pixels/source/key.
Scalar streams still upload on changed snapshots; geometry edits still upload
complete arrays. No per-tile GPU, frame-rate or production completion claim.

## Whole-core sanitizers

24/24 pass345.57s with ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. Logs
build-local-cut-gpu-sanitize.log, -sanitize-render.log,
tests-local-cut-gpu-sanitize.log and -sanitize-detail.log. The first build log's
unknown target is the corrected invocation typo described above, not a passing
build claim. All archives/tests in the executed set match current headers/core.

Coverage: cut-authoring/cancellation-boundaries, controller-controls,
texture-painting, scene-authoring, shared-document, mesh-degeneracy, all five
mask/controller/document/symmetry/dispatch/transaction tests, sculpt-geometry,
sculpt-layers, bake, scene-schema-compat, six-worker shutdown, local-surface,
authenticated-local-surface, actual Document buffers, renderer buffers, mask
renderer, MToon renderer and visibility/picking. The ordered asset cancellation
barrier and real allocation-failure/rollback cases remain active.

## Native GUI and actual DomeD compositor

All125 assertions pass across private buses/displays/projects; no active session
was restarted. Normal/compact cut screenshots were inspected. Logs:

| Workflow | Assertions | Log suffix after tests-local-cut-gpu- |
| --- | ---: | --- |
| General FLTK/universal menu |53| menu-final.log |
| Native cut workspace |12| cut-menu.log |
| Compact640×360/2×BDF cut controls |6| cut-compact.log |
| Mask/symmetry/worker/history |17| mask-menu.log |
| Compact mask controls |5| mask-compact.log |
| Native skin/bind/transfer |11| skin-menu.log |
| Unicode keyboard/clipboard/save/history |4| unicode-acknowledged.log |
| Actual DomeD Wayland mask menu |9| mask-wayland.log |
| Actual DomeD Wayland/menu/history |8| wayland.log |

Optional Python orchestration uses cached uv environments with dbus-next,
python-xlib and Pillow; these are not product runtime dependencies. The actual
DomeD compositor is the local canonical282467e release. GVFS/private-bus teardown
messages do not replace assertions or process status; both compositor commands
exit0 and reports pass. General menu behavior retains shell-owned Window actions,
Ctrl+W modal cancellation, native appearance/BDF and application identity.

An initial system-python invocation lacked optional dbus-next and stopped before
GUI startup. After using the existing cached environment, an initial Unicode run
saved one missing injected character. Inspection showed the temporary keymap was
restored after XSync, which acknowledges only the server. An unchanged isolated
run passed; the strengthened fixture retains the map until the actual native
UTF-8 clipboard returns every expected character. It passes4 checks, including
the original exact model/clipboard/history/save assertions. Earlier failure and
isolated logs are retained; no product Unicode code or expected bytes were relaxed.

## Earlier performance and physical scope

Pre-cut42b2ff3 has its own immutable manifest
artifacts/local-gpu-final-manifest-42b2ff3.json:78 native/CPU140/Arc140/Arc15native,
Document15/buffer24 and nativeGUI/compositor evidence. Authenticated producer886
has separate21-suite whole-core sanitizer proof and actual24-edit API-to-ready
measurements in AUTHENTICATED-LOCAL-MESHING.md. Those timings compare against
extraction-only oracles, not a baseline Document/GPU frame time, and do not apply
as production guarantees to this combined source.

Only8f4d67d has the user's rudimentary physical WiVRn confirmation. That test was
ended and recovery snapshots/report retained. No new physical test, installation,
session/display-manager restart, AMD/CUDA execution or full production completion
is claimed. Remaining requirements, private branches and bottlenecks are explicit
in HANDOFF.md and REQUIREMENTS.md.
