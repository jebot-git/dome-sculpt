# Native sculpt resolution

Native FLTK and XR controls change the current sculpt's object-local sampling
resolution through a bounded background worker, without flattening displacement
layers or replacing original painted assets. Source leases, cancellation, one
undo transaction and native save/restart preserve the original data on refusal.
This private authoring milestone builds on the tested resolution/archive and
authenticated local-meshing foundations; its validation and explicit limits
are tracked below.

## Physical meaning and persistence

The old layer composition is `band + sum(visible opacity * layer displacement)`.
Because the old band was always four voxels, changing voxel spacing alone shifts
that expression and the zero surface. Compensating a selected Base layer would
break its hidden/locked/opacity configurations. The foundation therefore keeps
an explicit physical distance band in metres while changing lattice spacing.
Every independent layer displacement retains metre units; IDs, names, order,
visibility, locks, strengths, active ID and future ID remain exact metadata.
Masks retain their separate global default, resampled sparse exceptions and
per-object mask/symmetry controls.

Default-band Volumes still emit their original versions 2/3 structure. A nondefault
physical band emits Volume 4 with `distance_band` plus the existing layer/mask/
control data. Readers validate the version, reject unknown or incompatible fields
and check the physical band before decoding samples. A band has the existing
physical range of 40 micrometres through 4 metres, independent of voxel spacing.
No mutable public band setter bypasses geometry provenance. A new resolution
Volume is a new immutable geometry owner; future local meshing must rebuild it.
Geometry snapshots explicitly retain the band. Cache identity includes geometry
owner, voxel spacing and physical band.

## Bounded detached operation

`resample_sculpt` returns an optional detached Volume and a quality/work report.
Identical voxel spacing returns no replacement and does not allocate or change
history. `resample_sculpt_object` validates a detached Scene candidate before
publishing a replacement Volume, retaining transforms, groups, paint/image/source
archives, object identities and every other object. Original inputs are untouched
when cancellation, allocation, storage, work or quality checks fail.

Independent trilinear layer and mask fields use a sparse union of their occupied
source interpolation cells projected to bounded target tiles. Far disconnected
supports do not enumerate their shared global bounding box. Displacement/mask
storage shares the existing 4-million-entry Scene budget. Separate support/work,
surface-cell/vertex/triangle, quality-sample and BVH-test limits bound operation
cost. This is a worker-oriented CPU foundation; it is not a GPU resampling claim
or a constant-time large-object locality claim.

Every original displacement/mask node also checks reconstruction from the target
field, with explicit metre/protection tolerances. A nonempty independent layer or
mask cannot silently disappear, including hidden/locked layers that do not
currently contribute a visible surface.

## Geometry and quality limits

Source and candidate extracted meshes must both be closed and consistently
oriented, with the same component count and per-component Euler/genus signature.
The comparison uses exact geometric positions; true collinearity is tested with
double coordinate differences, rather than a fixed square-metre cutoff that
misclassifies valid small triangles.

Bidirectional triangle subdivision checks nearest-surface distance. The distance
at a triangle center plus its farthest-vertex radius bounds the whole triangle
by the 1-Lipschitz property of distance to a fixed surface; a conservative floating
point guard is included. Subdivision stops only when that bound is acceptable,
or refuses when depth, precision or work limits prevent certification. Normal
directions have a separate sampled dot-product threshold. The report records
accepted distance bounds, sampled normal alignment and independent layer/mask
reconstruction errors.

These checks concern the current extracted surfaces. They do not prove every
future combination of layer opacity/visibility has identical topology, nor can
a coarse grid preserve arbitrary thin features. Existing component/handle loss,
independent layer/mask loss, excessive distance/normal error and uncertifiable
precision conditions are refused while retaining the original sculpt.

## Trilinear picking policy

A retained wide band can make old sphere-tracing steps jump entirely over a thin
feature, because the editable field is not globally a certified signed distance.
Picking now traverses lattice cells with bounded DDA. Inside each cell its
trilinear field restricted to the ray is cubic; derivative critical points split
monotone intervals, followed by first-crossing bisection. Equal-sign endpoints
therefore cannot hide an interior dip/crossing. No whole-field scan is added.

The existing positive authoring tolerance is bounded by both voxel spacing and
physical band so a narrow-band background never becomes a hit. A 20,000-cell
budget reports explicit exhaustion; empty fields return immediately. Picking
follows the trilinear authoring field, whereas rendering uses the tetrahedral
surface approximation. Their sub-cell difference is not claimed to vanish.
Ready-surface BVH picking and further locality/performance integration remain
separate work.

## Evaluated upstream basis

Pinned Blender 5.4 Alpha
[`mesh_remesh_voxel.cc`](https://raw.githubusercontent.com/blender/blender/3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2/source/blender/blenkernel/intern/mesh_remesh_voxel.cc)
uses a voxel transform, OpenVDB mesh-to-level-set and volume-to-mesh rebuilding.
Its
[`object_remesh.cc`](https://raw.githubusercontent.com/blender/blender/3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2/source/blender/editors/object/object_remesh.cc)
optionally reprojects volume/attributes and identifies data-layer loss. That
mesh roundtrip is not a substitute for preserving this native layer stack.
No Blender/OpenVDB remesh source is linked into this implementation.

Gmsh-XR commit 2ed37a92b4a6862a7ea1ff620ea5e37b8c6070f1's `StructuredField` in
`src/mesh/Field.cpp` was inspected for physical origin/spacing, trilinear sampling
and explicit outside values. The native sparse support, transactions and quality
policy are original adaptations of the existing Volume representation, not a
claim of porting Gmsh meshing kernels. Exact reference hashes are recorded in
`third_party/sources.json`; read-only copies are private validation artifacts.

## Validation status

Fresh coherent Release validation passes 10/10 suites in 12.79 seconds. The
whole C/C++ core (including xatlas/Mikk) passes ASan/UBSan/leak validation on the
same 10 suites in 100.34 seconds. Seven selected ONEAPI suites pass on actual
Intel Arc A770 in 6.52 seconds; the device check rejects CPU fallback. The
resampling operation itself remains CPU work, while source construction, wide/
narrow-band brush fixtures and existing compute comparisons use the selected
accelerator. Independent compute validation includes 184 analytic endpoints,
224 CPU-reference batches/445,088 samples and maximum error 8.9407e-8.

Focused checks include 51 resolution cases, 17 bounded-ray cases and 9 tiny-face
regressions. Allocation tests cover 92 fault boundaries through 138,668 measured
allocations, including late Scene publication preparation, and verify that
same-resolution and successful ray operations allocate nothing. Existing Scene,
shared Document, sculpt geometry/layers/masks and mesh-processing regressions
also pass. A centered unrepresentable coarsening probe is refused by normal
quality rather than accepted as a dramatically expanded thin feature.

Logs in the parent artifacts directory are `tests-resolution-release-frozen.log`,
`tests-resolution-sanitize.log` and `tests-resolution-oneapi.log`. The private
`artifacts/resolution-source-freeze.json` records the foundation source hashes.
Those results are the original foundation checkpoint, rather than evidence for
the later native authoring integration below. No physical headset test is claimed.

## Authenticated-cache and archive integration

The private combined base merges `1a42f18` (including tiny-face inspector
`6458b50`) with authenticated cache/renderer base `c1ba217`. Full authoring
`geometry_snapshot()` preserves both physical band and geometry journal. Detached
resolution output explicitly starts unknown lineage; the local cache rebuilds
for its new map/context, then uses authenticated local transitions for subsequent
stamps and history. Same-spacing no-op preserves exact source ownership and
journal. Minimal SDF sampling and static geometry identity compare both voxel
spacing and physical band.

Flat Volume 4 files now load through the real `Scene::load` route without being
misclassified as Scene 4. Recognized flat and Scene declarations cannot be mixed.
A shared Volume header preflight enforces version-compatible fields, physical
band and required format shape before nested lattice/paint allocation. Existing
aggregate Scene sculpt quotas include nested Volume 4 samples, displacement
layers and masks before row decoding. Outer Scene versions remain independent;
legacy Volume 1/2/3 and Scene 4–8 replay are covered.

Focused fresh CPU Release verification passes **13/13 suites in 3.50 seconds**:
resolution, resolution allocation faults, bounded ray picking, exact tiny-face
inspection, new archive/authentication integration, local cache, authenticated
Document integration, mask transactions/Document, Scene compatibility/authoring,
deformation and source-UV adoption. These checks preserve paint/source-image
archives, scalar-mask rollback and the existing five-worker Document behavior.
The new integration fixture checks real flat/native save/load, both historical
Scene 7 forms, malformed/mixed headers and aggregate quota ordering.

The combined core also passes fresh whole C/C++ ASan/UBSan/leak checks on the
same 13 suites in 34.97 seconds. Earlier standalone Arc evidence above belongs
to `1a42f18`; the authoring follow-up is recorded separately below.
Evidence: parent artifacts `tests-resolution-auth-release-focused.log` and
`build-resolution-auth/Testing/Temporary/LastTest.log` in this private worktree.


## Native desktop and XR authoring

`Sculpt > Resolution...` edits a detached draft in the canonical DomeD FLTK
DomeDialog. Target spacing and optional surface/layer quality bounds use world
millimetres, including the full object/group scale path; the model retains local
metres. An unchanged world-unit value preserves the original local float bits.
The continuous slider is logarithmic over the supported local 10µm–1m range;
Half/Double spacing presets retain that range. Optional normal/protection bounds
are degrees and percentages. Automatic bounds follow the target spacing.
Controls use the session BDF appearance, a scrollable compact layout and fixed
Cancel/Apply actions. Initial Return chooses Cancel, Ctrl+W/Escape and universal
Close discard the draft. No model geometry or meshing occurs while adjusting it.

XR Sculpt layers page 7 cell 7 opens a large logarithmic world-mm resolution panel.
Its joystick and ray controls stage the target; explicit confirmation queues
work, while Cancel, focus loss and leaving the session discard the unconfirmed
draft. Root Undo stays on its original cell 4; Recovery, Masks15, Skin14 and
Retopo16 retain their routes. XR uses the same conservative default quality
bounds as a newly opened native draft; advanced bounds are available natively.

The guarded asset worker validates the source generation/revision/object lease,
operates on a detached scene and rechecks the lease and cancellation before
publication. Object locks prohibit resolution changes; locked or hidden geometry
layers remain independent representation data and are preserved. Same-voxel
confirmation never queues an asset or changes revision/history, including after
world-unit conversion; stale same-voxel leases still refuse. Successful results
publish one undoable scene change and new immutable geometry lineage, retaining
source painting and archives. Failures, cancellation, queue/generation changes
and quality refusal retain the old scene. Native Save/restart restores Volume4
with its original physical band and no transient history.

A worker-only optional progress callback reports monotone completed phases and
bounded actual work, ending at .98 after quality. The generic asset publisher
marks 1 only after successful publication. Native status reads the live atomic
asset progress and offers the existing File/Cancel background work action.
Empty callbacks retain the allocation-free no-op. Throwing progress consumers
or late phase cancellation leave the source exact. Desktop bounded DDA exhaustion
cancels only its current gesture and becomes status, rather than escaping the
FLTK event callback into an alert; the grid-approximation/budget limits above
remain explicit.

Authoring verification so far:

- Fresh canonical toolkit282467e Release app/core build; seven focused native
  suites pass 2.10s, then expanded native Document/group-scale fixture passes .80s.
  The subsequent full CPU build passes all 85 native CTests in 227.94s.
- Normal FLTK/DBusMenu 12/12 and compact 4/4, with reviewed BDF screenshots. These
  cover actual worker application, exact full-scene undo/redo, unchanged Apply,
  modal keybindings, visible 20mm restart and fine-grid ray-status/Save behavior.
- Real OpenXR loader 17/17 uses actual software GL stereo swapchains and synthetic
  poses/actions: accepted 20mm Volume4 and unchanged physical band, complete-scene
  Cancel/no-op/focus/unconfirmed persistence, and balanced eye image ownership.
  This is loader/input/rendering proof, not hardware tracking proof.
- Fresh whole C/C++ core ASan/UBSan/leak validation passes 11 selected suites
  in 97.35s, with supplementary scene-authoring 1/1 in 3.59s. This covers the
  changed progress callback, native worker/controller paths, source leases,
  allocation faults, ray limits and archive/authentication/mask regressions.
- All87 coherent CPU CTests pass:85 native suites in 227.94s and both loader
  suites in 190.43s. The unchanged original loader regression passes 140/140
  checks in 169.89s, and the separate resolution loader passes 17/17 in 20.54s.
- A fresh ONEAPI selected-target build/device rerun is pending. Earlier
  accelerator results do not establish GPU resampling; this bounded quality/
  resampling path remains CPU work.

Current authoring evidence resides in this worktree's `artifacts/validation`,
`artifacts/resolution-xr-final` and `artifacts/menu-sculpt-resolution-*` directories.
`artifacts/resolution-authoring-manifest.json` pins the 21 owned source/doc files
against combined base 2c160cf. Build/runtime logs include `cpu-all-build.log`,
`cpu-all-native.log`, `cpu-all-xr.log`, `sanitize-build.log`,
`sanitize-tests.log` and `sanitize-scene.log`. Source/header consumers were
rebuilt coherently; no archive from another revision was reused. Root later
integration with upstream cutting controls/six-worker lifecycle needs separate
coherent validation; this private snapshot retains its tested five-worker base.
