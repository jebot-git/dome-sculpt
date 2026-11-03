# Full-affine automatic binding and weight transfer

The existing native Bind or transfer skin command now accepts targets and
references under reflected, nonuniform or sheared object/group placements.
This extends the shared desktop/controller worker; no new frontend, public
layout or Scene schema is introduced. Runtime `17cbf54` is based on public
`30991dd`. The complete production objective remains incomplete.

## Geometry and rig authority

The affine path uses complete Scene world matrices. Automatic binding measures
mesh edges and bone segments in physical world metres. Transfer measures both
meshes and its search radius in that same metric. One common world origin is
subtracted before conversion to native float geometry: a shared translation
must not change distances or reject an otherwise valid scene at the solver's
10,000-metre local-coordinate bound. Ordinary legacy TRS-only rigs retain their
existing path and output.

For a source-to-target frame change C, the copied skeleton uses space'=C*space
and explicit inverseBind'=inverseBind*C^-1. Bone-local rest matrices, pose
values, IDs, authored inverse-bind meaning and morph channels stay intact.
Implicit inverse binds remain implicit. The resulting skin matrices are the
conjugate C*skin*C^-1. Raw target geometry, topology, UVs, source data and paint
are retained; reflected world scratch geometry reverses triangle winding and
the corresponding seam-edge flags. Normals use inverse-transpose transport.

Copied landmark positions use C. Landmark left/up/front are derived orthonormal
authoring directions, not a replacement for the full affine rig authority.
Preparation visits consume the existing work budget and sample cancellation.
Every candidate remains detached until validation and atomic publication.
Closed hard locks retain the existing exact no-op behavior. A different existing
rest-rig identity still requires explicit joint mapping; names are not guessed.

## Valid glTF joint hierarchy

The actual independent export probe exposed a previous refusal for a sheared
bone-local rest/pose matrix. Export now uses the existing verified affine SVD
factor helper to emit a valid two-node TRS hierarchy for that joint. Joint leaves
remain contiguous and stable for JOINTS and semantic metadata; children attach
to the leaf and inverse binds are unchanged. No sheared glTF node matrix or
approximate TRS decomposition is emitted. This reuses the joint-factor approach
from the separate Source25 `b953e26` work without admitting its schema. Future
Source25 integration must retain its additional full_local_matrix predicate.

## Qualification

Fresh optimized CPU builds compile the complete native FLTK/OpenXR application
and the nine selected test targets. All nine suites pass in 9.96 seconds. Fresh
owned C/C++ core/support sanitizer builds use -O1, ASan and UBSan with leak checks;
UI/XR are disabled there. The same nine suites pass in 31.00 seconds.

New tests cover 136 authoring assertions, 733 persistent allocation-denial
boundaries (391 automatic, 342 transfer), 11 real controller-input assertions and
162 independent parsed-export assertions. The selected regressions also cover
legacy skin authoring, rig fitting/import, affine export and document export.
Tests exercise implicit, explicit identity and authored rest inverse binds;
nested shear/reflection; skin/pose/normal commutation; exact source/paint/morph
retention; physical-distance refusals; common far-origin invariance; budget and
cancellation failures; exact allocation-failure recovery; worker Save/restart;
and one-step global Undo/Redo. Controller tests submit actual SpatialFrames to
the native input pipeline; they are not a new OpenXR-loader or physical test.

The existing real FLTK/DBusMenu test runs with the affine fixture: 11 native
checks plus five compact 640x360/double-size BDF checks pass under private Xvfb
and software GL. Both ordinary transfer options and the scrolled compact view
were visually reviewed. Cancel, method selection, binding, exact transfer,
Save/history and DomeD Ctrl+W are covered.

Six actual Document-exported GLBs in each build pass cgltf parsing, hierarchy,
accessor and skin validation. Independent parsed joint/morph/IBM products match
native evaluated world positions with maximum error 1.05367e-7 metres, below
the unchanged 3e-5 bound; a wrong missing-IBM control is detectably wrong.
Khronos glTF Validator 2.0.0-dev.3.10 reports zero errors on all 12 files.
All 12 NODE_SKINNED_MESH_NON_ROOT warnings remain recorded without suppression.
This proves the tested geometry/hierarchy products, not a new rendered-pixel
or normal-map shader comparison.

All 1,177 captured tracked inputs, 20 Release binaries and 16 sanitizer binaries
match their before/after hashes through final validation. Documentation and
receipts are added after the frozen-source runs. Commands, scopes, artifact
hashes and retained initial failures are in validation/SKIN-AFFINE.json.

This milestone does not implement volume-aware weights, arbitrary existing-rig
mapping, broader source-rig/VRM/FBX adapters, a new GPU rig solver, full-suite
qualification or controller drawers. No system install, active-session restart
or physical headset test occurred. Native CPU binding remains explicit.
