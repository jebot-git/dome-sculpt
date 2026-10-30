# Native sculpt resolution and VRM expression identity — 2026-10-30

Application `803479d7478af8e771cdc3dbb6b2d1ad72728d77` preserves published
`a4af86d` and the verified resolution/cut union `fff726f` through merge5985858,
then fixes VRM0 split-mesh expression remapping. Test-onlyee2b57d adds actual
DomeD Wayland resolution authoring. Test-only19ad5aa orders native Unicode input
before cross-channel menu commands. All product code remains identical to803479d.
The original production objective in REQUIREMENTS.md/HANDOFF-466ebd1.md is active
and incomplete; this is a verified milestone, not completion.

Read ../AGENTS.md, AGENT-HANDOFF.md, VALIDATION.md, TOOLKIT.md,
SCULPT-RESOLUTION.md and the existing local-meshing/GPU/cut/material/frame notes.
Exact prior handoffs and proof are archived with-a4af86d, with earlier histories
retained. Builds are optimized regular-user releases using current canonical
DomeD282467e/FLTK1.4.5+22 patches and its explicit lib64 runtime. No installed
files, active sessions or display manager were replaced/restarted.

## Delivered in this source

- Native FLTK Sculpt→Resolution and XR Sculpt page7/cell7 open the same guarded
  background operation. World-mm controls preserve unchanged local-float spacing,
  support normal/large BDF fonts, slider/half/double spacing, quality controls,
  modal text/history routing and Cancel-default/Return/Escape/Ctrl+W behavior.
- Resolution resamples independent layers/masks without flattening them. An
  explicit physical distance band preserves metre-valued layer composition when
  lattice spacing changes. Nondefault bands persist as Volume4; default-band
  Volume2/3 bytes/shape remain compatible. FlatVolume4/Scene4 discrimination and
  nested aggregate quotas are validated before bulk allocations.
- Detached candidates retain object/group/paint/source identities, hidden/locked
  layers and future IDs. Bounded topology/component/Euler, bidirectional surface
  distance, normal and independent layer/protection checks refuse unsupported
  quality loss. Source data remains exact on cancellation/refusal/allocation
  failure. Same spacing is a no-op. Publication is one global Undo transaction.
- Worker progress, expected generation/revision/object leases, accepted cancellation,
  retries and ordered Save are connected. Masks15/Skin14/Retopo16/Cut17 and the
  SIX-worker predicate shutdown remain intact. A live cut excludes resolution
  and native draft bypasses. New geometry owners/bands force correct mesher/VBO
  invalidation, while unchanged material/mask/control cases retain reuse.
- Trilinear field picking uses bounded cell DDA and cubic first-root solving,
  avoiding uncertified signed-distance jumps over thin surfaces. Budget exhaustion
  reports native status and cancels the stroke rather than escaping to a modal
  alert. Empty-space acceleration/ready-surface picking remain followup work.
- VRM0 expression expansion now remaps each emitted mesh from the immutable
  original bind/index. Native-only target insertion and differing emitted target
  maps previously reused an already-remapped index. The independent regression
  fails against the prior matching archive, passes with the fix, and verifies
  original metadata is unchanged. Full avatar female/male Release fixtures pass.

## Current proof

CPU88/88 native suites pass177.13s; real-loader150 existing+17 resolution XR
checks pass in273.14s. ArcOneAPI25 selected native suites pass13.35s; its complete
150+17 synthetic XR checks pass297.61s. Separate actualArc render fixtures pass
Document22/buffer24/MToon81/masks38/visibility25. Tracking/software GL in loader
runs and actual hardware rendering/compute are explicitly distinguished.

Whole C/C++ core and matching render consumers with ASan/UBSan/leaks pass15/15
focused suites121.31s plus13 fast VRM metadata assertions. Prior broader24-suite
sanitizer evidence for unchanged meshing/render/cut code remains archived under
a4af86d; it is not relabelled as the new run.

Native GUI/compositor151 assertions pass, including new actualDomeD resolution10,
normal/compact resolution12/4, original menu53/cuts12+6/masks17+5/skin11/Unicode4,
and actualDomeD mask9/general8. Screenshots of both resolution layouts inspected.
Three additional fresh Unicode burst runs pass4 assertions each with no inter-key
sleep. See VALIDATION.md for exact logs, source hashes and failed-attempt scopes.

The original Unicode keymap acknowledgement alone was insufficient: menu D-Bus
callbacks can overtake queued X11 typing. The updated harness copies through the
same native key event queue and waits for new clipboard ownership/data before
D-Bus menu tests; all expected UTF-8 bytes and original history/save assertions
remain unchanged. This is test protocol ordering, not a product Unicode change.

Only8f4d67d has the user's rudimentary physical WiVRn confirmation. That test was
ended; recovery/report remain in artifacts/xr-milestone-8f4d67d. No new physical
HMD launch or complete controller/stylus/3D-mouse/performance claim follows.

## Active production continuation

1. Combine adaptive paged field/mask/history storage. Privatea135f48 passed74
   Release+74 whole-core sanitizer suites and12,920 fault boundaries;3 repeated
   trials match25 numeric-state checkpoints. Production477255b now preserves
   current resolution/cuts/mask precision and is running new fullFLTK/XR/Arc proof.
   Whole CPU assembly/geometric VBO replacement remain after paging.
2. Finish native desktop stroke workers. A separate seventh-worker prototype owns
   detached ordered gestures, exact Cancel/End/history/save/Close drain and evaluated
   posed/morphed paint→originalUV mapping. Native API fixtures pass; real FLTK,
   richer deformed paint and sanitizer proof are ongoing. Do not assume its worker
   count/schema are already in this SIX-worker milestone.
3. Review/merge source/canonical normal-frame Scene9 runtime/export checkpoint
   after scoped proof. It preserves existing sourceUV guards and exposes remaining
   canonical deformation representability/coverage limits. Correct PBR COLOR_0
   and versioned MToon ignore semantics are next; full colored import/export and
   actual avatar UV/bake coverage are not complete.
4. Add conservative occupied-support picking, bounded assembly/GPU tile updates,
   sustained production-scale strokes/frame pacing and device evidence. Do not
   raise DDA budgets or silently drop queued path samples to hide missing work.
5. Preserve every remaining requirement: advanced painting/projection/stencils/
   texture sets/baking, cuts/retopo/procedural tools, fit/weights/shapes and complete
   glTF/VRM/frontend/device/accelerator interoperability. Guards or passing builds
   are not substitutes for implementing the full objective.

Preserve unrelated primary frame/creator WIP, stashes and all commits. No primary
reset, force-push, blanket staging, system installation or headset restart.
