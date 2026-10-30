# Additional production requirements — 2026-10-30

These user specifications extend the complete objective in REQUIREMENTS.md and
HANDOFF-466ebd1.md. They do not replace or reduce any other required workflow.
Animation and final scene rendering remain outside scope.

## Polygonal edit mode

Provide a genuine polygonal edit mode within the existing native FLTK/OpenXR
application, using Wings3D as a reference. Preserve the original polygonal
vertex/edge/face topology and editable selection; triangulation for rendering
must not become the only stored authoring representation. Cover practical
selection/transforms, extrusion/inset, bridge, split/dissolve/weld and topology
editing, with undo/cancel, persistence and desktop/controller workflows.
Appropriate native algorithms and licensed reference reuse must be documented.
Keep original paint, UVs, rig/weights/morphs and source identities where the edit
is representable; otherwise provide explicit source-retaining conversion or a
specific diagnostic. Do not silently delete unsupported authoring data.

## Individual visibility and held-mesh isolation

Allow individual meshes to be hidden. Provide an option that displays all
visible meshes except the held mesh as transparent ghosts, with only the held
mesh editable. This is an editing/input authority rule as well as a rendering
mode: ghost meshes must not accept sculpting, painting, weight, polygonal,
transform or background asset edits. Preserve each object's normal visibility,
material and lock state when the held/isolation state changes. Controller
holding, desktop interaction, cancellation/release, stale jobs and document
replacement must leave consistent edit authority and undo state.

## VRoid-only humanoid bases

Use verified CC0 VRoid Studio models as the only shipped/default base meshes
for humanoid modeling. Remove MakeHuman default mesh data and creation choices.
Useful properly licensed MakeHuman algorithms/reference may remain. Preserve
compatibility with existing saved projects containing their own authored
geometry, paint, skeletons and morphs; do not require deleted default mesh data
merely to open or export such a project.

## FBX and OBJ import options

Include usable native import options for standard FBX and OBJ files, in desktop
and the shared XR authoring workflow. Preserve supported geometry, hierarchy,
materials/textures, UVs and deformation data with appropriate format conversion,
unit/axis handling and explicit unsupported-feature diagnostics. Retain actual
original format-tagged bytes/resources separately from any canonical GLB
adapter; never label converted GLB bytes as an original FBX/OBJ source. Keep
resource/file limits, cancellation, source-leased publication and one-step
undo/retry. Runtime remains native C/C++ without a Python or proprietary SDK
frontend dependency. Independent format/interoperability and real UI fixtures
must cover the implemented behavior.

Completion requires implementation and matching validation at the requested
workflow scope. The polygon/color milestone now connects actual native/controller
editing and Scene12 sources. Hiding/held ghosts, VRoid-only defaults and shared
FBX/OBJ options are integrated. Richer topology/color transport, complete import
conversion and full production/device workflows remain required; see the current
requirement matrix and POLYGON-COLOR-INTEGRATION.md.
