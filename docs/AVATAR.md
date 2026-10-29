# Native humanoid authoring foundation

`avatar.hpp` builds on the native rig/morph layer. It supplies explicit semantic
mapping, editable landmark fitting, safe bone preparation and named vowel-shape
bindings. It is an authoring kernel; desktop/XR marker editing, scene history,
VRM model metadata/licensing, materials, springs and actual VRM export still
need integration. It does not claim a complete Auto-Rig Pro or CATS replacement.

`HumanoidMapping` implements the 55 VRM 1.0 humanoid roles and its 15 required
roles. Mappings reject unknown roles, duplicate node assignments, incorrect
nearest humanoid ancestry, missing mandatory optional parents (upperChest,
thumb/finger chains) and non-positive scales on humanoid ancestry. Intermediate
non-humanoid helper joints are permitted. Partial authoring mappings can be
validated explicitly with `require_complete=false`; export should demand a
complete mapping. `mirror` returns an involutive semantic joint map and rejects
missing left/right counterparts. Unassigned helper joints map to themselves;
the caller must explicitly supply helper symmetry when appropriate.

`HumanLandmarks` stores editable model-space positions by semantic name and an
explicit orthonormal, right-handed body frame. Default coordinates are +Y up,
+X character left and +Z front, in metres. `fit_humanoid` requires all 15
essential landmarks, with optional torso, eye, finger and toe landmarks. It
checks triangle-surface proximity, upright torso/leg order, body-side separation
and noncollapsed joints before constructing parent-local rest transforms whose
world origins match the marked points. It supports rotated/transformed bodies
through the explicit frame. Limbs must start on their designated body side; crossed
arms, crouched figures and ambiguous poses receive errors rather than invented
anatomy. Fitting does not infer a silhouette, segment a character or guess unseen
fingers. It cannot certify anatomical correctness solely from surface distance.
Terminal lengths use an explicit option and are listed in the returned notes.

The fit accepts at most 55 markers, two million vertices and two million
triangles, with coordinates bounded to ±10000 metres and a configurable
marker/triangle work limit (default 20 million). Cancellation is checked during
triangle queries. Results are detached values; callers must publish on their
existing source/generation transaction boundary and retain the landmarks for
subsequent edits. The fit does not modify or bind the mesh: `automatic_bind` is a
separate surface-geodesic operation with its own seed constraints.

`compact_avatar` removes only joints outside the ancestor closure of mapped or
positively weighted joints. It retains all required helper transforms, preserves
remaining rest/pose indices through an explicit old-to-new table, remaps skin
influences and semantic mappings, and never collapses a transform-bearing parent.
Removed indices are `UINT32_MAX`. Geometry, materials and morphs remain untouched;
this is safe branch cleanup, not destructive heuristic avatar optimization.

`AvatarExpressions` binds VRM vowel identities aa/ih/ou/ee/oh to existing named
morph targets with explicit positive weights ≤1. Unknown targets, duplicated
bindings and identity collisions fail. `rename_shape` changes the morph and all
associated vowel references transactionally; it preserves displacement arrays.
This supplies identity-safe preparation, not audio analysis or the full VRM
expression evaluation/override system. Version-one metadata records bound
counts and validate against their skeleton or morph set during decode.

## Primary-source provenance

This layer is original GPL-3.0-or-later C++; no upstream implementation, schema
file or documentation is copied into shipped sources. Normative semantic names
and requirements were implemented after inspecting official sources. The VRM
specification tree has no repository license file at this revision; it is used
as a normative reference, not vendored or treated as a code license grant.

Official VRM specification revision:
`94e82dd346fa6cf0337c4421728640e5252dd38e`.

- [Humanoid semantics](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/humanoid.md),
  SHA256 `fa9f22dd491568a3b4437f85707bdc4381bd4f60e59bcd64b0ebaa741f6cdcf5`.
- [Human bone schema](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/schema/VRMC_vrm.humanoid.humanBones.schema.json),
  SHA256 `eb74f10a7bcae4a4fb44073ffc99b64cadb4c41c000ff137fac651106163f033`.
- [Expression semantics](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/expressions.md),
  SHA256 `e1d27d4717de6625189d01abbe77c46550ac71d4f1caed7ed5dccad1a6ebd0c9`.
- [VRM extension overview](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/README.md),
  SHA256 `290e45788090227d306a7bfb26d945e44a41c5e0e7578897689bf9f0ba2cb314`.

Official glTF specification revision
`3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded`,
[Specification.adoc](https://github.com/KhronosGroup/glTF/blob/3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded/specification/2.0/Specification.adoc),
SHA256 `b4c436a38c3ec374c47b8522b7c9e14c68890454a5bfe7d2b84642760e5d079b`.
The coordinate/units section establishes the right-handed metric body frame;
its header carries CC-BY-4.0 and copyright 2013–2021 The Khronos Group Inc.
Blender rest/local transform and locked-weight reference provenance remains
recorded in `RIGGING.md`; this layer introduces no additional Blender code reuse.

## Validation

`tests/avatar.cpp` exercises required and optional roles, helper ancestry,
mirroring, invalid mappings/scales, translated/rotated landmarks, collapsed and
crossed limbs, distant markers, work limits/cancellation, bounded JSON decoding,
compaction preserving world matrices and sparse weights, vowel serialization and
transactional shape identity edits. Run it with assertions enabled. Standalone
optimized and ASan/UBSan builds instrument this new layer and rig code; linked
existing core libraries are optimized, not fully sanitizer-instrumented.
