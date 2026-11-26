# Combined Release build — 2026-11-26

`5604ad54` combines the current authoring candidate, independent sculpt storage
and recovery, retained-source HDR worker, and corrected creator test fixtures.
The fresh optimized build completed all 290 actions in 277.556 seconds using
the canonical installed DomeD FLTK and bitmap runtime. It produced the
application and 13 fixture executables and libraries.

The build review records 259 complete compiler invocations, including all 197
core objects, and 1,679 actual dependency paths. Each of the 14 static archives
was checked against its compiled object payloads. The archive includes the
compiler commands, original dependency files, configuration and build log.
System dependency hashes describe the files observed after compilation.

The application SHA-256 is
`002685db461076056e4cfcceb8817201e45afd1fefb079d11578bbf58b2aa9c5`.
The application reports `Dome Sculpt 0.1.0 / CPU / Blender 5.4 Alpha trackball basis`.
Public compositor and XR runtime for this combined executable are pending.

[Build receipt and evidence](validation/combined-release-build-20261126.json)

The combined executable set passes 659 CPU-only workflow checks across the
creator workspace (44), shaped VRM export (448) and independent sculpt workflow
(167). The sculpt workflow edits four full-size objects with independent layers
and masks, then checks history, ordered Save, cold Open and durable recovery.

The first sculpt run reached the 4 GiB process limit with the default allocator.
Repeating the same executable and dataset with `MALLOC_ARENA_MAX=2` and
`MALLOC_MMAP_THRESHOLD_=131072`, as in the earlier standalone milestone,
passes in 55.86 seconds with a sampled peak of 3,555,766,272 bytes. The memory
limit and deadline were kept. The tests ran alongside separately bounded CPU
jobs. The first wrapper missed the sculpt and VRM aggregate count formats.
The independent review records the original process results, final reported
counts, executable identities and absent owned processes. The original wrapper
results and default allocator refusal are in the evidence archive.

HDR 2048 and the per-texture-set Bake recipe workflow are under separate
validation before the next combined build.

[Native workflow receipt and evidence](validation/combined-native-workflows-20261126.json)
