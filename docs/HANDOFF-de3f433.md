# Native plane-cut checkpoint — 2026-10-30

The user requested committing/pushing the current stage and preparing a handoff;
further implementation is deferred until work resumes. Application source:
`5b0f9f5ff317031ae92b800d094a62be532a3cf8`, based on `3d19776`, branch
`work/native-plane-cuts`. The later documentation commit records this checkpoint.
The full production objective in [REQUIREMENTS.md](REQUIREMENTS.md) is incomplete.
No agents were spawned for this stage.

Read [../AGENTS.md](../AGENTS.md), [AGENT-HANDOFF.md](AGENT-HANDOFF.md),
[VALIDATION.md](VALIDATION.md) and [PLANE-CUTS.md](PLANE-CUTS.md). Prior records
are archived in HANDOFF-3d19776.md, VALIDATION-3d19776.md and
AGENT-HANDOFF-3d19776.md; preserve earlier history too.

## Remote integration is the first resumption task

The checkpoint fetch found newer remote main `2c89e8d`, whose application merge
is `79fe64a`. It integrates this branch's earlier retopology base with additional
Scene8, material, skin, Unicode, worker shutdown and other production work.
This plane-cut source has NOT been merged with that newer main. Publish this
checkpoint on its feature branch; do not overwrite or force-push main.

Fetch again before resuming and read the latest remote HANDOFF/VALIDATION as
well as these branch-specific records. Preserve both histories and integrate
the plane-cut changes into the current production line. Known overlaps include:

- XR pages: this branch has Retopo14 and Cut15; newer main uses Skin14,
  reserves Masks15 and uses Retopo16. Allocate a nonconflicting cut page and
  update menu routing, labels and tests together.
- Document worker lifecycle: preserve newer predicate-mutex shutdown handshakes
  for all workers, including the new cut worker, and existing cancellation/save
  barriers. Source/header/library layout must remain coherent during validation.
- Scene8, material bindings, normal-frame provenance and newer GPU overlays must
  survive integration. New shell output is static PBR and must not silently
  relabel legacy normal pixels or claim source rig/MToon-extension transfer.
- Reconcile actual availability/check states, input parameter IDs, render
  snapshot metadata, CMake/test registration and the requirements ledger.

All evidence below applies to `5b0f9f5`, not the unperformed combined merge.
Do not replace newer main's validation with these older-base counts. The current
main handoff reports broader evidence; inspect its exact revision scopes there.

## Implemented in this stage

- Native active-object plane workspace shared by desktop and XR: offset in
  document millimetres, yaw/pitch, cap RGBA, flipped retained side, cap toggle,
  controller/stylus grip placement and local Undo/Redo. From hand respects the
  device confirming the command, including a left menu with both hands tracked.
- Native Mesh → Plane cut and XR Mesh tools commands with actual enabled/check
  states. BDF modal wheel/slider controls have scrolling content, fixed action
  buttons, Cancel default, universal Close and exact control cancellation.
  Default dragging remains trackball navigation; source/save edits are isolated.
- Dedicated worker and immutable preview: original selected PBR/MToon surfaces
  are clipped in the GPU, with real cap geometry, a plane grid and retained-side
  indicator. The authoritative Scene stays unchanged until explicit Apply.
- Apply publishes a new group, editable shell and independent cap, retaining
  the exact original as hidden. Exact source-triangle/barycentric bake mapping
  transfers PBR paint without choosing nearby surfaces by projection rays.
  Original paint, UVs, skin, morphs, layers, archives and topology are retained.
  New geometry is static at the displayed pose, using fresh 512-pixel atlases.
- Guarded ordered asset work gives one global Undo transaction. Invalid previews
  cannot apply an older plane; failure/cancellation retains the draft for retry.
  No-op Apply allocates nothing. Accepted Apply is included in wait_asset and
  survives the existing XR shutdown/final-save boundary.

## Verification and environment

All 51 Release CTests pass in 332.27s, including the 73-check real-loader
synthetic OpenXR report. Four whole-core C/C++ ASan/UBSan/leak suites pass in
45.28s. The cut suite has 55 assertions; existing retopology has 56. Dedicated
native GUI passes 12/12 normal and 6/6 compact 640x360 with 2x Unifont BDF.
The loader script retains prior XR coverage; new cut-specific controllers are
tested through production Document/SpatialInput and GPU fixtures separately.
Software graphics/synthetic tracking do not prove physical-device operation.
See VALIDATION.md for logs, exact scopes, fixture corrections and limitations.

All 30 manifest source hashes and the canonical qimgvr BDF helper match. This
branch uses the unchanged DomeD toolkit 282467e / 22 patches, prefix
`artifacts/toolkit-authoring`, library directory `lib`. See TOOLKIT.md. Newer
main documents a different local toolkit prefix; do not assume directories match.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit-authoring/share/fltk"
cmake --build build --parallel 2
LIBGL_ALWAYS_SOFTWARE=1 LD_LIBRARY_PATH="$PWD/artifacts/toolkit-authoring/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ctest --test-dir build --output-on-failure -j2
./build/run-dome-sculpt
```

GUI orchestration uses optional Python test dependencies dbus-next, python-xlib
and pillow, private HOME/bus/Xvfb, 90s cold-font startup and 30s interactions.
The runtime remains C++20/FLTK/EGL/OpenXR with canonical BDF Fontconfig/FreeType;
no Python runtime or Qt/GTK/Pango frontend is introduced. All validation handles
were terminal; inspect actual processes before starting another build. Artifacts
and builds are ignored local evidence, unavailable in a fresh clone. No system
install, active-session restart, fresh external Khronos validation or physical
device/accelerator test was performed for this checkpoint.

## Work after reconciliation

The next cut gap is repeated cutting across a generated shell/cap group. Current
commands affect only the active object: its open shell alone cannot receive a
closed cap; open trim affects no sibling cap. Implement a coherent multi-part,
multi-material source/output transaction rather than inferring groups by name.
General Booleans, nested/touching contour robustness, durable cut recipes,
quality/resolution controls, editable skin/morph transfer and draft recovery
remain unfinished. Source extraction expands render corners, so production
vertex compaction, GPU caching and large-mesh latency/memory evidence are needed.
This stage does not complete Divine Cut functionality.

Preserve every other REQUIREMENTS.md row: full sculpt/texture/UV/retopology,
rig/skin/morph/avatar/plugin workflows, format/material fidelity and physical
controller/stylus/3D-mouse/headset plus OneAPI/ROCm/CUDA evidence. Compare against
newer main before treating any earlier gap as absent or reimplementing it.

Repository: https://github.com/jebot-git/dome-sculpt at
`/home/blux/de-xr/dome-sculpt`. The parent is not a repository. Preserve unrelated
de-xr repositories and all newer remote histories.
