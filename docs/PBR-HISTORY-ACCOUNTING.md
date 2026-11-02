# PBR history accounting

Undo of a prepared PBR paint stroke on the shared synchronous desktop/XR stroke
history path restored its sparse tile owners but left the128MiB paint cap's
colour-tile counter stale. The history-target identity swap omitted
`before_colors_`, `allocated_color_tiles_` and `before_allocated_color_tiles_`.
All three now participate in the existing noexcept swap. No public API, layout,
codec, source-material interpretation or rendering changes.

The actual pre-fix Document test allocates four sparse PBR tiles, then Undo
restores the exact original encoded Scene and zero visible tiles, while the real
cap counter stays at four. The final fixture fails at that mismatch. Merely
checking pixels, encoded bytes or public storage_bytes cannot detect it, because
storage_bytes independently enumerates actual tile pointers. The test reads real
bookkeeping through test-only standard C++ explicit-instantiation member access;
it adds no production hook and never writes private counters.

The fixed test passes78 checks: actual shared Document stroke history, four
Undo/Redo/cancel cycles after genuine mutation, exact budget/storage invariants,
actual queued-native painting nonregression, Save/reopen, and the production
noexcept swap while two real active edits have different scratch colour owners
and before-budget states. The asynchronous desktop worker uses whole-Scene
history and did not itself exhibit the inherited target-swap defect.

Fresh regular-user Release2/2 passed0.30s. Fresh wholly owned C/C++O1
ASan/UBSan/leaks2/2 passed2.42s. Both scopes include the existing native-strokes
suite and the new PBR accounting fixture. Product source is78736d6 on base
main7a4443e. Exact sources/binaries/archives and retained negative logs are in
validation/PBR-HISTORY-ACCOUNTING.json. An initial test-only attempt to wait for
native_samples was invalid for a fast sample: that prefix is published only
under the30ms preview throttle. It was replaced by deterministic actual shared
stroke mutation/cancel checks, with the original diagnostic retained.

This is a narrow history bookkeeping fix. No GUI, GPU, physical headset,
installation or full Dome Sculpt completion is claimed.
