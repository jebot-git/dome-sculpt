# Agent resumption entry point — 2026-10-29

Read [HANDOFF.md](HANDOFF.md), [../AGENTS.md](../AGENTS.md),
[VALIDATION.md](VALIDATION.md) and [REQUIREMENTS.md](REQUIREMENTS.md).
HANDOFF.md is the current checkpoint; older HANDOFF-* files are historical.

The user requested committing and pushing the current stage for later resumption.
The application source is at `33f26a29c7eea51d9cb1a4ffb25fed7d2a3ff226`;
this checkpoint changes documentation only. No retopology integration was
implemented after that source checkpoint. No agents or validation jobs are left
running by this task. No system installation or session restart was performed.

Resume in `/home/blux/de-xr/dome-sculpt`, branch `work/live-weight-feedback`.
Fetch origin and inspect status and remote histories before editing. Preserve
newer remote commits and unrelated repositories under `/home/blux/de-xr`.
The parent directory is not a Git repository. Do not force-push.

The next investigated task is native retopology authoring. Read
[NATIVE-RETOPOLOGY.md](NATIVE-RETOPOLOGY.md), `include/dome/retopo.hpp`,
`src/retopo.cpp` and `tests/retopo.cpp`, then inspect Scene persistence, document
workers/history, mesh-authoring controls and desktop/XR input/rendering. The
kernel already supports stable-ID triangle/quad topology, wires, pins, seams,
projection, relaxation and independent persistence. Document/desktop/XR editing
is still missing. A separate capped plane-cut kernel also exists.

Before implementing, decide how authoritative topology and source identity will
persist in Scene, how an unfinished patch is displayed, and how controller and
mouse gestures cancel or commit exactly one undoable transaction. Keep heavy
projection/validation/triangulation on workers with immutable publication and
generation/source guards. Preserve original paint, skin, morphs and source
archives; retain stable topology rather than only its display triangles. These
are design questions for the next agent, not an already chosen implementation.

The full objective remains in REQUIREMENTS.md, including the other unfinished
sculpt, paint/UV/bake, rig/avatar, plugin, format and device/backend workflows.
Use the pinned canonical FLTK toolkit and BDF helpers; preserve universal menu,
Cancel/Close, bitmap appearance and XR controller conventions. See HANDOFF.md
for the current build environment and the precise scope of existing validation.
