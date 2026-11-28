# Closed-volume skin binding reference

This development branch connects the reference volume solver from
`6a184f1fd4115020a9062bd96804e3d8a43cef20` to the current skin-authoring worker,
FLTK and OpenXR. Commit `5e88fce20d832e43950b14f22519d3786278272c` is the tested
source, based on production `5440467a`. The volume field is computed on the host.
Production admission requires a solve on the selected GPU and explicit failure
when that solve cannot run. The host implementation serves as the numerical
reference.

The reference requires closed, consistently oriented rest surfaces with
internal deforming bones and a cell size that resolves the material. It handles
disconnected solids and reflected placements while preserving existing paint,
morphs and source geometry. Open VRoid parts still need surface binding or
weight transfer. The reference preserves complete-scene rollback when any
selected target fails and publishes an accepted batch in one Undo step.

## Validation on 2026-11-28

- The oneAPI Release application built against the DomeD FLTK/BDF installation.
  DomeD main was `acc2b70ae61dedec2a23727f67c60c0303d0f76f`. All 33 focused tests
  passed, including existing Surface Heat, anatomical authoring and VRM 1.0
  paths. The JUnit SHA-256 is
  `3abedffbb5fbeacf5f8700c6a317c3b3034dcde69cd62849dab1ff53f1b7cc61`.
- The continuous U-shaped reference gives maximum absolute weight errors of
  0.00788487 at 4 mm and 0.00684787 at 1 mm. The separate-solids case assigns
  zero influence across the air gap. The fixture passes 542 allocation failure
  boundaries with unchanged source state and a successful retry. The headless
  ASan/UBSan run uses the updated solver with the original numerical fixture and
  passes all 20 checks at 4 mm.
- Nested DomeD Wayland passes seven interface checks, including selecting the
  method and 4 mm cell size, Save, Undo and Redo. The compositor uses the Arc
  A770 with DMA-BUF import. The outer Xvfb presentation lacks DRI3.
- Synthetic OpenXR uses the Arc A770 renderer, submits 67 stereo frames and
  balances all 134 acquires, waits and releases. The native, Wayland and OpenXR
  saved results have the same SHA-256
  `0d8ae86e2d3e19da790e2f48d7a4e456f5303cfbc5498645f7ece8aecd2090d9`.

## GPU work

Keep exact topology and path checks as input preparation. Move per-bone
propagation to the selected backend with bounded device storage and bounded
dispatches, retaining the discrete upwind and edge rules. Compare the GPU
field against the host and continuous references at both tested resolutions,
then repeat the same archive, cancellation and history checks using traced GPU
execution. Positive AMD and NVIDIA execution and a physical-headset workflow
remain unverified.
