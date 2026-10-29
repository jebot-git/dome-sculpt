# Editable UV assets

Prepare a painting mesh with P or Mesh → Generate UV atlas, then open Mesh →
UV editor (Alt+U). Both commands retain an editable mesh/UV asset; generation
runs on the asset worker and participates in document undo.

Click a chart and drag to move it. Mode switches between islands and individual
UV vertices. The wheel scales; Shift+wheel turns. Buttons provide scale/turn,
next chart, pins, relaxation, packing and seams. Orange markers indicate pinned
vertices or explicit cuts. Pick near an edge before using Seam; its face-corner
cut flags split UV corner fans without changing the triangle surface. Removing
a cut reconnects coincident UV fans. Stitching displaced seams and regenerating
a conformal parameterization from an arbitrary cut graph are unfinished.

Pins retain their coordinates during edits and relaxation. Packing keeps pinned
charts in place, places other chart bounds with four-pixel padding and optional
quarter turns, and applies a shared scale to those charts. Relaxation retains
boundaries and pins, with a backtracking step to avoid triangle folds. Bounds,
collapsed triangles and positive-area overlap are checked; invalid drafts remain
visible and known-invalid Apply is disabled. Draft Undo/Redo is bounded by 32 entries and a
32 MiB target budget (one large prior state is retained).

A pointer drag is one local undo step, regardless of its sample count. XR stick
and fine-wheel adjustments share a step until both axes return to neutral; a
simultaneous ray drag keeps that gesture open until it also ends. Native wheel
events group until 250 ms of inactivity. Live previews and Undo remain available
while a gesture is held. Picks, mode changes, toolbar commands and Undo/Redo end
the current gesture. No-op or explicitly cancelled gestures retain redo history;
an accepted changed gesture replaces the redo branch. Identity transforms do not
introduce floating-point edits. Closing/cancelling the full UV workspace retains
the original document asset, as before.

UV overlap/bounds/degeneracy validation runs on a dedicated worker outside the
model lock. Previews publish immediately as "Checking UVs..."; newer meshes
cancel obsolete checks. Results require the same immutable mesh, workspace
session, document generation and request token. Undo/redo can restore a pending
snapshot and schedule its validation again. Mesh copying, selection, topology,
relaxation and packing still execute synchronously and need larger-mesh profiling.

Native Apply waits for a checked-valid draft. XR "Check & Apply" can capture a
preview while checking; the file/asset worker validates that exact target before
paint transfer. Editing/history are locked while it commits. Invalid targets
retain the editable draft and original paint. The native dialog stays open until
transfer succeeds, allowing correction/retry if work fails. An already accepted
Apply remains ordered ahead of subsequent focus loss; cancelling an unaccepted
draft cancels its checks and cannot resurrect an old workspace.

In XR, Materials → UV editor opens the same workspace. Ray-pick a chart/vertex,
then drag or use the stick. Move, Scale and Turn select continuous transform
modes. Scaling is logarithmic; vertical input supplies a ten-times-finer wheel
for scale/turn. A stylus can operate a panel opened by a controller. Toolbar
buttons also expose pins, relaxation, packing, cuts and draft history. Apply or
the owner's stick press accepts; before Apply, Cancel, Escape or owner
focus/tracking loss restores the previous asset. Invalid Apply retains the draft.

Apply transfers every editable paint layer in the background. Triangle identity
and barycentric coordinates map each destination texel back to its old UVs.
Color/emission are filtered in linear space; coverage, opacity, metalness,
roughness and masks remain separate. Geometry, layer identities, ordering and
selection are retained. Resampling is filtered and depends on texel density;
it is not lossless for arbitrary painted detail. Metadata-only edits retain
exact texture bytes. One document undo restores the complete prior asset.

Pins and explicit seam flags are optional binary fields in the version-three
mesh record; earlier version-three files remain readable. UV changes cannot
modify triangle geometry or order. The asset worker checks source identity and
revision before publishing. Pending accepted asset work marks the document dirty,
and save/export waits or reports that preparation must finish. A failed transfer
retains the prior asset and reports the error.

This extends the authoring workflow, not the full production requirement set.
Advanced conformal re-unwrapping, stitching, chart distortion/texel-density
analysis, texture-set/UDIM tools, large-mesh interaction optimization and physical
headset/stylus validation remain unfinished.

A reproducible queued-Apply data-loss defect is fixed by
[ordered spatial input](INPUT.md). The exact cause of the historical native XR
failure cannot be established from its missing comparison artifacts; subsequent
native runs pass. The complete production workflow remains unfinished; evidence
and next steps are in [the handoff](HANDOFF.md).

For native callers, `Document::edit_uv` publishes geometry without waiting for
validation. `wait_uv` waits for the current workspace's check/commit state;
`wait_asset` waits for accepted transfer work even if the workspace was closed.
Direct `UvWorkspace` use remains synchronous unless deferred validation is
explicitly requested. The optional CPU benchmark can be reproduced after the
OpenXR fixture has generated its persisted painting baseline:

```sh
cmake --build build --target dome-sculpt-uv-benchmark
build/dome-sculpt-uv-benchmark build/xr-client/paint-baseline.dsculpt
```

It reports edit latency and final background validation wait separately, checks
that all samples were applied, and verifies exact whole-gesture undo. Its single
fixture is not production-size or physical-headset performance evidence.
