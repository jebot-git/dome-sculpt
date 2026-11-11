# Full original P1 correctness workload

The unchanged b8c469ce Release application providers complete the full original
single-object P1 workload on authenticated Intel Arc A770 graphics, using CPU
brushes/meshing. This is a correctness milestone, **not acceptable XR performance
or full production-matrix qualification**. Read the
[receipt](validation/SUSTAINED-P1-20261111.json) for exact source/binary hashes,
commands, timings, device witnesses, conditions and raw-file hashes.

All three repetitions execute300 strokes,120 mask edits,2000 two-eye view pairs
and120 held-object pairs:900/360/6000/360 measured operations in total, plus
three excluded warm strokes with exact Undo. The actual initial sphere has
258852 triangles,147932 vertices,632 chunks and588739 stored samples at.0125m
voxels. No density, workload counts, original resource/model/history assertions
or per-publication deadlines were reduced. External guards did not fire.

The original grouped-coordinate regression and42 independent whole-mesher,
scalar/query/frame checkpoints pass. Every repetition proves exact Cancel,
Undo/Redo, ordered Save/reopen and stale native-generation rejection. Unchanged
views and held motion retain their geometry/scalar upload invariants. All6000
GPU timer samples are valid, with complete bounded drain and no reset/disjoint
failure. The run closes cleanly after3441.16s, with externally observed peak RSS
2124296192 bytes and all protected source/providers unchanged.

Observed timings expose substantial remaining work:

| Repetition | Stroke wall p50/p95 ms | View-pair wall p50/p95 ms | View-pair caller CPU p50/p95 ms | Held-pair wall p50/p95 ms |
| --- | ---: | ---: | ---: | ---: |
| 0 | 378.55 /1586.69 | 322.77 /915.00 | 248.11 /392.05 | 187.75 /298.14 |
| 1 | 159.39 /189.57 | 351.52 /2311.94 | 234.63 /436.48 | 433.67 /1035.57 |
| 2 | 453.83 /983.58 | 359.48 /660.10 | 251.64 /364.60 | 298.08 /351.11 |

Other CPU builds/qualification were active; host load exceeded270 on20 logical
CPUs. The Arc test lease excludes cooperating test jobs, not the running desktop
or an otherwise exclusive system. One main-thread stack observation briefly
inspected the process; its timing record is retained. GL elapsed queries also
include the interval while the CPU feeds commands, not solely GPU busy time.
Inclusive stage times must not be added. These figures are observations under
those conditions, not controlled platform limits or headset pacing evidence.

The recorded stack was in repeated material uniforms within ordered sculpt
cell runs. Per-object material and disabled-attribute submission repairs now
retain exact reference images and bounds; see SCULPT-OBJECT-STATE-20261111.md
and STRICT-ARC-CONSTANT-20261111.md. They were **not** used in this baseline run.
The optimized full workload remains required, along with multi-object/density
production cases, further order-preserving GPU batching and real compositor/
headset pacing. Main milestone integration does not complete those requirements.
