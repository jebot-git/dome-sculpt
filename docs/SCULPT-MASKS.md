# Sculpt protection and object-local symmetry

This milestone implements bounded sculpt protection and object-local symmetry
with native FLTK and XR authoring controls, exact history/cancellation and GPU
protection/mirror feedback. The complete CPU and actual Arc ONEAPI real-loader
regression suites pass140/140 checks each. Resolution changes, sparse local meshing and
sustained production-scale stroke performance remain separate active work.

## Cut integration and constant sampling

The combined cut/sculpt stage retains source protection/control data through cut
Cancel, Apply and global Undo. Original source overlay scalars stay intact;
ephemeral cap vertices use the ordinary unmasked sentinel, and the display
overlay is never baked into shell paint. Cut workspaces block mask mutation.
Geometry availability uses actual SDF samples rather than total mask storage;
a mask-only empty volume cannot be cut, retopologized, voxelized or offered as a
bake source.

Protection sampling uses nested difference-form interpolation. Uniform global
fields bypass lattice lookup after position validation, and constant sparse
neighbourhoods retain exact scalar bits. Independent fractional-position and
affine-field regressions preserve the original exact-value expectations. Earlier
upstream proof below remains scoped to its recorded source; current combined
verification is recorded in VALIDATION.md.

## Mask domain and layers

Each Volume owns a sparse protection field: an explicit global default in
[0,1] and sparse exceptions on the same object-local lattice as its SDF. Zero
means editable; one means protected. Fill changes that global default and
removes exceptions in constant work. Clear is Fill(0). Invert changes the
default to `1-default` and transforms existing exceptions. Consequently a full
mask also protects future geometry in the object's currently empty space.
Neither operation enumerates an unbounded or arbitrary bounding-box domain.

The mask belongs to the object, independently of displacement layers. All
four geometry tools multiply their proposed change by `1-mask` at each sample.
Pressure still controls brush radius and original tool strength exactly as
before. Protection only attenuates the final delta. Fully protected samples
retain their original float bits; a fully protected stroke creates no history.
Mask Add/Subtract/Smooth brushes edit protection rather than geometry and do
not block themselves with the existing mask. They can operate when the active
geometry layer is hidden/locked: object visibility/lock still belongs to the
caller’s permission checks.

Layer creation, selection, names, locking, strength, visibility and deletion
retain the independent mask. Deliberate visibility/strength changes continue
to recompose complete stored displacement layers; they are not partially
baked into protected areas. The mask constrains subsequent brush editing on
every active layer. Existing paint/texture masks and skin influence locks
remain distinct data with their existing semantics.

## Symmetry and compute transport

Per-object controls store X/Y/Z mirror bits, an object-local plane origin,
mask editing mode/tool/strength and overlay visibility. Reflection transforms
both capsule endpoints and the brush normal. The sample support is the union
of bounded individual mirror boxes; it does not voxelize the empty gap between
far-apart mirrored strokes. A path exactly on a symmetry plane drops that axis
when it produces an identical operation. Flatten retains a reflected normal
with a nonzero component through the plane, because that plane differs.

The shared CPU/CUDA/HIP/SYCL arithmetic evaluates all mirror candidates against
the same pre-stamp sample. It selects the largest absolute field change;
exact opposing ties select the smaller SDF value. Repeated/overlapping copies
therefore do not accumulate strength or double pressure at the seam. The
existing four tools and pressure-radius formula remain the default operation.
Mask brush operations and per-sample protection execute in the selected backend
with one dispatch/readback per bounded Volume stamp, rather than eight host
roundtrips or a silent CPU substitute. A failed selected backend is an error.

The public compute function signature remains `apply_brush(span<Sample>,Brush)`.
Sample now contains an immutable `influence` float (default 1), and Brush adds
internal operation/mirror controls (defaults preserve ordinary sculpting).
These are C++ layout changes: all consumers/backends require a coherent fresh
build. At 24 bytes per Sample, SYCL's distance-only readback transports 28 bytes
per sample (24 upload +4 result), instead of 48 bytes for full Sample upload
and readback. Historical 40-to-24-byte evidence described the preceding
20-byte Sample layout and must not be cited as the new layout’s payload.

## Transactions, snapshots and limits

Mask and displacement storage share the existing 4-million-entry object/scene
budget through `stored_samples()`. The global mask default does not consume
one entry for every absent lattice point. A stamp has a conservative combined
2-million-sample support budget across mirrors and bounded workspace support.
Cancellation is checked during input preparation, after compute and during
staged publication. Geometry, layer displacement, mask and stroke-history
updates are detached until all work/allocations and cancellation checks pass.
Mask or field cancellation restores shared immutable COW snapshots without
allocating nodes during rollback. Undo/redo prepare their destination/history
before publishing. A stroke remains one history transaction across all mirrors.

The current COW representation still copies complete maps when detached for
mutation. These changes establish transactional correctness and bounded work;
they do not certify local-page storage or constant-time production strokes.
That locality/performance work remains necessary for large objects.

Nested Volume format 3 is emitted when mask/control state differs from defaults.
It validates defaults, finite bounded exceptions, unique integer coordinates,
workspace/storage limits and control enums/origins. Legacy Volumes 1/2 remain
unmasked with symmetry disabled and retain their previous encoding when no
new state exists. Older readers reject Volume3 instead of silently discarding
its authoring data. Multi-object Scene persistence uses nested Volume payloads;
Scene version remains owned by the existing scene/material integration.

## Pinned Blender baseline and provenance

The repository pins Blender 5.4 Alpha at
`3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2`. The evaluated primary source is
[mesh/sculpt.cc](https://raw.githubusercontent.com/blender/blender/3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2/source/blender/editors/sculpt_paint/mesh/sculpt.cc):
`fill_factor_from_hide_and_mask` applies `1-mask`, and
`cache_calc_brushdata_symm` reflects current/previous positions and normals.
The native implementation follows those protection and reflection semantics.
The SDF support union and strongest-overlap rule are original adaptations to
this sparse field representation, not a claim of porting Blender's mesh brush
or PBVH implementation. No external sculpt source is compiled into this change.

Read-only audit copies/hashes live in `artifacts/sculpt-mask-reference/`:
`sculpt.cc` SHA-256 `725e49002004db2120bddd9f5086a059ef4b2f1cddd097dff43a94f5fb2902b9`;
`mesh_brush_common.hh` SHA-256
`fdfebee795eba846bd36f52fa3927d89176c9e4c63e78e30cc332a945e8278a1`.
The header supplies the pinned factor API declarations. New kernel/helper/test
code is original GPL-3.0-or-later project code.

## Current kernel and draft-control evidence

An isolated `1337dc1` source snapshot plus only this kernel scope freshly
rebuilt every Volume/Brush/Sample consumer before execution. Its CMake retains
that baseline's sources and adds only the mask module/fixtures; newer independent
atlas registrations are not mixed into the snapshot without their sources.

- CPU Release: 6/6 suites, 2.36 seconds (existing geometry/layers/compute plus
  masks, transactions and dispatch fixture).
- Changed Volume/mask/shared-kernel/helper/test code passes ASan/UBSan/leak
  checks against matching fresh Release support archives. This is changed-code
  coverage, not a claim of whole-core instrumentation. It includes preparation
  and post-compute allocation failures and successful geometry/mask edits whose
  cancellation must restore exactly while every subsequent allocation fails.
- Actual Intel Arc A770 via the selected ONEAPI backend: 7/7 suites, 4.96 seconds.
  The compute test executes 184 independent original/protection/mask/symmetry
  endpoints and 224 CPU-reference batches /445,088 samples, maximum error
  8.9407e-08. It retains the 2,000,007-sample chunk, independent concurrent
  queues, unavailable-GPU failure and immutable-input assertions.
- Unified Runtime tracing of one actual eight-way Volume mask stamp records
  exactly 1 kernel launch, 1 queue, 2 device allocations and 2 transfers.
  The mask values on the original/reflected supports are independently checked.
  `artifacts/sculpt-mask-arc-dispatch-counts.json` summarizes the full trace.
- HIP7.1 host/device compilation for `gfx1030` succeeds; AMD execution is not
  claimed. CUDA SDK/hardware was unavailable at this checkpoint; shared kernel
  compatibility is not represented as an actual CUDA compile or execution pass.
- A standalone private native FLTK dialog draft passes four Xvfb cases:
  Return defaults to Cancel, Apply retains edited mask/X settings, and Ctrl+W
  cancels at normal/2× BDF appearance. It uses paired real millimetre numeric
  inputs and continuous sliders, responsive stacked rows and scrolling at
  640×360. Normal/compact captures were inspected. This draft has no Document
  connection yet and does not substitute for native app/XR workflow evidence.

Kernel builds/tests live under `artifacts/verify-sculpt-controls-1337/` and
`artifacts/*sculpt-controls*.log` /`artifacts/*sculpt-mask*.log`. The dialog
prototype and screenshots live in that snapshot's `dialog-qa/` directory.
Source/font/toolchain data are local test artifacts, not installed assets.

## Native desktop and XR authoring

The native FLTK Sculpt menu exposes **Masks and symmetry**, protection painting,
Fill/Clear/Invert, overlay visibility and X/Y/Z mirrors. The existing DomeD
universal menu, appearance font/scale, modal identity, Cancel focus and Ctrl+W
conventions are retained. The draft offers continuous strength, Add/Remove/Smooth
mask tools and object-local plane distances in millimetres. Apply publishes one
undoable controls command; reopening and applying untouched nonzero plane values
retains their exact floats and creates no history entry.

XR reaches **Masks / symmetry** on page 15 through Sculpt layers' former
redundant Save cell. Recovery stays on cell 10; ordinary Save remains available
on the root menu. The page provides mask painting/resume, tool and strength
panels, bounded global commands, X/Y/Z toggles, and a four-row local-plane/view
panel. Resume closes the palette and retains the current mask mode so tracked
pressure can operate on the sculpt. Selecting a geometry tool leaves protection
painting while retaining the mask. Completed bulk worker publication rebinds
the Volume and closes the palette; reopen the page to Resume. Skin remains on pages 13/14 and Retopology
on page 16.

Object locks and visibility protect mask authoring. A locked, hidden or zero
opacity geometry layer still permits mask editing: protection belongs to the
object and constrains subsequent brushes across its layers. Geometry strokes
continue to respect the active layer's editability. Focus loss or cancellation
rolls back the current mask gesture while preserving earlier accepted strokes.

Document settings use explicit generation/revision/object leases. Bulk mask
operations use the existing ordered cancellable asset worker, detached Scene
snapshots, exact changed/no-op publication and one global undo transaction.
Queued operations cannot publish into a replacement document. Scene decoding
preflights the combined displacement/mask storage of all objects before decoding
their lattices. Flat Volume3 projects open through the ordinary native loader;
nested Volume3 works in ordinary Scenes and combined Scene7/8 archives.

The immutable render snapshot carries protection scalars per packed vertex;
other objects use a negative sentinel and hidden overlays emit no scalar buffer.
Sampling occurs before object/group transforms. Protection darkens the final
linear-light PBR/MToon result without changing alpha, depth, culling or picking;
weight visualization retains priority. A worker cache strongly retains the
immutable COW geometry owner and reuses source meshes for mask/control-only
changes. A linker-instrumented native regression counts actual surface
extractions and verifies that those edits cause none. This avoids repeated
whole-field extraction; local extraction after geometry edits remains separate
work.

Integrated workflow evidence uses the coherent `79fe64a` source snapshot with
these changes and canonical DomeD FLTK `282467e` (explicit `FLTK_DIR`, canonical
headers and runtime `lib64`, optimized Release builds):

- Native document tests cover exact controls/no-op/undo, stale leases, mask stroke
  cancellation, global worker Fill/no-op, immutable overlay values, geometry
  extraction reuse, layer/object locks, flat/nested persistence and aggregate
  mask quotas. A real save/fsync gate deterministically verifies queued mask
  cancellation and old-generation publication refusal.
- The controller fixture covers page15 entry/back, continuous mask/plane/view
  controls, mode selection, Resume, actual pressure, focus rollback and explicit
  geometry-tool exit, while preserving Skin14/Retopology16 and Recovery routes.
- Real FLTK/DBusMenu tests pass 17 normal and 5 compact cases, including numerical
  25 mm input, unchanged-plane/no-op identity, actual visible pressure painting,
  complete history, Fill worker publication and universal/keyboard Close.
- A private real DomeD Wayland session passes 9 mask-modal/text-focus/menu/Fill/
  Undo/Ctrl+W checks and all 8 existing menu regressions. Its application uses
  the canonical FLTK build.
- The protection/mirror GPU fixture passes 38 assertions on both software GL and
  Intel Arc A770, including independent linear pixel oracles, zero-mask identity,
  alpha/cutout/picking invariance, weight priority, invalid data refusal and
  transformed mirror cursors. Existing MToon renderer (75 assertions) and
  visibility (25 assertions) also pass on both devices.

Evidence logs are `artifacts/tests-sculpt-ui-native-pass3.log`,
`tests-sculpt-mask-render-{software,arc}.log`,
`tests-sculpt-mask-material-regression-{software,arc}.log`, and
`tests-sculpt-mask-dome-wayland-popup.log`; screenshots and native menu reports
remain under the isolated candidate's artifacts directory. Real OpenXR fixtures
and complete sanitizer/accelerator regression results are recorded separately
when their final runs finish; synthetic tracking is not physical headset proof.

The integrated whole-core ASan/UBSan/leak run passes 17 selected suites in
186.49 seconds, including both Mikk/xatlas C/C++ sources, masks, allocation
transactions, native/controller workflows, Scene compatibility, files/recovery,
atlas, glTF and all worker shutdown handshakes. The complete ONEAPI application
and tests build successfully with the canonical FLTK toolkit. Twelve selected
actual Intel Arc native/backend suites pass in 6.15 seconds, including explicit
unavailable-device failure without CPU fallback; the CPU-only allocation
instrumentation test is excluded from that accelerator scope. Logs:
`artifacts/tests-sculpt-integrated-sanitize.log` and the corresponding integrated
ONEAPI build/Arc test logs.


Final application source20c132d passes all73 native Release suites in173.67s,
whole-core C/C++ ASan/UBSan/leaks17/17 in186.49s, actual Arc native12/12 in6.15s,
and the full140-check real-loader suite on both CPU and Arc compute. The original
117 cases are retained alongside23 mask cases. See VALIDATION.md for exact logs,
failed fixture attempts and device/performance scope. No physical-device claim
or full production-objective completion follows from this milestone.
