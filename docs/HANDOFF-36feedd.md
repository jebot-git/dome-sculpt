# Agent handoff — continuous UV gesture history

Full objective: [REQUIREMENTS.md](REQUIREMENTS.md), still incomplete. Native
C++20/FLTK desktop/XR sculpting, texturing and avatar authoring retain the full
Blender 5.4 Alpha/Gmsh/OpenBrush basis and requested upstream/plugin workflows.
No Python runtime, Qt/GTK/Pango frontend, animation or final scene rendering.
Preserve BDF-only fonts, DomeD appearance, shell-owned Super+X, universal-menu
ownership and controller-oriented sliders/joysticks/fine wheels.

Repository: https://github.com/jebot-git/dome-sculpt, main. Previous checkpoint:
`d8e3699`; its full handoff is [HANDOFF-d8e3699.md](HANDOFF-d8e3699.md). The user
has authorized public checkpoints and handoffs. No system install or session
restart occurred in this stage.

## Changes

- `UvWorkspace` keeps one pre-gesture immutable view, updates previews and stores
  one history entry when the gesture ends. Undo during a held gesture closes and
  reverses that entire gesture. Cancellation/no-op gestures preserve the redo
  branch. Discrete pick/tool/topology/history operations close a prior gesture.
  Identity transforms avoid floating-point drift and spurious redo destruction.
- Native FLTK drags group until release/focus loss. Wheel bursts group until
  250 ms inactivity; widget destruction removes pending timers. XR ray/stick/fine
  wheel transforms share a gesture until all active continuous sources end.
  Idle owner samples cannot split a secondary pointer's drag. Tracking loss of
  that pointer ends its gesture while retaining the workspace; owner focus loss
  still cancels the complete draft. Apply remains one global paint/UV transaction.
- UV picking/mode changes no longer copy the complete mesh just for selection.
  Transform copies and UV validation are still synchronous and require further
  performance work. This is not a production-scale latency claim.
- Native/XR tests cover 100-sample gesture undo/redo, pending Undo, no-op/cancelled
  gestures, separate pin commands, invalid intermediate UVs, redo branching,
  60-sample ray drags, secondary tracking loss and separate coarse/fine gestures.
  GUI tests use actual drags and wheel bursts and verify one exact image undo.

## Build and validation environment

Read AGENTS.md and TOOLKIT.md before building. The pinned canonical DomeD
742d009f446cffdeb89031664b72885ff6f16a19 toolkit is now built locally in
artifacts/toolkit, with all 22 patch hashes/order verified. The clean source is
artifacts/domed-source. Release build uses that FLTK_DIR; actual linkage is in
artifacts/linkage-uv-gesture.log. No installed libraries were replaced. The parent
DESIGN_CONVENTIONS file is absent; the current DomeD README and universal-menu
audit were read. Generated source/build/test artifacts remain ignored.

Fresh checks: nine non-XR suites passed before the aggregate run was interrupted;
the separate reviewed OpenXR run passed all 40 checks (complete=true). GUI 22/22,
universal menu 25/25 and targeted UV/controller/document ASan/UBSan/leak suites
3/3 passed. See VALIDATION.md for exact logs. An isolated local benchmark of
20 UV translations on the 19,944-triangle persisted fixture measured 106.3 ms
mean / 110.1 ms maximum per Document::edit_uv, with exact whole-gesture undo.
This is a concrete interaction-latency problem to fix, not a production-speed
claim. Benchmark source/output are ignored under artifacts/uv-gesture-benchmark.*.

Inspect live
processes before starting/restarting jobs. An earlier aggregate run was terminated
with status 143 after nine suites passed, during OpenXR; its report remained
incomplete and is not a success. The later OpenXR result must be assessed from
its own terminal log and `status`/`complete` fields. GUI tracing proved edits were
processed while the test captured too early; temporary tracing was removed.
GUI assertions now wait for rendered changes/exact restoration instead of relying
on a fixed 300 ms delay. The underlying latency limit remains explicitly open.

## Resume work

1. Profile large UV drafts and move expensive preparation/validation off the
   interaction path with source/revision guards. Preserve invalid previews,
   disabled invalid Apply, exact rollback and gesture/global history semantics.
2. Complete advanced conformal unwrapping, stitch/texel-density workflows, native
   import/remeshing/baking and sculpt layers/masks/symmetry/local meshing. Use
   appropriate Gmsh/VCG/MeshLab/Wings/other native kernels without their frontends.
3. Complete stencils/projection/texture sets and humanoid rigs/automatic weights/
   skinning/morphs/full MToon/glTF/VRM plus the requested plugin-equivalent tools.
   Do not substitute static GLB export or reduce the original objective.
4. Verify lost-instance/session recreation, physical controllers/stylus/trackball/
   3D mouse and accelerator backends. CPU and synthetic/software-GL evidence does
   not establish physical-device or CUDA/ROCm/OneAPI operation.

Rendering reads immutable snapshots; model/input/file/asset work obey source and
revision guards, ordered saves and explicit cancellation. Input queues remain
bounded at 64; old-space poses are invalidated on rebases/document epochs. Read
INPUT.md, UV.md, PAINTING.md and UNIVERSAL-MENU-AUDIT.md for contracts and limits.
