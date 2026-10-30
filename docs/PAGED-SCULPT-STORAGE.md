# Paged sculpt storage (private integration foundation)

This work replaces the Volume-only whole-map copy-on-write implementation,
including both growing per-gesture `before_` journals. It builds on the
authenticated dirty-region/local-surface and physical-band Volume4 foundation
at `2c160cf`. It does not move input mutation off the Document mutex, introduce
incremental GPU tiles, or eliminate complete CPU mesh materialization.

## Storage and ownership

Each immutable root contains 64 shared directory shards. Shards contain ordered
maps of occupied 16³ pages. A sparse page stores sorted exact coordinate/value
pairs; at 1,024 occupied samples it promotes to a 4,096-value dense page with a
presence bitmap. Erasure demotes at 256 samples. Negative coordinates use floor
division; far separated supports allocate only their occupied pages. Float SDF
and mask values and double displacement/journal values retain their exact bits.

A write clones a shared root, touched directory shards, and touched pages.
Untouched pages retain their strong immutable identity. Directory cloning is
proportional to the occupied pages in the touched shard, **not constant time**.
The root has a fixed 64-pointer copy. No full sample map is copied on a stamp.
An independent empty root per logical map permits allocation-free clear and
Cancel even while old snapshots own their original roots.

`Volume::GeometryOwner` now owns the immutable paged root; weak authenticated
history still compares control blocks. `SurfaceGeometry` retains only this SDF
root and voxel size/physical band, not masks or layer authoring state. The dirty
producer, 64-event ancestry bound, removal handling, undo/redo publication,
transactional faults, and unknown-lineage fallback are unchanged.

`voxels()` and `mask_values()` return immutable owning `VoxelView` values with
count/find/contains/at and input iteration. These operations never create a
directory or flatten the field. `materialize()` is the explicitly named legacy
unordered-map adapter. `Volume::Voxels` remains the explicit legacy import type.
Native iteration order changes; archive schemas and numeric semantics do not.

## Resource accounting

Every page vector, directory node, shard, root, and shared control block uses a
tracked allocator. The process-wide default 512 MiB storage budget includes
retained snapshots and weak-root control blocks. Allocation charges requested
bytes plus a conservative 64-byte allocation allowance. `SculptStorageStats`
reports cumulative allocations, charged live/peak bytes, copy counts and logical
copied payload bytes. Logical copied bytes are not measured memory-bus traffic.
Set the configurable budget while producers are idle. Existing aggregate
four-million-sample limits still apply independently to displacement and mask
channels. Ordinary brush preparation, commands, JSON, mesh assembly and renderer
buffers are outside this storage allocator and retain their existing limits.

## Validation and measurement

The fresh coherent CPU Release build passes all 74 native suites (120.80 s),
including local meshing, Scene/Volume compatibility, masks, resolution, painting,
retopo, skin, source-material/UV/morph and VRM/export coverage. Its new direct
store fixture passes 14 assertions, including 4,096 far sparse pages and 128
retained-root edits (maximum 5,320 logical copied bytes and one page per edit),
allocation-disabled iteration/sampling/clear, promotion/demotion and accounting
of weak ancestry. The new Volume fixture passes 20 assertions and all 12,920
allocation-failure boundaries across continued Stamp, End, Undo, Redo and layer
recomposition, plus quota/budget refusal, rooted branch/expired-history fallback,
protected no-op semantics and allocation-disabled zero pressure/strength/Cancel.
A separate fresh whole-core C/C++ ASan/UBSan/leak build passes all 74 suites
(634.65 s). Both C and C++ use `-O1 -g1 -DNDEBUG -fsanitize=address,undefined
-fno-omit-frame-pointer`, including xatlas and MikkTSpace; executables use the
sanitizer linker flags, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. The exhaustive paged Volume fault test passes
in 132.15 s and actual Document replay in 30.65 s. These are CPU UI/XR-disabled
proofs; actual FLTK/XR/Arc consumer validation belongs to the later combined
integration checkpoint.

`tests/paged-sculpt-replay.cpp` compiles unchanged against the frozen `2c160cf`
core and against this implementation (the latter enables storage statistics).
It compares 25 sorted numeric-state hashes: the initial field, six checkpoints
through a 192-stamp gesture, End/Undo/Redo, mask and Volume4 restart, twelve
retained snapshots and actual Document output. The first comparison is exact.
Sorting, state hashing, snapshots, serialization and gesture End are outside
the reported per-stamp time; the Document measurement includes public API
mutation and waiting for its immutable mesh publication. Allocation counters
count requested bytes, including worker allocations during Document operations.

Three i7-12700 / GCC 16.2.1 / `-O3 -DNDEBUG`, nice 15 CPU-only
trials (builds and other desktop workloads were not isolated):

| Scope | Frozen whole-map | Paged |
|---|---:|---:|
| 192 stamps, one growing gesture | 4,738.87–5,132.49 ms | 91.08–130.10 ms |
| Gesture allocation calls | 29,273,770 | 152,179 |
| Gesture requested allocation bytes | 1,842,342,712 | 47,686,456 |
| 8 full iterations plus sampler workload | 138.41–159.92 ms | 46.82–68.35 ms |
| Read-workload allocation calls | 0 | 0 |
| 24 actual Document edits through publication | 2,148.72–2,351.97 ms | 1,085.70–1,183.91 ms |

The paged gesture retained twelve snapshots: charged peak 8,598,840 bytes;
maximum single-stamp logical copies 287,272 bytes and 30 pages across all
channels. All 25 state hashes agree in all three old/new comparisons. These
measurements are not whole-goal completion evidence. Source and binary hashes,
exact build configuration, logs and checkpoint identity are recorded in the
parent artifact manifest `paged-sculpt-manifest-<checkpoint>.json`.

Remaining performance work includes the bounded background input queue,
incremental CPU assembly/GPU tiles, and a uniform-full-protection fast path.
The inherited fully-protected brush prepares support vectors but leaves
geometry/history unchanged. Zero pressure/strength and Cancel retain their
allocation-free guarantees; this paging milestone does not conflate them.
