# Source-aware bake / rig integration — 2026-10-29

This stage joins b879ce4 and c95208b, preserving both histories and the complete
production objective. Their original records are archived in
VALIDATION-{b879ce4,c95208b}.md and corresponding HANDOFF files. Those historical
results must not be relabeled as verification of the combined tree.

## Release evidence

The combined application/core/XR and all 40 registered tests build successfully:
artifacts/build-source-bake-verified.log. Configure and earlier incremental build
logs use configure-current-integration.log / configure-source-bake.log and
build-current-integration.log / build-source-bake.log / build-source-bake-final.log.

The broad run passed 39/40 suites in 286.20 seconds:
artifacts/tests-source-bake-final.log. gltf-interoperability failed because the
merged fixture reset its rig maps to a normal-less texture bundle while still
asserting four images and a normal binding. The fixture now restores the actual
normal-bearing bundle; the strong four-image/normal assertion is retained.

After the final worker/renderer/export corrections and a fresh rebuild, all nine
affected suites pass in 17.84 seconds: artifacts/tests-source-bake-verified.log.
They cover glTF/PNG, real software GL, source GPU/maps/preview, native baking,
full sculpt/paint/voxel/atlas/bake/save/reopen/export workflow and document assets.
Detailed output is preserved in tests-source-bake-verified-detail.log.

The last two evidence-focused targets pass in 2.46 seconds:
artifacts/tests-source-bake-evidence.log and corresponding -detail.log. Final
baked-textures has 38 assertions, including the zero-preliminary-composite check;
source-gpu has 42 assertions including actual female/male profiles. Thus all 40
registered suites have passing evidence across the broad and final targeted runs;
there was not a single 40/40 final-source run. The final targeted repeats were
required by actual fixture/source changes and the stale-object audit, rather
than a blanket repetition of unchanged suites.

New assertions independently check rotated/mirrored/singular original normal
views, raw normal/AO/PBR/alpha factors, authoritative explicit tangent frames,
protected original normal detail, exact source retention, raw/baked/clear/Undo
preview selection and held immutable snapshots. GPU fixtures compare the actual
production shader's original-source output with the native bake (within 8-bit
quantization), then compare a mixed-source bake to a bake-only reference exactly.
Independent cgltf plus PNG checks verify baked normal bytes and neutral scale/
UV metadata take precedence over conflicting retained originals; composite AO
uses ORM red once. This is generic PBR evidence, not complete MToon/VRM parity.

The broad real-loader OpenXR suite passed in 276.50 seconds. Its persisted report
has 53 checks, zero failures, complete=true/status=passed:
build/xr-client/report.json. All tracking/controllers are synthetic. Release GPU
fixtures explicitly use software GL; no physical headset or hardware-accelerator
completion follows from them. The GPU fixture now labels software-permitted
runs accurately instead of printing a hardware-only assertion label by default.

## Whole-core sanitizer evidence

Seven selected ASan/UBSan/leak suites pass in 158.54 seconds:
artifacts/tests-source-bake-sanitize.log. Includes glTF, baked textures, full bake
workflow, actual source maps, native weight editing, source preview and rig editing.
Both C and C++ instrument the complete project core and Mikk/xatlas dependencies;
UI/XR are OFF. Configure uses Release with -O1 -g1 -DNDEBUG, sanitizer flags and
frame pointers retained. ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1 apply to the tests. This closes the unfinished
merged-core sanitizer rebuild recorded in the b879ce4 handoff.

The final expanded baked-textures target is rebuilt/relinked against the latest
core and passes separately in 0.56 seconds:
artifacts/tests-source-bake-sanitize-evidence.log, with full output in the
corresponding -detail.log. This includes all 38 assertions and the direct-source
worker path. Logs and CMake cache retain the actual flags; no diagnostic was
suppressed and C instrumentation was not omitted.

## Native frontends and provenance

- Native FLTK/private DBusMenu general suite: 53/53, exit 0, failed=0;
  artifacts/menu-source-bake-final.log and artifacts/menu/report.json.
- Native rig/shapekey modal Apply/cancel/global Undo/Redo suite: 8/8, exit 0,
  failed=0; menu-source-bake-rig.log and menu-rig/report.json.
- Real FLTK existing scene-v6 baked-asset compatibility regression: 5/5, exit 0,
  complete=true/status=passed/failed=0; gui-source-bake-final.log and
  gui-baked-surface/report.json. Paint overlays, exact GPU Undo and owned UV
  cancellation retain the canonical normal-mapped preview.
- All 30 pinned source_sha256 entries match. Mikk's marked C seed-rotation fix,
  reproducible patch and original/patched hashes remain unchanged.
- artifacts/linkage-source-bake-final.log resolves the canonical local 22-patch
  FLTK toolkit and contains no Qt/GTK/Pango runtime frontend. Python remains
  optional test orchestration; runtime and bake/weight operations are native.

The first test build needed an explicit nlohmann/json.hpp include for the new
preview persistence assertion. A build audit also caught an older baked-test
object compiled before added cases; that target was explicitly rebuilt and its
new assertions verified in detailed output. Neither initial output is reported
as verification of the expanded cases. Original license notices remain intact.

## Remaining scope

FLTK/XR bake authoring controls and the complete production requirement matrix
remain unfinished. CPU source sampling is bilinear at texel centers; source mip
footprints/minification, full authored-tangent/UV1/vertex-color rendering,
production-size timing/memory/cancellation and complete deformed-consumer
interoperability need further work. The raw derivative and canonical baked
normal conventions are explicit; no claim of every glTF material extension is
made. No fresh external Khronos validation, physical device/backend completion,
system install or active-session restart occurred in this stage.
