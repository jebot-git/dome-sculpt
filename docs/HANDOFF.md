# Combined plane-cut and production checkpoint — 2026-10-30

Application source 2ec8a5a59990f2d64887434560a6402d5a9ec665 merges plane checkpoint
de3f433 / source 5b0f9f5 with production main 2c89e8d / source 79fe64a, preserving
both histories. Branch: work/plane-cut-integration. This integration is verified.
Final fetch discovered newer main 9a0cfc4 / source 20c132d (sculpt masks/symmetry);
that newer milestone is not merged here. Publish this feature checkpoint without
overwriting main, and reconcile it first when work resumes. The full objective in
[REQUIREMENTS.md](REQUIREMENTS.md) remains incomplete. No agents were spawned.

Read [../AGENTS.md](../AGENTS.md), [AGENT-HANDOFF.md](AGENT-HANDOFF.md),
[VALIDATION.md](VALIDATION.md), [PLANE-CUTS.md](PLANE-CUTS.md) and the material,
normal-frame, worker and skin authoring notes. Prior main/feature handoffs and
validation are preserved byte-for-byte in corresponding -2c89e8d and -de3f433
files. Preserve older creator/integration history as well.

## Implemented integration

- Native active-object plane workspace, mapped PBR transfer, independent editable
  cap/shell and exact hidden-source retention now coexist with Scene v8, immutable
  material origins, source UV/skin/Unicode work and GPU-cached retopology overlays.
  Complete legacy v7 decoding and source normal-frame guards are retained.
- XR pages are distinct: Skin page 14, reserved Masks page 15, Retopo page 16, Cut page 17. Shared input
  dispatch, enabled/check state, BDF palette labels, two-row direction/four-row
  RGBA controls and confirming controller ownership are reconciled. Existing
  Retopo New/Resume/Mode pressure-access behavior is retained. Default desktop
  dragging stays trackball navigation; native cut dialogs use wheels/sliders,
  scrolling content, fixed actions and Cancel/default/Close conventions.
- All six workers use the predicate-mutex shutdown handshake, including the new
  cut worker. Dedicated geometry work stays off input/GUI/render threads, with
  immutable snapshots, session/source identity guards and stale-work rejection.
- Cancel cut during accepted publication now requests asset-owner cancellation.
  It retains the draft and preview until acknowledgement, allowing correction
  or retry without partial source/history changes. Desktop and joystick paths
  share the behavior. Accepted work drains before standalone final save; remaining
  unconfirmed cut/retopo drafts are then discarded explicitly.
- New deterministic cut cancellation/retry and six-worker shutdown coverage
  complements the retained original cut/skin/retopo tests. Real-loader cut replay
  uses actual rendered availability, joystick/ray controls and independently
  inspected native geometry, not a product-side authoring shortcut.

## Verification

All 69 non-XR Release CTests pass, zero failures, in 461.56s. Logs:
artifacts/tests-cut-integration-native.log, -native-detail.log and the preserved
full individual output -native-output.log. The registered OpenXR harness is
verified separately below; there are 70 registered suites in this configuration.

The full real-loader XR harness passes 127/127 checks, failed=0, complete=true,
status=passed. It retains 117 existing checks and adds 10 cut workflow checks.
Log: artifacts/tests-cut-integration-xr.log. Report:
build-cut-integration/xr-client/report.json, also copied to
artifacts/report-cut-integration-xr.json. All client subprocesses and the harness
exit 0. The registered harness command was invoked directly during non-XR CTest,
with the same explicit toolkit environment and unchanged 40-second client limit.

Nine whole-core C/C++ ASan/UBSan/leak suites pass in 91.44s. All 103 native GUI
checks pass: cut 12 + compact 6, retopo 12 + compact 6, general 53, skin 11 and Unicode 3.
The cut-authoring suite has 56 assertions. VALIDATION.md records exact revision
scopes, logs, rendered captures, diagnostics and limits. Older main's physical,
Arc compute and actual Wayland-host proof remains historical; it does not prove
those devices/backends on this source. Current Intel ADL-N graphics additionally
passes the 81-check MToon suite and full spatial renderer, separately from XR
tracking/compute. No system install or session restart.

## Build and continuation environment

The tested application is in build-cut-integration. The older build/ belongs to
the pre-merge feature stage; do not accidentally validate its executable. GUI
scripts were copied unchanged to artifacts/cut-integration-ui/tests with build
symlinked to build-cut-integration. The combined core/headers were rebuilt in
build-sanitize with UI/XR OFF and whole-core C/C++ instrumentation.

The unchanged canonical toolkit is DomeD 282467e / 22 patches, local prefix
artifacts/toolkit-authoring, library directory lib. All 30 manifest hashes and
canonical qimgvr BDF helper match. Newer main's lib64 prefix is not present in
this environment. The parent DESIGN_CONVENTIONS file is absent here; use the
committed menu audit, toolkit references and canonical helpers without claiming
fresh parent-file inspection. Preserve unrelated de-xr repositories.

```sh
cmake -S . -B build-cut-integration -DCMAKE_BUILD_TYPE=Release \
  -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit-authoring/share/fltk"
cmake --build build-cut-integration --parallel 2
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 LD_LIBRARY_PATH="$PWD/artifacts/toolkit-authoring/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ctest --test-dir build-cut-integration --output-on-failure -j2
./build-cut-integration/run-dome-sculpt
```

Keep native C++20/FLTK/EGL/OpenXR and canonical bitmap BDF Fontconfig/FreeType
behavior. No Python runtime, Qt/GTK/Pango frontend or system/session replacement
was introduced. Optional GUI orchestration uses uv/dbus-next/python-xlib/pillow,
private HOME/bus/Xvfb and the existing startup/interaction deadlines. All stage
build/test/app handles are terminal; the final process inventory found none
remaining. Artifacts
and builds are ignored local evidence, absent from a fresh clone. Inspect actual
live handles/processes before restarting work or validation after an observation
timeout. The isolated cold-software-loader diagnostics are recorded in VALIDATION.

## Next production work

First fetch and inspect newer main 9a0cfc4 / source 20c132d and its HANDOFF,
VALIDATION and SCULPT-MASKS notes. Preserve both histories. Masks now uses page15;
Cut page 17, Retopo page 16 and Skin page 14 remain distinct. Reconcile the new Volume v3/Brush/Sample
layouts and quotas, Document mask/mirror controls, mesh snapshots/cache fields,
shader attributes/varyings, cut-cap scalar attributes and the loader fixtures.
Rebuild all consumers coherently, including any selected compute backend; never
link the new public layouts with this checkpoint's older core. Keep cut's sixth
worker shutdown handshake and accepted cancellation/save barriers. Retain source
mask/control data exactly when hiding the original, and update the combined
requirements ledger without treating incoming masks/symmetry work as absent.
The new upstream's Arc/ONEAPI and host results are scoped to20c132d, not this
feature or its future merge. No force-push or unverified merge was performed.

After that reconciliation, repeated cuts across a generated shell/cap group are the next cut gap. Current
commands affect only the active object: closed-cap mode rejects the open shell,
and open trim does not cut its sibling cap. Extend coherent multi-part/material
source, preview and one-transaction publication rather than inferring group
semantics from a name. General Booleans, stronger contour predicates, source
compaction/GPU preview caching, durable recipes, bake quality controls, new
skin/morph and complete material/extension transfer and draft recovery remain.
Static output currently uses 512-pixel PBR atlases; retained original assets keep
all source paint/rig/morph/archive data. This is not a complete Divine Cut suite.

Preserve every unfinished REQUIREMENTS row: production sculpt masks/symmetry/
resolution/local meshing and sustained VR; projection/stencils/texture sets and
baking; advanced retopo/procedural/plugin workflows; interactive rig/landmarks/
fingers/automatic weights and morph authoring; complete avatar/MToon/glTF/VRM
fidelity and independent consumers; physical controllers/stylus/3D mouse/headsets,
reconnect/session-loss/performance and eligible OneAPI/ROCm/CUDA workloads.
Do not reimplement integrated main's skin/UV/material/Unicode work as absent.
Original/native frame work must preserve versioned provenance and the existing
live SourceUV guard; legacy UvDerivative pixels cannot simply be relabelled.

Fetch/status/collaboration inspection must precede new overlapping work. Public
repository: https://github.com/jebot-git/dome-sculpt at
/home/blux/de-xr/dome-sculpt. The parent is not a repository. Preserve remote
history and unrelated projects; never force-push a checkpoint over newer work.
