# Native plane-cut verification — 2026-10-30

Application source `5b0f9f5ff317031ae92b800d094a62be532a3cf8`, based on
`3d19776`, branch `work/native-plane-cuts`. The source commit contains the exact
product/test tree used below; subsequent handoff changes are documentation only.
Prior evidence is preserved in VALIDATION-3d19776.md and older archives.
Artifacts/build directories are ignored local evidence, absent in a fresh clone.

This branch is not integrated with newer remote main `2c89e8d` / source
`79fe64a`, discovered during checkpoint fetch. These counts do not validate that
main or the future combined merge. See HANDOFF.md for reconciliation requirements.

## Release and real-loader synthetic XR

All 51 registered Release CTests pass, zero failures, in 332.27s. Logs:
`artifacts/tests-plane-cut-full.log`, `-full-detail.log` and the preserved full
individual output `artifacts/tests-plane-cut-full-output.log`.
Final complete build: `artifacts/build-plane-cut-verified.log`.

The real-loader OpenXR client CTest passes in 221.64s.
`build/xr-client/report.json`: 73 checks, failed=0, complete=true, status=passed,
synthetic_tracking=true. Existing loader/rebase/stereo/pressure/menu/material/UV/
bake coverage is retained. The script does not specifically replay the new cut
workflow. Cut-specific controller/stylus cases instead exercise production
Document/SpatialInput plus separate GPU fixtures. Software GL and synthetic
tracking are not physical headset/controller verification.

The cut-authoring suite has 55 assertions. Coverage includes exact source mapping
versus a nearby competing surface, malformed mapping rejection, closed cap and
material topology, real editable paint/ORM/emission/normal transfer, UTF-8 group
name bounds, cancellation without partial publication, source/save isolation,
coalesced immutable previews, current-request validity, parameter rollback/local
history, single global Apply history, native save/open and glTF reimport, stale
replacement/no-op handling, transformed groups plus skin/morph displayed pose,
joystick multirow sliders, grip placement, left-menu ownership with both hands
tracked and accepted Apply immediately before XR shutdown. The existing
retopology authoring suite retains 56 checks.

GPU tests verify actual selected-source clipping, visible cap/plane feedback,
menu capture and restored unclipped state. Captures:
`build/gpu-cut-{surface,gizmo,menu}.ppm`. Six new V0/V1 MToon checks verify selected
clipping, exact kept-side shading and isolation from other object identities.
No new physical GPU or canvas-rotation-specific test is claimed for this stage.

## Whole-core sanitizers

Four C/C++ ASan/UBSan/leak suites pass in 45.28s: retopology-authoring,
cut-authoring, document-assets and bake. Logs:
`artifacts/tests-plane-cut-sanitize.log`, `-sanitize-detail.log` and
`artifacts/tests-plane-cut-sanitize-output.log`. Final successful instrumented
build: `artifacts/build-plane-cut-verified-sanitize.log`.

The build-sanitize configuration has UI/XR OFF, C and C++ core plus native
Mikk/xatlas instrumented with `-fsanitize=address,undefined`, frame pointers and
Release `-O1 -g1 -DNDEBUG`. Runs set
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. No TSAN or accelerator evidence is claimed.

## Native FLTK menus and bitmap appearance

Dedicated normal cut GUI flow passes 12/12, compact 640x360 with 2x Unifont BDF
passes 6/6. Both processes exit 0, both reports have failed=0. Logs:
`artifacts/menu-plane-cut.log`, `artifacts/menu-plane-cut-compact.log`.
Reports/captures: `artifacts/menu-cut`, `artifacts/menu-cut-compact`.

Actual universal menu export checks availability and checkbox state, source/save
isolation, live viewport feedback without authoritative Scene changes, native
modal ownership, Universal Close exact restoration, local Undo/Redo, cap toggle,
Apply/Save retained source and baked outputs, one global Undo/Redo and later
workspace cancellation. Compact checks verify four readable RGBA controls,
scrolling content with fixed Cancel/Done and source-preserving cancellation.
Visually inspected `menu-cut/cut-live-preview.png` and
`menu-cut-compact/cap-controls-after.png`. The former shows retained textured
geometry and the edge-on plane at the default camera; the latter shows readable
large bitmap labels/values without overlapping the action footer.

The existing general menu suite was not rerun this stage; its historical results
remain in VALIDATION-3d19776.md. Do not relabel those as current-source evidence.
Optional test orchestration uses dbus-next/python-xlib/pillow under private
HOME/bus/Xvfb with 90s cold-font startup and 30s interactions. Python is not a
runtime frontend or workflow dependency.

All 30 source_sha256 entries match `third_party/sources.json`. The shared BDF
helper is byte-identical to `../qimgvr/fltk/bdf_font.hpp`. Actual final linkage in
`artifacts/linkage-plane-cut.log` resolves FLTK from
`artifacts/toolkit-authoring/lib` with no Qt/GTK/Pango frontend. The toolkit is
unchanged pinned DomeD 282467e / 22 patches. Build/launch commands are in
HANDOFF.md; do not substitute older installed libraries.

## Diagnostics and limits

Earlier prototype/focused logs precede the final owner/paint/group assertions;
the final broad and sanitizer runs above include all refinements. An expanded
paint fixture initially used its default Y normal while painting a Z face;
setting the correct dab normal fixed the fixture. An owner-control fixture used
Up from row zero, which correctly wraps to Alpha; Down selects Green. The fixture
was corrected without changing the common input convention. Diagnostic logs
`tests-plane-cut-expanded.log` and `tests-plane-cut-owner.log` are failed
attempts, not successful evidence. Initial misleading-indentation warnings were
cleaned before the final builds. A test-insertion script pattern mismatch was
corrected before the final broad run.

Cuts currently operate on one active object. Repeated cuts of generated shell
and cap siblings, general Boolean operations, nested/touching contours and
self-intersection repair remain unfinished. Closed caps require consistently
oriented manifolds and reject ambiguous/near-vertex/empty cases. Extraction
expands selected render corners, so effective vertex budgets and production
compaction/performance need work. Output uses static PBR with 512-pixel atlases;
it does not recreate all original MToon/UV1/extensions or bind new skin/morphs.
Original assets remain retained. Durable recipes, quality controls, unconfirmed
draft recovery and the wider production objective remain incomplete.

Build/test handles were confirmed terminal and no matching validation/compiler
jobs remained at checkpoint inspection. No system installation, active-session
restart, fresh external Khronos validation, physical-device testing or real
OneAPI/ROCm/CUDA execution was performed for this checkpoint.
