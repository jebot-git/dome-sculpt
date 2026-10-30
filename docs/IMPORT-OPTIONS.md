# Native and XR model import options

Private continuation of ac1b7a8. The File/Import mesh universal-menu action uses
the native FLTK chooser and a DomeD-styled options dialog. GLB, glTF JSON, OBJ and
FBX are listed. Cancel has initial focus; Return on Cancel, Escape, Ctrl+W and
universal Close do not submit asset work. Explicit Import captures all selected
settings and runs preparation on the existing ordered asset worker.

`ImportSettings` shares format, units, axes, normal policy, materials/textures and
paint resolution between the native dialog and spatial workflow. Preserve is
the default; unsupported conversions still refuse before scene publication.
Native GLB/glTF conversion choices currently remain Preserve until their broader
adapters are completed. OBJ/FBX expose authored metadata and explicit unit/axis,
flat/smooth-normal and material choices. Unitless OBJ From File means metres;
FBX From File reads its authored unit and axis metadata. Custom input units are
positive metres per source unit.

Spatial Mesh tools page10 cell10 opens browser21; its Options cell5 opens page23.
The shared registry declares spatial_page_count24 and derived max23.
Back returns23→21→10. Existing root Undo, cut17 and the separately reserved
procedural18/polygon19,22/holding20 allocations are preserved at integration.
The browser reads one directory on the file worker, with bounded entry visits,
results and UTF-8 label storage. It sorts folders before models, reports unreadable
entries, and exposes previous/next, open/parent, refresh, Import and Cancel.
Published views are immutable. Stale, canceled or replaced-generation listings
cannot reopen the browser. No filesystem traversal runs on the controller/UI
thread. Page23 includes the same format/unit/axis/normal/material/paint choices,
a logarithmic custom-unit control with pressure confirmation, and defaults.
Unconfirmed values cancel without model or history changes. Choosing another
folder preserves staged settings; abandoning the browser restores accepted
settings. The folder path is ephemeral and does not change the project archive.

A real native qualification fixture exposed a unit bug:1000mm mesh coordinates
are physically1m, but the old adapter's root .001 transform reached an existing
100m mesh-local admission limit before conversion. Native adapter2 now uses
ufbx MODIFY_GEOMETRY for physical unit conversion. Explicit FBX overrides apply
the same additional positive scalar to local positions and all node translations;
axes remain an explicit graph transform. Canonical mesh coordinates are metres,
raw original bytes/options stay exact, and the root node is not silently flattened.
This is a native OBJ/FBX canonical adapter correction, not removal of the remaining
general-affine glTF guard or its coordinate-transport work.

Native canonical primitive extras retain original N-gons instead of guessing
coplanar triangle merges: `dome_source_polygons_version=1` and
`dome_source_polygons` entries contain `source_face`, ordered `vertices` (canonical
primitive indices), `first_triangle`, `triangle_count`. Existing
`dome_source_vertices` and `dome_source_corners` retain every expanded vertex's
pinned-ufbx control point/corner identity. Non-indexed canonical triangles have
identity indices. A polygon consumer must validate complete coverage, loop order,
all duplicate-corner attribute signatures and source IDs before adoption. A
nonplanar face with differing per-triangle flat normals cannot be merged while
pretending those corner attributes are identical. This metadata stays in the
immutable source GLB; no Scene12 polygon authority is invented here.

Triangulation reserves a bounded quadratic-work allowance before entering the
uncancellable C helper; conversion and source reads check cancellation. Cancel
inside an actual blocked source read leaves no partial groups, objects or Undo
entry. The same source/settings can retry as exactly one transaction. Import
diagnostics clip at complete UTF-8 scalars. Existing archive/source/frame/skin
semantics are retained.

Validation logs and exact source hashes are recorded in the private checkpoint
manifest. Missing-NORMAL base/key generation and ignored tangent semantics are
now implemented for the documented native UV0 material subset; see
GLTF-FLAT-NORMALS.md. Full affine and linked source-node pose transport, FBX skin/blend conversion, source PBR UV1 sampling, UV/COLOR morphs,
VRM bind conversion and the other cases in IMPORT-FORMATS remain required.
No headset, system installation, external push or live-session change is part
of this private qualification.

Frozen qualification: optimized regular-user CPU Release with UI/OpenXR enabled
and canonical FLTK282467e headers/libraries; four native suites pass126 assertions
(35 controller/options,50 source capture,15 imported skin,26 generic authoring).
The same four pass with the fresh C/C++ whole core, ufbx, Mikk and xatlas built
with ASan/UBSan and leak detection; the optional upstream binary FBX adds the
51st source check in both builds. GUI qualification uses private Xvfb/bus/HOME
with the canonical lib64 explicitly selected, not the inherited older toolkit
in LD_LIBRARY_PATH. Native OBJ, FBX, external glTF and GLB flows pass46 checks,
including real unit selection, modal ownership, default Cancel, raw-format
archive bytes, Save and one Undo/Redo. Native controller tests cover directory
failure/stale completion, byte-read cancellation/retry, custom pressure control,
UTF-8 diagnostics and registry24 admission. No physical headset or live DomeD
Wayland session is claimed by these private runs.

Independent original-file software rendering passes24 GLB/external-glTF checks
for PBR, normal maps, linear vertex RGBA, three alpha modes and morph channels,
and24 original skin checks for authored/implicit inverse binds, initial joint
pose and N/T morphs, all with maximum pixel error0. These prove the admitted
source paths, not the remaining guards above. Early GUI automation failures
included unacknowledged native popup/focus events; the fixture now observes
popup mapping/unmapping and verifies the actual persisted unit value. The
separate pre-fix1000mm failure is a real product negative retained as a static
probe/log; the corrected physical-unit tests do not relax the existing bound.
