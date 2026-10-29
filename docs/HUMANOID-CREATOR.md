# Native humanoid creator foundation

`create_humanoid(HumanoidControls, cancelled)` creates five coherent parts:
Body, Eyes, Irises, Pupils (including surface-projected brows), and Hair. Each
part has a mesh, suggested solid material color/toon flag, and validated native
deformation state with identical semantic joint indices. Root Scene integration
should publish the parts as one grouped transaction, retain the versioned
parameter record, and apply shared character transforms/posing deliberately.
Changing parameters rebuilds detached authoring geometry and source identities;
painting/sculpting edits require an explicit preservation/transfer workflow.
There is no hidden replacement of customized meshes.

This uses the authored MakeHuman hm08 body, not primitive tubes pretending to be
a finished human. The body contains 14,517 UV-split vertices and 26,756 triangles,
including detailed face, ears, fingers and toes. Body indices/UVs and the original
positional IDs remain stable across every bounded creator setting. Original UV
seams are retained; body normals are smoothed across corresponding source IDs.
Source helpers are used for rig/eye placement and never emitted as body geometry.

Native signed controls change build, shoulder/waist widths, limb lengths,
head width/height, eye spacing, nose, mouth and jaw. Independent style controls
adjust anime influence, eye size and groom length/volume/fringe. Official CC0
face target displacements are combined with original continuous body/ocular
warps. A larger head, larger eye region, narrower jaw/nose and editable colors
supply toon/anime direction while retaining coherent authored topology. This
is presently a realistic/toon doll foundation, not polished VRoid-level anime
artwork. Neutral, Toon and Anime presets provide explicit starting proportions; artistic
polish remains work.

The centred decimetre-scale source uses +X character left, +Y up and +Z front.
Native conversion fits the requested body height (1.2–2.2 metres), grounds the
body minimum at Y=0 and applies the same transform to geometry, joint helpers
and generated parts. Hair may extend above/below the body bounds. Body/face
proportion controls are finite within [-1,1]; style controls are within [0,1].
Fixed sample counts cap total generated vertices at 30,000. Cancellation is
checked between major phases and expression builds. Invalid controls or
degenerate-geometry failures reject the detached result before publication.

The default 163-bone MakeHuman skeleton and authored symmetric sparse skin
weights are reused as graphical assets. Native joint orientations choose local
+Y along head-to-tail segments and compute parent-local transforms; no
MakeHuman rotation-plane evaluator or pose runtime is copied. Source weights
are explicitly normalized, retaining up to their authored 12 influences rather
than assigning distant nearest bones. A complete validated 55-role VRM 1
humanoid semantic mapping covers spine, limbs, fingers, eyes and jaw. This does
not imply MakeHuman animation/pose-file compatibility or full VRM export.

Eye detail charts now separate pupil disks, brows, upper eyeliner and limbal
rings so each remains independently paintable. Upper eyeliner follows the actual
body/eye occlusion contour; iris caps have bounded radial relief and eye surfaces
have deliberately lower roughness than skin/hair.

Body morphs contain aa/ih/ou/ee/oh, left/right eyelid closures and smile. They
are editable stable-topology displacements with native vowel identity bindings.
Their authored source deltas pass through the same style/metric transformations
as the rest body. Eye and groom parts are independently disconnected surfaces
but explicitly weighted to their eye/head joints. They do not depend on an
automatic binding fallback. Eye whites are ellipsoids; iris/pupil caps follow
their curved front surface. Brows project onto the actual generated body surface.
The original groom combines a head cap and 24 closed swept locks with separated
UV cells and nondegenerate root/tip cap charts; no VRoid hair asset or proprietary code is involved.

## Actual source reuse and licensing

[MakeHuman's official license statement](https://www.makehumancommunity.org/about/license.html)
and its pinned per-asset grants distinguish CC0 graphical assets from its AGPL
application code. The generator incorporates only explicitly CC0 data. Exact
revision/file hashes, original credit statements, full CC0 grant, deterministic
converter and reproduction instructions are in [assets/humanoid](../assets/humanoid/README.md).
No MakeHuman application code is imported or executed.

[VRoid's official workflow](https://vroid.com/en/studio) informed the separation
of adjustable body/face/hair parameters and material colors.
[Its official guidelines](https://vroid.com/en/studio/guidelines) identify pixiv
presets as proprietary and require a separate license for generators based on
those meshes/textures. Accordingly there is no VRoid asset/source reuse,
proprietary runtime dependency or branded preset. These pages were inspected on
2026-10-29; their reference role is recorded alongside the asset notices.

## Quality and scope

The generator currently supplies an unclothed neutral base and one adjustable
original groom. Clothing libraries, cosmetic textures, detailed iris artwork,
advanced anime eye/lid presets, arbitrary character recognition, collision-free
hair/skin fitting, springs and hairstyle authoring, age/ethnicity databases,
shape-key-driven brows, facial capture, production deformation corrective shapes
and complete VRM packaging remain work. Dark eye-socket rims and the simple
simple iris treatment are visible presentation limitations. A nondegenerate
mesh check is not a proof against self-intersection or an artistic quality test.
Generated parts should stay grouped in subsequent editing and pose operations.

Offline posed preset QA found remaining groom/body penetration in the tuned
anime preset; no dynamic collision-free claim is made. The next requested
primary baseline is the separately verified older CC0 VRoid beta models; their
author grants are distinct from the modern proprietary VRoid presets.

`tests/humanoid-creator.cpp` checks all five parts and all presets against
PaintedMesh at resolution256 without reindexing/UV replacement, source counts, complete semantics, exact
metric grounding/height, all full-strength expression morphs, parameter
roundtrip, stable UV/topology across edits, disconnected part posing, zero groom
settings, eight extreme combined settings, cancellation and invalid records.
`DOME_HUMANOID_FIXTURE=/path/avatar.glb` exports a native interchange fixture.
Khronos glTF-Validator 2.0.0-dev.3.10 reports zero errors/warnings for that fixture;
unused-UV informational entries are expected for untextured solid materials.
Blender 5.4.0 Alpha independently imports and renders the exported rig/morph
scene for front-face and full-body visual QA. These are offline renderer checks,
not a headset authoring acceptance test. Standalone ASan/UBSan instrumentation
covers the new creator/test sources; linked existing core libraries are optimized.
