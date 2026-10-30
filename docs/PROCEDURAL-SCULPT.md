# Procedural sculpt detail

This native workflow implements seeded raised panels, recessed cuts and mixed
surface detail on an editable sculpt volume. Desktop uses the DomeD universal
Sculpt → Procedural detail command. XR uses Objects → Sculpt layers → Procedural;
the root Undo/Redo and existing resolution/mask/rig/retopology paths remain.
The formerly redundant Sculpt layers → Redo cell now opens this page.

Choose an object-local positive/negative X/Y/Z projection face, row/column grid,
seed, density, gap, depth and rounded corners. Desktop depth is in world
millimetres, including nested group/object scale. XR has large continuous
stick/ray panels, independent rows, fine wheel depth adjustment, parameter
Cancel/Confirm, a direct Next seed action, Apply, Undo/Redo and Cancel work.
Changing settings alone does not mutate the document. Focus loss discards an
unconfirmed parameter draft. The procedural page stays open between actions.

The implementation scans occupied negative field samples once, projects grid
centres onto the first exposed original surface and builds ordered rounded-box
unions/differences. A fixed integer generator supplies reproducible layout
variation, independent of standard-library distribution implementations.
Current object-local mirror planes duplicate/deduplicate primitives. Protection
is blended ONCE after complete Boolean composition, including partial masks.
The source is never changed while preparing, picking or evaluating a candidate.

Apply queues the existing cancellable source-leased asset worker and publishes
one global Undo transaction only when the complete result succeeds. Each result
adds a named sculpt layer containing its seed. Hiding/deleting the layer restores
the previous field; opacity remains editable. The layer delta subtracts the
original UNCLAMPED double layer sum, preserving exact recomposition through
visible-band clipping. Every existing layer, mask, setting, object property,
paint/deformation/source archive and unrelated object is retained. Native scene
and Volume4 persistence already store the generated editable displacement layer;
no new project schema or Volume state member is introduced.

CPU, OneAPI, CUDA and HIP implement the same bounded ordered CSG operation.
The selected GPU backend receives32 bytes per primitive once and24 bytes per
sample; only4-byte output distances return. Intermediate shapes do not incur
host readbacks. GPU work is split by sample count AND arithmetic work, checked
for cancellation between chunks and published to host only after the entire
operation succeeds. OneAPI queues/copies drain before failed or cancelled USM
storage is destroyed, including the pre-existing brush failure path.

Limits are explicit errors rather than silent truncation:32×32 grid,
2048 primitives after mirrors,4M candidate samples/aggregate layer+mask entries,
128M charged work units and256MiB host planning workspace by default. Each
projection reserves the full20k candidate-cell allowance of the bounded picker;
scan, support enumeration and CSG evaluation also consume the operation budget.
Host key/page/sample allocations use a counting memory resource. The backend
has separate bounded staging/device buffers; this workspace limit is not a
claim about total process or driver memory. Fully protected, empty, missed and
unchanged operations publish no layer or history. Geometry-owner lineage
identifies local changes for downstream extraction.

The current projection is axis-aligned in object-local space. It follows the
first visible surface at each grid centre; it is not a geodesic panel layout on
an arbitrary curved surface. Very shallow detail can be lost at coarse voxel
spacing. Meshes require the existing explicit source-retaining voxel conversion;
this operation does not implicitly discard painting, rigs or imported meshes.
There is no live geometry preview while moving settings, and the scalar-volume
borders remain limited by the existing field domain. Broader production and
physical headset performance remain part of the complete goal.

Validation fixtures cover independently computed rounded-box distances,
ordered mixed operations, metadata preservation, actual GPU execution where
available, all observed cancellation boundaries, failed-device retry,
concurrent workers, real added solids/cut cavities, layer hide/show, fractional
protection, six faces, seed reproducibility, symmetry, resource limits, native
persistence, one Undo/Redo, stale/cancelled jobs, FLTK universal menus and
controller input. Final qualification receipts are recorded separately; no
physical headset or AMD/NVIDIA execution is implied by compilation.
