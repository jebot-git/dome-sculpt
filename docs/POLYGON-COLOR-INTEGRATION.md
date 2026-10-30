# Polygon editing and source-color bake integration

This milestone extends published `7152f5e` in the existing native C++20
FLTK/OpenXR application. Product source is frozen at `27365c9`; subsequent
qualification documentation does not change those source inputs. The full
objective in REQUIREMENTS.md and HANDOFF-466ebd1.md remains active.

## Application behavior

Mesh / Polygon edit opens the shared desktop/controller workspace. Vertex,
edge and face selections, movement, extrusion, inset, bridge, dissolve, weld,
edge cuts, connect, delete and flip retain stable authoring identities and
ordered N-gons. Triangles are the derived rendering/painting surface. Local
Undo/Redo and Cancel preserve the committed scene; Apply is one global history
entry. Ctrl+W first dismisses the settings or cancels the workspace. A sculpt
can enter polygon editing, accept an atlas-backed edit, save/reopen and restore
its complete retained original. Posed/morphed conversion is explicit.

Topology, picking, atlas preparation and validation run on the ordered polygon
worker. Accepted geometry and its captured held pose publish together. Immediate
Save/Close observes the accepted endpoint; a dependent Save refuses a failed or
cancelled endpoint rather than writing an older scene. Immutable render buffers
provide displacement previews without CPU geometry readback.

Scene12 authenticates each current and retained PolygonAuthored painted owner
against its own active REST positions, ordered indices and UVs. PolygonAuthored
is distinct from SourceAuthored; unrelated retained origins cannot supply its
witness. Existing guard diagnostics remain where a representation is not yet
implemented. See POLYGON-WORKFLOW.md and POLYGON-SCENE-ARCHIVE.md.

The existing bake dialog now has explicit source-color preparation. Resolved
PBR and common color planes preserve their different semantics, original
material/paint sources and quality receipt. A polygon target may use this option
when its same mesh and UVs pass the real Strict validator. Its original polygon
paint and direct authority remain in an owning origin. Conversion, preparation
and normal/material baking commit together with exact Undo/Redo. Overlapping
UVs require an unwrap instead of a policy relabel. Save/reopen, UV Apply and
other-hand painting of a held prepared target preserve the original sources.
Retained PBR tiles and baked planes count toward aggregate decode budgets.
See COLOR-PREPARATION.md and POLYGON-COLOR-AUTHORITY.md.

Conflicting mesh workflows stay disabled while polygon, cut, retopology or UV
editing owns the document. Native and exported universal-menu capabilities use
the same context. Held-motion bake admission preserves its specific release
diagnostic. Compact large-bitmap-font dialogs scroll their content while keeping
actions fixed; scrolling cannot change a hidden slider.

## Qualification scope

The machine-readable receipt is
[POLYGON-COLOR-INTEGRATION.json](validation/POLYGON-COLOR-INTEGRATION.json).
It records the final source, build flags, exact binary/log hashes and terminal
results. Historical component receipts remain scoped to their original code.

| Final `27365c9` qualification | Result |
| --- | --- |
| Complete CPU / OneAPI Release suites | 195/195 in380.93s /196/196 in408.90s |
| Selected complete-own-code ASan/UBSan/leak suites | 72/72 in437.90s; no suppressions |
| Actual FLTK polygon dialogs | 13 normal +16 compact checks |
| Actual FLTK polygon color preparation/bake dialogs | 9 normal +9 compact checks |
| Actual current DomeD / native Wayland universal menus | 8 checks |
| Direct hardware graphical fixtures | 15/15, asserted Intel Arc A770 |
| Actual emitted GLB/VRM Khronos validation | 156 files, zero errors, 18 warnings |
| Official pinned VRM0 schema validation | 16 actual VRMs, zero errors; 15 schema blobs verified |

The 18 warnings comprise 16 legacy `VRM` extension-name warnings and two
generated-tangent-space warnings in the new polygon color-bake exports. No
diagnostic is suppressed. The compact polygon and completed color-report
captures were inspected; their actions remain accessible below scrolling content.

The preceding frozen combined source `84d9fe6` passed 194 CPU and 195 OneAPI
suites. Its 15 direct graphical fixtures asserted the actual Intel Arc A770
renderer, including polygon previews, normal/UV/export consumers, held ghosts,
PBR/MToon, buffer reuse and native PCF text. Its 154 emitted GLB/VRM files passed
Khronos validation with zero errors and 16 warnings for the legacy `VRM`
extension name. Those results do not by themselves qualify the later authority
and menu changes; the final receipt records their coherent reruns separately.

The source snapshot and compiled dependencies are audited: CPU and OneAPI each
have 257 translation units using pinned nlohmann headers, and the selected
sanitizer build has 145. No compiler dependency uses system nlohmann headers.
Own core, renderer, polygon and bundled C/C++ support code are instrumented in
the selected ASan/UBSan/leak build; system graphics/font libraries are not.

All new native dialog workflows use private buses/displays/HOME. Actual nested
DomeD checks use native FLTK Wayland and the current private toolkit. These do
not launch a headset or restart the running desktop. Hardware GL proves
rendering and export-consumer behavior, not GPU-accelerated color projection or
baking. The latter still execute on the existing CPU worker.

## Remaining production work

Arbitrary raw COLOR_0 topology transport and prepared-color topology changes
remain required. Current guards prevent data loss while their actual per-corner
representation and consumers are being implemented. UV1 paint-source sampling,
COLOR/TEXCOORD morphs and full affine/source-pose import remain incomplete.

Normal-bake fixtures cover the documented conservative canonical-frame subset.
Generic curved artist frames, mixed-joint skinning, nonzero direction morphs
and format-specific fractional coverage require a new versioned interpretation.
The generated-tangent warning for normal-mapped native exports without an
explicit TANGENT remains recorded, not suppressed. Schema validation alone
does not prove normal-map pixel interoperability.

The wider sculpt/paint/remesh, humanoid/rig/weight/shapekey, glTF/VRM,
accelerator and physical-device requirements are unchanged. Only the earlier
`8f4d67d` rudimentary milestone has user-confirmed physical headset operation.
No system installation or new physical headset qualification is claimed here.
