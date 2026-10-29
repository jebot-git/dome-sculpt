# Full objective and completion evidence

The goal is the complete native FLTK VR production workflow requested by the
user. An executable, a successful build, or the initial tests do not complete
this objective. Every row below must have matching implementation and evidence
before the thread goal can be marked complete. Animation and final scene
rendering are explicitly outside the required scope; interactive viewport
shading is inside it.

| Requirement | Current evidence / status | Required completion evidence |
| --- | --- | --- |
| Blender 5.4 Alpha basis | Pinned version header and native trackball adaptation | Audit reused sculpt, geometry, rigging and weight algorithms against the pinned baseline; document deviations |
| Prior Gmsh basis | FLTK, appearance, trackball and XR integration inspected; shared UI helpers reused | Reuse appropriate native meshing/remeshing functionality with real mesh fixtures |
| Prior OpenBrush DomeD basis | Direct EGL/OpenXR transport, profiles, one/two-hand navigation and BDF palette adapted; synthetic runtime and installed simulated-HMD tests pass | Integrate native XR session lifecycle, spatial UI and shared editing, with input/render tests and headset verification |
| Wings 3D where beneficial | Reference evaluated: winged-edge editing and AutoUV; no Erlang runtime dependency planned | Incorporate suitable topology/workflow techniques and record actual reuse |
| Seamless3d where beneficial | MIT cross-platform source identified; not incorporated | Evaluate reusable surface/skinning algorithms and integrate where beneficial |
| OpenFX where beneficial | GPL native modeling/skinning source identified; not incorporated | Evaluate appropriate native geometry/weight functions and integrate where beneficial |
| MeshLab and related projects where beneficial | Native mesh-processing basis evaluated; not incorporated | Integrate appropriate cleanup, resampling and retopology algorithms without its Qt frontend |
| Native FLTK suite | Built native C++20 FLTK viewport, BDF appearance and DBusMenu | Complete all authoring controls in FLTK; verify palette/fonts, shortcuts and universal menus |
| No Python dependency | Runtime is native C++; Python only in optional test harness | Audit all shipped workflow/import/export paths for native implementations |
| Medium-like robust VR sculpting | Sparse field, four tools, shared transactions and background meshing tested on CPU and through synthetic XR input | Production-size multi-object/layer sculpting, masks, symmetry, resolution changes, performant local meshing, sustained VR strokes and recovery |
| Full VR-driven workflow | Shared FLTK/XR document, stereo sculpting, joystick undo/redo/save/export connected; complete authoring workflow remains pending | Create, sculpt, texture, retopo, unwrap, rig, skin, save and export entirely in headset |
| VR controllers | Live native action mapping implemented; synthetic controller edits, focus/tracking loss and reference-space changes tested | Live pose/action/haptic mapping; reconnect/focus/session-loss and frame timing verification |
| Spatial stylus | Native MX Ink calibrated-tip/middle-force/tip-force bindings implemented and exercised through the synthetic runtime; physical verification pending | Actual stylus OpenXR/driver bindings, tip calibration, pressure/buttons and physical stylus verification |
| Joystick-spawned/driven menus | Eight BDF-rendered anchored sectors with selection highlight; synthetic joystick tool/file commands tested | Render and navigate every required workflow menu using live joystick actions |
| Default trackball/3D mouse input | Trackball orbit/pan/zoom verified in FLTK | Complete intended 3D-mouse device workflow and confirm physical trackball behavior; plain-mouse emulation is optional |
| Substance-like texturing | Not implemented | Native layered channels, brush/stencil/projection painting, masks, texture sets, bake workflow and editable persistence |
| Skinning/weight painting | Not implemented | Native weight painting, constrained normalization, lock/prune, symmetry and reliable transfer |
| Mesh baking | Not implemented | Normal/AO and required material-channel baking, cage/ray control, seam padding and error handling on real meshes |
| Remeshing | Only scalar-field surface extraction implemented | Robust mesh-to-volume and topology-preserving/adaptive/isotropic/quad remeshing as appropriate |
| Robust automatic atlasing | xatlas integrated with source mapping and chart packing; initial UV tests | Diverse production fixtures, overlap/stretch/packing bounds, seam preservation, durable chart state |
| Compact human-editable UVs | Automatic chart generation only | Seam marking, island selection, pin/relax, transforms and manual edits in desktop and VR |
| MToon shaders | Simple toon preview only, not the MToon specification | Native preview and matching VRM material serialization, outlines, transparency, lighting and texture properties |
| PBR shaders | GGX PBR shader shared by FLTK and XR, with constant clay material; static export has basic metallic-roughness metadata | Native PBR viewport/material editing and glTF material/texture round trips |
| Humanoid rigging | Not implemented | Bone placement/fitting, fingers, skeleton validation, coordinate conventions and VR rig-edit workflow |
| Robust automatic weights | Not implemented | Surface/volume-aware weighting with disconnected parts, thin limbs, topology stress tests and deformation fixtures |
| CATS basic functionality | Not implemented | Avatar cleanup, bone/material consolidation, pose conventions, shape/viseme tools and export preparation |
| Auto-Rig Pro basic functionality | Not implemented | Native humanoid auto-fit, editable rig controls and reliable automatic binding; no proprietary code dependency |
| Divine Cut basic functionality | Not implemented | Interactive cuts, boolean/trim operations, cap/material handling and undo in VR |
| Retopo Flow basic functionality | Not implemented | Surface-constrained retopology drawing/patches, relax/snap and topology editing in VR |
| Random Flow basic functionality | Not implemented | Seeded procedural panel/cut/extrusion/detail tools with usable parameters and undo |
| Shapekeys | Not implemented | Editable/mirrored morph targets, stable topology rules, preview, persistence and glTF/VRM expressions |
| Full glTF support | Static GLB subset implemented | Required scene, node, mesh, skin, morph, texture/material and extension features; independent validation and interoperability fixtures |
| Full VRM support | Deliberately unavailable for static meshes | VRM humanoid, metadata, expressions, first-person/look-at, MToon, spring bones, constraints and all selected version features validated against official schemas and consumers |
| Build-time OneAPI | Actual SYCL brush source exists; not built or device-tested | Compile, CPU-reference comparisons and hardware execution; integrate required accelerated workloads |
| Build-time ROCm | Actual HIP brush source exists; not built or device-tested | Compile, CPU-reference comparisons and hardware execution; integrate required accelerated workloads |
| Build-time CUDA | Actual CUDA brush source exists; not built or device-tested | Compile, CPU-reference comparisons and hardware execution; integrate required accelerated workloads |
| User chooses backend during build | CMake enum implemented; only CPU verified | Verified selection and failure behavior for every supported toolchain; no silent CPU substitution |

## Next integration work

1. Add object/layer scene state, crash recovery, importer/remesher interfaces,
   and durable material/UV/morph/skeleton data before expanding authoring tools.
2. Extend spatial menus to the complete document and authoring workflows,
   including error/status feedback and recovery entirely in the headset.
3. Integrate native surface processing, layered painting/baking, humanoid rigging
   and weight solving, then complete the named plugin-equivalent workflows.
4. Complete full glTF/VRM and real accelerator implementations/validation.
5. Verify sustained use on physical controllers, headset and spatial stylus,
   including device reconnect and runtime session-loss recovery.

These are sequencing notes, not reductions of the original goal.
