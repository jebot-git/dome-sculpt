# Persistent sculpt displacement layers

Each source-volume object now has up to16 independently named sculpt layers,
with stable IDs, selection, visibility, locking and strength. A layer stores a
sparse displacement contribution; this is not a list of brush commands and
not a stack of independent boolean solids. The editable field is
`clamp(band + sum(visible layer strength * displacement), -band, band)`.
A brush edits the current field and back-solves only the active contribution.
Changing an earlier layer does not replay or alter another layer's stored data.

Use Sculpt → Layers on desktop for Add/Next/Previous, visibility, locking,
strength cycling, Delete, and the native layer-properties dialog (name, strength,
visibility and lock). Apply is one document transaction; Cancel, Escape and
Ctrl+W preserve the source. The properties dialog uses the current DomeD FLTK
appearance, application identity and universal-menu modal routing. The status
bar identifies the active layer. Object → Sculpt layers in the headset exposes
creation, selection, visibility, locking, deletion and a continuous strength
slider with existing fine-wheel/ray controls. The prior Center command remains
available on this layer page. Layer actions close the command palette just like
other scene commands; reopen the joystick menu to continue.

Locked, hidden, zero-strength and numerically tiny-strength (<0.0001) layers
reject strokes. Strength remains editable while a layer is locked: locking
prevents brush edits, not deliberate metadata changes. Object locks/visibility,
mesh-paint mode and pending asset jobs also gate layer commands. Sculpt and paint
layers remain distinct. Editing source geometry does not silently discard an
existing painted mesh; the explicit source/mesh workflow is retained.

Stroke cancellation and undo/redo retain original cached field values and
per-layer contributions. Layer creation/settings/deletion participate in the
same document history as strokes, object edits and paint. Deleted data remain
available through history. Immutable geometry/file snapshots use copy-on-write
voxel and displacement maps: sharing a published snapshot never permits later
brush edits to mutate it. Geometry extraction and file work stay on workers.
The source field is still a bounded sparse CPU lattice, not production-scale
adaptive meshing or a completed accelerator implementation.

Nested volume persistence is now version2, with checked layer IDs, names,
strength, finite bounded displacement samples and a validated flattened cache.
Scene version3 retains that nested volume. New readers accept version1 flat
volumes as one editable Base layer. Older volume readers reject version2,
rather than silently dropping new layer authoring data. Scene and volume file
size budgets remain enforced; importer `Volume::from_samples` validates data
and creates a canonical Base layer. Sculpt displacement storage is bounded to
4million entries across layers per scene, plus the existing file budgets.

Evidence: native sculpt-layers tests cover exact stroke history/cancel,
composition/strength/visibility/locking, snapshot isolation, legacy and new
persistence, invalid IDs, source-field construction and global document history.
The real-loader synthetic XR fixture creates/selects a layer, sculpts into it
and saves/reopens the project. The native universal-menu suite exercises layer
creation, property Apply/Undo, modal disabling, Close cancellation and creation
Undo. These are simulated inputs, not a physical headset/stylus validation.
Masking, symmetry, layer ordering UX, production-size locality, recovery and
all remaining avatar/remesh/bake/rig/export requirements remain separate work.
