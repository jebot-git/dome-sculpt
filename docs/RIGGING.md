# Native rig, skin and morph foundations

`include/dome/rig.hpp` and `src/rig.cpp` are native C++20 authoring kernels.
They do not yet constitute a scene-integrated avatar editor. Desktop/XR editing,
scene ownership/history, glTF/VRM skin/morph serialization, humanoid semantic
mapping/fitting and physical-device authoring remain integration work.

## Skeleton and deformation

A skeleton has1..256 uniquely named bones. Parents may precede or follow their
children; invalid parents, cycles, zero lengths, non-finite transforms,
non-unit quaternions and singular/extreme scales are rejected. Bone rest and
pose transforms are parent-local TRS in metres; length extends along local+Y.
Matrices are column-major. Quaternion records deliberately store internal
`[w,x,y,z]`; glTF adapters must convert to its `[x,y,z,w]` convention explicitly.

`world`, `inverse_bind` and `skin_matrices` provide hierarchy evaluation and
`poseWorld * inverseBind`. `deform_mesh` applies sparse linear blend skinning
and recalculates area-weighted triangle normals; it retains mesh indices/UVs.
Dual-quaternion deformation and authored split-normal preservation are not
implemented. Degenerate deformation output is reported rather than published.

## Weight constraints and surface-aware binding

`normalize_weights` reserves the exact mass of positive hard-locked influences,
prunes only unlocked entries and retains the strongest unlocked entries under
an explicit influence cap. It normalizes them into the remaining mass. Duplicate
joints, negative/non-finite values, impossible locks/caps and unweighted vertices
fail visibly. No arbitrary fallback bone is chosen. `paint_weight` keeps the
chosen unlocked value exact while redistributing other unlocked influences;
painting a locked group fails. The caller owns spatial brush falloff and undo.

`automatic_bind` builds a welded surface graph from original triangle edges.
Each deforming bone seeds nearby surface vertices by point-to-segment distance;
multi-source Dijkstra distances then travel along the surface instead of
jumping directly across nearby limbs. Welds are exact positional coincidences,
so UV/split-normal seams are connected without merging authoring vertex IDs.
Disconnected parts must have a deform-bone seed within the explicit maximum
seed distance. Missing seeds identify the failing component; the whole operation
fails transactionally. Only the nearest capped candidates are retained per
vertex before falloff/normalization. Work and geometry budgets plus cancellation
bound worker use. Mesh coordinates are bounded to10000metres to reject
non-finite intermediate distance/BVH calculations before processing. This is a graph-geodesic surface solver, not Blender's heat
or voxel/volume solver. It cannot prove anatomical suitability, infer semantics,
or resolve coincident overlapping surfaces. Seed radius/margin must be chosen
for actual object scale. Tests include a connected narrow folded strip whose
spatially nearest opposite bone is not its strongest geodesic influence, and a
far disconnected part that must fail.

`transfer_weights` finds the nearest source triangle using a native BVH and
barycentrically interpolates its vertex influences before constrained pruning
and normalization. An explicit transfer radius prevents distant automatic
assignments. Source/destination normals, groups and locks are not implicitly
inferred; callers must reapply authoring locks deliberately after a transfer.

Symmetry requires an explicit involutive bone mapping. Vertex pairs are matched
geometrically across local X=0 within a supplied tolerance, preferring reflected
normals for positional seam duplicates; each pair is bijective. Missing
counterparts or unmatched seam multiplicity fail, rather than silently applying
weights to the wrong index. This is geometric symmetry, not anatomical fitting.

## Editable morphs

`MorphSet` stores named per-vertex displacement targets, supports replacement,
mirroring and weighted preview, and uses a stable identity over original vertex
positions and triangle order. A changed base/triangulation cannot silently reuse
old indices. UV coordinates and triangle IDs are retained by preview. Pin/seam
splitting or remeshing requires explicit morph transfer; it invalidates the old
identity. Target edits validate a detached candidate before publication. Resource
budgets cover target count and total displacement entries. Version-one JSON
records are independent of scene file schema. They do not implement VRM
expressions/visemes or expression-to-material bindings yet.

## Pinned source inspection and actual reuse

Blender5.4.0 Alpha source was inspected at the repository's pinned commit
`3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2` using official raw sources. The local
Blender working checkout had a different revision and was not mistaken for the
pinned baseline.

- `source/blender/blenkernel/intern/deform.cc`:
  https://raw.githubusercontent.com/blender/blender/3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2/source/blender/blenkernel/intern/deform.cc
  SHA256 `4cf438a44ce906bdece4a94625955ecc5aa52dc11c56b0d1f49843277a1e9ac1`.
  `BKE_defvert_normalize_ex` locked-mass reservation and remaining-weight scaling
  semantics are adapted into the sparse C++ normalization function. Our API
  deliberately rejects impossible hard locks and zero unsupported assignments
  instead of modifying locks or choosing a fallback group. No Blender runtime
  type, allocator or frontend is linked. Original copyright:2001–2002 NaN
  Holding BV; license GPL-2.0-or-later (compatible with this GPL-3.0-or-later project).
- `source/blender/editors/armature/armature_skinning.cc`:
  https://raw.githubusercontent.com/blender/blender/3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2/source/blender/editors/armature/armature_skinning.cc
  SHA256 `fd449cd73c2dd7066b9b6c38f7222ef3275864cb79760cacac9a06d260c57297`.
  Inspected deform-bone filtering, root/tip world-space setup and its separate
  envelope/heat weighting paths. The heat solver is not ported or claimed here.
  Same original copyright/license. The graph solver, BVH, transform, closest-
  triangle, symmetry and morph implementations here are original project code.

Optional reference downloads/hashes stay local under `artifacts/rig-reference`;
the native build/runtime has no downloader, Python or Blender dependency.

## Reproduction and integration

After building the existing core library:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude \
  src/rig.cpp tests/rig.cpp build/libdome-sculpt-core.a build/libxatlas.a \
  -lpng -pthread -o /tmp/dome-rig-test
/tmp/dome-rig-test
```

Root integration should add `src/rig.cpp` to dome-sculpt-core and a native test
executable for tests/rig.cpp, then place immutable rig/skin/morph asset ownership
on Scene objects with source identity and generation checks for background work.
Use the existing exact document history, not a second isolated undo stack.


Milestone evidence (2026-10-29): the standalone optimized native tests pass,
including folded thin-limb and disconnected-part binding, hierarchy/rest binds,
locked/pruned/painted weight constraints, seam symmetry, surface transfer,
morph persistence/topology checks and malformed input. The new rig/test sources
also pass AddressSanitizer, UndefinedBehaviorSanitizer and leak detection when
linked against the existing optimized core library; that run does not instrument
the prebuilt core library and is not a whole-application sanitizer claim.
