# Editable mesh bake cages

This private authoring milestone extends the existing desktop and XR baker.
Duplicate the prepared low target and move its cage vertices with the existing
polygon editor, keeping logical vertices, faces and corner connectivity
unchanged.
Polygon Apply may pack separate face-corner vertices: their retained original
source and stable IDs now preserve the cage mapping across that packing.
Keep every displacement outward and within the chosen Front bound. The cage
may be hidden or locked: baking reads it without changing its geometry, paint,
materials, rig, morphs or placement.

In **Mesh → Bake surface**, choose the high-detail Source and the Cage mesh.
**Normal offset (automatic)** retains the original projection behavior. Front
remains the maximum cage displacement in world millimetres; the ray starts at
the actual cage surface and extends Back beyond the low surface. Projection
miss/backface policies remain explicit. Only Bake starts the worker; a
successful result is one Undo step beneath the existing editable paint layers.

In XR, **Objects → Mesh tools → Baking → Source / cage** opens two large rows.
Use ray/trigger dragging or joystick row/value navigation, then Done to confirm
configuration. Cancel and focus loss discard the draft. Configuration does not
change the Scene or add history; Bake surface explicitly starts the existing
worker. The page identifies the configured cage. Other document commands
cancel an unfinished source/cage draft rather than implicitly executing Bake.

The cage uses its complete displayed mesh: native morph/skin evaluation and
full object/group affine placement precede conversion into target coordinates.
Vertex identities use explicit ordered indices or authenticated retained
polygon source/vertex/provenance/face/corner identities. No nearest-position or
weld inference is used. Existing split/canonical/prepared-normal low bases
receive cage positions through their authenticated ordered target corners.
Unwitnessed or changed topology, conflicting displayed corners of one original
vertex, missing meshes, inward or excessive offsets and invalid placement
refuse with the complete prior scene and bake retained. A missing selected cage
never silently becomes automatic projection. Document replacement clears the
old document's cage reference before object IDs can be reused.

The cage geometry is an ordinary editable/persistent mesh. Like the existing
projection options, the chosen source/cage settings are session configuration;
a durable bake recipe is still required by the full goal. Target rest-pose,
source shading, normal/color preparation and original-frame guards remain.
This does not introduce a general arbitrary-topology cage correspondence or
complete the remaining multi-source/mask/recipe/GPU/device baking requirements.

## Ownership and implementation

`ObjectBakeSettings::cage_object` and `EditorState::bake_cages` append C++ state.
These change public C++ layouts and require matching complete consumers. No
native Scene/archive/schema field is added. The first parent qualification
therefore builds the complete application/core/support/renderer/FLTK/XR and
selected consumers freshly in each profile; no old-layout archive is borrowed.

Input-side enumeration only lists candidate IDs. Ordered topology comparison,
deformation, affine conversion, corner remapping and existing projection-bound
validation run on the detached worker. Renderers retain immutable old snapshots
until accepted publication. Source/cage meshes are read-only, including hidden
or locked meshes. Existing generation/revision/authority, held-mesh edit rules,
Cancel, ordered Save and one-step history remain in force.

The new source/cage confirmation fault test exposed a genuine prefix1 bug in
the shared setting route: an allocating title copy could lose the native input
lease. Nothrow control-state moves and a real configuration/publication
transaction now restore the exact draft, input ownership, configuration,
Scene/history and prior immutable publication at every denied boundary. The
same draft retries without a scene edit. No callback replay or synthetic
success is used.

## Qualification boundary

Fresh owned optimized parent Release and sanitizer builds each compile220 tasks
for the full native application and nine selected test targets, then pass9/9.
The final complete document, palette and bake-authoring TUs are subsequently
compiled and relinked against those exact owned parent artifacts after matching
all other product/header inputs. The final body therefore has matching component/app
qualification, not another fresh whole build or an all-project test pass.

Each final profile passes the nine selected commands plus explicit nonzero-cage
caller mode,32 custom-cage workflow checks (including displayed morph, reflected
shear and prepared-normal bases),49 persistent source/cage confirmation fault
prefixes, and127 persistent native Bake-admission fault prefixes. Refused caller
admission preserves a previously confirmed nonzero cage, all source/paint/
history/input state and the real queue; same-controller retry and dependent
Save execute actual native computation.

The final real OpenXR loader/EGL/OpenGL app passes21 checks each Release and
sanitizer for Apply/Cancel/focus, both real1024px eyes, an independently located
off-axis source, every projected blue pixel and exact source/cage/paint/native
Save. Normal rays cannot reach that source; the edited cage redirects them.
The sanitizer XR run uses the staged optional Mesa26.2.5 allocator patch from
public8878f64, with no leak suppression. Installed Mesa remains unmodified.
These are software rendering and synthetic poses/actions, not a headset or
positive GPU qualification of this new workflow.

The real polygon Move → Apply workflow exposed a second implementation bug:
four original shared vertices became six packed face corners, so the first
strict-index cage adapter rejected a valid native edit. The original failure is
retained. A bounded worker adapter now verifies the initial retained geometry
and stable vertex/provenance/face/corner identities, maps every original target
vertex explicitly, and rejects ambiguous displayed skin/morph corners. No
projection tolerance or bounds are relaxed. A separate29-check native workflow
passes each Release/SAN, including prepared normals, Save/restart, one Undo/Redo,
actual FlipFaces refusal and failed dependent Save with the prior file intact.

The final application/helper pair also repeats the real loader21, FLTK28 and
Wayland8 gates using a cage generated by the actual native polygon pointer
Move/Apply workflow. Thus these application gates exercise the packed-corner
adapter, rather than only a directly constructed four-vertex cage. Six new
CTest commands are registered; fresh configuration/query does not imply a new
all-project build or full-suite execution.

Actual FLTK normal and640×360/font32 compact cage workflows pass28 checks each
Release and sanitizer: real native selection, slider wheel, Bake, immediately
ordered Save, exact source/layer retention, one Undo/Redo and clean exit.
The existing projection/color/portable-normal GUI modes all pass after their
obsolete fixed coordinates/tab counts are replaced by observations of actual
FLTK widget geometry and focus. Input still goes through real native events;
the optional test observer only reads widgets after the real `Fl::wait`.
The observer is not installed or part of the shipped runtime.

Native FLTK Wayland also passes8 checks through a private nested DomeD fixture,
including the real Wayland popup, exported menu, Unicode cage choice, Ctrl+W
cancellation and clean exit. This uses the exact existing compositor fixture
and current canonical27/POLL client SDK; it is not a fresh DomeD build or a
restart of the user's desktop. Captured XR/native normal/compact panels were
visually inspected. The early unrelated RGB swatch in the two-row selector was
removed before the final runs.

Read validation/BAKE-CAGES.json for exact commits, commands, binary/SDK/source
identities, preserved original negatives and scope. The branch is a private
milestone awaiting full coherent ABI integration and broader regression;
all unfinished original/additional requirements remain active. No system
installation, physical-headset launch or active-session restart occurred.

## Coherent production continuation

The subsequent fresh new-layout application integration is qualified by
608/608 Release,27/27 ownedSAN, native desktop/Wayland and actual strictA770
GL/controller gates. Read COHERENT-CAGE-INTEGRATION.md for current scope; the
component qualification above remains historical evidence rather than a new
whole-build claim. Durable recipes and every remaining full-goal row remain.
