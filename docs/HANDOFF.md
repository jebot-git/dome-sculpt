# Resume checkpoint — 2026-10-29

The user requested: “Commit and push current stage, work will resume. Prepare
agents handoff.” This checkpoint changes documentation only. The application
source remains at `33f26a29c7eea51d9cb1a4ffb25fed7d2a3ff226`, already published
to origin/main and origin/work/live-weight-feedback. Both remote refs matched
that source commit when fetched for this checkpoint. The working branch is
`work/live-weight-feedback`. The full objective remains incomplete in
[REQUIREMENTS.md](REQUIREMENTS.md).

Read [../AGENTS.md](../AGENTS.md), [AGENT-HANDOFF.md](AGENT-HANDOFF.md),
[VALIDATION.md](VALIDATION.md) and [HANDOFF-33f26a2.md](HANDOFF-33f26a2.md).
The last file preserves the detailed live-weight milestone handoff. Earlier
integration records remain in HANDOFF-e65db2a.md and VALIDATION-e65db2a.md.

## Current application stage

Desktop/XR skin-weight gestures have real worker-coalesced immutable previews,
fixed posed geometry until release, exact cancellation and one global Undo on
acceptance. Material-aware posed picking preserves alpha, culling and retained
PBR/MToon source semantics. Desktop picking uses the displayed frame timestamp
and rejects stale geometry. The final source commit also fixes input ownership
when another command finishes a held gesture while its accepted edit is pending.
Native compact BDF weight controls and universal-menu tests are retained.

No application changes were made during the subsequent retopology investigation.
It inspected the existing topology kernel and requirement gaps only. There is
no partially implemented UI, Scene schema, worker or input integration to recover.

## Verified evidence

Existing local logs/reports were inspected again for this documentation-only
checkpoint; no new application build/test run is claimed:

- The bf4fb86 implementation passed all 49 Release CTests in 333.39 seconds.
  Its real-loader synthetic XR report has 73 checks, failed=0, complete=true,
  status=passed. Four selected whole-core sanitizer suites passed in 28.72s.
- Final frame-time/UV1/layout refinements passed three focused Release suites
  in 3.91s and two whole-core sanitizer suites in 14.09s.
- The final ownership regression passed Release in 2.94s and whole-core
  ASan/UBSan/leak checking in 12.96s. Application/test rebuilds succeeded.
- Native GUI reports retain 14/14 normal-size and 4/4 compact 640x360 at 2x
  Unifont BDF. These GUI runs precede the final ownership-only fix.

[VALIDATION.md](VALIDATION.md) names exact logs, scopes, diagnostic attempts and
limitations. Builds, reports and captures under artifacts/ and build/ are ignored
local evidence, unavailable in a fresh clone. Synthetic tracking/software GL is
not physical headset/controller evidence. No installation, active-session restart,
new external Khronos validation or physical device/accelerator completion is
claimed. No build/test/application-validation jobs from this task were running
when inspected. Check live processes before resuming; an observation timeout is
not grounds to restart a job.

## Resume work

Fetch origin and inspect status/history before editing; preserve any newer remote
work. The repository is https://github.com/jebot-git/dome-sculpt at
`/home/blux/de-xr/dome-sculpt`. The parent is not a repository. Preserve unrelated
de-xr projects. No agents were spawned in this stage.

The next investigated gap is surface-constrained native retopology authoring.
The existing RetopoMesh kernel has stable IDs, triangle/quad faces, wires,
projection/relaxation, pins/seams, atomic edits and independent serialization;
it is not yet integrated into Scene/document/desktop/XR authoring. Read
NATIVE-RETOPOLOGY.md and the concrete kernel before designing persistence,
source identity, overlays, input gestures and exact transaction/cancellation
behavior. AGENT-HANDOFF.md identifies extension points and unresolved choices.
Retain original assets and stable topology; heavy geometry belongs on workers.

Do not narrow the wider requirements: production sculpt masks/symmetry/local
meshing, texture/stencil/source-UV/atlas quality, retopology/cut/procedural tools,
interactive rig/bind/transfer/morph authoring, complete glTF/VRM/MToon fidelity,
plugin basics and real device/backend validation remain unfinished. Live weight
previews still recompute bounded strokes and rebuild snapshots; production-size
latency/memory and efficient updates remain to be evaluated. Alpha picking uses
base-level sampling; swept-volume painting is not geodesic/occlusion-clipped.

## Build environment and conventions

Use C++20/FLTK/EGL/OpenXR and BDF through canonical Fontconfig/FreeType helpers.
No Qt/GTK/Pango runtime frontends. Preserve DomeD universal menus, modal/history
routing, Cancel-default confirmation, Ctrl+W/Escape and controller sliders/wheels.
Rendering consumes immutable snapshots; retain generation/source guards, ordered
file work, exact undo and source paint/skin/morph/archive fidelity.

[TOOLKIT.md](TOOLKIT.md) records the pinned DomeD `282467e` toolkit with 22
verified patches. This workspace uses artifacts/toolkit-authoring and its lib
directory; older toolkit/lib64 observations are historical. The canonical BDF
helper and third-party source hashes were unchanged by this stage.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit-authoring/share/fltk"
cmake --build build --parallel 2
LIBGL_ALWAYS_SOFTWARE=1 \
  LD_LIBRARY_PATH="$PWD/artifacts/toolkit-authoring/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ctest --test-dir build --output-on-failure -j2
./build/run-dome-sculpt
```

Prepare the pinned toolkit first in a fresh checkout. Use the launcher or explicit
library path to avoid inherited same-SONAME FLTK libraries. GUI orchestration uses
`uv run --with dbus-next --with python-xlib --with pillow python tests/menu.py`
with a private HOME/bus/display and the correct library path; Python is optional
test orchestration only. Cold Fontconfig discovery remains a canonical qimgvr
follow-up (90s GUI startup, 30s interactions). Do not edit sources while a compiler
may be reading them. Apply appropriate focused/broad/native/sanitizer validation
to subsequent source changes and report exact tested revisions.
