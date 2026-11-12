# Resolved PBR and logical artist role repair — 2026-11-12

Source a8fa5424 is a scoped child of frozen5c. Its single product change is in
emission-bake-source.cpp: PBR image roles read their already transported current
coordinate carrier; typed MToon slots read their retained original logical
ordinal. The earlier code always preferred original logical UV1 when the
resolved carrier happened to be1. A separate genuine authored UV1 field can
legitimately coexist with a generated carrier containing original PBR UV0.

The same self-contained public prepare_source_paint/native Scene case fails on
both complete frozen5c and isolated artist cores: the callback returns
(1.8,0,.4), while the independent original-PBR-UV0 reference is (.2,0,3.6).
Real bake_object projects4096 texels;1024 independently checked interior texels
have maximum radiance error3.875. Native Save/history preserves the wrong result,
so this is an actual production sampling defect, not reader refusal or a test
contrast issue. All old sources and raw failures remain intact.

The repaired one-TU component passes19 controls in Release and strict
ASan/UBSan with leak detection, halt-on-error and no suppressions. Every profile
compiles the complete changed owner and current fixture, authenticates all193
core members, keeps192 byte exact, and freshly links the same old/fixed tests.
Old native exits1; fixed exits0. Input/source drift is zero. Maximum error is
4.47e-8 for the PBR callback,8.94e-8 for the distinct legal typed logical-UV1
control, and2.38419e-7 over1024 actual Bake texels. Correct typed factors
(.5,.25,1) times strength4 retain the independent HDR reference.

Six prospective quota refusals cover source coordinate bytes, working bytes,
transport work, Bake texels, rays and raster work. Immutable callback Cancel and
four actual early/middle/final Bake cancellation boundaries from4257 observed
polls preserve exact source/target state. The real Document worker immediately
queues ordered Save, reopens exactly, and restores exact Undo/Redo history.
Fresh historical whole-workflow links against each repaired core also pass
UV1worker180373, source27, originalworker16417 and publicproducer32794 controls.
All runs are CPU-only. No graphics/application/Arc/HMD/performance acceptance is
claimed, and frozen5c/public admission remains held for the integration owner.

All existing public/private headers, packet/shader layouts and Scene42/Paint8
wires are unchanged. CMake adds the native regression under BUILD_TESTING with
a180-second timeout. The new selected native cohort retains old85 and adds this
case as86. The independent new artist-normal resolver requires the same role
distinction on its own future branch; that artist product is not admitted by
this repair.

Evidence is under artifacts/source-artist-normal-proof-20261112:
role-domain-repair-release-v21; role-domain-repair-sanitize-v22/compile-status;
role-domain-repair-sanitize-v25; role-domain-historical-release-v24; and
role-domain-historical-sanitize-v26. Full pinned summary
role-domain-repair-handoff-a8fa.json has SHA256 `237d3244f0fe330a235307b3c1ffd1ceba6ad8defa18ec9c0352be8041c6466b`.
The sanitizer core derives from the independent immutable193-member5c snapshot;
its supporting fresh archives must have actual completed0 events before use.
The first v22 finish correctly refused a renderer archive still being built;
v25 qualifies only after that archive completed. No provider was modified.
Fixture compile/domain failures and the v23 wrong expected log literal remain
failed in their original namespaces; no source, bound or guard was weakened.
