# Native authoring and bake-control integration

This checkpoint on work/integrated-native-authoring combines
0141926 (desktop/XR bake controls) with 88cde79 (weight painting, conformal UV,
MToon rendering and source-preserving avatar export), preserving both histories.
Read AGENTS.md and REQUIREMENTS.md; the full production objective is incomplete.

The original stage records are HANDOFF-0141926.md / VALIDATION-0141926.md and
HANDOFF-88cde79.md / VALIDATION-88cde79.md. AGENT-HANDOFF.md describes the discovery
of these divergent branches; its unimplemented notes apply to the bake-only
branch, not the combined source. Remote agent names are historical, not live
ownership in this thread. No agents were spawned here.

## Integrated behavior

- Desktop Mesh > Bake surface and joystick Objects > Mesh tools > Baking
  (page12) retain source/bounds/AO/padding controls, cancellation/progress and
  exact Bake/Clear history. Rig > Weights (page13) and desktop Scene > Skin
  weights retain explicit joints, pressure capsules, locks/limits/pruning,
  semantic symmetry, visible linked parts and a selected-joint GPU heatmap.
  Page navigation, labels, slider parameters and command identities coexist.
- The common scene worker carries both atomic cancellation/progress and an
  explicit operation-changed result. A wholly constrained weight gesture creates
  no dirty/history entry. Cancellation and every source rejection check occur
  before releasing the operation lease. Returned cancelled candidates cannot
  publish. A new regression bakes a skinned target, paints linked weights,
  clears the bake, and verifies separate exact global Undo/Redo, retained maps,
  authored deformation and the complete hidden source.
- Worker conformal UV/stitching, full deformation/source-identity remaps,
  source-derived MToon/linear GPU composition, shared source morph transport,
  background GLB/selected-avatar VRM0 export and their guarded workflows are
  retained from 88cde79. Canonical Mikk render-only corner expansion retains
  every weight scalar and original-image vertex identity. It does not mutate
  rest topology, skin/morph arrays or archived creator data.
- The canonical 282467e FLTK toolkit was rebuilt locally, with all22 patch
  hashes verified. Use artifacts/toolkit-authoring as in AGENTS.md/TOOLKIT.md;
  build/run-dome-sculpt selects its lib directory. Source hashes still match
  all30 manifest entries; shared BDF helpers remain canonical qimgvr copies.

## Evidence and remaining work

All48 registered Release suites pass on the combined source in328.08 seconds.
The real-loader synthetic XR report is complete/passed with73 checks and zero
failures. Six selected whole-core C/C++ ASan/UBSan/leak suites pass in153.30
seconds. Native GUI checks pass: universal menus53, bake19, running cancellation3,
compact 2x-BDF bake4, rig/weights12, avatar export5 and UV workflow12. See
VALIDATION.md for exact logs, scope and the interrupted first validation attempt.
The separate nested DomeD Wayland universal-menu retry passes8/8 with the local
host binary identified in VALIDATION.md; the active desktop is untouched.
Neither parent's earlier test results are substituted for these combined runs.

Continue the full REQUIREMENTS.md objective. In particular, weight edits still
compute after release; live stroke feedback, alpha-aware picking, production
timing, automatic binding/transfer and full rig/morph creation tools remain.
Conformal/stitching fixtures do not establish production atlas quality or full
source-map UV bake/adopt. VRM0 source export is deliberately guarded; generated
or painted avatar export, full VRM1/constraints/expressions and full glTF fidelity
remain incomplete. Keep the existing retopology/cut kernels and add complete
desktop/XR workflows, procedural tools and production sculpt/mask/symmetry/local
meshing. All physical devices and OneAPI/ROCm/CUDA execution remain unverified.

Cold private-HOME Fontconfig discovery can scan all installed fonts before
App/Document construction. Initial GUI startup gets90 seconds; interaction
deadlines remain30 seconds. The UV GUI harness now uses the same startup-only
allowance. Profile/fix discovery in canonical qimgvr before propagating copies.

Preserve ordered workers, immutable snapshots, source/cancellation guards, exact
gesture history, retained source archives, BDF fonts and universal menu ownership.
No system installation, active-session restart or physical-device/backend
completion is claimed. Inspect real process state before starting validation;
never treat a tool observation timeout as termination. No agents were spawned.
