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

All three fresh configurations pass with canonical EEE toolkit/bitmap prefixes.
CTest registers395 cases. Runtime/source/header/test inputs are frozen:

- Optimized CPU Release alltargets: exec94973, live.
- Owned C/C++ ASan+UBSan core/support/render/frontend and selected fixtures:
  exec26164 terminal0. Selected native/render/GUI-lifetime gate exec75541 live.
- Optimized OneAPI alltargets: exec74603, live. Uses the existing pinned
  user-owned DPC++7.2 compiler and its separately documented local libumf
  compatibility copy. This is not yet a current hardware execution result.

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
