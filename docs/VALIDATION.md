# Combined Scene8 / Retopo16 integration

Application source `79fe64a` preserves remote3d19776 and local6df3cf2.
All67 Release native suites pass121.84s (tests-retopo-native-full.log), excluding
only openxr-client, which was exercised separately through the real loader:
117/117 checks, complete=true, failed0, exit0 (tests-retopo-real-loader.log and
publish-final/build-retopo/xr-retopo-full/report.json). The original96 cases are
retained;21 new cases cover actual menu16 Resume readiness, pressure projection,
Apply, reference preservation, focus-loss rollback and unconfirmed final save.

Whole-core C/C++ ASan/UBSan/leaks14/14 pass102.12s, including Mikk/xatlas:
tests-retopo-sanitize.log; build-retopo-sanitize.log. Scene compatibility33
assertions, codepoint-safe generated names,5-worker deterministic shutdown and
forced pre/post-asset cancellation boundaries have native regression coverage.
Retopo GPU buffer reuse/GL state fixtures pass actual Arc and llvmpipe:
retopo-gpu-{arc,software}.log. No normal-frame guard has been removed.

Final combined native GUI checks pass (all exits0/reports failed0):
- Retopo12/12: menu-retopo-final.log; compact2xBDF6/6: menu-retopo-compact-final.log.
- General universal menus53/53: menu-retopo-general-final.log.
- Skin bind/transfer/history11/11: menu-retopo-skin-final.log.
- Unicode clipboard/history/save3/3: menu-retopo-unicode-final.log.
- Actual latest DomeD Wayland/universal-menu8/8: menu-retopo-dome-final-staged.log.

The scripts are exact source copies under artifacts/retopo-ui-staging/tests;
its build symlink targets publish-final/build-retopo, never the older build.
The first DomeD staging attempt lacked packaging/org.domed.Sculpt.desktop;
adding a link to the publication packaging fixed the harness. Its failed log
remains menu-retopo-dome-final.log. Earlier Retopo GUI failures remain
menu-retopo-integrated*.log: project-info payload hashing differed because
Scene8 inserts null optional fields absent from Scene5. The diagnostic now
canonicalizes only those two null fields. The fixture also waits for atomic
save completion as other authoring flows do. Nonnull source payload, geometry,
paint, retained identity and exact Undo/Redo assertions remain unchanged.

Canonical toolkit/compositor282467e remains latest on remote. No system install,
physical test relaunch or active desktop/session restart occurred. The full
production objective remains incomplete. Older source scopes follow.

# Verified authoring checkpoint `6df3cf2`; full production goal remains active

The exact optimized application source `6df3cf2` in artifacts/publish-final passes
all 64 Release suites in 208.57 s (tests-production-final-worker-safe.log), including
the full real-loader syntheticOpenXR suite in 155.07 s and deterministic worker shutdown.
The subsequent metadata commit does not change application code.

- Final native universal menus53/53, Skin bind/transfer/history11/11, Unicode
  input/clipboard/history/save3/3 and actual DomeD Wayland/universal-menu8/8 pass;
  process exits0, reports failed0. Logs: menu-production-final.log,
  menu-skin-production-final.log, menu-unicode-production-final.log and
  dome-menu-production-final.log.
- Whole-core C/C++ ASan/UBSan/leaks:15 authoring/export suites in 165.53 s, then the
  final shutdown/file/recovery/GLB/atlas6-suite run in 91.27 s. C Mikk and xatlas are
  instrumented. Logs: tests-production-integration-sanitize.log and
  tests-worker-shutdown-sanitize.log. The final atlas capacity refinement has
  additional17-case sanitizer proof in tests-atlas-column-capacity-sanitize.log.
- The same native ONEAPI FLTK/OpenXR application passes all 96 syntheticXR checks
  with Arc A770 compute, complete=true/failed0/exit0. Software EGL isolates loader
  and input. Log: tests-production-oneapi-final-worker-safe.log. Earlier numeric
  native5suite7.10s and shared-document6.38s retain separate source scopes.
- Actual Arc OpenGL and llvmpipe material/picking parity25/25; actual Arc Unicode
  palette/layout passes. Logs: mtoon-visibility-{software,arc}.log and
  tests-production-unicode-arc.log. Unicode dark/light/2x captures inspected.
- 16 baseline/posed/raw/tinted/painted/expression/explicit-role/name-identity avatar
  variants pass all 15 pinned official VRM0 schemas with blob SHA verification,
  and Khronos Validator with 0 errors / 20 warnings each. Logs:
  production-vrm-official-schema.log and production-vrm-khronos.log; JSON hashes
  in production-vrm-schema-report.json. Warnings remain visible.

Canonical DomeD/FLTK source282467e was rechecked remotely; Release22patch toolkit
prefix artifacts/toolkit-282467e/lib64 and matching standalone compositor were
used. No system installation or active session restart occurred. Physical basic
WiVRn operation was user-confirmed only for8f4d67d, then ended as requested.

Earlier integration attempts below are historical evidence for their sources.
The60-suite attempt exposed atlas precision and recovery pruning defects; the
63-suite attempt exposed band rounding and shutdown lost-wakeup defects.
The old shutdown executable deterministically fails the new native rendezvous;
fixed code passes. Atlas17 checks and the unchanged bake39 checks pass without
relaxing geometry/UV/watertightness/timeout assertions.

A newer remote3d19776 native-retopology milestone must still be integrated with
this checkpoint and independently rebuilt/tested. Its separate published proof
does not validate a combined schema/page/worker merge. This checkpoint and all
of its passing checks do not complete the original production objective.

# Authoring and bake-controls integration — merge pending

The full production objective remains active and incomplete. Current work merges
incoming `7817ca2` native bake controls with local `88cde79` / application
`8f4d67d`. No result below proves this in-progress combined merge. Rebuild and
verify the final combined source before publishing main; never force-push over
remote work.

The exact local application checkpoint `8f4d67d` was built in
`artifacts/verify-authoring-merge` and passed:

- 47/47 Release suites in 147.34 s (`tests-authoring-pinned.log`).
- Five whole-core ASan/UBSan/leak suites in 18.24 s, including C Mikk/xatlas
  instrumentation (`tests-authoring-pinned-sanitize.log`): glTF interoperability,
  baked textures, source morphs, UV workflow and weight authoring.
- Native universal menus 53/53, rig/weight GUI 12/12, selected-avatar export GUI
  5/5, baked-surface GUI 5/5 and actual latest DomeD Wayland/universal menus 8/8.
  All process exits were zero and reports had zero failures. Logs use
  `authoring-pinned` names; see the exact record below.

FLTK and compositor were built from clean canonical DomeD `282467e`, including
its updated popup-parent patch. Toolkit: `artifacts/toolkit-282467e`; compositor:
`artifacts/domed-282467e-target/release/dome-compositor`. The verified local
checkpoint and evidence notes are published on
`checkpoint/native-xr-authoring-8f4d67d` at `88cde79`, independently of main.

Incoming `7817ca2` has its own prior-workspace bake-control Release, GUI,
sanitizer and synthetic XR evidence, archived verbatim in
[VALIDATION-7817ca2.md](VALIDATION-7817ca2.md). Those results do not establish this
combined merge. Its desktop/controller bake controls, cancellation/progress,
source selection and exact Bake/Clear Undo must coexist with local weight
painting, conformal UV, source VRM export and linear/sRGB XR transport.

Pending lazy-UV initialization, all-slot source-UV baking and bind/transfer helper
work is preserved in `stash@{0}` (`Preserve lazy UV source UV baking and skin
authoring during7817 merge`). It is outside both application checkpoints until
explicitly restored, integrated and verified. No installation, active-session
restart, physical input/headset or accelerator-backend completion is claimed.

The following records are historical evidence for their stated sources; older
"pending" notes describe those stages, not the current merge state.

---

# Merged avatar/UV/voxel verification — 2026-10-29

Latest exact-source evidence:8f4d67d, artifacts/verify-authoring-merge:
47/47 Release suites147.34s (tests-authoring-pinned.log); five whole-core
ASan/UBSan/leak suites18.24s (tests-authoring-pinned-sanitize.log), including
instrumented C Mikk/xatlas dependencies. Native universal menus53/53,
rig/weight GUI12/12, selected-avatar export5/5, baked-surface GUI5/5 and real
latest DomeD Wayland/universal-menu8/8 all exit0 with failed0 reports. Logs:
menu-authoring-pinned.log, menu-weights-authoring-pinned.log,
menu-export-authoring-pinned.log, baked-gui-authoring-pinned.log and
dome-menu-authoring-pinned.log. Both FLTK and compositor are built from clean
DomeD282467e; exact compositor SHA256 is
d54c05abd1d742c9473f275a670030b1689943242f31d958023b6fb1e6c63732.
No physical device/accelerator/system installation claim. Remote7817ca2 arrived
after this pin; these results do not prove that forthcoming bake-controls merge.

Pinned cf6d6b3 passed29/29 optimized suites in126.78s and four whole-core
ASan/UBSan/leak suites in55.35s (groups with real avatars/controller actions,
source UV, creator archive and independent glTF). Logs: tests-avatars-pinned.log
and tests-avatars-pinned-sanitize.log. Native universal menu42/42 passed.

After preserving remote0c266de UV/voxel/wakeup work, combined30/30 Release
checks passed in77.44s (tests-avatar-merge.log). XR pages9Avatars/10Mesh tools
are distinct and source voxel output composes group world placement.

Latest clean DomeD7322b0e Release build uses unchanged canonical22-patch FLTK.
Private Wayland/universal-menu8/8 passed with pinned cf6client and latest host
(dome-menu-avatars-pinned-final.log). Initial test assumptions broke because
new menu items shifted rows and Right enters submenus. The harness now chooses
actual exported rows, navigates headers with Left and explicitly selects Close;
all original Cut/Undo/modal/desktop-routing checks remain.

Cache and MToon component targets are newly registered with actual baseline
arguments. They have their own component checks; full exact merged checkpoint
verification is next, with no full MToon/VRM/device/backend completion claim.

# Actual CC0 grouped avatar integration — 2026-10-29

All28 Release suites passed in56.86s (`artifacts/tests-avatar-controls.log`),
including actual female/male grouped native creation/restart, joystick creation/
group operations, independent grouped glTF skin evaluation and synthetic OpenXR.
Native universal-menu42/42 passed (`menu-creator.log`); explicit source unlit/
opaque/mask/blend GPU checks passed (`tests-source-alpha.log`). Actual file
save/reopen QA retained exact source and parameters with files119,795,368bytes
female /124,948,360bytes male, below256MiB (`vroid-native-workflow-qa.log`).

The newly registered source-uv target and actual baseline arguments for source
map/archive tests await a final pinned run. Shared component sanitizer results
retain their own scope. Root combined pinned verification is next. No physical
input/headset/accelerator-backend or integrated full MToon/VRM completion claimed.

# Guarded asset-worker and native import milestone — 2026-10-29

All21 Release suites passed in50.68 seconds (`artifacts/tests-assets-final.log`).
The subsequently added adaptive-remesh target passed with worker/import/BVH/bake
checks (`tests-assets-remesh.log`), and the paint-preservation/warning refinements
passed UV/shared-document/worker/remesh/bake checks (`tests-assets-report.log`).
The affected real-loader OpenXR client passed separately in56.85 seconds
(`tests-assets-xr.log`). All39 native universal-menu checks passed, including
Import chooser ownership and exact cancellation (`menu-assets.log`).

Final source-pinned native sanitizer verification is pending; shared agent
sources changed during the first build, so its running logs are not treated as
verification of this exact checkpoint. Prior component/integration evidence
below retains its stated scope. No physical device/accelerator/install claim.

# Integrated native authoring and recovery — 2026-10-29

All19 Release CTests pass (50.47 seconds), including native input/recovery,
rig/avatar/deformation/import/retopology and the real-loader synthetic OpenXR
client: `artifacts/tests-authoring-milestone.log`. Native GUI22/22 and universal
menu37/37 checks pass in `artifacts/gui-recovery.log` and
`artifacts/menu-recovery-chooser.log`. The latter creates actual private
checkpoint fixtures, discovers them through the application worker and cancels
the native recovery chooser without changing the current document.

Seven focused whole-core ASan/UBSan/leak suites pass in1.10 seconds:
`artifacts/tests-recovery-sanitize.log`. Later notification/UI refinements await
the final focused rebuild; their coverage is not implied by that earlier run.
Per-component source/validator/sanitizer limits are in RIGGING.md, AVATAR.md,
DEFORMATION.md, IMPORT-AND-MESH.md and NATIVE-RETOPOLOGY.md.

All local GUI/OpenXR checks select artifacts/toolkit, the canonical22-patch
Release FLTK build described in TOOLKIT.md. No physical headset/input device,
accelerator, system installation or session restart is claimed. Earlier remote
checkpoint evidence below belongs to its stated environment.

# Continuous UV gesture history — 2026-10-29

Native FLTK drags/wheel bursts and XR ray/stick/fine-wheel adjustments now undo
as gestures. Shared workspace regressions preserve exact undo, cancellation,
redo branching and invalid intermediate drafts. No-op scale input no longer
introduces floating-point UV edits that destroy redo.

- Nine non-XR suites passed in `artifacts/tests-uv-gesture-final.log`. That
  aggregate process was terminated with exit 143 during OpenXR; it is not a
  completed ten-suite success and its incomplete report was not reused.
- The separate reviewed OpenXR CTest passed in 125.40 s:
  `artifacts/tests-uv-gesture-xr-reviewed.log`. Its report has 40 checks,
  status=passed, complete=true and zero failures.
- All 22 native GUI checks passed in `artifacts/gui-uv-gesture-reviewed.log`,
  including an actual multi-event mouse drag and wheel burst with one exact
  image undo each. All 25 universal-menu checks passed in
  `artifacts/menu-uv-gesture.log`.
- Three focused ASan/UBSan/leak suites passed in 207.45 s:
  `artifacts/tests-uv-gesture-sanitize.log` (UV/controller/shared document).
- Focused optimized UV/controller/document checks passed in 29.23 s:
  `artifacts/tests-uv-gesture-focused.log`.

The canonical pinned DomeD toolkit was rebuilt locally and its 22 patch hashes,
application order and combined digest match third_party/sources.json. Source:
`artifacts/domed-source`; Release libraries: `artifacts/toolkit`. Actual ldd
resolution is recorded in `artifacts/linkage-uv-gesture.log`; no Qt/GTK/Pango
libraries are listed. Build logs: `artifacts/build-toolkit-gesture.log`,
`artifacts/build-uv-gesture-reviewed.log`, and
`artifacts/build-uv-gesture-sanitize.log`. No system install/session restart,
new nested-compositor, physical-device or accelerator check is claimed.

The first GUI assertions captured before queued drag processing/repaint finished.
Temporary traces proved actual coordinate changes and were removed; final GUI
assertions wait for a changed image and exact restoration. This revealed a real
remaining synchronous-validation latency limit. A separate isolated benchmark of
20 serial translations through Document::edit_uv on the persisted 19,944-triangle
painting fixture measured 106.3 ms mean / 110.1 ms maximum, with exact gesture
undo. Source/output: `artifacts/uv-gesture-benchmark.cpp` / `.json`. This single
CPU workload does not prove production-size interaction performance.

Machine-readable local evidence: `artifacts/validation-uv-gesture.json`.
Full scope remains incomplete in REQUIREMENTS.md; resume via HANDOFF.md.

# Reconciled checkpoint — 2026-10-29

Merged upstream toolkit/test work from `96bc62f` with the ordered-input repair.
The combined build passed. All ten CTest suites passed, including 40 real-loader
synthetic OpenXR checks (`status=passed`, `complete=true`, zero failures).
Log: `artifacts/tests-checkpoint-merge.log`; runtime report/comparison projects:
`build/xr-client/`. The merged UV tests also passed ASan/UBSan/leak checking
(68.61 seconds): `artifacts/tests-checkpoint-merge-sanitize.log`.

These fresh checks used the available `/opt/fltk-vr/toolkit` installation,
not upstream's newer ignored canonical-toolkit build, which is absent here.
Actual linkage is recorded in `artifacts/linkage-checkpoint-merge.log`: patched
FLTK resolves from that prefix, with no Qt/GTK/Pango libraries listed. Build logs
are `artifacts/build-checkpoint-merge.log` and its `-sanitize` counterpart.
Vendored source hashes, Python harness syntax and Git whitespace checks pass.
No new GUI/nested-compositor, physical-device or accelerator run is claimed.
The earlier seven-suite sanitizer and upstream current-toolkit evidence below
remain separate results. See [HANDOFF.md](HANDOFF.md) for resumption instructions.

# Ordered spatial input and session resumption — 2026-10-29

A reproducible queued-confirmation defect is resolved: a focus boundary
previously cleared queued controller releases and Apply actions. A deterministic
event burst failed before the fix and passes afterward. The exact cause of the
historical native XR failure remains unproven because its comparison artifacts
were unavailable; subsequent native runs pass.

- All ten optimized CPU integration suites passed in
  `artifacts/tests-input-order-full.log` (223.63 seconds).
- The subsequent expanded native OpenXR run passed all 38 checks, including
  stop/IDLE/READY resumption and a new post-resume sculpt stroke
  (`artifacts/tests-session-resume.log`, 155.65 seconds).
- All seven native ASan/UBSan/leak suites passed in
  `artifacts/tests-input-order-sanitize.log` (352.59 seconds).
- Focused UV/controller/document checks cover queued confirmation before focus
  loss, cancellation of unfinished work, final event draining at shutdown,
  rejection of late samples, input reopening, continuous UV scale/turn and fine
  wheel rates. Their earlier focused run passed in 22.28 seconds.
- The XR harness resets its report at startup, updates it after each check,
  records completion/error status and preserves fixture projects and UV/restart
  comparison JSONs. It cannot leave the old complete report after a later failure.

Machine-readable local evidence is in `artifacts/validation-input.json`.
See [INPUT.md](INPUT.md) for the contract and [HANDOFF.md](HANDOFF.md) for remaining
work. These are synthetic/native CPU/software-GL tests; physical devices,
production-size performance and accelerator execution remain unverified. The
full objective is active and incomplete.

# UV and universal-menu handoff checkpoint — 2026-10-29

The current stage adds editable UVs and filtered layer transfer, native FLTK/XR
controls and the universal-menu fixes documented in [the audit](UNIVERSAL-MENU-AUDIT.md).
The user requested committing/pushing the current stage for later resumption.

- 18 FLTK GUI checks, 25 exporter/dialog menu checks and eight tests through an
  actual nested DomeD/Wayland compositor passed.
- Nine suites passed in the latest optimized ten-suite run; the OpenXR suite
  failed its new continuous UV-transform persistence assertion. Earlier complete
  ten-suite successes predate the final continuous-transform refinement.
- All seven native sanitizer suites passed in 331.55 seconds. The completed
  handle and logs are recorded in [HANDOFF.md](HANDOFF.md).
- Stale success reports must not override the failing final log. The original
  production objective remains active and incomplete.

# Layered painting, XR controls and DomeD integration — 2026-10-29

This checkpoint adds stable UV painting assets and sparse editable paint layers,
controller-oriented parameter controls and concrete DomeD convention fixes.
The full production objective remains active in [the requirements](REQUIREMENTS.md).

- All nine optimized CTest suites passed: controller controls, texture painting,
  scene authoring, shared document, sculpt geometry, independent glTF/PNG
  interoperability, session appearance, real GL rendering and OpenXR client.
  A final four-suite run covers the updated controller buttons, version-two
  migration fixture and complete slider framing. A further GL run verifies
  visible aim-ray/crosshair feedback above the slider panel.
- All 32 synthetic OpenXR checks passed through the real loader and GL
  swapchains. The added cases paint and save a blue texture layer, embed its PNG
  in GLB, adjust a material through continuous stick and aim-ray input, and
  verify slider interaction leaves source geometry unchanged.
- All 13 private-Xvfb FLTK checks passed, including opening the native brush
  editor, painting a visible blue patch and restoring exact viewport pixels
  with undo. All ten private DBusMenu-host checks passed for real exported
  actions, radio/enabled states, local-bar handoff, Ctrl+W dialogs and the
  Cancel-default confirmation. The GUI fixture now isolates its Fontconfig
  cache and allows a longer startup deadline under concurrent test load.
- Painting tests cover pressure footprints, independent channels/coverage,
  layer visibility/masks, exact stroke undo/redo/cancel, immutable snapshots,
  persistent mesh/UV/layer round trips, independent duplicate edits and malformed
  input. cgltf parses the exported scene and libpng independently decodes its
  embedded maps, checking UV indices, PNG rows/bytes, material texture bindings
  and factors.
- Controller tests cover continuous preview, scalar fine-wheel rate, ray drag,
  secondary stylus interaction, RGBA row isolation, logarithmic radius,
  single-step persistent undo, focus cancellation and ray-clickable Cancel/Done.
  Trigger capture prevents sculpting behind a control panel.
- Appearance checks cover live standalone XR BDF face/size/2× scale, theme,
  accent, 25/50/75/100% chrome presets and recovery from partial preference
  writes. GL checks cover exact #303030/#d0d0d0 backgrounds, transparent overlay
  space, moving sliders, visible pointer feedback and 25% panel alpha.
- Runtime linkage uses the patched FLTK libraries with no Qt, GTK or Pango.
  Vendored checksums pass; canonical qimgvr appearance/menu helpers are unchanged.
  Blender ray picking provenance and the system libpng dependency are recorded
  in the [source manifest](../third_party/sources.json).

All six native suites passed ASan, UBSan and leak checks in the final run
(279.54 seconds). The initial concurrent sanitizer run exceeded the 60-second asset
wait in two fixtures; it did not report an address/undefined/leak error. Core
asset fixtures now allow 180 seconds for instrumented extraction/atlas work,
while independently checking that render-snapshot reads remain responsive.
The application still prepares meshes asynchronously.

Reproduce the native instrumentation checks with:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDOME_BUILD_UI=OFF -DDOME_BUILD_XR=OFF -DDOME_COMPUTE_BACKEND=CPU \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure
```

Machine-readable evidence is in `artifacts/validation-paint.json`; log paths
and tested binary hashes are recorded there. These generated artifacts are
excluded from source control. Physical headset/controller/stylus behavior,
production-scale performance and CUDA/HIP/SYCL execution are unverified. Baking,
remeshing, manual UV editing, humanoid rigging, weights, shapekeys, full MToon,
full glTF/VRM and the remaining plugin-equivalent workflows are unfinished.

# Scene and material integration checkpoint — 2026-10-29

The persistent scene supports independent sculpt objects, uniform transforms,
visibility, edit locks and per-object PBR material channels in FLTK and XR.

- All six optimized CTest suites passed: scene authoring, shared document,
  sculpt geometry, independent glTF interoperability, GPU rendering and the
  OpenXR client. The scene suite verifies object identities, version-one
  migration, transformed sculpt isolation, metadata/stroke history, exact
  persistence, saved-state markers and malformed-input rejection.
- All 26 synthetic OpenXR checks passed, including creating an independent
  sculpt and changing its material through joystick submenu actions.
- All ten private-Xvfb FLTK checks passed, including object duplication/undo,
  the native object/material inspector, applying a blue material and undoing it.
  Captures include `artifacts/gui/08-material-editor.png` and
  `artifacts/gui/09-material-applied.png`.
- The real EGL/GL rendering fixture displays separate red and blue sculpt
  objects. Independent cgltf validation checks separate nodes/meshes/materials,
  exact local geometry, node transforms and PBR values including emission,
  opacity and double-sided flags.
- ASan/UBSan/leak checks passed for all four native core suites. The final
  scene and document suites were rerun after ordered save-queue changes.
- The save regression queues export and an older asynchronous save ahead of a
  newer desktop save, then verifies the newest geometry remains on disk after
  every worker drains. Save-failure/retry and exact cancellation checks pass.

Machine-readable results are in `artifacts/validation-scene.json`. Full native,
GUI and sanitizer logs are named there. These generated artifacts are excluded
from source control. The full objective remains active; production authoring,
painting, rigging, VRM, hardware and accelerator requirements remain unfinished.

# Native XR integration checkpoint — 2026-10-29

The OpenBrush DomeD basis is pinned at
`1373d5e839a9720adaeb187064785f9d9d0e8492`; the Blender basis remains 5.4 Alpha
at `3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2`. Source roles and checked hashes
are recorded in [the provenance manifest](../third_party/sources.json).

- Optimized CPU build: all five CTest suites passed: shared document,
  sculpt geometry, independent cgltf interoperability, EGL/OpenGL rendering
  and native OpenXR client integration.
- The native synthetic runtime uses the real OpenXR loader and real GL
  swapchain textures. All 23 client checks passed, covering tracked controller
  and MX Ink tip-pressure edits, stereo eye rendering, focus/tracking loss,
  scheduled reference-space rebases, joystick menus, background save/export,
  and explicit primary-session selection when overlay support is absent.
- All five private-Xvfb FLTK checks passed: shaded surface, carve, exact undo
  pixel restoration, default trackball orbit and view reset.
- ASan, UBSan and leak checks passed for all three native core suites.
  The shared-document suite was rerun successfully after the final recenter
  and worker-notification changes.
- The GPU rendering test checked PBR/toon output, light/dark grey backgrounds,
  a transparent XR overlay, and visible BDF menu selection. Rendering used
  software OpenGL; it does not validate CUDA, HIP or SYCL execution.
- A bounded installed-Monado run exited successfully after 12 frames with
  11 stereo submissions. Monado reported a simulated HMD and no controllers.
  Physical headset, controller and stylus behavior remains unverified.
- Runtime linkage inspection found the patched FLTK frontend and no
  Qt, GTK or Pango dependencies. No system installation was performed.

Local logs and captures are generated under `artifacts/`, with machine-readable
results in `artifacts/validation-xr.json`, `artifacts/gui/report.json` and
`build/xr-client/report.json`. These generated files are intentionally excluded
from source control. Reproduction commands are in [the README](../README.md).

This checkpoint establishes a connected native editing path. The full
production objective remains active; incomplete authoring, scene, material,
rigging, export and hardware requirements remain in
[the completion matrix](REQUIREMENTS.md).


## 2026-10-29 resume, current DomeD toolkit

See TOOLKIT.md and HANDOFF.md. Release build uses canonical DomeD742d009's
FLTK1.4.5 and all22 committed patches, rather than the old inherited /opt build.
Ten native suites pass, with36 real-loader synthetic XR assertions; GUI18/18,
menu25/25 and newly built nested DomeD8/8 pass. Continuous UV scaling/turning,
fine-wheel rate and exact cancellation have new focused regressions. Five
additional UV replays from one exact persisted baseline preserve field/paint
hashes and change UVs consistently. The old handoff's persistence failure has
not reproduced; a root cause is not claimed. New artifacts retain projects and
separate comparison hashes, and a failed run cannot leave a stale passed report.

Latest canonical DomeD was built from a clean pinned worktree. Existing capture
source edits in DomeD/qimgvr remain untouched. No session restart, installation,
physical-device test or new sanitizer/accelerator result is claimed. The first
nested run failed from a distribution-specific software Vulkan manifest path;
the portable test driver selection fixed the harness, after which all8 pass.


## Pose/shapekey and source-preview integration

- Exact811eb69 was built in a detached worktree:33/33 Release tests passed in
  83.60s, plus four whole-core ASan/UBSan/leak suites in167.83s. It was pushed
  to origin/main without force, preserving remote0c266de history.
- Native rig authoring focused suite passes both actual CC0 avatars, independent
  weighted vertex rotation, all19 linked source parts, unchanged paint/source
  identities, native restart, guarded worker/undo/redo, staged controller XYZ
  and shapekey accept/cancel. Related controller/document/groups/glTF suites
  pass5/5 in6.67s (artifacts/tests-rig-regression.log).
- Whole-core rig editing ASan/UBSan with leak detection passes in8.52s
  (tests-rig-sanitize.log). This covers native operations, not FLTK/driver code.
- Focused private-bus/Xvfb FLTK menu workflow passes8/8: native modal ownership,
  parameter entry, universal Close cancellation, worker Apply, exact pose
  Undo/Redo and shapekey Apply/Undo (menu-rig-expanded.log). Dialog capture
  menu-rig/rig-options.png was visually inspected for BDF/geometry/layout.
- Source-preview regression proves actual Document mesh workers publish raw
  original image bindings with zero CPU composites, while exports and painted
  overlays still receive complete composites (tests-preview.log).

The source-preview-only checkpoint0231951 has a fresh detached optimized build
under artifacts/verify-preview; all35 registered Release suites passed in96.65s (tests-preview-pinned.log). This
working rig evidence is separate from that pinned source. A combined pinned run
is still required after the root rig commit and agent dependencies. No physical
input-device or compute-backend verification and no installation is claimed.


Groom integration: registered native groom suite and actual append_humanoid
20part pose/native restart fixture pass2/2 in3.57s. Original groom material uses
its declared unlit appearance and follows the exact head pose across group
parts. Source-neutral19parts and original archives remain intact. Independent
Blender QA details and known posed collision limit are in VROID-CREATOR.md.


## Verified published rig/groom checkpoint1f6698a

Fresh detached source in artifacts/verify-rig-groom:
- All37 Release CTests passed78.17s (tests-rig-groom-pinned.log).
- Four whole-core ASan/UBSan/leak suites passed20.25s: creator archive,
  groom, source-preview and rig editing (tests-rig-groom-pinned-sanitize.log).
- Native general universal-menu53/53, focused rig8/8 and actual latest DomeD
  Wayland/universal-menu8/8 pass (menu-rig-groom-general-pinned.log,
  menu-rig-pinned.log, dome-menu-rig-groom-pinned.log). All process exits0 and
  all JSON reports failed0. Test displays/buses/HOME were private.
- Canonical DomeD remote main rechecked: still7322b0e; its unchanged22-patch
  FLTK1.4.5 Release toolkit and rebuilt compositor remain current. No session
  restart or system installation. No new physical-device/compute evidence.

Weight helper cd064a3 follows this checkpoint as four new independent files;
42 Release/sanitizer assertions reported by creator agent. It still needs core
registration and actual desktop/XR weight-paint controls/history/visualization.
It is not part of the37-suite published application checkpoint. The same agent
now owns NEW uv-solve.hpp/cpp/tests/docs for full conformal unwrap and explicit
seam stitching with complete source vertex/corner remaps and exact pins. Root
will wire worker/UV workspace/native/XR paths; do not run solves on input thread.

Paint agent is implementing TextureMaps.color_coverage plus shared MToon
renderer/outline/UV1 and frame-time contracts. Current main archives are older
than that public header change; wait for his final ABI barrier before rebuilding
main. Source-pinned rig/groom artifacts above remain coherent. Export agent's
VRM0 source-only actual-avatar outputs pass schema/core glTF validation and
physics/basis/normal preservation fixtures; final sanitizer, painted coverage,
generated groom and VRM1/full required semantics remain incomplete. He owns
export.cpp/gltf-document.hpp/new VRM files. Root next should integrate committed
VRM export into guarded background File jobs and native/XR export controls,
finish interactive weights, then UV solve, remaining sculpt/retopo/bake/plugin
work and physical/compute verification. Preserve full REQUIREMENTS objective.


## Current authoring before remoteaf492ee merge

Working integrated build completed;44registered suites have evidence across
42/44 broad run plus two corrected focused suites. Export test now independently
decodes red PNG (generic factors are neutralized); XR fixture now reads released
texture and checks correct linear/sRGB display colors, with both swapchain modes.
Focused export passes6.11s, full real-loader XR passes68.06s. Native rig/weights
GUI12/12 and avatar export GUI5/5 pass; screenshots inspected. Shared MToon
hardware54/software54/instrumented54 and source/morph/UV component scopes are
recorded in focused docs. This is NOT an exact merged-source full run. Remote
baking/Mikk af492ee must merge preserving both histories, then get fresh pinned
Release/GUI/sanitizer checks before publication. Full goal remains incomplete.


## Remote source-aware bake merge now in progress

Origin af492ee is being merged preserving both histories. See VALIDATION-af492ee.md
for the other workspace evidence; it is not fresh validation of this merged tree.
Scene-v6 baked persistence, original-aware normal/AO projection, canonical Mikk
tangents and GPU/export coverage must coexist with local MToon/linear targets,
UV/weight/controller authoring and source morph/VRM guards. Exact merged-source
Release/GUI/sanitizer verification is still required before push.

## Working merged tree with updated canonical FLTK

DomeD main282467e includes the eaeb8e0 popup-parent fix. Its clean canonical
22-patch Release toolkit was built into artifacts/toolkit-282467e and the client
rebuilt against it; actual ldd resolution is in linkage-toolkit-282467e.log.
All47 Release suites pass147.90s in tests-authoring-merged.log, including the
real-loader synthetic XR client87.87s. General menus53/53 and baked-surface
painting/Undo/UV-Cancel5/5 pass in menu-authoring-merged.log and
baked-gui-authoring-merged.log. Generated bake fixture remains in
artifacts/bake-workflow-merged; baked-baseline.png was visually inspected.
The owned merged UV checks pass79existing +55workflow cases, and the renderer
agent reports64Arc A770/64llvmpipe cases plus39cache cases. Agent GPU sanitizer
scope instruments renderer/paint/source/tangents/test, with Release core/Mikk
dependencies and Mesa leak detection disabled; this is not whole-core evidence.
The working tree includes an unstaged additive lazy-UV helper; these results
must not be relabeled an exact merge-commit validation. A fresh committed-source
Release/sanitizer build and current-compositor verification are next.
# Physical headset milestone8f4d67d

Launched the exact verified8f4d67d standalone overlay through the installed
WiVRn runtime. It selected the real WiVRn HMD and both controller devices;
runtime performance telemetry advanced while the process was running. The
user reported: "rudimentary functionality confirmed", then requested ending
the test. Only the test process was terminated (exit143); recent native recovery
snapshots were copied into artifacts/xr-milestone-8f4d67d first. The requested
CLI final-save path was not produced by signal termination. The compositor and
runtime remain running. Local physical-test-report.json records the scope.
This verifies basic physical operation by user report; it does not validate
every workflow, sustained performance, stylus, reconnect or full device scope.

# Exact combined native/XR bake-authoring milestone7cc0d42

Frozen source in artifacts/verify-bake-controls-merge passes all48 optimized
Release suites262.50s, including the real-loader synthetic OpenXR client105.37s
(tests-bake-controls-pinned.log). Four whole-core ASan/UBSan/leak suites pass6.71s:
document-assets, bake-controls, uv-workflow and weight-authoring. C Mikk and xatlas
are instrumented; log tests-bake-controls-pinned-sanitize.log. Native menus53/53,
bake dialog19/19 and current clean DomeD282467e Wayland/universal-menu8/8 pass
with failed0 reports and process exits0 (menu-bake-controls-pinned.log,
menu-bake-authoring-pinned.log, dome-menu-bake-controls-pinned.log). The native
bake-options capture was visually inspected for BDF/layout. Toolkit/compositor
source still282467e when rechecked. Remote0141926 changes only documentation;
the merged application source remains7cc. Future dirty schema/lazy-UV/skin/signal
changes are outside this evidence. No new installation/session restart occurred.
# XR signal shutdown and oneAPI compiler qualification

The unmodified7cc client failed the synchronized SIGINT regression because it
did not write the requested final save (tests-xr-signal-before.log). A candidate
app with signal-stop handling, compiled against the exact7cc headers and linked
to its unchanged core/render/other UI objects, passes all83 real-loader XR checks
(tests-xr-signal-after.log). Both SIGINT/SIGTERM exit0, balance eye/frame resource
calls, preserve the independently completed stroke and cancel the unfinished
gesture. This is the signal candidate proof, not yet a combined lazy-UV/schema
build. The old physical test's exit143/recovery-only behavior is retained above
as historical evidence; the fix does not retroactively change it.

The ONEAPI configure probe now compiles and links a real SYCL queue/kernel.
System clang22 accepting -fsycl but missing sycl/sycl.hpp fails configure with
the explicit SDK diagnostic; user-owned official DPC++7.2.0 passes the probe.
Logs: configure-oneapi-missing-sdk.log and configure-oneapi-complete-sdk.log.
Compiler qualification/device enumeration alone do not prove production compute.

## Publication integration with e65db2a

The incoming independent-workspace validation is archived in
VALIDATION-e65db2a.md. Its worker/enum layout and additional regression checks
are preserved alongside deferred UV, graceful signals and Arc/mesh fixes. This
new combined source is not yet covered by either parent's evidence. Root will
build/test the exact integration before updating origin/main. The ongoing v7
material/export and skin UI changes in the primary working tree are excluded.
