# Baked-surface / creator integration checkpoint — 2026-10-29

This record describes the merge of baked stage 72d9127 and creator baseline
811eb69. The separate historical records are VALIDATION-72d9127.md and
VALIDATION-811eb69.md; do not relabel their results as merged-source evidence.
Newer remote main (observed c95208b) is outside this checkpoint. See HANDOFF.md
for publication branch, limitations and the full resume sequence.

## Current source checks

- Combined Release configure/build completed successfully, including native FLTK
  application, OpenXR, MikkTSpace and all 35 registered CTest targets:
  artifacts/configure-baked-creator-merge.log and
  artifacts/build-baked-creator-merge.log.
- Focused baked-textures test passes all 30 assertions:
  artifacts/tests-baked-creator-cross.log. New integration cases cover legacy
  standalone baked-v5 upgrade to v6, mixed original shared image archives with
  baked payloads, composite-cache invalidation/source retention and grouped
  world-scale cage rejection/success. Existing tangent, normal/AO, UV transfer,
  exact source/layer/rig retention, cancellation, Undo and ordered save cases pass.
- Retained end-to-end native sculpt/paint/voxel/atlas/bake/reopen/GLB/render run
  passes on scene v6: artifacts/workflow-baked-creator-merge.log. Reproduce with
  build/dome-sculpt-bake-workflow-test --artifacts artifacts/bake-workflow-v6.
- Real FLTK scene-v6 bake regression passes 5/5 with complete=true,
  status=passed and zero failures: artifacts/gui-baked-creator-merge.log and
  artifacts/gui-baked-surface/report.json. Includes painting over the base,
  exact GPU-pixel Undo and owned UV cancellation. Private bus/display/home;
  no user document or active session changed.
- All 30 third_party/sources.json source_sha256 entries match their files.
  Linkage recorded in artifacts/linkage-baked-creator-merge.log resolves the
  canonical local toolkit and contains no Qt/GTK/Pango runtime frontend.
- Integration diff versus 811eb69 passes git diff --check. Original imported
  upstream license/notice files retain their upstream whitespace and headings;
  a whole-history diff check flags those verbatim source notices.

## Release and menu verification

All 35 Release CTests pass in 297.96 seconds, zero failures:
artifacts/tests-baked-creator-merge.log. Includes independent glTF/PNG and
actual software-GL shading, source archives/UV/cache, groups/creator, voxel,
paint, document/worker/history, MToon and native bake workflow suites. The
real-loader synthetic OpenXR suite passes in 266.64 seconds; its persisted
report contains 53 checks, zero failures, complete=true and status=passed
(build/xr-client/report.json). Actual female/male composite-cache profiles
pass in 125.82 seconds under the explicit memory/entry bounds. These timings
include concurrent validation and are not production performance benchmarks.

Universal menus pass 53/53, zero failures, with the script exiting 0:
artifacts/menu-baked-creator-final.log and artifacts/menu/report.json. Includes
creator isolation/cancel, voxel remesh/feedback/Undo, text history, exact UV Undo
and Redo, native chooser cancellation and host fallback/restoration. The first
merged run compared pixels immediately after dispatch before GL redraw. The
fixture now waits for the same exact pixels before asserting Redo; it does not
relax the expected image. That initial failed run remains diagnostic evidence
in artifacts/menu-baked-creator-merge.log, not a pass. A direct system-Python
attempt lacked optional dbus_next; the documented uv test environment was used.

## Pending merged sanitizer validation

The merged sanitizer configure succeeded. Its rebuild was intentionally stopped
for the user's checkpoint/handoff request (exit 143), before any sanitizer tests
started. This is not a compiler diagnostic or a passed merged sanitizer run.
Logs: artifacts/configure-baked-creator-sanitize.log and
artifacts/build-baked-creator-sanitize.log. No sanitizer build remains active.
Resume with the existing configured build rather than deleting its cache:

```sh
cmake --build build-sanitize --parallel 2 --target \
  dome-sculpt-baked-textures-test dome-sculpt-composite-cache-test
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure \
  -R '^(baked-textures|composite-cache)$'
```

Both C and C++ have ASan/UBSan/frame-pointer instrumentation; UI/XR are OFF.
Prior successful whole-core sanitizer evidence is archived under the separate
72d9127/811eb69 records and must not be reported as verification of this merge.
Broader merged affected-source/archive/group sanitizer checks remain advisable
once the two selected integration targets complete.

## Scope of evidence

CPU/software GL and synthetic real-loader OpenXR fixtures do not establish
physical headset/controller/stylus/3D-mouse behavior, production-size latency or
accelerator backends. No fresh external Khronos validator or system install is
claimed. Baked normal bindings/bytes and GPU shading do not prove full posed
consumer interoperability. Raw original source normal views explicitly reject
object baking until their sampling/tangent semantics are integrated; original
archives remain retained. Full FLTK/XR bake controls remain unfinished.
