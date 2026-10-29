# Agent resumption entry point — 2026-10-30

Read [HANDOFF.md](HANDOFF.md), [../AGENTS.md](../AGENTS.md),
[VALIDATION.md](VALIDATION.md), [RETOPO-AUTHORING.md](RETOPO-AUTHORING.md) and
[REQUIREMENTS.md](REQUIREMENTS.md). HANDOFF.md is authoritative; HANDOFF-* files
are historical. The full production goal remains incomplete.

Repository: /home/blux/de-xr/dome-sculpt, branch work/native-retopo-authoring.
The parent is not a repository. Fetch and inspect status/remotes before editing;
preserve newer histories and unrelated de-xr repositories. Main implementation
fdb132b and final source 251cd3d are committed. This checkpoint's later docs
commit records the verified state. No agents or validation jobs are left running
by this stage. No system install or active-session restart was performed.

Retopology is now integrated; do not implement the earlier kernel-only plan again.
Scene has immutable RetopoPatch metadata on SculptObject. Scene v7 embeds the
independent stable topology codec and source ID beside the derived painted mesh.
RetopoWorkspace owns source-local topology/reference queries and local history on
a dedicated Document worker (src/document-retopo.cpp). Frontends publish bounded
sample jobs; move samples coalesce without crossing press/release/cancel barriers.
EditorState::retopology is immutable. Session plus generation/revision/active-ID
checks reject stale work. Input, geometry kernels and GL stay on their respective
threads; source mutation/save is blocked until explicit Apply/Cancel.

The existing desktop/SpatialInput stroke backend handles topology gestures before
paint/weight/sculpt dispatch. XR page 14 is reached through mesh tools; while a
workspace exists its joystick menu opens directly. RetopoDistance uses common
slider controls. Native menus export real availability and Place/Move radios;
Ctrl+W/Close cancels the projection dialog while retaining the workspace. Undo
routes to local topology history, then Apply commits exactly one global scene
transaction. Preserve release-to-rearm: after reopening resets ownership, a fresh
neutral controller sample must precede the next press.

Apply queues ordered asset work using an appended FileJob::retopo_session marker.
Success closes the draft; failures/cancellation keep it for correction. Snapshot
visibility reveals the retained reference while editing a previous target;
common cancellation/no-op acceptance must force the authored mesh preview back.
The idle worker drops obsolete BVHs/history outside model/input locks. wait_asset
first drains an accepted topology barrier, because it can precede asset_busy;
keep this contract at XR shutdown and ordered final saves. Inspect the 56-case
retopo-authoring suite before changing these worker/input/history paths.

No bridging/extrusion, automatic quads, wire-only scene objects, draft recovery or
production topology/overlay performance is complete. Atlas UVs are derived and
do not yet consume topology seams/pins as a prescription. Further Apply creates
a separate retained version; paint/skin/morph transfer remains explicit. The
plane-cut kernel still lacks native desktop/XR authoring. Continue these gaps
and all other production/format/rig/device/backend rows in REQUIREMENTS.md.
Preserve C++20/FLTK/EGL/OpenXR, canonical BDF Fontconfig/FreeType helpers, DomeD
universal menu/modal/Cancel conventions and controller sliders/wheels. Build and
validation environment, exact revision scopes and known limits are in HANDOFF.md
and VALIDATION.md. No Python runtime or other frontend toolkit is permitted.
