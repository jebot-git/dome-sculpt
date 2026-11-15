# Missing logical typed UV1 guard and pause checkpoint — 2026-11-12

Source e10b2456c927ff72d2799fec54df85b65df85167 contains product41ac6f7b
plus a test-only sampler correction. One product TU changes:
emission-bake-source.cpp, SHA256
389a4d1568636f9789d6dab7d2877f513ca69a8353fd9f0a06b4cefc9d1500ca.
A shared private check runs before both raw-source capture and the HDR selector.
A genuinely prepared source requesting typed emission UV1 must retain its actual
original UV1 field, through the original source map or the genuine separately
retained authored field. Its generated PBR carrier1 cannot substitute for that
logical role. A private point guard also refuses that substitution. Genuine
unprepared current UV1 keeps its existing fallback. An explicit physical
background still skips an image it never samples.

Release and strict ASan/UBSan with leak detection and halt-on-error pass62 new
controls plus the unchanged19 source-role regression. Each profile freshly
compiles the complete changed owner and current native fixture, authenticates
all193 core members, retains192 exact, and freshly links old/fixed/regression
executables. Old missing-role tests exit1; fixed62 and retained19 exit0. All
input/source/provider drift is zero. The prior failures remain intact.

Both actual LDR and HDR old paths accepted the missing-role Scene/native wire,
silently projected4096 texels, completed dependent Save and changed path/history.
The HDR callback incorrectly read (.2,0,3.6) from the generated PBR carrier;
the LDR callback returned its legacy empty selector, leaving raw material Bake
unprotected. Both now explicitly refuse emitter/raw/Bake/dependentSave requests.
Exact Scene data, source/target state, prior file bytes, project path and history
remain unchanged. Cancellation is atomic. Valid LDR/HDR prepared logical UV1
and unprepared genuine current UV1 still each project4096 texels through real
Document workers, ordered Save/reopen and exact Undo/Redo. The physical-background
control also passes. No coordinate is fabricated, and no guard is relaxed.

Existing public/private headers, shaders, packets, Scene42/Paint8 wires and native
capabilities are unchanged. Editable native Scene roundtrip is deliberately
unchanged; missing coordinates refuse when requested. CMake registers the new
native case under BUILD_TESTING with timeout180, retaining the prior86 and adding
case87 for a later owning cohort. This component is not application or graphical
admission. Frozenad99, all failed public runs and all providers remain untouched.

Receipts are artifacts/source-artist-normal-proof-20261112/
missing-repair-{release,sanitize}-v33/status.json. Full pinned summary
missing-logical-uv1-handoff-e10.json has SHA256 `bb071ac49790e5d387647fa02ebfbbb19e5f50319c2160e8107176e6347c21b5`.
Release core SHA256 is
672e719ca712a9e2bf909e91afff63a39bdc7f6ad93063c6408f0b700b7b43c7;
strict sanitizer core SHA256 is
4343411302cb88aaf35402a07bcb4f2f3f0a9c27a2738592d40bf26c9906e09f.
The v32 fixture correctly failed for an invalid unresolved typed sampler view;
e10 keeps sampler0 while the MToon slot owns the genuine coordinate1. That
fixture failure is preserved separately from the production counterfactual.

The user requested pause after this round. StrictS85702 and push73316 are
terminal0; an actual /proc audit found no owned live build/runtime process and
no graphics lease is held. No additional historical, graphics, feature or cohort
job starts. The parent may combine the qualified guard with f9 metadata/source
without a new full build before pausing the full, incomplete goal.

Future artist source853 is preserved with CPU9+139+856+32 passing R/S; metadata
7820e2ce records it. Its one corrected software-GL run reached four exact RGB
comparisons but failed negative sensitivity (gap4 instead of7), so it has no
graphical qualification. Strong valid fixture2f89410c has only Release compile
and CPU contrast forecasting; all six modes predict comfortable gaps and its
next authorized run will retain full correct/omitted/wrong images and RGB.
It has never executed GL. Native32/prepared/canonical/current-baked normal and
owning-source-pose support, independent target normal decode and artist/imported/
CC0 graphical consumer proofs remain future work. All sources and failures stay
frozen; none of these future branches is admitted by this checkpoint.
