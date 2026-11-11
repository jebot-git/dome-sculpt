# Combined GPU painting and sculpt arena milestone — 2026-11-11

Main admits qualified product source `27f01cad` through merge `f3eaf881`.
This combines native active-layer GPU color blur, shared EGL lifetime fixes,
desktop/XR commands and ordered GPU sculpt draw batching with the existing
authoring implementation. Public workflow fixtures are retained in `6e931301`.
The application remains native C++20 FLTK/OpenXR, with DomeD menus, bitmap
appearance and shared document history.

Independent admission checks find every file under `src`, `include`,
`third_party`, `assets` and `cmake` byte-identical to qualified `27f01cad`.
CMake differs only by five lines defining two qualified test targets. No
untested plain-preview ABI change is included. The final actual-app receipts
and 4,180 unique input/device pins were rehashed without drift.

The combined source passes Release and sanitizer software/Arc graphics gates,
including 1,753 graphics checks and 405 arena controls per profile. Final public
XR workflows pass 39 checks in each profile, and the Release desktop workflow
passes 10. Exact native outputs and Undo/Redo/reopen canvas comparisons pass.
See [PAINT-ARENA-QUALIFICATION-20261111.md](PAINT-ARENA-QUALIFICATION-20261111.md)
and [GPU-COLOR-BLUR-COMBINED-20261111.md](GPU-COLOR-BLUR-COMBINED-20261111.md).
These are incremental matched-source builds: four fresh complete translation
units, a rebuilt render archive and nine relinked consumers reuse the audited
331-output painting closure per profile. They are not whole fresh core builds.

## Original full sustained workload

The unchanged original P1 benchmark completes all three repetitions on Arc A770:
900 strokes, 360 masks, 6,000 stereo view pairs and 360 held pairs, at the original
single-object voxel 0.0125 and 768 × 768 dimensions. All 42 independent checkpoints,
original grouped regression, exact cancel/Undo/Redo/ordered-save/reopen and stale
generation controls pass. All 6,000 GPU timers are valid; unchanged view and held
rows issue zero buffer calls, allocations or payload/dynamic-array uploads.

| Observed measurement | Repetition 1 | Repetition 2 | Repetition 3 |
| --- | ---: | ---: | ---: |
| View pair wall median, ms | 2.650 | 2.641 | 2.645 |
| View pair wall p95, ms | 4.825 | 4.783 | 4.834 |
| Caller CPU median, ms | 0.515 | 0.515 | 0.521 |
| GPU elapsed median, ms | 2.149 | 2.152 | 2.152 |
| Held pair wall median, ms | 2.198 | 2.234 | 2.201 |

Total workload wall time is 142.08 seconds; external peak RSS is 2,164,125,696
bytes. No resource guard fired and no protected input changed. The earlier run
took 3,441 seconds with view-pair medians of 323–359 ms under severe host
contention. These observations are not a controlled attribution to batching:
host load differed and other renderer fixes are included. GPU elapsed includes
submission gaps; it is not GPU busy percentage. Offscreen uncapped stereo pairs
do not establish physical headset frame pacing or the multi-object/density matrix.

The sealed receipt is
[SUSTAINED-GPU-ARENA-20261111.json](validation/SUSTAINED-GPU-ARENA-20261111.json).
Its unchanged benchmark source and baseline options/initial inventory are
verified. The 13-member raw evidence archive is
`artifacts/sustained-gpu-arena-evidence-20261111.tar.gz`, SHA-256
`83130bca975b99a0462323bb223afd9a468a01b5508d88227184ab9b2f72bcfd`.
Earlier failures and original evidence remain preserved.

## Pause and remaining work

The full objective remains incomplete. Pause after this milestone as requested;
do not start further implementation, builds or tests until resumed. The separate
plain GPU preview candidate stays private and requires a coherent combined build
before admission. Full painting/material/baking, remaining source adapters,
interoperability and accelerator coverage, broader workloads and physical XR
acceptance remain required. The standalone OneAPI quality leaf has its own
[limited device proof](ONEAPI-QUALITY-20261111.md), not full application coverage.

Controller tool/function/selection trays and spatial node-based editing/history
remain required later phases after basic authoring targets. Adding these goals
does not resume the paused objective or declare their implementation complete.
No system installation or desktop/headset session restart occurred.

The compact admission record is
[PAINT-ARENA-MAIN-ADMISSION-20261111.json](validation/PAINT-ARENA-MAIN-ADMISSION-20261111.json).
