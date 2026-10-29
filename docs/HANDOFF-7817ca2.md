<!-- Historical evidence from incoming 7817ca2, before the local authoring merge. -->

# Native FLTK / XR bake authoring milestone

The complete production objective remains active and incomplete. Read AGENTS.md,
REQUIREMENTS.md, BAKING.txt, BAKE-CONTROLS.md and VALIDATION.md. Preserve all sculpt,
UV, texture/skin/rig/morph/avatar, MToon/glTF/VRM, named plugin, device and compute
requirements. No agent was spawned here. No system install, session restart or
physical-device/backend completion is claimed.

This stage extends public af492ee. Its exact handoff/validation are archived as
HANDOFF-af492ee.md / VALIDATION-af492ee.md. Earlier baked/creator/rig histories
remain intact; source CC0 VRoid female/male remain primary with MakeHuman explicit.
Always fetch remote before overlapping work and retain newer commits.

## Implemented

- Mesh > Bake surface: native FLTK source choice (including hidden/locked high
  sources), logarithmic world-mm front/back/AO sliders with wheel adjustment,
  integer AO-sample/padding sliders, progress/status/title, asynchronous Bake,
  rebake and Cancel. Default Enter is Cancel; Escape/Ctrl+W/universal/WM Close
  cancel options or request cancellation of running work. Clear baked surface
  removes only the base. Bake/Clear are exact global Undo/Redo transactions.
- Objects > Mesh tools > Baking (XR page12): joystick-spawned navigation, source
  selection, three projection/AO-distance ray sliders, two AO/padding rows,
  explicit Bake/Clear/Cancel work and regular Undo/Redo/save/export. Settings
  stage until Done; Cancel/focus loss discards them without scene/history edits.
  Source names/hidden markers and worker progress are visible. Sculpt layers
  remains on Objects; its duplicate Mesh-tools shortcut now enters Baking.
- Atomic per-job cancellation/progress shared only as worker status. No worker
  closure accesses widgets/model state. Cancellation is checked under the model
  lock before publication, leaves history/source/target exact, clears busy only
  once terminal and reports cancellation without a false error. File > Cancel
  background work exposes cancellation for supported scene operations.
- New native bake-controls suite, four real-loader XR replay variants and native
  GUI acceptance/cancellation/compact-font suites. --project-info adds a per-object
  baked flag for durable independent scene comparisons. Numerical/kernel/source
  preservation, scene-v6 and legacy checkpoint contracts from af492ee remain.

## Verification and next work

All 41 registered Release suites passed; the expanded real-loader report has 62
checks, complete/passed with zero failures. Six selected whole-core C/C++
ASan/UBSan/leak suites passed. Final boundary/label refinements and a stale-operation lease-release correction
have focused native and whole-core sanitizer verification. Native GUI result details, captures, exact source scopes and cold
Fontconfig startup diagnosis are in VALIDATION.md; do not substitute an older
baseline's evidence. Inspect actual process/log state before restarting jobs.

Continue full weight painting/visualization and guarded scene integration of its
registered kernel, conformal/stitching/production atlases and retained source UV
workflows, retopo/procedural/cut/plugin authoring, complete MToon/VRM/glTF fidelity,
sculpt production quality and all real devices/compute backends. Durable bake
recipes, explicit cage editing, partial/multi-source masks, supersampling and
large-asset VR timing/history memory remain open. Default-source bilinear bake
sampling and raw derivative normals are not proof of all material extensions or
full deformed consumer interoperability.

A cold private HOME can make the canonical Fontconfig startup enumerate all
installed fonts before the app constructs. GDB proved this, rather than a model
lock. Test startup permits 90 s; interaction checks keep 30 s deadlines. Shared
bdf_font.hpp remains byte-identical to canonical qimgvr and pinned hashes.
Profile/fix that cold discovery in the canonical helper, then propagate exact
copies, instead of casually forking the desktop layer in this repository.

Preserve immutable snapshots, worker-only geometry/file work, source/generation/
revision guards, ordered saving, exact cancellation/Undo and original archives.
