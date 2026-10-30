# Native polygon workspace milestone

This private checkpoint extends the existing C++ FLTK/OpenXR application. It
combines the published 8ad6d30 baseline with the separately qualified polygon
kernel, native N-gon source recovery, Scene12 retained-source archive,
cancellable atlas overload and PolygonAuthored paint policy. It is a usable
scoped milestone; the complete production objective remains active.

Mesh / Polygon edit opens a document-owned workspace. Shift-click selects on
the desktop; Move selection uses Shift-drag on a view-facing plane. The existing
universal menu exposes vertex/edge/face selection, Move, extrude, inset, bridge,
dissolve, weld, edge cuts, connect, delete and flip. Native settings use scene
millimetres and a logarithmic distance slider. Controller pages19/22 provide
large selection and topology controls; the shared24-page registry preserves
holding20, import21/options23 and all earlier authoring pages. A short depth-tested
aim line replaces the unrelated sculpt brush rings during polygon editing.

Stable vertex/edge/face/corner identities and ordered N-gons remain authoritative;
the triangle mesh is a derived display/painting surface. Component motion uses
immutable buffers and a GPU displacement preview. A selected group remains
selected when a drag begins, including additive mode. Local Undo/Redo and Cancel
do not mutate committed source or global history. Ctrl+W dismisses local settings
or cancels the workspace before closing the main window.

Expensive picking, topology mutation, atlas/raster preparation and validation run
on an eighth ordered worker outside the UI/model mutex. Bounded absolute motion
updates can coalesce without losing the final displacement; queue overflow is
an explicit refusal. Source generation/revision/object/edit authority, geometry,
paint, deformation, material and polygon identities are checked again at
publication. Apply publishes one history entry; unchanged Apply publishes none.
An accepted-token endpoint orders immediate Save/Close and shutdown draining.
A dependent Save refuses cancelled/failed acceptance instead of writing an older
scene. Held same-object motion uses the effective object-local workspace; accepted
geometry freezes and commits its captured pose in the same transaction. Explicit
unhold cancels pending geometry and its dependent Save. Existing deferred weight
release semantics remain independent.

Sculpt entry prepares editable topology without changing the Volume. Applying a
real edit creates a bounded UV atlas and keeps the original complete sculpt,
layers, masks, physical band and settings in an immutable source capsule. Explicit
conversion of posed/morphed meshes captures the evaluated visible shape and keeps
the full original rest mesh, skin, morphs, pose, materials and source archives for
Restore. Delete-all captures the latest complete source before exposing an empty
active authority: no stale sculpt/paint fallback is rendered or editable. Restore
is explicit and undoable; Cancel is the default in the XR source-choice panel.
Starting Save cancels an unconfirmed source choice rather than executing it.

Existing paint tiles/layer identities and properties stay exact when their corner
UV correspondence can be preserved. PolygonAuthored is distinct from
SourceAuthored: standalone decoding still refuses it, and Scene12 requires an
active nonempty authority with exact REST positions, ordered indices and UVs for
every current and retained painted owner. Scalar edits reuse only strong immutable
(authority, mesh owner) pairs that were already deeply validated. Older/unbound
records and PA paints inside unrelated SourceUvOrigin records refuse. UV editing
updates the matching polygon corner UVs and stable IDs; incompatible internal
chart/pin splits require cutting the face first.

## Completed evidence

Fresh CPU Release uses the current eee23b7 bitmap runtime and23-patch FLTK prefix;
compile flags exclude the old /opt toolkit. The Document fixture passes103 checks,
including the actual default30mm/.4m sculpt entry, real extrusion/atlas,
save/reopen/Restore, full original paint, local/global history, delete-all,
posed/morphed conversion, held capture/focus/unhold and an injected worker
allocation failure with exact content/history/revision rollback. Controller31
checks include real ray/pressure selection, pages19/22, source Restore/default
Cancel and staged-Restore then Save. Logs are
`artifacts/tests-polygon-latest-{document,controller}.log`.

The synthetic real OpenXR loader passes41 checks across accept, cancel, no-op,
unconfirmed, focus, Restore, Restore Cancel and default Cancel. Both real GL eyes
and session ownership balance. Rendered enabled controls acknowledge readiness;
focus cancellation first observes an actual GPU component move, excluding cursor
colors from its surface-change oracle. Exact complete source/paint/sculpt archive
comparisons remain. See `artifacts/tests-polygon-latest-xr-final.log` and
`build-polygon-latest/polygon-xr/report.json`. This is not physical headset proof.

Native FLTK/DBusMenu normal10, actual default-sculpt10 and compact13 checks
pass: component selection, local Undo/Redo, Cancel, Ctrl+W, Apply/immediate Save
and one global Undo/Redo. The640x360 large-font dialog measures wrapped bitmap
text, scrolls to every remaining setting and keeps Cancel/Apply fixed. Initial
readiness/oversized-window fixture errors and the actual overlapping-help defect
were corrected; final captures were viewed. Exact final logs are
`artifacts/tests-polygon-latest-menu-final-{normal,compact,sculpt}.log` and
captures/reports are in `artifacts/polygon-menu*`.

The15 selected fresh Release suites pass in10.11 seconds; the same15 complete
own-core/support C/C++ ASan/UBSan suites with leak checking pass in127.76 seconds.
This includes kernel/workspace/source allocation fixtures, UV cancellation,
Document/controller, paint witness, Scene12 archive, renderer and all eight worker
shutdown handshakes. See `artifacts/tests-polygon-latest-{release,sanitize}.log`.
The current fresh renderer also passes25 checks on actual Intel ArcA770, including
immutable geometry reuse, GPU motion/selection, zero CPU read/map traffic,
staging recovery, state/lifetime and pixels. See
`artifacts/tests-polygon-latest-render-arc.log` and `artifacts/polygon-final-arc`.
Earlier frozen kernel/source and direct paint/UV/Scene12 receipts remain separate
proof, not substitutes for this combined workflow.

## Remaining scope and explicit refusals

This milestone does not provide every Wings3D operation or primitive creation UI.
Kernel cube/grid primitives exist; richer native primitive and nonplanar/smooth
normal workflows remain. Strict manifold/planarity/quality limits refuse edits
that cannot be represented safely. New faces without useful inherited UVs report
that UV editing/unwrapping is needed; overlapping authored UVs are not called
unique. Raw COLOR_0 modulation and UV1 material roles still require a proven
canonical conversion; the separate general-color projection/receipt work must
be integrated without losing current appearance or inventing authority from a
retained origin. Affine/sheared source placement is not relabelled as a similarity
transform. The later source-affine/archive work must preserve that full placement.

No full combined old-regression suite, complete OneAPI workflow, system
installation or physical headset test is claimed here. Generic persistent JSON
allocation-denial fixes are a separate latest-main integration input; the small
paint-policy fault fixture does not prove all Scene encode/import boundaries.
Cooperative atlas cancellation retains its single vendor-iteration latency limit.
