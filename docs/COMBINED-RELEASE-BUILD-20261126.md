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
Its runtime checks and the combined HDR, sculpt, and creator workflows are
pending. The per-texture-set Bake recipe changes are under separate validation.

[Build receipt and evidence](validation/combined-release-build-20261126.json)
