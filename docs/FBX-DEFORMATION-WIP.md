# Native FBX skin and morph conversion — private prototype

The subsequent bounded component now has fresh native, owned sanitizer,
independent export and actual FLTK qualification. Read
FBX-DEFORMATION-QUALIFICATION.md and its receipt for the new exact scopes.
This earlier prototype history is retained; full FBX support remains incomplete.

This branch extends the existing native ufbx importer; it is not a replacement
frontend and is not yet qualified for main. The public authoring application and
the complete FBX requirement remain separate from these initial results.

Implemented conversion includes linear/rigid skin clusters, original affine
inverse binds, normalized complete positive influences up to the native bound,
multiple JOINTS/WEIGHTS sets, labelled conversion nodes for repeated joint binds
and unweighted geometry, and independent blend targets/default effective weights.
Sparse shape offsets include Blender offset weights. Source node identities,
channel/key provenance and original bytes remain retained. New captures identify
this implementation as native adapter3; existing adapter2 project data is not
rewritten. No public Scene/deformation ABI or archive version changes here.

Legacy FBX Lambert/Phong can declare transmission with factor1 and black
TransparentColor. The converter now recognizes that inactive legacy term rather
than rejecting ordinary opaque materials. Actual nonzero legacy transmission and
unsupported PBR effects remain guarded. Requested generated morph normals are
also guarded until an explicit target-normal adapter exists.

## Initial evidence

An exploratory optimized adapter object and probe were compiled against this
branch and linked with the frozen Scene20 archives from
`artifacts/color-rigid-union-d5de43b/build`. The public ABI is identical; the link
map excludes the old adapter object. This is not a fresh whole-project build,
sanitizer, GUI, GPU, or complete import qualification.

Five genuine fixtures from pinned ufbx `26a482ae66871d7de36eb722aa060bce95bce274`
pass real capture, generic native authoring import, independent ufbx evaluated
world-position comparison and exact native Save/restart. They cover two Maya
linear-skin files (132 vertices each), Blender partially unweighted skin (6),
Maya two-target morphs (36) and Blender weighted shape offsets (36):342 vertices.
The largest observed absolute position error is1.54678e-7. The fixed bound is
`2e-5 + 1e-6 * magnitude`; no bound was changed after an observed failure.
These compare default evaluated poses/weights, not newly authored pose changes.

## Subsequent editable-pose and independent-export work

The new `work/fbx-authoring-3a55c84` child composes this adapter with published
main7a4443e. Reproducible pinned upstream inputs/license/provenance and registered
`fbx-deformation-authoring` / `fbx-import-document` fixtures now live in the
repository. This does not retroactively make the initial mixed-object probe a
fresh whole-project qualification.

The expanded exploratory consumer runs9073 native checks across1152 independent
world-vertex comparisons: five default inputs plus nine newly edited translation,
rotation and anisotropic-scale poses. Every posed case must produce measurable
motion, retain exact native Save/restart, and preserve actual editable skin/morph
channels in independent cgltf evaluation of14 emitted GLBs. The original wrong
vertex-index correspondence negative is preserved: retained normal-frame export
legitimately reindexes seam vertices. The corrected oracle compares exact ordered
triangle corners, retaining the same numerical bound and source geometry.

The new Document fixture is a real source-read-barrier test for accepted import
Save, old-file preservation, cancellation, generation replacement, retry and
one-step global Undo/Redo across all five files. It requires a fresh coherent
current-ABI build; no result is claimed before that run. Full frontend/device,
attribute-quality, allocation and broader semantic coverage remain required.

The genuine 3ds Max dual-quaternion input still refuses with the exact unsupported
deformation diagnosis. A derived Maya fixture with actual coloured transparency
still refuses with the material-semantic diagnosis. Earlier real failures are
preserved: inactive Lambert rejection and a missing shape offset-weight factor
(maximum error2 before correction). Inputs are validation references, not new
default humanoid assets; shipped humanoid bases remain verified CC0 VRoid.

`validation/FBX-DEFORMATION-WIP.json` records exact input, helper, binary, source
and log hashes and the limited linking scope. Test artifacts remain local and
ignored. Continue with registered reproducible fixtures, independent posed/
unit/axis/instance/combined skin+morph and attribute oracles, coherent fresh
Release/sanitizer consumers, worker cancellation/history/Save, real FLTK/XR and
interoperability checks. Dual-quaternion/multiple-deformer semantics, named UVs,
full materials and target-normal options remain required. No unsupported source
effect may be silently discarded to admit a file.
