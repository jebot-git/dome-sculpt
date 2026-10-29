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
visible and Apply is disabled. Draft Undo/Redo is bounded by 32 entries and a
32 MiB target budget (one large prior state is retained).

In XR, Materials → UV editor opens the same workspace. Ray-pick a chart/vertex,
then drag or use the stick. Move, Scale and Turn select continuous transform
modes. Scaling is logarithmic; vertical input supplies a ten-times-finer wheel
for scale/turn. A stylus can operate a panel opened by a controller. Toolbar
buttons also expose pins, relaxation, packing, cuts and draft history. Apply or
the owner's stick press accepts; Cancel, Escape or owner focus/tracking loss
restores the previous asset. Invalid Apply retains the draft.

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

The historical native XR persistence failure did not reproduce in the resumed
validation. Separate UV/field/paint evidence and the new continuous scale/turn,
fine-wheel and cancellation checks are recorded in [the handoff](HANDOFF.md).
The complete production authoring objective remains unfinished.
