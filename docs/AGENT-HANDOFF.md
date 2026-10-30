# Resumption entry point — plane-cut checkpoint, 2026-10-30

Read [HANDOFF.md](HANDOFF.md), [../AGENTS.md](../AGENTS.md),
[VALIDATION.md](VALIDATION.md), [PLANE-CUTS.md](PLANE-CUTS.md) and
[REQUIREMENTS.md](REQUIREMENTS.md). This is a checkpoint at the user's request;
implementation resumes later. The full production objective remains incomplete.

Source `5b0f9f5`, branch `work/native-plane-cuts`, repo
`/home/blux/de-xr/dome-sculpt`. The parent is not a repository. The final docs
commit follows the source commit. No agents were spawned and no build/test jobs
were left running by this stage. No system install/session restart was performed.

IMPORTANT: fetch discovered newer main `2c89e8d`, application merge `79fe64a`.
This checkpoint is a divergent feature branch, not its replacement. Fetch/read
the latest main handoff, preserve both histories, and integrate before continuing
production work. This branch uses Retopo14/Cut15; newer main uses Skin14,
Masks15/Retopo16. Reconcile page/parameter IDs, Scene8/material/normal-frame
contracts, worker shutdown handshakes, renderer changes and all test registrations.
Do not claim the current 51-suite evidence validates that future integration.

Key code contracts to preserve:

- src/document-cut.cpp owns a dedicated worker. Preview uses an immutable
  displayed MeshSnapshot and extracted BakeSource, guarded by session,
  generation/revision and active ID. Input stages settings; geometry stays off
  GUI/input/render threads. Requests coalesce, but finite in-flight work may
  publish useful older feedback while the latest request is pending.
- CutView::preview_request must equal request before Apply. Invalid settings
  may retain old visual feedback, never accept it as the current plane.
  Control Done groups one local history entry; Cancel restores its baseline.
  Workspace cancel restores the exact unclipped base snapshot. Idle cache
  disposal stays outside model/input locks. Source/save mutation is blocked.
- MeshSnapshot carries cut_session/cut_request/clipped_object/clip_plane.
  Clipping uses document position before canvas transforms and only the selected
  source object. A synthetic UINT64_MAX range holds the cap. Original texture
  bindings/shading stay intact until Apply; the authoritative Scene is untouched.
- BakeOptions' appended source_triangles/source_corner_barycentrics require
  valid counts, barycentrics and reconstructed geometry. Atlas corner order is
  checked. Empty mappings retain the established ray/cage path. Mapped transfer
  uses no projection rays; source AO is retained without new discarded-solid AO.
- FileJob::cut_session guards ordered Apply. Success closes the workspace and
  records one global transaction; cancellation/failure retains it. wait_asset
  includes accepted cut work before final save. Preserve retopology's existing
  accepted-input drain and all newer main worker shutdown fixes during merging.
- SpatialInput::command_device records the confirming controller. From hand
  uses that grip, not an idle preferred right hand. Common release-to-rearm and
  multirow joystick conventions remain unchanged. Cap RGBA has four rows,
  direction two. Desktop dialogs use BDF, scrolling content and fixed Cancel/Done.

Tests: cut-authoring has 55 assertions, including exact mapped paint, group/pose
coordinates, left-owned input, stale/invalid/no-op handling, cancellation,
native/glTF persistence and accepted-Apply shutdown. Release 51/51, sanitizers
4/4, normal/compact native GUI 12/12 and 6/6 pass at this source. The real-loader
XR report is 73/73 synthetic; it does not specifically replay the new cut workflow.
Detailed logs, environment and diagnostic fixtures are in VALIDATION.md.

After integration, prioritize group-wide repeated cuts: current shell/cap
outputs are separately editable, but commands cut only the active object.
Closed-cap mode rejects the open shell; open trim leaves siblings untouched.
General Boolean/robust contours, source compaction, durable recipes, production
quality/performance and new skin/morph/advanced material transfer are unfinished.
Retain the whole requirements ledger and compare with newer main's implemented
work before selecting the next task. Preserve native FLTK/EGL/OpenXR, canonical
BDF Fontconfig/FreeType and DomeD universal menu/history/cancellation conventions.
