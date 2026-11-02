# Current basic authoring integration candidate

Frozen runtime/test source25fe2089cea72c2ab5e8ba4a942cb8d3de58e52c in
`artifacts/basic-authoring-source20-7a4443e` combines published main7a4443e with:

- Image projection49c06b7 and owned-modal selectionadcb068, through649befe.
- Portable actual PBR preview618000b, including epoch/source/queued-Save and
  real cached/fallback projected-material query handling. No Source21 ancestry.
- Initial rig fittingc18a010: landmarks, world-metric fit/bind, native/joystick
  controls and overlay. Existing-rig/source-rest transport remains required.
- Paint-history bookkeeping78736d6: three noexcept state swaps, actual78-check
  counter/history/cancel regression. Metadata-only11e2a55 imports its final
  component receipt3d74da8 without changing runtime/test inputs.
- Native FBX adapter3a55c84 and registered fixturesd95e29e on current public
  source: bounded skin/morph conversion, genuine pinned inputs, independent
  edited poses/GLBs and actual import-worker Save/cancel/generation/history.

This is an unqualified combined candidate, not a main publication or completion
of any remaining full-goal row. It adds no Scene21/22/23/24/25 persistent data.
All newer normal/colour/paint-domain/rig-rest work remains separate.

The actual composed change additionally fences PBR preview during physical
landmark placement, while an idle fitting draft keeps its exact source lease
across view changes. Four new mixed-mode assertions exercise placement refusal,
preview re-enabling, exact source and history. Neither parent proof alone covers
this new composition. Spatial pages25 projection,26 preview and27 rig fitting
use28 shared pages; explicit preview IDs1024+ and rig IDs2048+ do not collide.

Merge review retained both sets of appended fields/commands/test registrations,
the preview epoch input fence and both fixture reference-space conditions.
Appended CMake blocks were corrected to retain their own condition nesting
before successful configuration. The projected painter's evaluated/world metric
sampling fix and all three paint-counter swaps remain present.

## Live qualification

All three fresh configurations and builds pass with canonical EEE toolkit/bitmap
prefixes. CPU registers395 cases; OneAPI396 includes its extra no-GPU gate.
Runtime/source/header/test inputs remain frozen:

- Optimized CPU Release alltargets94973: terminal0. Actual full395 regression
  exec38150 is live; do not restart on an observation timeout.
- Owned C/C++ ASan+UBSan core/support/render/frontend+selected fixtures26164:
  terminal0. Selected21/21 pass85.49s, including123 PBR/mixed-rig assertions.
  Separately registered actual-avatar1/1 passes172.54s (111checks/18images).
- Optimized OneAPI alltargets74603: terminal0. Actual A770 level_zero:0 selected
  compute/native9/9 pass13.87s;184analytic endpoints,224CPU-reference batches/
  445088samples,maxerror8.9407e-08 and explicit no-GPU refusal. GL stayed software
  for this compute scope. The SDK's documented local libumf copy is unchanged.
- Strict PCI03 A770 renderer254 checks pass:123PBR+111actual-avatar+11projection+
  9rig-guide. Source/binary manifests before/after are byte-identical.
- Actual native FLTK normal-size PBR13, projection11 and rig-fitting8 pass.
  Two initial GUI runners used absent default build directories; logs retained,
  corrected explicit paths only. No new all-routes sanitizer-GUI claim.

Composed compact FLTK11projection+13PBR+8rig pass. Actual instrumented native
frontends11projection+8rig+15PBR normal-close checks pass; the last is a test-only
artifact copy performing real Close Window before restart and at final exit.
Latest private DomeD Wayland9projection+9rig pass; actual-loader/syntheticXR
10projection+10PBR+11rig pass in separate output directories from the live full
suite. Pinned Khronos validates34 current emitted GLBs with0errors/354unfiltered
warnings (20Arc PBR/avatar +14FBX/pose), not all future full-suite outputs.

Read validation/BASIC-AUTHORING-UNION-WIP.json for exact scopes/hashes. Full395
regression remains live and must be diagnosed/completed before publication.

Build logs, configurations and the395-case manifest are under this tree's
`artifacts/`. The focused sanitizer JUnit is under `build-sanitize/`.
Inspect exact handles/processes before changing or restarting work. An
observation timeout is not terminal. Do not edit or mix ABI consumers during
these jobs. No physical test, system install or session restart occurred.

The parents retain separately qualified scopes: root649 Release23/SAN20,
FLTK11normal+11compact+11SAN, Wayland9, Arc11; portable PBR Release8 plus separate
projection-loader1, nativeGUI13+13, Arc116+111; initial fitting's own native/
SAN/FLTK/Wayland/loader/Arc evidence; actual paint-history Release2/SAN2; and
FBX standalone native/consumer/worker/GUI evidence on its coherent parent.
These counts cannot be added into a fictitious combined all-green run.

After build/gate completion, run the actual full Release suite and composed
FLTK/compact/sanitizer/Wayland/loader workflows, independent format/device and
selected OneAPI runtime/CPU-reference checks. Preserve every initial failure
and diagnose real scope changes. Publish only with accurate integration
receipts and main source equality. The full production objective stays active;
controller-mounted drawers remain required after the basic targets are met.
