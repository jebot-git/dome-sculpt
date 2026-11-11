# Shared COW sculpt geometry: independent native qualification

Source `b6e366fd` passes the actual affected native controls in Release and strict
SAN. The source review found no need to change product code or raise a limit.
This is a focused core composition and native qualification, not a whole fresh
application build or a production four-object benchmark result.

| Fresh consumer | Controls | Release seconds | SAN seconds |
|---|---:|---:|---:|
| Sculpt chunk pipeline |78|0.52|3.92|
| Local surface |77|0.64|6.17|
| Sculpt surface chunks |22|0.11|0.70|
| Small chunk workspace |27|0.24|2.38|
| Small placed workspace |373|0.31|3.43|

The new pipeline controls distinguish actual shared COW copies from independently
allocated equal-valued fields. Four duplicates share the complete local geometry
owner while retaining separate placement/query owners. Exact scene budget admits;
one byte less refuses. A real mask edit changes only active scalar authority, a
real geometry edit detaches only its object, and restoring the genuine source
reuses an unaffected prior owner. Full whole-mesher corners/normals, including
reflected/sheared placement, and cancellation preserve their independent oracles.
The existing native Document Cancel/Apply/Save/Undo/Redo and stale-generation
workspace controls also run on this changed core.

The review follows the exact pointer and shared control block plus bit-identical
voxel size/band check in `src/sculpt-scene-chunks.cpp:106`. Numerical equality
receives no sharing credit. The shared owner is already charged in the current
scene; its complete reservation is deducted only from the nested operation's
external allocation envelope. Ordinary caller geometry/extraction/working limits
are retained. `SculptSurfaceChunks::update` checks reduced requested limits
before returning an unchanged owner (`src/sculpt-surface-chunks.cpp:586`).
Each unique complete geometry is charged once; per-object placed/query and mask
ownership remains separate. No budget/count/deadline was increased.

Both profiles use the current `f47348eb` private owning providers, whose product
source is `1452ddec`, with the new TextureMaps ABI. The sealed original core and
native-graphics archive hashes match. All 192 actual core archive members are
verified byte-for-byte against their owning object files, and all unchanged
source/public/private header dependencies match the frozen candidate. Across
src/include/third_party, 710 files match; only the renderer/arena three files and
the patched sculpt-scene-chunks TU differ. The renderer-only differences are not
core dependencies. Among all 192 declared core TUs only sculpt-scene-chunks
changes. No 775/27f core or old pre-ABI native executable is used as pass evidence.

The qualification compiles that entire changed TU and six entire test TUs, builds
an isolated thin core archive with the new object and 191 exact existing objects,
and freshly links all five consumers. The original current-ABI archive and all
providers remain unchanged. Persistent before/after manifests pin 3,712 inputs
and 13 fresh outputs per profile; every drift list is empty. These are not 192
fresh core compiles. The root's coherent renderer/application and benchmark
composition remains separate.

SAN uses ASan leak detection1 and strict UBSan without suppressions. Every run
exits0 without a sanitizer report. The maximum observed instrumented RSS across
the SAN compile/link/run steps is 2,971,422,720 bytes, inside the 4 GiB external
envelope. Native internal limits remain unchanged; external native tests use
180 seconds and the original small manual workspaces 300 seconds. Actual mapped
consumer ELF identity is recorded, and no DRI FD target appears. No GPU context
or lease is used. All compiler/native processes have terminated.

Release and SAN pipeline input and accepted native Save files are byte-exact
across profiles. The accepted SHA-256 is
`627cf8b51a7167c1209d69bb2b94c039f557625f33beb178831738798352cd0c`.
This supplements the independently checked native history endpoints.

The original 27f four-object production run remains a failure: 6.01 seconds,
582,770,688-byte peak RSS, `Sculpt manifest retained byte limit exceeded`, no
external guard or protected-input drift, and zero completed repetitions.
Its exact status/result/log are preserved; this focused native proof does not
rename it as a pass. The root must run the unchanged full four-object production
workload on the coherent new composition before qualifying that row.

The companion JSON pins the source review, raw statuses, exact archive/providers,
native logs/maps, original failure and native R/S comparison. All evidence is in
`artifacts/sculpt-shared-qualification-20261111`. Root source and main are untouched;
this evidence branch adds documentation only.
