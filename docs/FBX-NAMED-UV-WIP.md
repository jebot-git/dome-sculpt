# Per-mesh named FBX UV bindings — private implementation

This child of26bf0aa fixes an actual incorrect import: a texture named UVMap
was always bound to TEXCOORD_0 even when that exact name denoted the second
coordinate set on a mesh. The same original material can refer to UVMap at
different indices on different meshes.

The converter now resolves exact names on each primitive and emits shared
canonical material variants keyed by all four supported texture-role coordinate
indices. It retains original texture/image resources and material/element
identity. The normal tangent/bitangent stream follows the selected normal UV
set. Missing or ambiguous names refuse rather than choosing another set.
Explicitly disabled materials retain original resources without demanding
active UV bindings. New captures identify native adapter4; older saved sources
are not rewritten. No public C++ ABI or Scene format changes here.

The new generated conformance fixture has two meshes, one shared original
material, UVMap at indices1/0, different authored tangent bases and shared
colour/normal images. The exact final test fails on frozen26bf code at the
per-role binding assertion. Exploratory optimized new-adapter/fixture objects
linked to that unchanged Scene/import ABI pass independent ufbx coordinate/
tangent/material/image and native Save/restart checks, plus missing/ambiguous
and disabled-material cases. The link map excludes the old adapter object.
This is not yet a fresh coherent whole-project/sanitizer/frontend qualification.
Artifacts and hashes are under artifacts/named-uv/; the fixture is registered
as fbx-named-uv for subsequent coherent builds.

Native UV1 authoring still requires the existing explicit Prepare editable
texture atlas option; the proof exercises that actual path. The direct-import
guard and its negative remain preserved. Direct multi-coordinate editing,
native UV2+ ownership, broader FBX semantics and independent graphics/device
coverage remain required. Genuine upstream three-set reference files are pinned
separately as research inputs, not represented as passing two-set imports.
This implementation extends the existing native frontend; the full production
goal remains active and no installation or physical test occurred.

## Fresh coherent qualification of the bounded binding fix

Frozenb603e0b now has fresh optimized Release57227 and whole-owned C/C++
core/support/render/frontend SAN11485 builds, both terminal0. Registered8/8
pass4.42s Release and8/8 pass8.51s SAN, including the actual new per-mesh UV/
tangent/material-variant oracle and native explicit-atlas Save/restart, plus
9073 deformation/1152 independent world vertices,51 real worker assertions and
related skin/affine/export pixel regressions. The exact strengthened fixture's
old-code negative remains preserved.

Actual FLTK Maya-skin14 and Blender-morph14 checks pass in BOTH Release and
ownedSAN, using the same current converter; all four Apps close normally with
complete teardown. These are genuine deformation inputs, not a new graphical
named-UV fixture. Fourteen current emitted deformation GLBs pass pinned Khronos
2.0.0-dev.3.10/maxIssues0 with0errors/12unfilteredwarnings. Read
validation/FBX-NAMED-UV-QUALIFICATION.json for source/binary/evidence hashes and
limits. This qualifies the bounded binding correction; direct multi-coordinate
editing/UV2+, broader FBX semantics and combined-main admission remain required.
No new physical test, installation or session restart occurred.
