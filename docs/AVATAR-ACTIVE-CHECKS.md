# Avatar checks in optimized builds

Both existing Release and sanitizer avatar consumers compile with `-DNDEBUG`.
The old `assert` predicates therefore do not run, including the checks that a
negative fixture actually failed. A concrete `assert(false)` program built with
the same Release flags exits0. Historical green avatar invocations do not prove
those omitted predicates and are not retroactively qualified here.

The test now uses an explicit counted check with source-location diagnostics.
All original conditions, tolerances and fixture data remain unchanged. No
production source, header, compiler flag, library or application default changes.
Each profile passes92 executed checks, including landmarks/fit, mirror identities,
humanoid metadata, compaction and preserved joint transforms, and vowel/morph
identity validation. Both profiles reject a deliberately wrong expected role
count. A registered `--verify-failure`/CTest `WILL_FAIL` control also proves that
the checks can fail in each compiled consumer.

Fresh complete test TUs link with immutable matching main/C179 support. The
sanitizer consumers retain ASan/UBSan/leak instrumentation; DWARF is compressed
with `-gz=zstd` and `--compress-debug-sections=zstd`, not stripped. All protected
input hashes remain exact and no sanitizer diagnostic is emitted. The final
wrapper64299 exits0. Earlier pre-control consumers44890 remain separate evidence.

Fresh CMake configuration registers the positive and rejection tests. Both
registered tests pass/profile using symlinks to the exact manually compiled
consumers. UI/XR are explicitly disabled for this kernel-only configuration;
their application build defaults stay enabled. This is not a fresh core build,
broader avatar/UI qualification or full-goal completion claim.

Commands, hashes, negative controls and scope are in
[the receipt](validation/AVATAR-ACTIVE-CHECKS.json).
