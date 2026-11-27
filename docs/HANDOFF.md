# Dome Sculpt current handoff — 2026-11-27

The full production goal is ACTIVE after the user's Nov26 resume and remains
incomplete. The Nov18 [assessment](PROGRESS-ASSESSMENT-20261118.md) preserves
the prior round's results and remaining goals. Earlier pause/resume records
below retain their dated scope.
Read [REQUIREMENTS.md](REQUIREMENTS.md) and the original
[HANDOFF-466ebd1.md](HANDOFF-466ebd1.md) for the complete scope. This concise file
replaces the accumulated checkpoint diary; all previous text is preserved,
byte-for-byte, in [the historical handoff](HANDOFF-HISTORY-20261117.md).
Update this current summary at milestones rather than prepending more history.

## Nov26 resume: rebuild from committed sources

No Dome Sculpt build jobs were running at resume. The Nov18 temporary SDK,
source worktrees, executables and prepared runtime plans are absent; their Git
commits and committed evidence remain. Do not claim old executable reuse or
rewrite historical manifests to make missing paths appear current.

Canonical DomeD local and remote main remain `acc2b70ae61dedec2a23727f67c60c0303d0f76f`.
The installed Release toolkit `/opt/fltk-vr/toolkit` passes44 static checks of
canonical builder/27 patches, archive, generated poll configuration, library
digests and BDF helper. Receipt: `artifacts/sdk-20261126/installed-release-check.json`.
This permits fresh configuration; actual new loader/render/UI checks are separate.
The explicit bitmap prefix remains `/opt/fltk-vr/bdf-runtime`.

Fresh builds now pass for creator's current05 application, HDR provenance514,
sculpt independent6d3 and root's headless strict05 core/render/13 fixtures. The
strict parent audit verifies229 real whole compiler owners/197 core owners,
1224 dependency paths and actual archive payloads/13 sanitizer-linked consumers.
An additional sampler fixture TU/link is independently audited against unchanged
parent archives. No runtime acceptance follows from these builds. A supplemental
compiler-tool observation is not claimed prospectively for completed compiles.
The canonical full sanitizer FLTK SDK is also freshly rebuilt and checked;
read [the toolkit record](CURRENT-TOOLKIT-20261126.md).

Creator's fresh clock regression passes6014 checks. The current05 application's
actual Arc XR accept/cancel/focus workflows and ordinary fresh-App reopen now
pass58 checks. The [creator XR receipt](CREATOR-CURRENT-PUBLIC-XR-20261126.md)
records the six application processes and the matched sanitizer build. The strict
accept and desktop workflows passed 19 and 14 checks respectively. The accept
wrapper miscounted exit-time descendants after the actual checks passed. Both
strict cancel and focus-loss/regain passed their complete wrappers, with 19 and
20 checks. Read [public creator validation](CREATOR-CURRENT-PUBLIC-STRICT-20261126.md)
and [cancellation and focus validation](CREATOR-CURRENT-PUBLIC-STRICT-ROUTING-20261126.md).
The prior-authored stress fixture
moves the head joint5m, so its cropped screenshot does not establish ordinary
default-base presentation quality. A separate ordinary creation used the real
initial XR canvas placement and found that 80.85% of evaluated vertices fit
the synthetic vertical view. A normal rendered reopen confirmed cropping at
the head, hands and feet while preserving all native bytes. Keep that case
separate from the authored stress fixture when testing an initial view fit.
Read [the rendered observation](DEFAULT-CREATOR-RENDERED9E4-20261126.md)
and [the geometry observation](DEFAULT-CREATOR-FRAMING-GEOMETRY-20261126.md).

The HDR2048 path first reached a 128 MiB paint-channel allocation refusal
after the provenance fix. The isolated worker avoids the discarded 112 MiB
field with a certified retained source domain. The later combined qualification
is recorded below. Sculpt's new workflow first exposed
an invalid mask-brush fixture, then an inappropriate initial mesh wait after
file-only recovery catalogue refresh. Correct those fixtures without weakening
product guards. The corrected full-size native sculpt workflow now passes167
Release checks in49.304s, including cold reopening and durable recovery, with
peak owned RSS3847172096 below4GiB. Root independently rehashes all518 archived
and current authorities; read [the scoped proof](SCULPT-INDEPENDENT-NATIVE-20261126.md).
The production-size strict sculpt run exceeded 4 GiB after completing recovery
and fitting the exact cold-capture quota. Original sustained independent Arc
qualification and the new fixture lifetime comparison remain open.

Inspect live units before taking action. Graphics, display and loader validation
jobs take the common nonblocking lease, with no idle reservation. Confirmed
CPU-only workflow checks can run concurrently in memory-bounded units. Record
comparative timings in their own controlled runs. The timing fixture `91a90a67`
uses the same strict05 core, data, checks and resource limits.
The strict native/software cohort passed 88,724 checks across the first 11 cases.
Its first graphics case stopped with `Frame range is invalid` because the test
fixture left `SurfaceRange::first` uninitialized. The corrected fixture
`2e9f5873` rebuilds and links against the same strict core and renderer. All
three graphics cases then pass 176,543 checks, with exact retained HDR
coefficients and maximum RGB error 1. The HDR test exited successfully while
the original wrapper missed its aggregate count. Its captured logs support a
read-only correction. Both subsequent graphics tests passed in the next run.
Together the 14 selected cases pass 265,267 checks. The
[native receipt](validation/strict-native-runtime-11-20261126.json) and
[graphics receipt](validation/strict-graphics-tail-20261126.json) retain
the original invocations, results, terminal logs and live sanitizer witnesses.
The full-resolution advanced-normal baseline and experimental texel-reuse
variant each reached the 900-second deadline after seven of 17 families.
The [deadline receipt](validation/advanced-normal-full-deadline-20261126.json)
includes phase timings. The expensive intervals are the full Bake and ordered
Save, with runs overlapping other bounded CPU jobs. The texel-reuse change
remains experimental. The same 17 families and all original common checks
passed in six independently monitored groups on the unchanged strict core.
Group source `60b403e0` retains the ordinary full-workflow entry point. The
groups passed 645 native checks, and the runner separately verified complete
family and refusal coverage. Each group used the original 900-second and
3 GiB limits, including its fresh-process reopen. Read the
[six-group receipt](validation/advanced-normal-strict-groups-20261126.json).
The HDR877 isolated 2048 worker passes its 22 checks. Its complete 2048 joint
Bake reached a further `Baked maps exceed paint storage budget` refusal.
Component `725ea100` shares identical immutable intrinsic normal images and
retains recipes in the texture-set archive. Its Release cohort passes 111,306
checks, including the full 2048 and 512 inputs, export and historical replay.
Read [HDR source closure](HDR-SOURCE-CLOSURE-RELEASE-20261126.md) for the
component, source-projection fixture and independent exporter evidence.
The [strict HDR receipt](HDR-SOURCE-CLOSURE-STRICT-20261126.md) preserves
63,897 passing checks and the original full 2048 deadline. The full strict
case completed its first native Bake and Save, then exceeded 900 seconds
during the dense numerical oracle. The remaining original endpoint checks
need separate verification.
The new combined candidate `9e4f79f6` builds all 294 selected actions with
261 complete translation units, 14 verified archives and 16 outputs. The
eight native HDR and recipe tests passed in 229.77 seconds, including full
2048, export and historical replay. Read the
[combined native and loader receipt](validation/combined-native-and-open-20261126.json).
Whole Release build `c26a8573` includes the corrected projection fixture and
uses identical product source. It passed all 1,802 actions in 1,818.85
seconds, including both separately requested sculpt fixtures. Its full
1,349-test CTest suite finished in a private software-only environment.
It reports 1,284 passes, 52 failures and 13 cases that could not run because of
failed fixture dependencies. The raw JUnit and terminal status are under
`artifacts/production-full-regression-release-20261126/full-private-ctest/`.
Later focused fixes retain the original broad results in that directory.
The exact `9e4f79f6` combined candidate passes all 14 native
Wayland assertions and clean private teardown on Arc. The original wrapper
refused a deleted non-executable Mesa
shared mapping belonging to the compositor because private cleanup removed
its cache before stopping the compositor. The corrected private scenario kept
the observer and all checks. Read
[the clean Wayland report](COMBINED9E4-NATIVE-WAYLAND-CLEAN-20261126.md)
and [the original cleanup failure](COMBINED9E4-NATIVE-WAYLAND-FUNCTIONAL-20261126.md).
The isolated integration candidate
`5604ad54` combines current05, HDR877, sculpt6d/fe94 and corrected creator
fixtures550. The [fresh Release build](COMBINED-RELEASE-BUILD-20261126.md) for
the application and 13 selected fixtures passed all 290 actions in
`artifacts/production-combined-release-20261126`. The terminal unit was
`ds-production-combined-release-20261126.service`, invocation
`593996bfc65d47d0a641ffcb5e4769ef`. The combined application version check and
659 native sculpt and creator checks pass. Sculpt passes with the matched
allocator environment under its original 4 GiB limit. The build record links
the first memory refusal and the runtime observer correction. The combined
renderer passes 4,422 retained HDR checks each on software and Intel Arc with
the corrected fixture. This exact App also passed all
[14 nested DomeD native Wayland checks](COMBINED5604-NATIVE-WAYLAND-20261126.md)
on Intel Arc. The branch is
`work/production-resume-integration-20261126`.
The [streamed native Save experiment](validation/stream-native-save-experiment-20261126.json)
preserves atomic replacement, recovery and the exact 50,061,799-byte project.
The default-allocator sculpt workflow still exceeds 4 GiB. The matched
allocator run passes all 167 checks. The save-only probe measures similar
memory peaks for the original and streamed writers on this project, so the
change remains on its experimental branch.

Bounded native Open `9368689d` reads CBOR in 64 KiB chunks directly from
the project file and checks cancellation and the existing size cap on refill.
Editable GLB package loading retains random access. File and recovery tests
pass 20 and 21 checks against the combined Release core and an independently
linked strict component with equivalent loader bytes. The first build plan
used a colliding output basename. The successful build uses distinct core and
test output directories. Earlier count observers reported failures after
successful test exits. The corrected strict driver verifies every check.
Isolated Open lowers peak RSS from 1,268,932 to 1,220,144 KiB on the sculpt
project and from 1,041,932 to 921,436 KiB on the HDR project. Normal Save
reproduces both input files byte for byte. The same combined full-size sculpt
workflow still exceeds 4 GiB with the default allocator. The Open change is
on `work/stream-native-open-20261126`. Main runtime admission is unchanged.

Save candidate `07025cd3` releases each live object's volume JSON tree after
writing its CBOR fields. It retains the existing file format and atomic writer.
Isolated sculpt Save peak RSS fell from 786,304 to 347,400 KiB. The 2048 HDR
project fell from 1,071,712 to 1,034,300 KiB. Both saves match every source byte.
The fresh Release build passed all 234 actions. The candidate passes 23 scene
file, 21 recovery, 22 atomic allocation and 16,400 HDR export checks. On the
default allocator the original four-object sculpt suite now passes all 167
checks in 94.41 seconds at 3,553,742,848 bytes peak owned RSS, within the original
4 GiB limit. The earlier default-allocator failure with bounded Open alone
remains in the original receipt. Read
[the one-volume Save validation](validation/native-save-one-volume-20261126.json).
The full-App build completes all 40 additional actions. The exact App passes
[14 native Wayland checks](COMBINED07025-NATIVE-WAYLAND-20261126.md) and
[58 public creator synthetic OpenXR checks](COMBINED07025-PUBLIC-XR-20261126.md),
including three fresh ordinary reopens, with all complete wrappers exiting
successfully. The 12 native HDR and creator CTests also pass, including the
complete 2048 Bake and historical replay. The native file, recovery and
atomic-write sanitizer cases pass. The full strict sculpt workflow runs
separately. The source is pushed on `work/serialize-sculpt-objects-20261126`.

The corrected projection fixture `2472718c` passes 11 checks against the
same Save-qualified product core. The original fixture refuses an input
that omitted its authored normal frame. The broad regression exposed a
separate assertion in the layered-normal case that expected a genuine
prepared source to be refused. Fixture `c0a67586` verifies Bake success,
exact source preservation and native round trip, plus refusal when the
field's owner is removed. The corrected fixture and its seven dependent
cases pass. Product sources are unchanged in these fixture commits. Read
[the build and fixture receipt](validation/production-bounded-project-integration-20261126.json).

The original independent Arc sculpt run reached 300 strokes, 120 masks and
2,000 stereo pairs in its first repetition, including 13 independent exact
checkpoints. It then failed while charging new representative index paths
against its internal working quota. The first repetition did not finish all
post-edit checks. The original cap stays in force while live persistent index
and scratch ownership are investigated. Candidate `8c3b9aea` credits only
actual retirement of newly allocated persistent index nodes and retains the
original source and live scratch charges. It passes the 26 surface and 92
pipeline checks in Release and strict builds, including exact and one-byte
tight working limits. Full native Release passes 167 checks on the sculpt
component. The original Arc workload and strict sculpt are running on that
component. Integrated candidate `ec578e56` combines it with the Save-qualified
App and both fixture repairs. The integrated Release build completed seven
whole translation units and five links. Its core archive retains 194 exact
prior payloads and replaces the three translation units that include the
changed index header. The integrated Release native cohort passes all 285
checks with the default allocator. The full sculpt case peaks at 3,554,258,944
owned RSS bytes and produces the same native Save and two recovery files.
Read [the integrated native receipt](validation/production-sculpt-index-integration-20261126.json).
The component's [original sustained Arc workflow](SCULPT-INDEPENDENT-ARC-20261126.md)
now passes all three repetitions, 900 strokes, 360 masks, 6,000 stereo pairs
and 360 held pairs in 812.33 seconds. It completes three history and
stale-generation sequences and 42 independent checkpoints under the original
5 GiB cap. The `ec578e56` integrated App separately passes 14 native Wayland
checks and 58 public creator synthetic OpenXR checks on Arc. Its own full
sustained Arc run also completes all three repetitions in 1,036.979 seconds
within the original 5 GiB limit, with 42 independent checkpoints and three
exact history sequences. Both runs use the original matched allocator
controls. Read [the integrated Arc receipt](SCULPT-INTEGRATED-EC-P1-20261126.md).
The earlier `07025cd3` full
sanitizer sculpt case exceeded 4 GiB after writing the Save and two recovery
files. The exact integrated build passes five short sanitizer regressions.
Its full sculpt case reached the successful exact-quota check at 164, then
stopped at 4,295,278,592 owned RSS bytes in 791.01 seconds under the same
4 GiB guard.
Read [the integrated UI and source-regression receipt](validation/production-index-ui-and-source-regressions-20261126.json).

Source candidate `5735ab7f` moves persisted alternate-coordinate paint
validation ahead of baked-plane allocation. The unowned UV1 substitution
refuses at the decoder's ownership check, and the legitimate current-corner
UV Bake still succeeds. The original UV1 emission regression now exercises
its legacy raster policy explicitly, preserving the individual texel,
source, history and export checks. It separately verifies that default
continuous transport refuses the nearest-sampled discontinuity without
changing the Document. That source still needs multi-domain continuous
emission transport for an exact default Bake. The six current Release
regressions and original broad-suite failures are in the UI and source
receipt above.

Candidate `50f7abfa` reuses retained geometry charges only for an identical
immutable owner prefix in the same generation. Current limits, scalar owners
and placement owners still have their own checks. On the exact four-object
native project, the median warm mask capture fell from 170.57 ms to 1.33 ms
and the p95 fell from 217.56 ms to 1.66 ms across 32 samples per build.
The probes alternated build order and used the default allocator. The
candidate passes 94 pipeline checks, including exact-quota and one-byte-short
captures, and all 167 native sculpt checks with byte-identical Save and
recovery. The matching frontend, renderer and sustained fixture builds
pass. The App version check also passes. Read
[the warm-capture validation](validation/sculpt-scene-prefix-accounting-20261126.json).
Full scope in REQUIREMENTS is unchanged.

Candidate `0cdd1511` combines the warm-capture change with compact sculpt
index allocation. The Release build replaces one complete core translation
unit and links the app and four matching fixtures. Its app version check and
all 287 selected native checks pass with the default allocator. The complete
167-check sculpt workflow peaks at 3,414,052,864 owned RSS bytes within the
original 4 GiB cap. The saved project and both recovery files have the exact
expected SHA-256. Read
[the combined Release receipt](validation/production-compact-sculpt-release-20261126.json).
The same App passes the 14-check native Wayland scenario and all 58 synthetic
OpenXR creator checks on Arc. Seven matched ASan/UBSan native cases pass.
The full strict sculpt case wrote the expected project and two recovery files,
then reached the original 900-second deadline during the cold-capture quota
checks. Peak owned RSS was 4,078,325,760 bytes. Read
[the combined graphics and strict receipt](validation/production-compact-sculpt-ui-strict-20261126.json).

Candidate `f9c20398` narrows the immutable search roots during retained-byte
accounting for changed sculpt geometry. Every subtree credit still requires a
shared owner. Temporary search roots are charged against the existing working
limit. On the same four-object project, 32 changing captures per build reduced
the median from 375.93 ms to 315.97 ms and p95 from 428.53 ms to 355.34 ms.
Retained bytes matched at every revision. All four profile outputs had an
identical project hash. The 94 pipeline checks and the full 167-check native
workflow passed, with exact Save and recovery bytes. The matching optimized
frontend and renderer compiled and the app version check passed. Read
[the changing-capture receipt](validation/sculpt-sharing-range-search-20261126.json).
The benchmark measures native CPU snapshot capture. Seven focused ASan/UBSan
cases pass on this source. The full case reaches the original 900-second
deadline during cold-capture quota checks at 4,075,298,816 bytes of owned RSS.
The separate sustained Arc run passes all three repetitions in 843.133
seconds, with 900 strokes, 360 mask edits, 6,000 stereo pairs, 360 held pairs,
42 independent checkpoints and three exact history sequences. The fixed
input and all three native project outputs match the earlier integrated run.
Peak owned RSS was 3,949,670,400 bytes within the original 5 GiB limit. This
run used the original allocator controls. Read
[the Arc and fixture receipt](validation/sculpt-range-arc-and-combined-fixtures-20261126.json).

The broad regression found that command availability could throw when a
sculpt volume retained a dormant painting mesh. Candidate `c5fbb0bd` checks
the active mode before resolving the paint field for color blur. The
original source-mode adoption regression and all 73 sculpt Undo allocation
boundaries pass on the matching Release build. The software color-blur
controls also pass, including the active-paint and backend-refusal cases.
The same three cases pass on the matched ASan/UBSan core. Main carries the
scoped fix as `6a63f6fe`. Read
[the Release receipt](validation/dormant-paint-command-release-20261126.json)
and [the sanitizer receipt](validation/dormant-paint-command-strict-20261126.json).
The exact main source also completes a fresh 266-action Release build using
the canonical FLTK and BDF prefixes. The application version check and all
three focused native tests pass. Read
[the exact main and projection receipt](validation/dormant-main-and-sculpt-projection-20261126.json).

Candidate `2aff79f7` resolves a render-only current-grid paint field from its
owning scene on the plane-cut worker. It restores mapped baking of the plain
painted source. All 23 original desktop/controller cancel-and-retry checks
pass. The colored cut cohorts pass all 174 Apply admission fault boundaries
and 157 native/controller checks. Read
[the plane-cut receipt](validation/cut-current-paint-release-20261126.json).
The same 23 cancel-and-retry checks pass under ASan/UBSan. Read
[the range and cut strict receipt](validation/sculpt-range-and-cut-strict-20261126.json).

Four older sculpt fixtures now check the actual immutable chunk geometry,
scalar owners and current publication boundary. The corrected fixtures and
regular native strokes pass against the exact `f9c20398` core. Read
[the sculpt fixture assessment](SCULPT-PREVIEW-FIXTURES-20261126.md).
Combined candidate `c90c9fed` includes the dormant-paint and cut fixes and the
four fixture changes. All five focused cases also pass against its verified
`2aff79f7` product core.
The broad matrix and remaining failures are still under review.

Candidate `f5136b36` restores visible-scene image projection blocking by
sculpt volumes in the same immutable material query used for packed meshes.
The original failure is reproduced by an authenticated chunk hit in front
of a painted target. All 124 native checks pass in Release and ASan/UBSan,
including sculpt alpha, visibility, ghosting and target-only projection. The
matching Release App passes 10 software OpenXR loader checks covering the
original projection accept and focus-loss scenarios. Read
[the projection receipt](validation/dormant-main-and-sculpt-projection-20261126.json).

Candidate `7b009c92` builds the cold sculpt indexes directly from extracted
tile requests, keeping the same first edge representative and global cell
order. The former dense paged-ray case reached 16,000,000 work steps before
the render baseline was ready. The original case now passes all 90 native
cancel, replacement and shutdown checks with the same limit in Release and
ASan/UBSan. The strict surface and pipeline cases also pass.

Combined Release `e375b31d` passes all six focused cases and 524 checks,
including projection, cut retry, the paged-ray test and the four-object
sculpt workflow. Its accepted Save and two recovery files match the previous
native outputs byte for byte. All 22 SDF physical export controls pass in a
separate rerun against its core. The full strict cold-sculpt workflow reached
164 checks, including the exact retained quota, before exceeding its unchanged
4 GiB owned RSS limit at 710.886 seconds. The three smaller strict cases pass
90, 26 and 94 checks. The full run's status and log are under
`artifacts/sculpt-cold-representatives-strict-20261126/native/`. Read
[the combined authoring receipt](validation/cold-sculpt-and-combined-authoring-20261126.json).

Release candidate `460a30ee` retains a physical paint field across certified
affine bake UV layouts. The native test passes Bake, ordered Save, exact Undo
and Redo, native reopen and editable GLB replay. PBR and MToon pass on software
and Intel Arc with one RGB8 level of maximum error. A genuine earlier Scene43
file still reopens and saves byte for byte. The two original-image HDR failures
still reproduce and require separate image and paint sampling. Read
[the mapped emission result](MAPPED-EMISSION-20261126.md).

Combined candidate `25632dd3` adds that mapped bake to the separately
qualified HDR channel reuse, five sculpt regression fixtures and first-open
creator framing. Its fresh optimized App build and all 21 selected native
and software cases pass. A separate default-allocator run passes all 167
full sculpt checks in 83.91 seconds at 3,820,740,608 bytes peak owned RSS,
within the original 4 GiB limit. The smaller mapped candidate passes all
three selected strict sanitizer cases and reproduces the three original
identity-case GLBs byte for byte.

Candidate `08936bd8` retains original PBR and MToon emission images separately
from source paint. Both original-image regression cases now pass, including
missing-role refusals, native replay and ordered history. The separate image
and paint GLB passes the focused export and forged-map checks. Both renderers
pass linear, nearest and rotated MToon cases in software and on Intel Arc.
The strict native and corrected software renderer cases pass with leak detection.
The optimized build for the wider 86-case run on exact `08936bd8` passed.
The private software CTest run finished with 57 passes, 29 failures and
zero skips. It re-ran all 65 failures or dependency skips from the earlier
1,349-case run and cleared 36 of them. The other 21 selected cases all pass.
The full 1,349-case suite has yet to run on this newer source. Open areas
include joystick navigation, anatomical binding, source-native VRM export,
paint/archive ownership and GPU resource lifetime. The older joystick
fixture still drives the previous material page layout. Raw CTest and JUnit
results are under `artifacts/production-original-hdr-regressions-release-20261126/`.
Read [the original image result](ORIGINAL-IMAGE-EMISSION-20261126.md).

On candidate `367a4313`, replaced GL material and geometry stores retire
against completed GPU work even while more frames arrive or the active view
stays unchanged. The earlier ledger fails the isolated regression. The new
ledger passes 106 software controls with ASan/UBSan and leak detection, and
108 on authenticated Intel Arc A770. Actual renderer recovery passes 828
software and 831 Arc checks. Source GPU, packed MToon and native physical
MToon pass in both profiles. Each cold material budget probe rejects a limit
one byte below the measured requirement before material allocation. The
exact limit reproduces the original pixels.

The real-loader synthetic OpenXR client passes 150 checks with current
Base Color and weight-transfer controls. Affine retopology, import, groups,
emission and both editable XR export cases also pass. Both CC0 bases pass
the anatomical native worker tests, including controller method selection,
ordered Save and exact Undo. Creator VRM0 export now accepts changed UV
seams when each rest corner's original accessor, position and normal still
match. A 19-part avatar saves and reopens exactly, exports at three morph
weights, and passes 27 independent checks each in software and on Arc. A
forged source-corner remap leaves the existing export unchanged. Khronos
glTF validation reports zero errors on six exports, with the VRM0 naming
warning and 19 skinned-node hierarchy warnings per file. The focused logs
are under `artifacts/gpu-retirement-progress-release-20261126/`.
The same candidate passes all 828 software renderer recovery controls and
27 VRM export controls under ASan/UBSan with leak detection. The strict
build recompiled both changed production translation units, checked each
reused archive member and freshly linked both tests. The build and runtime
receipts are under `artifacts/gpu-creator-strict-20261126/`.
The same 86-case cohort on the frozen candidate completed with 71 passes,
15 failures and zero skips. Fourteen former failures now pass and all
57 earlier passes hold. The remaining failures involve VRM source metadata
and expressions, creator rest/placement export, normal-bake quality, paint
archive guards and one projection case. The exact case comparison is under
`artifacts/gpu-retirement-progress-release-20261126/combined-regression/`.
Product changes are on `work/gpu-retirement-progress-20261126`; main runtime
admission is unchanged.

Candidate `d39ebf8d` extends texture resolution authoring to untouched
source-authored meshes. The actual XR controller changes one FaceSkin
grid from 256 to 1024 on both CC0 VRoid bases. The mesh, image, rig and
material owners stay identical, and ordered Save/reopen and one-step
Undo/Redo preserve complete Scene data. Painted grids on this revision require
a paint-transfer adapter. The early 2048 FaceSkin probe explicitly used the
generic 64M work budget. Both bases reach 2048 under the editor's existing
128M budget, as verified by the later native grid probe.

The same candidate adds matching ordered source triangles to desktop
Bake and the XR Projection Policy panel. It transfers a displaced mesh
through corresponding corners within the world-space bounds. A changed
triangle order or excessive displacement refuses atomically. The new mode
uses recipe version 2. Older ray and cage recipes retain their encoding.
The original ray policy still refuses the male FaceSkin case at the
unchanged .02 radian bound. Matching triangles passes that bound at 256 px,
and both avatars pass independently rendered VRM0 and core-PBR GLB across
three poses and three morph weights. Software and authenticated Intel Arc
each pass 72 VRM0 and 92 GLB checks. The real-loader synthetic OpenXR
projection suite passes all 16 checks, including save and reload of the
actual selected policy and its baked output. The focused Release suite
passes 9 tests. The independent physical-emission transfer sample also
passes. Logs are under `artifacts/filtered-normal-authoring-release-20261126/`.
The 86-case Release replay finished with 74 passes, 12 failures and zero
skips. The two avatar normal-domain consumers and zero-morph rest check
recovered. All previous passes held. The fresh strict build passed all five
selected tests with ASan, UBSan and leak detection. The raw JUnit reports are
under the combined-regression subdirectory above and
`artifacts/filtered-normal-authoring-strict-20261126/`.
The candidate branch is `work/filtered-normal-authoring-20261126`.

### Shared affine placement in creator VRM0 export

Candidate `adb805a4` supports a complete creator avatar under one shared affine
placement when its creator shape is uniform or it has no active spring
geometry. Export preserves the placement in the glTF hierarchy and keeps
first-person and spring parameters in the same bone-local frames. Nonlinear
spring refitting remains supported under positive similarities. A divergent
part placement or nonlinear spring refit under a reflected, sheared or
non-uniform placement refuses atomically and preserves the existing output.
This qualification covers geometry, metadata and native reimport. Dynamic
spring simulation in an external runtime remains open.

All five selected Release tests pass on the fresh FLTK/OpenXR-capable build.
The two cases previously failing at the affine guard now complete. An
independent cgltf reader checks 268,704 corners over six direct exports
with a maximum world error of 0.431 µm. The native source-hint
workflow passes 80 checks across both CC0 bases in ordinary and reflected
sheared placements, including fit, ordered Save, Undo/Redo, pose, worker
re-export and fresh reimport. All three ordinary female VRM outputs are
byte-identical to `d39ebf8d`. Six direct exports and twelve source replay
exports pass the pinned VRM0 schemas and Khronos glTF validation with zero
errors. The existing VRM naming and skinned mesh hierarchy warnings remain.
Both placement regressions also pass the fresh headless ASan/UBSan build with
leak detection. All eighteen matching VRM outputs are byte-identical between
the Release and sanitizer runs.

The 86-case Release replay on exact `adb805a4` finished with 76 passes,
10 failures and zero skips. Both creator placement cases recovered and every
previous pass held. Logs and the case-by-case comparison are under
`artifacts/creator-vrm-placement-release-20261126/combined-regression/`.

Test-only follow-up `62e7b6ec` replaces the obsolete generated-part rejection
in `document-export` with a worker export of all 20 meshes on one skin. The
independent reader confirms the generated groom's textured unlit material.
An unsupported emissive groom still fails and leaves the previous VRM and
native scene byte-exact. The updated fixture passes 30 checks per Release and
strict run against the unchanged `adb805a4` archives. The original 86-case
result retains its `document-export` failure.
See [the validation receipt](validation/creator-vrm-placement-20261126.json).

### Resizing painted original grids

Candidate `07d70f24` enables the shared FLTK and XR resolution controls to
queue a detached operation
for painted single-mesh writable grids, including original VRoid FaceSkin
paint. Enlargement preserves every authored texel in the corresponding
destination cells. Reduction filters colour, opacity, masks, and emission
and checks every original authored cell against its result. The composed
paint overlays are compared over the same coarser texel footprints in
both directions. Rebuilding the writable raster retains the original
geometry, source images, material and rig owners. Prepared and baked fields
retain their separate adapter requirements. Painted resampling currently
requires the CPU backend and refuses explicitly on other build choices.

Both CC0 avatars pass exact 128→256→128 recovery of authored cells, including
hidden emission and fractional masks. The 128→256 controller operation adds
one undo step and survives ordered Save and fresh reopen. Separate Release
probes pass both default 256→512 workspaces and reduce the saved 256 grids
produced by the committed fixture. The maximum composed overlay component
errors in the reduction probe are 0.000588238 and 0.147059 under the existing
0.30 bound. Both empty FaceSkin grids also reach 2048 through the same
controller controls. The five selected Release tests and the same five
headless ASan/UBSan tests with leak detection all pass. Four saved native
projects are byte-identical between those profiles. Controller tests use
synthetic input. The 86-case combined Release replay on exact `07d70f24`
finished with 77 passes, nine failures and zero skips. The updated groom
worker fixture now passes in the combined build and every previous pass
held. See [the validation receipt](validation/source-authored-painted-grid-20261126.json).

### Static VRM1 expressions on core materials

Candidate `217b4ad2` evaluates base colour and texture transforms for plain
PBR and unlit VRM1 source materials. Lit PBR emission evaluates in the
original emissive-strength units. Each texture keeps its source image,
sampler, UV set and rotation. Native Preview and Apply retain both original
rasters and independently writable atlas paint, with one undo step and
ordered Save. Transformed PBR normals require a genuine fixed source tangent
frame. Existing MToon frame and paint guards still apply.

The six focused Release tests pass, including the existing software OpenGL
preview and a 326-check static expression suite. A separate Mesa llvmpipe
test renders the plain PBR and unlit original-source cases through Preview,
Cancel, Apply and native reopen. Reopening the exported PBR package reproduces
the applied native pixels through its embedded project. The standalone
portable payload is exercised separately below. An independent reader checks
144 exact corner tuples and 201 texture and factor values in the unpainted
portable PBR export. The export passes five pinned VRM1 extension checks and
Khronos glTF validation with four skinned-node placement warnings.
The fresh instrumented headless build passes five ASan/UBSan tests with leak
detection, and all four native projects and the exported VRM match Release
byte for byte. An earlier launch dropped the sanitizer flags and is recorded
as a repeated Release run.
The combined Release replay finishes with 78 passes, eight failures and zero
skips across the same 86 tests. The expression test recovers and all previous
passes hold. Generated tangent transport, live painted Core PBR expression
export and external HDR adapter composition remain open.
See [the validation receipt](validation/vrm1-core-pbr-expressions-20261126.json).

### Generated normal tangents for core source materials

Core glTF and VRM1 sources without authored tangents can generate their rest
and morph tangent frames from a rotated, scaled or reflected normal sampler.
Generation uses the effective KHR UV set and the sampler's linear transform.
Offsets affect texture lookup independently. VRM1 texture expressions edit
sampler scale and offset while keeping the imported vertex frame. Export
replays the original frame and writes explicit glTF base and morph tangents.
A singular sampler is rejected on import and a mismatched generated frame
cannot replace an existing portable file.

Candidate `fc528aca` passes seven focused Release tests and six headless
ASan/UBSan tests with leak detection. The expression fixture passes 492
checks in each profile. The focused tests exercise rotated and mirrored UV1 inputs, analytic tangent
directions and handedness, genuinely painted atlas inputs, native reopen and
standalone portable reimport. The portable tests strip the embedded project
from a copy before importing. The separate OpenGL check compares the actual
Preview, Cancel, Apply and native reopen pixels. Standalone portable reimport
matches exactly on llvmpipe and within one RGB8 level on Intel Arc A770.
That comparison also covers the earlier authored-tangent PBR fixture.
Independent schema and Khronos validation find zero errors in the checked
source and output files. Mixed MToon frame generation, portable live
expressions over paint, and other transformed-normal GLB material adapters
still require separate qualification.
The selected 86-case combined Release replay finished with 78 passes, eight
previous failures and zero skips. Every previous pass held.
See [the validation receipt](validation/vrm1-generated-tangent-20261126.json).

### Transformed core normal maps in GLB export

The standard GLB and Editable GLB file workers now export transformed core
normal maps when they can replay the actual source frame and verify the
original image, sampler, rest geometry, and emitted UV corners. The output
uses explicit base and morph tangents. Both authored and generated frames
work on original geometry and unpainted prepared atlases. Existing source
VRM1 frame replay shares the same verifier.

Tests exercise a skinned core glTF source, authored UV1, generated mirrored
UV2 and native atlas preparation. Standalone output reimports with exact base
and morph tangent streams. The editable output retains the native scene.
Altered source UVs and cancellation at commit preserve the existing GLB.
The headless software and Arc comparisons match the standalone reimport
exactly against native PBR pixels across both eye positions and all four input
cases. A separate editable export probe retains genuine authored atlas paint
for both authored and generated frames. Khronos reports zero errors across
the checked source and output files.

Core candidate `fed5cd80` passes nine Release and eight ASan/UBSan tests
with leak detection. Follow-up `2cbc1f1a` routes the headless
`--export-project` command through the standard file worker. The fresh
FLTK application exports the previously failing UV1 project and the sculpt
demo, and both files validate. The 86-case replay on `2cbc1f1a` finished with
78 passes, eight previous failures and zero skips.
See [the validation receipt](validation/core-gltf-normal-export-20261126.json).

### Bake, projection and archive regression integration

Candidate `9a2958a8` updates five regressions against the current authoring
paths. The retained-paint Bake matches a Bake from the restored original
pixel for pixel, with a spatially asymmetric colour witness and atomic late
cancellation. A second Bake from imported prepared normals checks 2,304
interior texels against the known source tangent basis. Its maximum direction
difference is 0.005282 against the 0.02 test bound. The actual Document worker,
immediate Save, reopen and Undo/Redo agree with the detached result.

The PBR projection fixture checks the current immutable query with and
without object grab, plus independent reconstruction from the captured
streams. Its delayed-publication gate now intercepts the current mesh
worker. All 134 checks pass on software and Intel Arc A770, including the
51 allocation-failure boundaries. Polygon archive checks distinguish an
owner cycle from excessive graph depth and retain the enclosing FLOAT
payload guard. Five focused Release and five ASan/UBSan suites pass with
leak detection. These tests use verified staged archives with a newly
compiled graph validator. The fresh FLTK Release build completed. The same
86-case replay passed 83 with no skips, improving on 78 on `2cbc1f1a`.
All five focused fixes passed in the combined run. The three remaining VRM
fixtures still report a creator-source SHA256 mismatch after modifying their
embedded source bytes.
See [the validation receipt](validation/authoring-regression-integration-20261126.json).

### VRM0 fixtures through normal import

Candidate `ccbbe020` authors the modified VRM0 inputs before import, so Dome
Sculpt constructs the original node graph, materials and deformation owners
through the normal import path. The broad suite exercises both verified CC0
VRoid bases. It retains an explicit negative test for altered embedded
creator bytes, checks exact material-expression forks, and verifies that an
unrepresentable painted colour expression leaves the old export byte-exact.

The two independent graphics consumers run using real imported skin and
morph data, with per-corner RGBA and UV checks, original and rebaked UVs,
texture role aliases, animated material factors and an opaque canonical atlas
fixture. All 61 assertions pass on software and Intel Arc A770, with a
maximum difference of one RGB8 step in both profiles under the original
four-step tolerance. Arc execution was witnessed on `/dev/dri/renderD129`.
The broad suite passes 167 checks under Release and ASan/UBSan. Both software
graphics consumers also pass under ASan/UBSan, with leak detection enabled
throughout that profile. The candidate changes test code. The fresh optimized
FLTK build and 86-case replay on the exact pushed source finished with 85
passes and no skips. All three VRM cases pass. The sole failure was a host
resolution fixture that rearmed a readback pause before the earlier worker
had exited its wait. See [the VRM receipt](validation/vrm-consumer-source-20261126.json).

The exact combined Release application passed 14 native Wayland checks through
canonical DomeD on Intel Arc, 14 public creator desktop checks, and 58 public
creator synthetic OpenXR checks on Intel Arc. XR covers accept, cancel, focus
loss/regain and three ordinary reopens. The wrappers verify input digests and
clean teardown. The native Wayland run uses a nested compositor with diagnostic
presentation. Physical headset and scanout acceptance remain open.

Fixture-only candidate `1ab9d535` deliberately queues a new resolution request
while the previous generation is still paused after readback. Ticketed release
keeps both waits valid and isolates the outgoing job from the new job's fault
injection. The exact old scene and file survive cancellation and the injected
failure, and the new transaction retries successfully. All 23 assertions pass
in 20 Release and eight ASan/UBSan repetitions with leak detection. The whole
Release build with all 1,360 registered CTests and the separately built 86-case
sanitizer candidate are active on `ccbbe020`, with the fixture correction
qualified separately. Read [the combined receipt](validation/vrm-consumer-combined-public-20261126.json).

### Portable live expressions on opaque unlit paint

Candidate `b2bfba2d` exports genuine source-atlas colour paint on an opaque
VRM1 unlit material. The lower primitive retains all original colour and
texture expression bindings and sampler transforms. A derived unlit blend
primitive carries the fixed paint. It reads the same vertex RGB but uses its
paint coverage for alpha. The native package retains the complete editable
source and paint. The worker refuses a live expression whose RGB range would
clip before the two draws are combined, preserving the previous export.
[glTF unlit specification](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit/README.md) · [VRM expression specification](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_vrm-1.0/expressions.md).

The focused suite passes 620 native checks and the existing MToon expression
worker passes 44 checks in both Release and ASan/UBSan with leak detection.
The source, exported VRM and saved native project match across profiles.
Independent glTF parsing and rendered comparison cover five expression states,
including a singular UV target and simultaneous expressions. On llvmpipe and
Intel Arc A770, the largest difference is one RGB8 step and alpha is exact.
Freezing the source UV or using source vertex alpha for paint yields differences
of 157 and 41 RGB8 steps. Khronos validation reports zero errors on the source
and exported VRM. Read [the validation receipt](validation/vrm1-unlit-live-paint-20261126.json).

### Portable live expressions on masked unlit paint

Candidate `e09f2f4b` extends the same native worker to original unlit
alpha-cutout materials when the source and native composite both retain
every fragment where fixed colour paint contributes. The original cutoff,
vertex alpha and expression bindings stay on the lower draw. The upper
draw uses paint coverage. A conservative proof evaluates the original
sampler and vertex alpha over the editable footprint and the full
expression weight range. The local proof requires an explicit nearest
or linear filter. It refuses with the previous export and native bytes
intact when a moved cutout reaches the paint, vertex alpha can put the
paint below the cutoff, or the portable viewer can choose an unspecified
minification filter. Live core materials keep the authored rotation
and signed scales because equivalent rest transforms can animate
differently. [glTF alpha and sampler specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)

Release and ASan/UBSan each pass 641 native expression checks and 45
MToon worker checks. A startup preflight fix restores the failed MToon
control and preserves cancellation through large binding tables. Both
profiles finish the 2,180-check MToon sweep, including 827 persistent
allocation denial prefixes and 1,337 cancellation boundaries. The source,
exported VRM and editable project are byte-identical across profiles.
Independent OpenGL checks on llvmpipe and Intel Arc A770 cover five
expression states, including a rotated, repeating original sampler with
a negative scale and real cutouts. The largest RGB8 difference is one
and alpha matches exactly. Bad UV, vertex alpha and rotation-basis
controls change the visible result. Khronos reports zero errors, with
five warnings on input and four on output. All 25 instances checked by the
pinned VRM schemas pass, including the masked unlit and MToon inputs and outputs.
[Validation receipt](validation/vrm1-unlit-mask-paint-20261126.json)

Live lit PBR, blended unlit, cutouts that cross painted fragments and
additional paint channels remain open. The full Release and combined
sanitizer runs use `ccbbe020`. Its sanitizer run finished with 82 of 86
cases passing. Standalone XR and affine retopology report the same
112-byte software Mesa teardown leak. The original full-size HDR seed
timed out and its provenance dependent was skipped. The exact
`e09f2f4b` optimized application completes 275 build actions with the
canonical DomeD FLTK toolkit. Its real OpenXR loader passes all 53
controller-driven checks with synthetic input, including editable export
at 256, 512 and 1024 pixels, Cancel, retry and reopened project Save. An
independent reader renders each resolution across five expression states
on llvmpipe with one RGB8 step maximum and exact alpha. All exported files
in the workflow pass the pinned VRM schemas and Khronos validation with
zero errors. The same binary passes 14 native Wayland checks through
nested DomeD on Intel Arc, including menu, modal text and teardown.
Physical headset and direct scanout acceptance remain open.
[Mesa teardown evidence](MESA-XR-RTASM-LIFETIME.md)

### Independent HDR provenance fixture

Candidate `9a942772` gives the provenance and retained-emission worker tests
their own pre-Bake fixture. It uses the same real source construction and
native destination resize, ordered Save, reopen, Undo/Redo and stale
authority checks as the full 2048 workflow. The original 2048 Bake,
numerical sweep and file/history checks remain registered with their
900-second limit under `source-hdr-joint-full-size`.

The independent CTest suites finish 5/5 in Release and 4/4 in ASan/UBSan.
The seed passes 17 checks and both dependent tests pass 22 each in both
profiles. The two new seeds are byte-identical. A read-only comparison
with the old and new monolithic seeds finds only the generated fixture
group name differs. The full Release workflow passes 47,409 checks in
197.50 seconds and retains all 73,530,600 independent samples per role.
The original monolithic sanitizer timeout remains recorded and its full
workflow still needs a strict pass.
[Validation receipt](validation/hdr-closure-independent-seed-20261126.json)

### Strict HDR checkpoint and Arc regression follow-ups

Candidate `0833d18a` keeps the original monolithic HDR command and adds
independently bounded capture, numerical and history commands. The capture
writes the original native source and pre-Bake and baked native states.
The consumers reopen those files read-only, use the same numerical and
Undo/Redo checks, and verify the ordinary and ordered saves against the
captured Bake byte for byte. The first prototype's printable JSON
comparison failed on native source ownership. Reloading the original
native archive preserves the encoded data types and closes that setup
error. The final Release 512 checkpoint chain and original 47,409-check
workflow pass. Its parent `55e67401` also passes the full 2048 Release
checkpoint chain with all 73,530,600 samples per role. The 2048 sanitizer
capture passes on that parent. Its first strict dense oracle and history
jobs each reached the 900-second deadline. The ordered native file was
written before termination. The final `0833d18a` capture also passes with
43,293 checks. Its unpartitioned dense and history consumers also
reached the original deadline.

The exact `ccbbe020` ASan/UBSan application passes all 27 affine
retopology checks on Intel Arc through the real OpenXR loader with
synthetic input and leak detection. All four app processes were observed
on `/dev/dri/renderD129`. This covers projected points, preserved paint
and pose, focus loss, unconfirmed drafts and clean shutdown.
The broader Arc replay passes its first 58 checks, including
initialization and session restart, then the radial voxel-remesh Undo
case saves the same state as Accept. A separate delayed direct Undo action
restores the exact baseline. The original software and Arc failures
remain recorded.
[Follow-up validation](validation/strict-regression-followups-20261126.json)

### Rendered controller readiness and full-size dense rows

The Arc stereo trace shows the second voxel-Undo click opening the palette
with Clay selected. Candidate `5a89ea5a` checks the actual root and enabled
Undo highlight before activation. If remesh publication closes the palette,
the synthetic controller reopens it after the released images show it gone.
The focused Arc ASan/UBSan replay and complete software Release replay
restore the original native project byte for byte. The latter passes 151
checks.

The broader Arc replay on `5a89ea5a` stops earlier at an independent
material edit. Candidate `6f4743db` waits for the actual highlighted
Materials sector and three RGB tracks before the existing ray actions.
The focused Arc sanitizer replay reaches both acknowledgements and saves
the exact expected RGB fields. The full software Release replay passes 152
checks. Its Arc counterpart passes the two controller paths and stops after
135 checks when the initial `--paint` project reaches the 40-second deadline.
An isolated replay reaches the same deadline before an EGL log entry, an Arc
render-node open or a project save. The process continues to consume CPU time.
The exact startup stage still needs profiling.

Candidate `871513fa` adds four disjoint row ranges to the existing dense
HDR oracle. The complete 512-pixel Release reference and aggregate agree
on sample counts and maxima for PBR, MToon and HDR. All four 2048-pixel
ASan/UBSan ranges pass on one read-only native capture. They match the
unpartitioned Release reference's 73,530,600 samples per role and error
maxima. The slowest range takes 382.069 seconds and the largest sampled
owned RSS is 2,098,626,560 bytes. The 2048-pixel ordered Save, reopen and
Undo/Redo history consumer still needs a complete strict pass.
[Validation receipt](validation/controller-feedback-and-dense-rows-20261126.json)

### Atlas worker scheduling and strict history replay

Native stack samples identify the pre-EGL delay in the worker's xatlas
chart-generation phase. Two waiting threads also repeatedly yield while
the chart worker runs. Main `732780d3` sleeps those waiters until the last
task completes and publishes a queued task's count under its queue lock.
The patch preserves the atlas calculations. Matched Arc startup saves are
byte-identical across both profiles. Median pre-EGL CPU falls 64.8% across
two sanitizer pairs. The single Release pair uses 61.6% less pre-EGL CPU.
Time to EGL improves by 1–2 seconds in the sanitizer pairs and 0.24 seconds
in Release. Six focused cases pass on main Release and on combined Release
and ASan/UBSan. The combined software XR replay passes all 152 checks.
The combined Arc replay passes 140 checks, including paint startup and
the persistent stroke, then the UV edit saves the unchanged project.
Three focused traces on the parent and three on the patched build reproduce
the unchanged save from one prepared project. The stereo capture shows the
UV grid without published chart edges at the start of the fixture's edit.
Fixture `25a29897` waits for visible chart edges and an enabled Apply tile.
The focused Arc ASan/UBSan replay then changes the saved UVs while
preserving the source sculpt and paint hashes. The full software Release
and Arc ASan/UBSan replays both pass all 153 checks. The Arc UV fixture
observes the enabled panel after 23 rendered-wait steps. Forty application
processes witness the Arc render node during the full replay.

Main `c352cb72` adds cooperative cancellation inside connected chart growth.
A regression using the actual starter sculpt cancels before its first
chart group completes and verifies exact input geometry and authoring data.
The same test fails against the previous dependency. All six focused cases
pass on main Release and on the combined Release and ASan/UBSan builds.
The main Release Arc startup still saves the same native paint project.

The HDR replay `78e94585` keeps the original monolithic workflow and
separates the native Save/reopen proof from the real Document worker replay.
Bake, immediately ordered Save, Undo and Redo stay in a single worker
session. At 512 pixels both phases pass in Release and both files match the
captured native Bake. At 2048 pixels the independent strict Save/reopen
phase passes with the same byte identity. The isolated strict worker writes
the identical ordered Save and verifies the accepted and undone native
states. The combined Redo/preview/equality endpoint fails within the time
budget. Diagnostic `d257ea07` confirms Redo returns success and restores
the exact native state. The mesh publication exceeds the existing
10-second wait and the document reports an empty error during the wait.
A separate native 2048-pixel Open and Clear Bake/Undo/Redo control shows
that the restored baked preview can publish in 15.55 seconds under
ASan/UBSan. Its extended observation measures latency independently of
the original, unchanged history gate.

The broad combined Release run found two VRM1 fault suites expecting the
pre-refactor diagnostic for a foreign retained primitive. Candidate
`3130bc2b` updates that expectation to the shared guard and retains each
negative and the full original-state equality check. The original 1,360-case
Release run finishes with 1,358 passes and those two failures. Both fixed
suites pass their full Release and ASan/UBSan reruns, with 2,664 Normal and
2,736 Shift checks in each profile.
[Validation receipt](validation/atlas-worker-and-history-20261126.json)

### Baked preview work and combined build

Main's channel decoder `88f95028` reads only the selected baked plane and
uses the same sRGB conversion for byte samples. It keeps the original
sampler interpolation for source textures. The combined counterpart
`ff06dfb8` reproduces all ten 2048-pixel preview planes byte for byte
in Release and ASan/UBSan. The Release preview uses 0.983 seconds of CPU
versus 1.239 on its parent. The sanitizer pair uses 6.749 and 6.613 seconds
respectively. The existing strict history wait is being exercised on this
build. The main Release application links with the canonical DomeD toolkit
and passes five native bake, source UV and cache cases. The first combined
source UV test used a read-only scratch directory. The same binaries with
a writable scratch directory and both VRoid inputs pass in both profiles.
A separate candidate `8a4d7d35` extends the existing size-bounded
composite cache to authenticated HDR previews so editable history can
reuse them when the renderer releases its previous frame. Native Release
and ASan/UBSan cases pass for eviction, exact geometry ownership, explicit
physical emission keys and cancellation. The full-size worker is running.
The first full-size Clear Bake/Undo diagnostic with the cache still takes
15.49 seconds to restore the baked preview. The Release ownership probe
shows the 2048-pixel entry rebuilt after Undo. Its overlay already owns
275,696,172 bytes and the composed map separately owns a 104,857,600-byte
baked base. Candidate `feef71a8` completes the legacy preview in fresh
maps and attaches the exact immutable bake already owned by the HDR
packet. Its matched runs measure plane fidelity and history reuse under
the existing 512 MiB retention limit. Release reproduces all ten preview
planes byte for byte. The restored HDR frame and ordinary cleared frame
still require more than the cap together. Candidate `f9830df4` lets an HDR
entry survive one intervening ordinary cache insertion under pressure.
Additional pressure can evict it, and the foreground always receives its
composited frame. In the 2048-pixel Release Clear Bake/Undo control, the
same immutable preview survives and restores in 0.666 seconds. The
shared-owner build alone recomposes it in 1.601 seconds. The full-size
ASan/UBSan Clear Bake/Undo control also reuses that immutable preview,
publishing in 8.374 seconds without another build. Retention is
444,041,876 bytes against the 512 MiB cap. The same diagnostic needs
13.159 seconds to publish the ordinary cleared preview again on Redo.
The original full-size Bake, ordered Save, Undo and Redo test also passes
all eight ASan/UBSan checks with the cache candidate. It takes 633.363
seconds within the 900-second phase limit and peaks at 2,503,692,288
bytes under 4 GiB. The ordered file matches the observed second Bake
byte for byte. Redo publishes the baked preview within the existing wait
and restores the exact native state. The main-based port `ead80bb5`
passes the optimized link/version check and five focused bake, UV and
cache tests with the two CC0 VRoid inputs. Its fresh ASan/UBSan application
build, version check and the same five native CTests also pass. Main
includes this cache port.

Combined candidate `3cb2fe81` keeps the `ccbbe020` source baseline and adds
both atlas patches, the rendered XR readiness fixtures, and the two VRM1
diagnostic oracles. Six native cases pass in both profiles. Its full
software Release replay passes 153 checks. The matching full Arc replay
passes 146 checks including UV, then reaches the file workflow's 40-second
deadline after all 45 frames. The isolated replay reaches the same deadline.
Stacks show the ordered final Save waiting for the export worker's active
xatlas chart growth. The `b3b583b8` scheduler candidate
passes that full Arc replay through its existing background file checks.
An isolated pair with an extended observation limit terminates after
40.70 and 39.30 seconds respectively on the scheduler and combined
builds. Their native Save and glTF export finish, and the exported GLBs
match byte for byte. The full Arc rerun under the original limit again
passes 146 checks and times out in the same file workflow.

The sparse boundary grid candidates `24bd45a5` and `b949cfef` retain
cell order while resetting occupied cells only. Release and ASan/UBSan
analytic tests each check 574 intersections over 160 grids. Six native
cases pass per profile on the combined candidate. In four matched Arc
runs, both app versions save byte-identical native projects and GLBs.
The parent takes 44.57 and 44.16 seconds and the candidate takes 44.36
and 43.55 seconds. These observations used an extended diagnostic limit.
The original workflow budget is still 40 seconds. A separate candidate
`6cca3fc2` keeps each chart's ordered boundary across growth and merges
and uses it to refresh neighboring candidates. Six native tests pass in
each profile, along with the full 153-check software Release replay.
A test build compares its retained edge list to a full scan before every
intersection query. The four curved and perforated meshes produce
identical chart membership, topology and UV output in both profiles.
The matched Arc comparison retains byte-identical native Save and GLB
output. The parent takes 41.52 and 42.12 seconds and the candidate takes
40.73 and 39.52 seconds. Candidate `0fa8eaba` also visits grid cells
directly in traversal order. It passes the 574-check grid oracle and six
native cases in each profile. Two isolated Arc candidate runs complete
in 34.88 and 34.08 seconds. The matched parent takes 41.12 and 36.68
seconds. All four Save and GLB pairs are byte-identical. The original
software Release and Arc ASan/UBSan suites each pass all 153 checks
with unchanged file deadlines. The Arc suite attests 40 real device
witnesses, including the Files workflow on renderD129. Headless replay
of that workflow's recovered project finds 21.04 seconds in chart growth
and 11.04 seconds in placement search on the candidate. Experimental
packing-bounds patch `2d08e63e` preserves the exact output in both
profiles, including block-aligned packing. Its matched headless probe
changes placement search from 10.45 to 10.33 seconds, so it stays on
its own branch.

The main-based atlas port `66d31263` passes six native cases in both
profiles. Its software XR replay stops at the existing fixture's material
menu assertion after 48 checks. The prior main build fails the same
assertion with the same 48 passes and a byte-identical scene. Main
includes the atlas port. Its new replay `518a1d89` observes the rendered
Materials page and RGB sliders, uses separate ray gestures, and checks the
exact saved RGB values. The same main app binary now passes all 153
software checks and saves a second object with RGB `0.04, 0.12, 0.80`.
This changes only the test fixture and the exact skin Restore oracle.
The main Arc sanitizer replay passes the material and UV cases. It stops
after 146 checks at the Files deadline, after all 45 XR frames have rendered.
The main versus combined timing replay uses the same synthetic runtime and
produces four byte-identical native and GLB pairs. During the clean full
build, the combined binary takes 48.61 and 46.97 seconds and the main
binary takes 50.39 and 50.18 seconds in isolation. The diagnostic observes
completion after the 40-second test limit. The original passing combined
run and main timeout remain separate evidence.

Candidate `5c977be9` combines the exact qualified atlas source with the
matching full HDR preview cache source. Its full software Release and
Arc ASan/UBSan replays both pass 153 checks under the original deadlines.
Both produce the same GLB byte for byte. Khronos glTF Validator reports
zero errors and zero warnings on both files. The ordinary Document Import
worker imports the exported mesh and passes exact Undo/Redo. After a native
Save and re-export, the 20,320 triangles retain their positions, winding,
UVs, 60,960 corner normals and materials. The round-trip GLB also passes
Khronos validation with zero errors and warnings. Integration merge
`4f083e6e` preserves main's history, the later mapped-emission evidence
and the exact combined sources, assets, tooling and tests. The clean whole
Release build completed. The private suite recorded 1,359 passes, one
failure and zero skips across all 1,360 cases. Both previously failing
VRM1 allocation-fault cases passed. The one UV observer failure and its
corrected focused replay are recorded below. The full-size native HDR history passes eight Release
checks, with the ordered save matching the independent Bake capture.
The exact combined application
passes all 14 native Wayland checks through the nested canonical DomeD
compositor on Intel Arc, with no input drift or owned survivors.

The whole Release run exposed a synthetic UV readiness error in
`native-key-uv-openxr`. Three exact isolated replays reproduced the
unchanged Save. The observer counted 97 light pixels and missed selected
edges drawn in the DomeD accent. Fixture `f7877989` counts both colors
while requiring the enabled Apply indicator and completed controller
sequence. The corrected observer sees 212 chart pixels and passes all 26
checks on the frozen combined app and the current active-set app, including
a changed chart on Apply and exact native bytes after Cancel or focus loss.
The original full-suite result and the focused replay remain separate.
[Native XR UV observer evidence](validation/native-key-uv-rendered-atlas-20261126.json)
[Full Release regression evidence](validation/combined-atlas-hdr-full-release-20261126.json)

The combined ASan/UBSan history run accepts the original Bake, writes the
exact ordered Save and restores the exact native state through Undo/Redo.
Redo's preview misses the original ten-second wait. A separate sampled
Clear Bake/Undo run reaches the same publication boundary in 12.292 seconds
with the same texture-map owner and no additional composite build. Four
mesh-worker stacks show the prepared-color numerical validation under
`MaterialSurfaceQuery`. Candidate `b458d7bd` reuses numerical bounds across
jobs through exact weak owners and the existing image and field content
checks. It keeps all receipt and cancellation checks, and clears the cache
on a new document generation. Both profiles rebuild all 26 affected core
translation units and link the app and test targets. Release passes 3,471
memo checks and all eight 2048-pixel history checks. The ordered Save bytes
match the independent Bake capture. The focused ASan/UBSan Clear Bake/Undo
probe now restores the same texture-map owner in 9.485 seconds with the original
ten-second wait and no additional composite build. It also completes ordinary
Redo. This probe and the original history replay use the same captured project.
The full sanitizer history test retains its independent Bake, ordered Save and
Redo preview assertions and now passes all eight checks. The ordered native
Save matches the independent Bake capture byte for byte, including exact
Undo/Redo. The sanitizer run stays within the original 900-second and 4 GiB
limits.

Main merge `dd69136a` integrates the exact tested port `fa288931`.
Clean optimized and sanitizer application builds both pass all eight
native cases. The same applications pass all 153 checks in software
Release and on Intel Arc with ASan/UBSan through the real OpenXR loader
and a synthetic runtime. The Arc Files workflow attests renderD129.
The separate cache sanitizer run passes 3,471 controls, including 3,438
allocation-denial cases. Combined candidate `3e4f1756` has identical
application, build, test, asset and third-party inputs to `b458d7bd`.

The wordwise atlas candidate `19f125ae` checks clipped overlap without
splitting aligned chart words. A matched native export keeps the same
GLB and native file bytes while packing placement search drops from
14.01 to 3.07 seconds. Both profiles pass 734,580 independently evaluated
overlap cases and six native cases. The software Release replay passes
153 checks. The Arc run during the whole build stops after 135 checks
at paint startup. The main port `5c8c8fb1` passes six native cases in each
profile and the complete 153-check software Release and Arc ASan/UBSan
replays with the original Files deadline. Main includes this packing change.
In four matched Arc Files runs, the previous build takes 44.56 and 43.56
seconds and the new build takes 33.11 and 32.31 seconds. All four produce
byte-identical GLBs and native projects. Integration merge `5ae10f18`
preserves current main with identical combined sources, build inputs and
tests to `19f125ae`.
An experimental sorted-frontier child `4021dc13` keeps four mesh outputs
exact, including 57,311 comparisons of cost ties and candidate face order.
A matched headless run drops from 38.38 to 34.60 seconds overall.
[Atlas packing and HDR publication evidence](validation/atlas-packing-and-hdr-redo-20261126.json)

The headless `--export-project` port `7470d805` routes standard GLB export
through the same Document worker as the native application. Both Release
and ASan/UBSan produce the exact Files workflow GLB for the ordinary
sculpt. A separate native fixture with genuine physical emission paint
now exports and reopens to the exact original editable HDR scene. The
prior executable refused this fixture before writing. Both new builds
emit the same HDR GLB and pass Khronos validation without errors or
warnings. A truncated project leaves an existing GLB byte-identical.
[Preview and integration evidence](validation/hdr-preview-composite-20261126.json)
[Cache and Arc rerun evidence](validation/atlas-grid-hdr-history-20261126.json)
[Combined OpenXR and independent GLB evidence](validation/combined-atlas-hdr-openxr-20261126.json)

## Texture-set bake recipes

Candidate `fe16a684` persists individual Scene40 texture-field Bake recipes
while retaining shared raster storage. Plain Scene40 projects still encode
through the same legacy records. A saved project with three distinct
current, retained and additional-field source/cage contexts reopens and
saves byte for byte. Five suites pass in Release and ASan/UBSan, including
20 focused recipe checks and exact output parity between profiles.

Deleting a recipe source and appending the same project exposed an
additional-field reference bug. Candidate `273344dc` keeps original IDs
and marks deleted live references unavailable. Native append remaps current
and retained recipe sources and cages to the new object namespace. Both
profiles pass all five suites again, including 27 recipe checks across the
two paths and exact current and retained Scene replay.

The original prepared-polygon probe exposed an origin ownership mismatch.
Native prepared paint then passed deletion, append and byte-exact reopen
on `ef96e7c2`, while the prepared PBR path found a second retained recipe
in canonical UV history. Candidate `c0c1265c` rebases both kinds of
material owner and shares their authenticated sampling and preview data.
Archives explicitly identify shared sampling with earlier-node references,
checking the actual geometry and original Paint owners. Release and
ASan/UBSan each pass 75 prepared recipe controls across native paint and PBR, including
deleted-source persistence, append and forged archive refusals.

The unchanged Paint6 fixture expected older exception wording. Its
failure reproduces on main. Test-only `a3d59082` checks the exact missing
physical-context refusal and passes all 16,407 controls against both
existing Release cores and the canonical candidate's sanitizer core. The original failures and corrected results are
preserved. Combined application `b89b1f8c` pairs the recipe transport with
the admitted HDR fix. Its test-only companion `6b5d338e` passes all ten
native cases in optimized Release and ASan/UBSan. The matching applications
each pass all 153 synthetic OpenXR checks, with exact voxel Undo bytes.
The sanitizer XR process uses Intel Arc at `/dev/dri/renderD129`.
Main integrates the qualified recipe-persistence source as `06d1c5fe`.
[Texture-set recipe and reference evidence](validation/texture-set-bake-recipe-20261126.json)
[Combined runtime evidence](validation/texture-set-recipe-runtime-20261126.json)

Main merge `70e9855d` adds Bake, Clear and Recall for the selected plain
native texture set. Projection uses the selected triangles, their complete
mesh tangent frame and an optional full matched cage. A plain source with
several texture sets supplies each hit's own material and maps. The Release
and ASan/UBSan native cohorts each pass all eight tests, including ordinary
Bake and 22 active-set assertions. The finalized app passes 15 desktop checks
through nested canonical DomeD and 19 synthetic OpenXR checks per software
Release and Intel Arc sanitizer profile. Cancel and focus loss preserve
original project bytes. Accepted outputs match the independent recipe
replay and agree byte for byte across the XR profiles. The original
llvmpipe sanitizer run saved the exact accepted project and reported a
112-byte leak at teardown, with both allocation addresses in Mesa's
libgallium mapping. The Arc replay kept leak detection enabled and exited
cleanly. Retained prepared targets and physical-input verification remain open.
[Active-set acceptance and original leak evidence](validation/active-texture-set-bake-20261126.json)

Main merge `72155951` adds static retained texture sets as Bake sources for
ordinary and selected plain targets. Each hit uses its source field's actual
original paint coordinates and before/current normal frames, including
prepared Core PBR normal textures and absolute FLOAT emission. The worker
shares the authenticated corner streams and preserves the source project.
The mixed Scene40 preflight now opens the same saved bytes when retained
source fields and plain Bake recipes coexist. It rejects a plain recipe with
a forged set reference.

Release and ASan/UBSan each pass all ten focused tests and 569 retained-source
assertions. The exact app passes 15 native Wayland checks through nested
DomeD and 19 synthetic OpenXR checks per software Release and Intel Arc
sanitizer profile. Both XR profiles save identical bytes, including exact
Cancel and focus-loss results. The fresh Release build for this checkpoint
finished all 1,766 build steps. Its 1,334-test regression is running.
[Retained-source Bake evidence](validation/retained-texture-set-bake-source-20261126.json)

Main merge `7ae33ac1` adds affine placement for plain and retained
texture-set donors. Bake uses the viewport's placed normals and tangent
handedness for shear, reflection and nonuniform scale. Reflection changes
the front-face test while source triangle and corner identities stay
stable. The native tests include a rotated and scaled recipient, original
paint coordinates, prepared PBR normals, FLOAT emission and front-only
projection from a reflected plain donor.

All ten focused tests pass in both Release and ASan/UBSan, with 1,100
retained and affine assertions in each. The exact build passes 15 nested
DomeD Wayland checks and 19 synthetic OpenXR checks per software Release
and Intel Arc sanitizer profile. Both XR profiles produce identical
native projects.
[Affine-source Bake evidence](validation/affine-texture-set-bake-source-20261126.json)

Main merge `1afdbf95` adds world-metric Bake for affine native targets,
including prepared normal targets and selected plain texture sets. Rays, AO
and independently placed cages use physical distances. Normal encoding uses
the target's original corner tangents placed in the rendered frame, including
reflected handedness. Projected maps preserve local geometry and placement
and publish through the existing selected-field, history and Save paths.

Ten focused tests pass per Release and ASan/UBSan profile, with 1,146
source and target assertions each. The exact build passes 15 nested
DomeD Wayland checks and 19 synthetic OpenXR checks per software Release
and Intel Arc sanitizer profile. The two XR replays produce byte-identical
native projects. In the independent translation probe, moving all three
objects by several million metres preserves the raster bytes for 780
projected texels and 3,900 projection and AO rays. Retained writable
targets, deformed retained donors and physical-device acceptance remain open.
[Affine-target Bake evidence](validation/affine-target-bake-20261126.json)

Main merge `df636b4d` adds Bake, Clear and Recall for selected retained
Core PBR targets. The worker rasterizes material projection in the
retained paint grid and uses a separate pass when the original normal
coordinates differ. It encodes projected normals in the inverse of the
actual placed before-to-current frame and keeps the original normal
field in the new field's ancestry. Clear restores the inherited bake
while keeping the last recipe. Set selection uses the existing FLTK
dialog and XR set commands. Scene40 preserves both the authored member
and its original record, and the existing Scene41 envelope carries
retained FLOAT baked emission.

Eleven focused native tests pass in Release and ASan/UBSan, with 76
retained-target checks in each profile. Seventeen nested DomeD Wayland
checks cover selection, Bake, Clear, Undo and ordered Save. Nineteen
software and nineteen Intel Arc ASan/UBSan synthetic OpenXR checks
cover Recall, accepted Bake, cancellation and focus loss. The two XR
profiles produce byte-identical native endpoints. An independent
15-point reflected world-normal comparison measures a maximum vector
error of 0.000935. Portable LDR PBR output uses the exact normal image from
each field. The broader production gate still includes N-only retained
recipients, deformed retained sources and physical input.
[Prepared retained-target Bake evidence](validation/retained-target-bake-20261126.json)

The follow-up fixes a prepared retained project's standard GLB export.
A false HDR capability in its material archive had routed the project
through the physical-emission exporter. The normal GLB worker now
exports the actual active and neighboring material primitives. A cold
headless export of the accepted Wayland/XR scene is byte-identical in
Release and ASan/UBSan. The genuine HDR export worker also passes in
both profiles. Per-field portable HDR export remains open.

The broader 1,334-case run on the older retained-donor build exposed
a separate UV1 archive diagnostic. The reader now rejects writable
alternate-UV data before calling the native bake installer if the
input lacks its owning Paint4 context. The unchanged failing fixture,
its valid paired UV Bake and both retained bake roles pass in Release
and ASan/UBSan.
[GLB worker and UV archive evidence](validation/retained-target-export-worker-20261126.json)

Main merge `d88726a5` adds retained Bake for native authored normals
without an inherited normal map. The selected field owns a normal detail
image on the original paint grid. Its coverage keeps the inherited
smooth normal outside projection. Successive partial bakes keep the
earlier covered pixels. Clear restores the original field and leaves its
last recipe available. Scene40's source member owns the new normal
image in the existing archive. Scene41 also carries FLOAT emission.
That milestone exported constant-frame LDR GLB with the actual
field image and authenticated tangent basis.

The twelve focused tests pass in each Release and ASan/UBSan profile,
with 112 new native normal checks. A separate primary-field test passes
59 checks per profile, including repeated partial Bake and original-grid
FLOAT replay. The independent software and Intel Arc renderer matrices
each check 4,058 baked and 4,058 unchanged field pixels over four
reflection/winding cases. Maximum RGB8 error is one. Seventeen nested
DomeD Wayland checks and 19 synthetic OpenXR checks per renderer pass
with exact native Save, Cancel, Undo and focus-loss endpoints.
Physical input and deformed retained donors remain open.
[Native retained-target Bake evidence](validation/retained-native-target-bake-20261126.json)

### Portable retained normal bakes

Product `33bd2460` exports static LDR native normal bakes with varying
authored normals to Standard and Editable GLB. The exporter transports
the original corner normals and selects a normal texture with a matching
explicit tangent frame. Prepared Core PBR fields preserve their original
image bytes when the three emitted tangent columns agree. The fallback
can allocate separate per-triangle normal coordinates and preserves
source texel alignment where the chart fits. Editable UV0, paint
coordinates, geometry and the native Scene retain their original data.

Twelve focused tests pass in each Release and ASan/UBSan profile.
Each profile checks 6,500 directions with a maximum sampled error of
0.00556 radians. A separate alternate-atlas fixture checks 5,151
directions. Standard and Editable GLB bytes agree across profiles and
pass Khronos validation with zero errors and warnings. Dome reimport
preserves the full silhouette and differs by at most one RGB8 level in
two views on software GL and Intel Arc. The exact Release application
passes 17 nested DomeD Wayland and 19 synthetic OpenXR checks.

A varying field with opposing baked directions is rejected before
writing a destination file. Cross-renderer reflected tangent handling,
per-field portable HDR, deformed source fields and full physical input
qualification remain open.
[Portable retained-normal evidence](validation/retained-smooth-glb-20261126.json)

The historical `5ff0da2b` full Release run finished with 1,307 passes
and 27 failures. Five sculpt/scene failures reproduced on export
candidate `55a5bc69`, including the dense paged sculpt baseline
exceeding its work limit. The archive preserves the full failure list
and the scoped reproductions. A complete combined regression on the
latest product remains open.
[Raw validation and regression archive](validation/retained-smooth-glb-20261126.tar.gz)

### Cold sculpt chunk construction

Product `a2b46ea7` collects canonical edge representatives in a single
cold pass and records each cell's draw-run bounds while assembling its
final triangles. The 24,001-sample ray corridor builds within the
unchanged 16-million-step default and publishes its native preview.
The candidate and the earlier kernel run with a larger diagnostic
budget produce the same position, normal, index, origin and ordered
draw-run digest, with 306,020 chunk vertices and 96,008 draw runs.
The smaller 12-million-step diagnostic budget still refuses.

The existing native paged-ray test passes all 90 cancellation,
replacement and shutdown checks. The scene, held preview, mask scalar
and controller fixtures now exercise the actual chunk owners. Sixteen
selected tests pass in each Release and ASan/UBSan profile. The separate
Arc A770 render tests cover exact pixel parity, ordered draws, mask
values and GPU-state recovery. The combined app also passes 17 nested
DomeD Wayland checks in the Bake and universal-menu smoke workflow.
The earlier 27-case failure list remains historical. These results
close the five reproduced sculpt/scene failures for this candidate.
The full latest regression and physical device acceptance remain open.
[Cold sculpt build and runtime evidence](validation/sculpt-cold-chunks-20261126.json)

### Plane cut material and cancellation recovery

Product `fadacc00` resolves the actual editable paint field on the Cut
worker. Desktop and controller Cancel both acknowledge an ordered
asset request, preserve the exact draft and source, and allow the same
preview to be applied again. The protected sculpt fixture now reads
the actual chunk mask packets and confirms that the cap stays
unmasked.

Plain texture-set cuts use the complete authenticated face partition.
The shell Bake keeps each field's color, roughness and FLOAT radiance,
including spatial brush detail on a reflected and sheared source.
The new static affine outputs have world-oriented geometry and a group
pivot at the original object origin. The hidden source retains its exact
affine placement and editable fields. Standard GLB carries the visible
shell and cap, and its native payload restores the complete Scene
including hidden fields and FLOAT emission. Both native Undo/Redo
and cold reopen reproduce the original payloads.

Twelve registered tests pass in each Release and ASan/UBSan profile.
The test matrix covers the native Cut, exact cancel/retry, colored
source admission, ordered/held publication and rendered regressions.
Twenty-one synthetic OpenXR checks pass in each software Release and
Intel Arc ASan/UBSan profile. The relinked application passes the
separate 17-check nested DomeD Wayland Bake and universal-menu smoke
test. Texture sets with incompatible material policies and retained
source domains still need their own Cut representation. A clean
latest full regression and physical device acceptance remain open.
[Plane cut and FLOAT validation](validation/cut-owning-materials-20261126.json)

### Current controller regression fixtures

The import, emission and two editable-export fixtures now exercise the
33-page controller menu and the added texture and colour-blur actions.
The anatomical binding fixture selects Surface Heat at the upper Method
endpoint and Anatomical at the preceding detent. Method selection leaves
the Scene and history intact. The following Bind, ordered Save and Undo
operate on both bundled CC0 VRoid bases.

All four menu and export cases and both 35-check anatomical replays pass
in Release and ASan/UBSan. The frozen full Release run started before
these fixture changes and still uses the earlier test sources.
[Controller fixture evidence](validation/controller-page-fixtures-20261126.json)

### GPU store retirement and sculpt projection

The shared OpenGL store ledger retains the oldest pending completion point
alongside the latest submission. The renderer observes retirement on steady
views and through continuing edits. Stores remain charged until their fence
completes. Retiring an alias can release backing storage at the same proven
epoch, while an independently used backing store keeps its later epoch. The
baseline fails the same real-buffer regression. The new ledger passes 106
software and 108 authenticated Intel Arc checks. The renderer recovery suite
passes 828 software and 831 Arc checks with actual GL faults, bounds and retry
controls. The production path uses zero-timeout fence queries.

Visible Scene projection now uses sculpt chunks alongside packed meshes for
occlusion. Opaque sculpt geometry blocks the image. Hidden and ghosted
objects, fully transparent surfaces and Target Only mode let it reach the
target. The PBR regression checks authored and PBR alpha roles through the
published scene query and gates the actual worker to check publication races.
The native probe reproduces projection through an opaque sculpt volume on
the earlier core and passes all 124 assertions with the fix.

The component matrices pass 12 renderer and seven projection tests per
Release and ASan/UBSan profile. The combined product passes eight targeted
tests per profile, 31 synthetic OpenXR checks per software and Intel Arc
profile, and 17 nested DomeD Wayland checks. The full Release sweep uses
the earlier plane-cut source. Physical-device acceptance remains open.
[GPU retirement evidence](validation/steady-gpu-retirement-20261126.json)
[Projection and combined application evidence](validation/renderer-projection-combined-20261126.json)

### Current selection and isolation regressions

The selection and isolation fixtures now check the working material-aware
ray query used by ordinary painted editing. The modal fixture samples an
actual placed sculpt chunk after Cancel, close, focus loss, tracking loss
or Confirm, then checks that the still-held trigger cannot change the scene.
The three previous fixtures reproduce their old assumptions on the frozen
plane-cut build. The updated cases pass 257 checks per Release and ASan/UBSan
profile. The same application passes 11 synthetic OpenXR checks each on
software GL and Intel Arc for direct selection, multiselect, translucent
reference meshes and the rendered hierarchy.
[Selection fixture evidence](validation/selection-fixtures-20261126.json)

### Creator VRM0 export on current main

The selected-avatar worker now reconstructs the certified creator shape
from its embedded licensed source and saved controls. Shaped CC0 VRoid
avatars export their complete skin, current pose and morph weights with
refitted first-person and spring metadata. The generated unlit groom
uses its own genesis and material identity and shares the avatar skin.
Supported UV seams preserve the original rest corner and accessor
identity while exporting the edited UVs. A common affine placement is
emitted through the hierarchy for uniform creator shapes and for shaped
avatars without spring geometry.

The earlier core refuses the same shaped groom fixture. All 11 Release
cases pass, including 460 independent shaped-avatar checks across both
CC0 bases. Nine matching ASan/UBSan cases pass at this checkpoint. The
shaped-avatar and native source-hint cases are still running in the
full sanitizer matrix. The optimized Intel Arc consumer passes 27 checks.
The combined and isolated Arc sanitizer runs stop at the canonical
preview readiness check. The software sanitizer consumer passes that check.

Native Wayland export and chooser cancellation pass ten checks on DomeD
`acc2b70a`. The exact app also reopens and saves the authoring project
byte for byte in synthetic OpenXR startup on software GL and Intel Arc.
All 32 emitted files pass the pinned VRM0 schema and Khronos glTF validation
with zero errors. Khronos reports 647 extension-name and skinned-node
hierarchy warnings. The 13 completed placement and normal-frame files
match across Release and ASan/UBSan, and the seven Arc consumer files match.

The path requires verified creator provenance, compatible material
roles and one shared rig placement. Older generated grooms without a
genesis record, arbitrary creator rest edits, nonlinear spring refitting
under general affine placement and external dynamic spring simulation
remain open. The separate full Release regression still uses the
earlier plane-cut snapshot.
[Creator VRM0 evidence](validation/creator-vrm0-current-20261126.json)

The frozen ASan/UBSan run subsequently passed all 11 cases, including
both cases still running at the Release checkpoint. All 31 emitted files
match Release byte for byte. The release viewer's seven Intel Arc
consumer files also match. The same sanitizer binary passed the
isolated five-check canonical workflow on Intel Arc and software GL
with the original readiness deadline. Two earlier Arc readiness failures
remain in the evidence. Stack samples locate the expensive work in
CPU source-image sampling inside the native paint composite.
[Completed sanitizer and format evidence](validation/creator-vrm0-strict-closure-20261126.json)

### Source texture sampling during paint preview

The CPU paint composite validates each source view once and precomputes
wrapped taps per output row and column for axis-aligned UV transforms.
Coupled transforms evaluate the same sampler at each output texel. Both
paths use exact byte-to-float decoding and the original accumulation
order. The scalar sampler keeps its original source and coordinate
validation. An independent reference checks 183,816 samples per profile
across sRGB and linear data, all wrap combinations, source filtering,
and diagonal, reflected, constant and coupled UV transforms.

The cold canonical avatar worker takes 3.68–3.93 seconds in optimized
Release on software GL, compared with 8.91–11.34 seconds on the prior
core. Two sanitizer captures on Intel Arc take 22.32 and 25.80 seconds,
both within the original 30-second deadline. The prior captures take
20.70 and 35.64 seconds. All captures reproduce the independent rendered image and
canonical VRM output. Each native profile passes ten selected source,
paint, material, cache and renderer cases. The seven normal-frame VRM
outputs match the prior files byte for byte. Nested DomeD Wayland passes
ten selected-avatar checks and emits the prior exact VRM. The synthetic
OpenXR startup and exact Save pass on software GL and Intel Arc.
[Source sampling validation](validation/paint-source-grid-20261126.json)

### Current native paint resolution

The universal Paint menu opens the FLTK resolution picker for an original
source mesh. The XR Material Texture Sets page uses the same detached
worker. Choosing the current size preserves the existing raster. Enlargement
copies the stored planes and skips empty tile footprints. Reduction measures
common paint, masks, alternate PBR color, and FLOAT/legacy emission. It also
compares the visible overlay over common footprints while honoring padding.
An over-budget or out-of-tolerance request leaves the source Scene intact.

Both verified CC0 VRoid faces go from 128 to 256 and back to 128 with every
tested authored color, mask and hidden emission byte recovered. Both empty
faces resize to 2048. The desktop worker enlarges the sparse painted face to
1024. A separate ordinary native mesh preserves hidden FLOAT emission with
sub-UNORM8 coverage. Release and ASan/UBSan with leak detection each pass
three suites, and all seven native project files match byte for byte.
Nested DomeD Wayland passes 12 checks through the actual FLTK dialog and
universal menu. The OpenXR loader passes 18 synthetic-controller checks each
on software GL and Intel Arc, including the rendered dialog, Save, Undo and
Redo. All three runtime paths write the same 256-pixel project bytes.

Prepared and baked sampling domains, raw linked-source HDR authoring,
accelerated painted resampling and physical headset checks remain required.
The fresh combined Release build uses frozen `d9ea0716` in
`artifacts/native-grid-combined-source-20261126`. Follow the user service
`ds-native-grid-combined-release-20261126.service` and its status in
`artifacts/native-grid-combined-release-20261126/status.json`.
[Current resize evidence](validation/native-authored-grid-20261126.json)

### Matching topology Bake

The FLTK Bake dialog and XR Projection Policy panel expose an explicit
matching-triangles mode. A source and target must share ordered vertices
and triangle indices. The bake interpolates the corresponding source point
for each covered target texel and tests signed front/back and total
displacement in the existing projection metric. Partial mode keeps previous
bake samples where the new source misses. The existing backface and AO
paths apply and the sampled material still follows the true source triangle.
The worker uses the existing normal, color, material and FLOAT emission
resolvers. The FLTK dialog disables the cage in matching mode. Explicitly
choosing a cage in XR returns to ray projection.

Matching recipes use recipe v2 and the ordinary ray recipes retain their
v1 encoding. Native fixtures cover the whole target of a displaced source
that the ray mode partially misses, a measured normal gradient, FLOAT
radiance, AO ray counts, preserved misses, wrong index order, cage conflict,
detached cancellation, exact Save and history. Both original CC0 VRoid
FaceSkin probes use real displaced detail with prepared normals and a
common scaled group. Their 256-pixel grids record 50,680 and 49,931
projected samples and zero projection rays. Both projects match byte for
byte between Release and ASan/UBSan with leak detection. The eight focused
native suites pass in each profile. Nested DomeD Wayland covers keyboard
selection, Cancel, Bake, Recall, Save, Undo and Redo. Both
synthetic OpenXR profiles exercise the rendered policy and use the same
native worker. All three frontend runs save the same bytes. Physical
headset verification, the complete current regressions and broader
integration work remain open.
[Matching Bake evidence](validation/matching-bake-20261126.json)

The older plane-cut Release job on `fadacc00` has completed with 1,318
passes, 18 failures and zero skips. Its evidence retains the original
test outputs and candidate identity. On the current matching Bake
product, emission controls, editable XR export and native MToon HDR
pass the unchanged tests. The polygon archive diagnostic and the previous
prepared-normal and VRM1 expression guard fixtures still fail their
assertions. The separate combined Release build on frozen `d9ea0716`
remains active.
[Full plane-cut Release results](validation/full-release-fadacc00-20261126.json)

### Retained archive depth and normal authoring regressions

The retained ownership walker memoizes each validated owner's longest
descendant path. Revisiting the owner under a deeper route charges that
remaining depth against the existing limit. An independent probe uses two
objects with shared polygon history. The earlier core accepts the graph
after visiting the shorter route. The revised core reports the depth refusal.
The archive tests also exercise an in-budget shared graph, an actual cycle
and byte-identical preservation of an existing project after refused Save.

The prepared-normal regression now uses a genuinely prepared donor with
a clear target. Matching topology Bake reproduces the non-neutral normal
field within two RGB8 levels, retains exact source and frame owners, and
continues through the real UV editor. The VRM1 regression explicitly removes
base and morph tangents for the generated-frame refusal, and uses a separate
real authored-tangent asset for the rotated normal UV check. It samples
independent core and MToon transform coefficients and tests exact native
archive replay. Static Apply retains fixed editable paint. The mixed moving
upper-role expression remains guarded during dynamic export. All seven
selected suites pass in Release and ASan/UBSan with leak detection, with
five byte-identical output files across the two profiles. Intel Arc passes
39 normal consumer assertions. Across nine pose and morph combinations
the independent renderer agrees within one RGB8 level.
[Retained archive and normal evidence](validation/retained-depth-authoring-20261126.json)

### Native affine Bake donors

Product `6f11e65e` uses the ordinary Bake chooser for hidden affine native mesh
and sculpt donors. The reflected, posed retopology fixture transfers sparse
color and physical FLOAT emission onto the applied patch at 2 mm projection
bounds, with exact source retention, ordered Save, reopen and Undo/Redo.
Native tangent-map transfer preserves source winding and reflected handedness.
The translated sculpt at 2 m projected all 4,096 texels after recentering the
projection workspace before float conversion. The unrecentered fixture and
independent mesh probe preserve the earlier degenerate-triangle failure.

The exact build passes eight native CTests in both Release and ASan/UBSan with
leak detection. Release also passes the registered affine OpenXR retopology
test with 27 checks after allowing the full controller material edit to finish.
The production app passes ten nested DomeD Wayland checks and ten synthetic
OpenXR Bake checks each in software Release and Intel Arc ASan/UBSan. Native
Release and sanitizer patch outputs match byte for byte. The two XR outputs
also match each other. Their controller settings differ from the desktop test.
Original and prepared affine material domains and physical headset use remain
open. [Native affine Bake evidence](validation/affine-native-bake-20261126.json)

The older `d9ea0716` combined suite remains live. Its first observed failure
is the allocation partition `canonical-paint37-key-replace-fault-part-86`.
The native child completed all 345 prefixes and exited successfully. The outer
ELF observer captured Python mappings during child startup. The shared observer
now checks executable identity before and after sampling. A gated child verifies
the Python-to-native transition. With the original allocation binary and
fixture, partition 86 passes on a fresh plan with the exact original native
result. The original wrapper failure remains in the older suite log.

### Affine authored normal sources

Product `71f60afa` applies the same normal and tangent placement as the
viewport to each evaluated source corner. It keeps separate original UV roles,
inverse-transpose normals, linear tangents and reflected handedness. Initial
prepared normal planes use the existing source decoder in that placed basis.
Imported source skin positions reuse the authenticated original joint graph and
enter the centered Bake metric before float conversion.

Fixture `d087d27d` verifies a real imported authored VRM1, independently posed
VRM0 and VRM1, and initial prepared normal donors. The independent samplers
observe both normal roles. Three native matching outputs are byte-identical
between Release and ASan/UBSan with leak detection. The exact original skin
test distinguishes centered double evaluation from shifting pre-rounded world
positions. The focused replay retains exact source ownership and ordered
Save, reopen, Undo and Redo. On the earlier fixture, strict ray projection of
coincident edges missed 87 texels, while matching triangles covered all 3,969.
The inset ray fixture projected all 3,969 through the public controls.

Advanced retained UV and polygon normal domains and physical headset
verification remain open.
[Affine artist Bake and allocation observer evidence](validation/affine-artist-bake-20261126.json)

### Coincident Bake rays

Product `c9929606` first performs the existing exact ray query. For a miss, it
checks real source edges against the same finite depth segment and a radius
derived from float spacing at the target before and after recentering. The
query keeps source corner weights and shares the ordinary triangle work limit.
Exact hits, AO rays, backface policy, and partial-miss coverage keep their
existing behavior.

The original imported VRM1 chart had 87 ray misses at texel centers on
coincident borders. Every observed miss passed within about 0.11 micrometres
of a source edge. The revised build projects all 3,969 texels, with both normal
planes matching the corresponding-triangle route to within two byte levels.
The tests cover actual deeper-hit precedence, a genuine lateral gap, and
out-of-range front and back endpoints. Release, ASan/UBSan, nested DomeD
Wayland and software and Intel Arc synthetic OpenXR exercised the fix.

The fresh all-target Release build on frozen `c9929606` is running in
`ds-coincident-bake-ray-full-release-20261126.service`. Its predecessor on
`d9ea0716` completed with 1,330 passes, eight failures and one aggregate case
skipped because its allocation-partition dependency failed. Focused follow-ups
cover all eight failures. The archive preserves the original results and the
separately verified corrections.
[Coincident ray evidence](validation/coincident-bake-ray-20261126.json)
[Completed older Release sweep](validation/full-release-native-grid-20261126.json)

### VRM0 export regressions on main

Commit `a5a54e91` creates modified VRM0 fixtures before import, so the normal
importer assigns their material, rig and source identities. The tests preserve
the bundled creator hash check and its refusal when embedded bytes change.
The selected-avatar worker exports a painted derivative while ordered Save
retains the exact editable scene. A missing derived normal frame still refuses
export and preserves an existing file.

The two CC0 bases pass 179 checks each in Release and ASan/UBSan with leak
detection. Independent vertex-color and MToon consumers pass 63 checks in
each of software Release, software sanitizer and Intel Arc A770 Release,
with at most one RGB8 level of difference. All 15 consumer VRM files agree
byte for byte across profiles. Both selected-avatar exports and both
ordered native saves also agree between Release and sanitizer runs.
The integrated all-case sweep continues.
[VRM0 export evidence](validation/vrm-derived-source-20261126.json)

### Bake from UV-edited prepared normal sources

Bake now accepts a prepared normal source after either Rasterize or Retain
Source. Rasterize samples the edited atlas. Retain Source samples the saved
PBR and MToon coordinates with their paired normal detail. Both paths use the
placed, posed corner frames. The worker checks the complete UV replay graph
against its existing quota and pairs the resolver with the current normal,
material and sparse paint owners before projection.

Release passes 16 tests and ASan/UBSan with leak detection passes 15. Each
profile passes 263,498 advanced checks, including 17 normal and material
cases, four reflection/shear cases, and rejection of missing, foreign or
stale authority. Across the sampled cases, the maximum second-Bake normal
component error is 0.004875 against the 0.035 limit. All 39 native project
files match byte for byte across the two native profiles. Nested DomeD
Wayland, software synthetic OpenXR and synthetic OpenXR on Intel Arc A770
each pass 20 public action checks. All three routes save the same accepted
project bytes for each UV method.

The all-target build on frozen `afd3d40c` runs in
`ds-uv-edited-normal-bake-full-release-20261126.service`. Its full test run
queues behind the already-running `c9929606` sweep.
[UV-edited normal Bake evidence](validation/uv-edited-normal-bake-20261126.json)

### Bake from a converted native normal field

The shared Bake worker now accepts a mesh with a converted native normal
field, including sources with retained normal UV correspondence and current
skin or shapekey changes. It samples the two stored normal planes in their
retained coordinate domain. After Clear Bake it evaluates the live intrinsic
normal. Paint, material and FLOAT emission keep their own sampling data.
The worker pairs the normal resolver with its exact material snapshot.
Bound polygon texture-set fields keep their existing separate resolver.

All 17 focused Release tests and 16 ASan/UBSan tests with leak detection
pass. The 32-case native matrix and the affine and rejection controls pass
525,112 checks per profile, with maximum sampled rebake
normal error 0.004809 against the existing 0.035 bound. The fixture checks
exact native Save/reopen and Undo/Redo, and transfers painted FLOAT emission
above 7.5. All 67 native fixture projects match byte for byte between the
two profiles. The dependent Save regressions hold the real Bake until Save
has queued, then exercise the original failure. The prepared-source fixture
passes 20 runs and 40 dependent Save refusals with exact prior bytes.
Nested DomeD Wayland and synthetic OpenXR on software GL pass 20 public
checks each. Intel Arc A770 synthetic OpenXR passes 20 checks each with the
Release and sanitizer applications. Each route saves identical project bytes
for a given field state.
[Canonical Bake source evidence](validation/canonical-donor-current-20261126.json)

### Bake from independent writable paint grids

Native and canonical source meshes remain eligible after UV edits that keep
their writable paint coordinates. Bake maps each source hit through its
owning paint corners for colour, material and FLOAT emission. Normal fields
and original texture roles keep their own coordinates. The worker binds the
exact immutable material snapshot and the paint correspondence to the source
resolver. This also admits the tested prepared polygon source with separate
PBR colour and normal domains.

The 64 native and canonical cases pass 619,644 checks. The eight combined
colour and normal cases and the LDR and HDR app-ready cases pass 346,313
checks. Maximum sampled rebake normal error is 0.004871 against the existing
0.035 bound. The cases include current poses and keys, reflected and sheared
placement, HDR above 7.9, and exact ordered Save, reopen and Undo/Redo. The
19-test focused Release suite and the 18-test ASan/UBSan suite with leak
detection pass. Both profiles pass the same native checks. The 130 native
and canonical projects match byte for byte. The 18 joint projects differ
only in a generated import group name containing the fixture process ID.
Nested DomeD Wayland, software OpenXR and Intel Arc A770 OpenXR with the
Release application each pass 30 public checks. All three routes save
identical project bytes for each tested source family.
Intel Arc with the sanitizer application also passes 30 public checks and
saves the same accepted project bytes.
[Independent writable source evidence](validation/independent-writable-donor-20261126.json)

### Bake into canonical meshes with retained writable UVs

The shared Bake worker uses the target's retained writable UVs for colour,
material, AO and FLOAT emission. It projects PBR and MToon normals in their
separate normal field. The normal pass preserves the FLOAT emission
representation through validation. Before the fix, a low-dynamic-range Bake
used the display coordinates and a FLOAT emission donor could fail with
`Invalid texture image`.

Three cases distinguish the display, paint and normal coordinates, including a
seam split and a partial HDR projection. The complete material and normal
maps match the independent writable-atlas control. Matching-triangle cases
project 2,304 texels with zero rays. Partial HDR projects 1,152 texels and
preserves all 1,152 missed texels. Existing layers and sampling owners stay
attached. Dependent Save, reopen, Clear Bake and exact Undo/Redo pass.

The focused Release and ASan/UBSan cohorts each pass ten tests, including 77
recipient checks. Nine native fixture projects match byte for byte. Nested
DomeD Wayland and synthetic OpenXR on software GL and Intel Arc A770 each pass
ten checks and write the same accepted bytes. The Arc run uses the sanitizer
application. Physical headset qualification remains open.
[Canonical writable recipient evidence](validation/canonical-writable-recipient-20261126.json)

### Bake into plain native meshes with retained writable UVs

The shared Bake worker now uses the retained paint atlas and its Mikk tangent
basis on supported plain native meshes. Colour, material, AO, normals and
FLOAT emission remain writable across later display-UV changes and reopening.
The viewport and subsequent Bake jobs read the same retained coordinates.
Clear and Undo refresh the viewport's corner layout. The FLTK and XR controls
keep the established basis for this target.

The LDR and HDR matching-triangle fixtures each project 4,096 texels with
zero rays and match the original-atlas control. Both profiles pass 60 native
recipient checks, including ordered Save, exact Undo/Redo, Clear, later UV
seam splitting, rebake-source sampling and standard and editable GLB export.
All eight native projects and four exported GLBs match byte for byte between
Release and ASan/UBSan. The focused suite passes 11 tests per profile.

Software GL and Intel Arc A770 each pass 13 framebuffer and independent GLB
consumer checks. The retained Bake matches the original-atlas framebuffer at
rest and with mixed skin weights under a nonuniform pose. Standard and editable
LDR GLB agree within one RGB8 level. The independent Khronos validator reports
zero errors on all four files and one skinned-node parent warning per file.
Nested DomeD Wayland and synthetic OpenXR on software and Intel Arc each save
identical accepted project bytes. Wayland passes 10 checks including explicit
dialog cancellation. The two synthetic OpenXR routes each pass 11 checks
including focus-loss cancellation. The original XR test sequence sent a
second Bake command after closing the settings panel and is preserved.
Physical headset qualification remains open.
[Native writable recipient evidence](validation/native-writable-recipient-20261126.json)

### Bake into native meshes with authored shapekey frames

The shared Bake worker now keeps normals in the native shapekey's authored
frame while using the retained paint atlas for all baked channels. The
viewport, Bake donor resolver and GLB exporter use the same frame and atlas.
The native record identifies the authored normal interpretation and checks its
owning paint and frame on reopen. Standard and Editable GLB keep the existing
position, normal and tangent morph channels.

Nine focused tests pass in both Release and ASan/UBSan. The 49-check native
fixture projects 4,096 texels in both the LDR and HDR matching-triangle cases.
Both partial rebakes preserve all 2,048 misses. Ordered Save, Clear Bake,
exact Undo/Redo, a later UV seam split, an additional key and rebake-source
sampling pass. All eight native files and four GLBs match byte for byte
between the two native profiles.

Software GL and Intel Arc A770 each pass 45 graphical checks. An independent
GLB consumer agrees within one RGB8 level before and after UV edits with active
keys and mixed skin weights. The wrong tangent control differs by 11 levels.
The first graphics fixture used a displacement too small to test the visible
key change. The final replay uses a larger edit through the native authoring
command. Khronos reports zero errors and one skinned-node parent warning per
file for the four native fixture exports. Nested DomeD Wayland passes 10
checks. Software and Intel Arc synthetic OpenXR each pass 11, including focus
cancellation. All three save the same accepted bytes.

[Authored native recipient evidence](validation/native-key-writable-bake-20261126.json)

### Convert a Bake that predates the first shapekey

Use **Mesh → Convert to portable normals** in the universal menu or **Portable
normals** in the XR Bake source panel. The worker uses the original writable
paint coordinates and the sealed shapekey frame. It re-encodes the raw normals
and checks both decoders at rest across covered texels and filtered cells,
including chart borders and partial coverage. The command reports the maximum
sampled rest error. Posed and morphed shading follows the authored frame.

Color, AO/material, both emission representations, coverage and sparse layers
retain their values. The command retains the original rig, geometry, UV, key
and Bake recipe owners. Ordered Save stores the converted project. Undo restores
the original Bake bytes, and Redo restores the converted project. Failed quality
checks and cancellation preserve the existing project. Standard and Editable
GLB can export the converted project with position, normal and tangent targets.
The new frame also updates live GPU geometry identity when the packed layout
changes. The first Wayland replay exposed this stale identity during live
conversion. The corrected replay renders and saves successfully across
Convert, Undo and Redo in the same application process.

Release and ASan/UBSan each pass ten CTests and 50 focused checks. The partial
FLOAT-emission case measures 0.318 degrees of sampled rest error. The six native
projects and four GLBs match byte for byte between build profiles. Software GL
and Intel Arc A770 pass 43 graphical checks each. The converted rest view and
independent GLB views agree with their controls within one RGB8 level, including
a live edited pose and later UV seam edit. The four Khronos reports each show
zero errors and one skinned-node parent warning. Nested DomeD Wayland passes
eight checks. Synthetic OpenXR on software GL and strict Intel Arc each pass
11, including focus-loss admission. The three frontends save identical bytes.

An independent 1024-square fixture converts all 1,048,576 texels. At 2048 the
ordinary UV worker's fixed-work quota rejects the predecessor. The following
probe raises only that existing API limit and reaches the scene paint-map quota.
With the hidden donor removed after the successful 4,194,304-texel Bake,
the scene paint-storage quota still stops the workflow before conversion.
Full-resolution retained authoring, affine-placed conversion targets and physical
headset verification remain open.
[Legacy Bake conversion evidence](validation/legacy-key-bake-conversion-20261126.json)

### Keep a full-resolution writable atlas through UV editing and Bake

Native Keep paint detail now shares the immutable raster when its ordered
source corners still match. The display mesh and its sampling attachment retain
their own owners. The original raster supplies the texel positions, barycentric
coordinates, padding and paint-picking bounds. Native source replay uses the same
proof under the existing limits. The largest allocation in the 1024 fixture
fell from 40 MiB to 16 KiB. At 2048 the previous build refused the UV edit and
the current build's largest allocation is 64 KiB for the 2048 fixture.

The actual Document workflow preserves sparse paint through a 2048 UV transform,
projects all 4,194,304 texels with matching source triangles and AO disabled,
and preserves the new display UVs and bake recipe through ordered Save, Undo,
Redo and cold reopen. Release and ASan/UBSan each pass 11 focused CTests and 13
full-resolution checks. Five further regressions cover paint sampling,
allocation faults, rig/key UV edits and Source8 rendering. The outdated
transition fixture assumed both native Bake directions would refuse. Its
corrected replay passes 12 checks, including refusal of a fabricated authored
normal interpretation without the genuine frame.

Nested DomeD Wayland passes nine checks. Software and Intel Arc synthetic
OpenXR each pass ten, including focus cancellation. The accepted files match
the native workflow byte for byte. The successful frontend inputs retain a
genuine ray-Bake recipe with AO disabled. Separate probes confirm that the
default 16 AO samples can hit the ray quota at 2048 and the first key on a
fully baked 2048 target hits the Scene paint-map quota. Preserve the complete
history while addressing those limits. Physical headset verification remains open.
[Full-resolution writable atlas evidence](validation/native-writable-raster-2048-20261126.json)

### Default ambient occlusion at 2048

The shared Bake worker budgets projection and AO against the actual target
resolution and pass count. It reuses the deterministic hemisphere sample
pattern across texels. A second pass that writes the independent normal atlas
keeps both normal planes and performs only the required projection queries.
The low-level baker still enforces explicit caller limits and the existing
absolute cap.

At 2048, ordinary projection traces 71,303,168 rays and matching triangles
trace 67,108,864. Both use all 16 default AO samples on all 4,194,304 texels.
The independent wall fixture verifies actual darkening near an occluder and
unoccluded distant texels. Save, Undo/Redo and cold reopen preserve the complete
project and recipe. Release and ASan/UBSan pass all 12 focused CTests and save
byte-identical projects. AO, ORM and normal bytes match the previous kernel
at seven sample counts from 0 through 256. The independent normal pass fixture
confirms that both normal planes stay byte-identical when AO is enabled and
the ray count rises by exactly 16 per projected material texel. Nested DomeD
Wayland and synthetic OpenXR on software GL and Intel Arc save the same matched
Bake using the default settings without a previously saved recipe.

[Default AO evidence](validation/default-ao-bake-2048-20261126.json)

### First shapekey after a full-resolution native Bake

The native material frame and key derivation now share the exact finished
paint record when their geometry, Bake, layers and revisions match. Native
reopen replays the declared key geometry before preserving that identity.
Strict linked paint also shares immutable raster samples within one Scene
read. Each current geometry, material attachment and paint payload still
has its own validation. An altered REST corner is rejected even when it
points to the same raster. The native archive schema and its byte and
texel limits are unchanged.

The 2048-square LDR worker fixture creates and saves the first key over the
full Bake, reopens the saved project, edits the key, converts its normals
and exports Standard GLB. A fresh reopen followed by Shape Restore reproduces
the entire original project file. The saved first-key archive is 214,323,981
bytes. Release and ASan/UBSan each pass 12 focused tests and eight additional
tests covering UV, texture sets, raster replay and fault recovery. Both profiles
produce byte-identical projects and the same Standard GLB. A separate Release
probe creates and saves a first key on the previous fixture with all 16 default
AO samples. Nested DomeD Wayland and synthetic OpenXR on software GL and
Intel Arc each pass 10 checks, including cancellation. The first Arc observer
looked for the Release comparison fixture in the sanitizer output directory.
The corrected observer passes with the same application and runtime.

The full Release regression for `ed491ddf` finished with 1,335 passes,
two failures and 13 dependency skips. The failed cases are
`layered-normal-native` and `retopology-worker-accepted-publication`.
The terminal result and raw JUnit are under
`artifacts/first-key-2048-full-release-20261126/`. This run covers that
earlier snapshot.

Standard GLB passes Khronos validation with zero errors and a skinned-node
parent warning. The first-key checkpoint's default Editable GLB worker reached
its cumulative sample budget at this resolution. Its diagnostic run with a
larger explicit limit recovered a byte-identical native project.
[First shapekey evidence](validation/native-writable-first-key-2048-20261126.json)

### Editable GLB from the retained 2048 atlas

CPU composition now visits occupied visible tiles in original layer order.
The plain LDR retained export preflight charges the actual padded texel
references, coverage scans, and normal-plane pass under the existing limits.
The normal pass reads the baked directions directly. A tighter caller budget
still refuses before publishing a replacement file.

The default worker export of the converted 2048-square Bake and shapekey
matches the previous raised-budget diagnostic GLB byte for byte. Its native
payload reopens to the exact 214,324,184-byte project. The portable core view
uses the four retained 2048-square maps, including when the general export
dialog shows its default 512 setting. Khronos reports zero errors and one
skinned-node parent warning.

Release and ASan/UBSan each pass 13 focused tests and eight additional tests
covering source paint, UVs, texture sets and repeated package export. A
14-layer scene with overlapping sparse colour, roughness, metal and emission
layers produces the same Editable GLB as the previous compositor with a
higher diagnostic limit. The Wayland export and the synthetic OpenXR
controller export use the same bytes. The first software OpenXR test stub
omitted the active flag on the joystick click. Its corrected input replay
uses the same application binary.

A follow-up probe used a genuine physical-emission donor with full
2048-square coverage. Bake rejects the prospective 164 MiB of dense FLOAT
planes and other retained channels at the existing 128 MiB per-paint limit.
The probe reaches that refusal before the first key or portable HDR export.
Physical headset verification remains open.
[Editable GLB and physical Bake probe evidence](validation/native-editable-glb-2048-20261126.json)

### Lossless FLOAT Bake at 2048

The physical Bake worker now stages resolved RGB and reuses the projection
coverage states. When the original dense FLOAT layout exceeds its paint
allowance, the field stores bit-identical shader coefficients using shared
component constants, varying FLOAT values and exact binary coverage words.
Version 1 remains the wire format for fields that fit the dense path.
Decode validates the channel layout, values and retained size under the
existing Scene41/42 authority. OpenGL uploads the packed words and reads
the constants in the shader without expanding the field on the host.

The exact full-resolution matching Bake fixture keeps the radiance,
coverage, editable paint and coordinates through ordered Save, first key,
Undo/Redo, a cold reopen and byte-identical Shape Restore. The original
Bake uses 54,525,984 retained bytes and the keyed project uses 163,992,646
archive bytes. The independent varying-field oracle checks every sample in
two 2048-square grids. It retains 50,331,680 bytes with constant coverage
and 50,855,968 with binary partial coverage.

The full application and selected fixtures build cleanly in Release and
ASan/UBSan, with all 13 focused CTests passing in each profile. Both produce
identical native HDR files. Nested DomeD Wayland and synthetic OpenXR on software GL and Intel Arc pass
10 checks each. The Arc workflow passes in Release and ASan/UBSan. The
first Release Arc observer loaded the sanitizer SDK and stopped during
startup. The corrected observer loads the Release SDK. Actual OpenGL
contribution checks compare dense, packed and binary-coverage fields against
an independent oracle on software GL and Arc in both build profiles.

A separate spatially varying physical donor produces a 104,857,632-byte
Bake. The subsequent first-key command reaches the aggregate Scene paint
limit because the archive still charges the Bake in several paint owners.
The constant-radiance key converts its normals, then Standard HDR GLB
reaches its aggregate extraction limit when staging ten floats per texel.
Both limits reproduced against that Release build. The following checkpoint
closes the native shapekey failure. Physical headset verification remains open.
[Physical FLOAT Bake evidence](validation/native-hdr-compact-bake-2048-20261126.json)

### Shared full-resolution HDR Bake through shapekey history

Scene44 serializes a qualifying packed FLOAT Bake once when distinct native
paint leaves share it. Each Baked7 leaf retains its own mesh, layers and
sampling-domain reference. The reader checks the pool and every reference
before it shares the immutable Bake. The per-paint and aggregate Scene
budgets still apply, and unique Bakes keep the existing inline format.

The varying 2048-square donor reaches first shapekey and saves a
105,444,452-byte project. The live mesh, key frame and Restore state share
the decoded Bake. Shape Restore recovers the original project byte for byte.
The constant case now saves its keyed project in 54,940,517 bytes. Both
Release and ASan/UBSan pass 14 focused CTests, reject the foreign-domain
control, and write identical native projects. Nested DomeD Wayland and
synthetic OpenXR on software GL and Intel Arc pass 10 frontend checks each.
A separate ordered edit, normal conversion and cold reopen on the varying
fixture preserve the exact FLOAT field. The following checkpoint closes the
Standard HDR GLB staging limit for this fixture. Full integrated regression
and physical headset checks remain open.
[Shared HDR Bake validation](validation/native-shared-hdr-bake-20261127.json)

### Bounded Standard HDR GLB at 2048

Native HDR package5 reads the owning immutable paint raster in place and
encodes ten exact FLOAT32 OpenEXR channels in ZIP scanline blocks. The
writer evaluates the same texels for the portable PNG and measured
emissive strength. The native reader checks actual FLOAT samples, the
portable fallback metrics and the emitted paint coordinates where needed.
Existing package versions keep their original aggregate dense preflight.
Package5 bounds sample work, scanline scratch, encoded EXR and complete
container storage independently.

The varying 2048-square Bake with an edited shapekey and converted normal
frame writes a 51,266,256-byte Standard GLB. Release, ASan/UBSan, nested
DomeD Wayland and synthetic OpenXR on software GL and Intel Arc produce
identical bytes. The controller checks include focus-loss cancellation.
The focused native fixture reopens
the exact project under a 96 MiB image working budget and checks the
encoded EXR quota. The Khronos validator reports zero errors and one
warning about the skinned mesh's position in the node hierarchy.
The Editable GLB reached a separate non-emission sample limit in that build.
A separate full Release run builds the
exact product and waits for the earlier sweep before running the full suite.
[Bounded Standard HDR GLB validation](validation/native-hdr-standard-streaming-20261127.json)

### Editable GLB for varying full-resolution HDR Bake

Editable export budgets the actual occupied tiles in the non-emission
composite, including the transient byte Bake copy. Its HDR fallback samples
the existing padding, baked data and visible sparse layers directly. Each
tap evaluates only the original emission RGB contributions and coverage
before filtering. The colour, normal and ORM paths retain their own existing
compilers.

The 256, 512 and 1024 portable texture presets preserve the exact native
project from the edited, varying 2048-square Bake. Release and ASan/UBSan
write identical packages. The focused native test compares the emission
sampler against the existing five-plane sampler at padded, interior and
filtered samples. It also verifies that a tighter cumulative budget leaves an
existing output intact. The LDR fixture remains byte-identical to its
previous result.

The default 512 package matches the nested DomeD Wayland and synthetic
OpenXR exports on software GL and Intel Arc. All three presets pass the
Khronos validator with zero errors and one skinned-node warning each.
The visible retained texture-set follow-up has two actual FLOAT-painted
material members. Its Standard worker stops at surface extraction because it
needs the owning subset adapter. Its Editable worker reaches that adapter
and stops at the per-material precision check. The release fixture and
exact refusals are identified in the receipt. Physical headset checks and
the full integrated regression remain open.
[Editable HDR GLB validation](validation/native-hdr-editable-2048-20261127.json)

### Editable HDR per texture-set material

The Editable worker now authenticates the native texture-set plan and
compiles physical emission for each emitted PBR primitive. The finite
PNG and emissive strength use the existing writable-grid coordinates
and measured texel error. The native package preserves all exact
materials, membership, FLOAT layers, geometry and history.

Release and ASan/UBSan write identical files for the original retained
fixture and the distinct-radiance plain and retained fixtures. The
retained test reads the actual emitted corner accessors and PNG channels.
The plain case keeps one mesh, shared skin and morph data. Nested DomeD
Wayland and synthetic OpenXR on software GL and Intel Arc match the
native export byte for byte. Khronos reports zero errors for all three
files and one skinned-node warning on the plain fixture.
The following checkpoint adds exact Standard GLB per-field FLOAT packaging.
[Texture-set Editable HDR validation](validation/native-texture-set-hdr-editable-20261127.json)

### Standard GLB with exact HDR per texture-set material

Package6 maps separate ten-channel FLOAT OpenEXR resources to the actual
texture-set primitives. The shared PBR compiler supplies each portable
PNG, emissive strength and measured texel error. Native reopen checks the
primitive membership, paint coordinates, FLOAT coefficients and portable
fallback against the preserved native Scene. The tests reject stale
material plans and rehashed files with wrong coordinates, swapped FLOAT
resources or omitted visible materials.

Release, ASan/UBSan, nested DomeD Wayland and synthetic OpenXR on software
GL and Intel Arc write identical files for the visible two-field fixture
at 64-square per field.
Native tests also cover distinct plain and retained HDR fields, and a
texture-set mesh beside standalone HDR. Khronos reports zero errors for
all four files and one skinned-node warning on each of the two skinned
fixtures. The corresponding Editable output remains byte-identical.
Typed MToon sets, alternate source emission coordinates, texture-set HDR
VRM, production-resolution multi-field budgeting, physical headset checks
and full integrated regression remain open.
[Texture-set Standard HDR validation](validation/native-texture-set-hdr-standard-20261127.json)

### Production-size writable HDR texture sets

Product `81231961` opens and paints two independent 1024-square HDR PBR
fields under the default worker limits. The prior application refused the
project while forecasting a dense preview. The worker now charges the
retained grids, sparse paint directories and tiles that can affect alpha.
Desktop painting uses the same authenticated sparse packet for picking.
Cold Open and an accepted desktop stroke avoid dense CPU image generation
and preserve ordered Save and exact Undo/Redo.

Standard and Editable GLB keep each field's full-size portable images.
On plain unbaked fields the compiler resolves the ordered paint only where
the actual padded raster references an occupied tile. Empty regions reuse
their material baseline. Release and ASan/UBSan produce identical files
and exact native round trips for 1024+1024 and 2048+1024. The latter probe
raises the fixture creation work limit to construct the initial 2048 grid.
Open, preview and both exporters use the default limits. The registered
1024 fixture uses default limits throughout. It verifies every albedo,
alpha and ORM byte against the original compositor and every FLOAT EXR
channel bit against the dense sampler. Existing low-resolution GLBs
remain byte-identical, and the retained HDR controls pass separately.

Nested DomeD Wayland and synthetic OpenXR on software GL and Intel Arc
match the 1024+1024 Standard GLB. Khronos reports zero errors for all six
high-resolution outputs and the existing skinned-node warning on the two
with a rig. Production-resolution baked and retained sets, typed MToon,
alternate emission coordinates, HDR VRM, full integrated regression and
physical headset verification remain open.
[Production texture-set validation](validation/native-texture-set-hdr-production-20261127.json)

### Production-size baked native HDR texture sets

Product `9dcf0355` applies the existing exact FLOAT storage layouts when
baking each material member. The generator chooses the smaller layout and
keeps the dense form when all channels vary. An accepted rebake uses the
same selection. On the matching-triangle test, paint storage falls from
43,278,336 to 26,632,224 bytes per 1024-square member. Both members then
fit the existing Scene40 archive cap. A previously written dense project
still reproduces the same native Save and both GLB files byte for byte.
The first legacy replay wrapper used one basename for its executable and
output directory. The corrected wrapper ran the byte-identical executable.

The preview and portable exporter compile albedo, ORM and canonical
normal from the same exact padded grid. The renderer keeps the existing
GPU FLOAT emission packet. Two independently baked 1024-square fields
pass cold Open, rebake, exact Undo/Redo, both GLB formats and exact native
reopen. The test compares all material and normal bytes with the original
compositor and all exact EXR channel bits with the dense sampler.

Nested DomeD Wayland and synthetic OpenXR on software GL and Intel Arc
match the native 1024+1024 Standard GLB. The 512+512, 1024+1024 and rebake
Standard and Editable files pass Khronos validation with zero errors and
warnings.
A prepared retained-source 1024+1024 probe reaches the preview work cap
and the two export work caps. Its 512+512 control passes. Large retained
fields, typed MToon, alternate emission coordinates, HDR VRM, full
integrated regression and physical headset verification remain open.
[Baked texture-set validation](validation/native-texture-set-hdr-baked-production-20261127.json)

### Production-size retained-source HDR texture sets

Two independent prepared-source 1024-square members now cold-open, paint
through the desktop worker and export in Standard and Editable GLB. The
preview and exporter use the actual original padded raster for material
filtering. Work accounting charges both material grids, regional paint
and normal copies, and the shared original raster once. The compiler
reuses Bake samples only when their coefficients are constant. It keeps
the unpainted background separate and counts both in the measured
portable emission fallback. The source membership and distinct paint
and normal coordinates remain intact.

Release and ASan/UBSan compare every preview material and normal byte
against the original compositor and every Standard EXR coefficient
against the dense per-texel sampler. The native workflow verifies desktop
paint with exact Undo/Redo, independent normal accuracy, exact Save and
GLB reopen, and a smaller work limit that still refuses the export.
The older 512-square retained source and both exports reproduce the
original bytes. The two new 1024 cases are registered in CTest.

Nested DomeD Wayland and synthetic OpenXR on software GL and Intel Arc
match the native Standard package byte for byte. All six checked GLBs
pass Khronos validation with zero errors and warnings. Larger retained fields,
production-resolution retained rebaking, typed MToon, alternate source
emission coordinates, HDR VRM, full integrated regression and physical
headset acceptance remain open.
[Retained texture-set validation](validation/native-texture-set-hdr-retained-production-20261127.json)

### Layered-normal Bake and retopology regression follow-up

The earlier full Release sweep on `ed491ddf` finished with 1,335 passes,
two failures and 13 dependency skips. The layered-normal producer still
expected the independent Bake rejection that preceded authenticated source
frame transport. It now verifies the paired material and normal capture,
refuses a missing capture, then uses the actual Document worker to bake the
converted donor onto a separate native mesh. It verifies donor ownership,
direction at each covered interior texel, ordered Save and cold reopen,
and exact Undo/Redo. Software Release, software ASan/UBSan and the Arc A770
ASan/UBSan replay measure 272 samples with maximum direction error 0.00631529
under the existing 0.035 limit. Omitting the source normal raises the maximum
error to 0.98783.
The three profiles produce identical saved project bytes.

The retopology acceptance test now holds the actual worker before editing the
draft so Save attaches to the accepted publication before the injected fault.
It separately checks that a Save submitted after the failure refuses the still
open draft. Both paths preserve the prior file. Twelve repeated acceptance
runs pass 102 checks each in Release and ASan/UBSan. The product source for these
checks is unchanged from the retained HDR milestone.

All 19 selected Release CTests pass, including all 13 cases skipped in the
earlier sweep. Seven of the matching ASan/UBSan CTests have passed, including
both revised fixtures and the independent canonical source Bake. The
remaining sanitizer archives are running. The current full Release sweep
has started its all-target build on fixture commit `6b1d0c48` with the
canonical `acc2b70a` DomeD toolkit.
[Focused regression validation](validation/layered-normal-retopo-followup-20261127.json)

The earlier sanitizer archive sweep reached the union cases. Its rebaked UV
archive reached the 600-second CTest limit during the exhaustive Encode
fault pass. A separate replay of the same binary and input is exercising
all boundaries with a longer process window.

### Production retained-source LDR Bake

Two prepared retained 1024-square LDR PBR members now preview and Bake
within the existing work and byte limits. Preview and Standard GLB compile
color, ORM, byte emission and replacement coverage from the authentic
read raster and sparse tiles. The snapshot budgets each distinct retained
raster once and accounts for the actual layer capture and output arrays.
The normal field keeps its existing coordinate and material authority.

The registered workflow checks all material, coverage and normal bytes
against the original compositor. It exercises cancelling projection,
ordered Bake and Save, exact native reopen, Undo/Redo and Clear of the
selected field. The independent member keeps its original Bake and source.
Release and ASan/UBSan produce identical native and Standard GLB files.
Khronos validation reports zero errors and warnings and confirms two
1024-square color, normal and emission sets. Private DomeD Wayland and
synthetic OpenXR on software GL and Arc A770 reproduce the Standard GLB.
The three Editable GLB presets reopen the exact native scene and retain
both original 1024-square material fields. They emit the same validated
package for this fixture.
The previous production HDR Standard and Editable cases retain their
existing byte-identical outputs.

Larger retained fields, typed materials, alternate emission coordinates,
HDR VRM and physical input acceptance remain open.
[Production LDR Bake validation](validation/native-texture-set-ldr-production-bake-20261127.json)

### Repeated production Bake on retained material sets

Normal history shares immutable full coverage, byte-identical role images
and repeated write payloads. The renderer counts a common normal ancestor
once across retained members. Each accepted Bake keeps its own event,
selected material, source recipe and access to the prior field. Scene reopen
replays the recorded fields and retains the existing image references.
The older two-member project that failed the snapshot staging check now
opens with both ranges and saves byte for byte.

The 1024-square fixture alternates four more Bakes across two independently
painted retained fields. It repeats one recipe and projects a different
normal donor into each field. The test checks source isolation, exact
undo/redo, cold reopen, Standard GLB and unchanged material membership.
Editable GLB also restores the full native Bake history.

A separate reflected and sheared 1024-square fixture projects into distinct
paint and normal grids. Its donor is offset along the world-space geometric
normal. The strict Bake retains physical FLOAT emission and the projected
normal direction, and a short ray bound refuses atomically. The Standard
and Editable outputs preserve the owning native project. The previous
local-Z test placement has a real sideways shift under shear, so its strict
ray miss remains a valid control.

All 15 focused cases pass in Release and ASan/UBSan. Both production
cases produce the same native scenes and GLBs across profiles. The
isolated strict owning-archive replay passes all 3,257 decode and
4,903 encode allocation-failure checks and all 19 file-load cancellation
checks. Synthetic
OpenXR on software GL and Arc A770 reproduces both Standard GLBs. Native
DomeD Wayland reproduces the repeated Bake package. The four Standard and
Editable GLBs pass Khronos validation with zero errors and warnings.

Larger fields, other production scenes and physical headset acceptance
remain open. The full Release and sanitizer archive sweeps from the earlier
regression milestone retain their own source scopes.
[Repeated and reflected Bake validation](validation/native-texture-set-repeated-bake-20261127.json)

### Native MToon material assignment for texture sets

A native UV0 texture set can own a resolved MToon material, its original
role images and samplers. The shared material archive retains those
owners across save and reopen. Adjacent PBR sets keep their separate
materials and paint grids. The regular range renderer, alpha-aware
picking and desktop paint gesture share the selected field's identity.
The FLTK texture-set dialog and the XR View options both expose an
ordered MToon 1.0 assignment with Undo and Redo.

Release and strict ASan/UBSan each pass the four focused suites with
323 checks. The new shader and mask fixture passes on llvmpipe and
Intel Arc A770. The earlier shader suite passes all 81 checks on
llvmpipe. The actual FLTK universal-menu and modal replay on a private
Xvfb display passes 11 checks, including Cancel, ordered Save and fresh
application reopen. The native test exercises the real joystick input
worker. Both profiles emit byte-identical Editable GLB and recover the
same MToon graph, role image, paint, skin and morph data.

The Editable GLB's visible primitives use core PBR fallback. Typed
portable material output, imported artist frames and alternate
coordinates, typed Bake/resampling, matched Wayland, and physical XR
remain open.
[Native MToon validation](validation/native-mtoon-texture-sets-20261127.json)

### Portable MToon 1.0 on unpainted texture sets

The next export checkpoint writes `VRMC_materials_mtoon` for unpainted
native UV0 MToon 1.0 sets in Standard and Editable GLB. It preserves
authored alpha, toon factors, UV animation and per-role image bytes,
samplers and transforms. The adjacent PBR primitive, shared geometry,
rig and morphs retain their own data. Painted MToon remains editable
through the exact native data in Editable GLB. The visible fallback
identifies that embedded material. Standard GLB refuses unsupported
painted composition before replacing an existing file.

Release and ASan/UBSan each pass 187 export checks. Four exported
files match byte for byte across profiles. The independent cgltf
consumer validates the shared rig and morphs. The Khronos validator
reports zero errors. The unrigged MToon fixture has zero warnings.
The rigged outputs retain the existing non-root skinned mesh warning
from the PBR control. Each of the three typed outputs passes the
pinned MToon schema, including independent missing-index and
wrong-coordinate-type controls. Larger retained and painted typed
fields, alternate coordinates, VRM output and full device
qualification remain open.
[Portable MToon validation](validation/portable-mtoon-texture-sets-20261127.json)

### Painted MToon 1.0 texture-set export

Standard and Editable GLB now carry layered native RGBA and relative
emission on resolved MToon 1.0 UV0 fields with uniform static background
roles. The current compositor supplies color, emission and their
separate coverage. The exporter bounds the continuous clamp-linear
reconstruction at its filter taps and checks the uniform zero-coverage
branch. Masked and blended fields retain alpha compatibility. The
independent shade texture keeps its own UV and sampler, and the core
fallback retains the field's ORM image. Editable GLB reopens the exact
native layers and typed material. Standard GLB preserves the previous
output when a painted composition would change cutoff classification.

Release and ASan/UBSan each pass 204 native export checks. The two
standalone files replay against the original viewport on software GL
and Intel Arc A770. Each graphics profile passes 87 checks with
identical sampled alpha, 35 matching picking rays per fixture and
at most two RGB bytes of difference. The three registered Release
CTest cases pass. The four typed packages match byte for byte across
profiles and pass the pinned MToon schema. Khronos reports zero
errors and repeats the existing non-root skin warning on the two
rigged fixtures.

Varying background roles, shared base/shade textures, live painted UV
animation, physical HDR, typed normal Bake, additional source UVs,
VRM material integration and physical-device verification remain open.
[Painted MToon validation](validation/painted-mtoon-texture-sets-20261127.json)

### Painted MToon with a shared base and shade texture

Standard and Editable GLB export separate painted base and shade images
when both roles share one uniform static source image. The shade compiler
accounts for the viewport's positive paint clamp. For each filter cell
it bounds the difference between interpolating the clamped taps and
clamping the interpolated value. It also checks the original shade
branch at zero coverage. The native package retains the original alias
and exact authoring layers. A shade that exceeds the normalized portable
range leaves the previous Standard GLB intact.

Release and ASan/UBSan each pass 230 export and 131 software graphics
checks. The 512-square painted set exports beside a 64-square PBR
set and cold reopens with exact native paint. On the masked shared
shade fixture, software GL differs by at most two RGB bytes and
Intel Arc A770 by at most one. Alpha coverage and all 35 tested
picking rays agree. The five typed outputs pass the pinned MToon
schema with no Khronos errors. The rigged files keep the preexisting
non-root skin warning. Earlier painted files with independent shade
keep their exact bytes.
[Shared MToon shade validation](validation/shared-shade-mtoon-20261127.json)

### Painted MToon over varying source textures

Standard and Editable GLB support varying static MToon base, shade and
relative emission when the source images match the native paint grid
and use level-zero clamp-linear UV0. The worker bounds the difference
between multiplying the filtered source and uncovered fraction in the
viewport and filtering their product in the exported PNG. The bound
covers all locations between texels. Shared shade also carries its
positive-clamp error, and masked paint preserves the alpha decision.
Native reopen restores the original source textures and layers.
Mipmap or out-of-tolerance composition leaves the previous Standard
GLB intact.

Release and ASan/UBSan each pass 256 export and 175 software graphics
checks. Both export formats qualify at 64 and 512 square. Software GL
and Intel Arc A770 match sampled alpha and all 35 picking rays on the
varying fixture, with at most two RGB bytes of difference. The three
registered Release tests pass, and the five typed files match across
profiles and pass the pinned MToon schema. Khronos reports zero errors
with the existing non-root skin warning on the rigged files. Earlier
uniform shared shade packages retain their exact bytes.
[Varying MToon source validation](validation/varying-role-mtoon-20261127.json)

## Source and integration boundary

- Main now admits product1bc13125 and public fixturef1e193c7 after complete
  Release and strict sanitizer qualification. Read
  [the current admission](NORMAL-RUNTIME-ADMISSION-20261117.md) for exact
  owning-source/test/runtime roles. The newer advanced90fad product remains
  separate and is not admitted by this milestone.
- The earlier3d164382 baseline with the explicitly exercised
  c54 test reporter/02d coordinator remains separately qualified. Read
  [baseline admission](GUARD87-RUNTIME-ADMISSION-20261115.md): Release87 and strict
  sanitizer87, desktop208, syntheticXR776 and separately qualified software/Arc.
- The successor normal-source product1bc13125 has native/sanitizer and
  software/Arc component evidence. Read
  [the normal-source qualification](ARTIST-NORMAL-GRAPHICS-20261115.md).
- Frozen combined candidate090f009ede2c0623ca616fe892223b4f68f1c2c4 is pushed on
  work/authoring-normal-integration-20261115. Its worktree is
  artifacts/authoring-normal-integration-20261115. It merges main302e847d and
  test source6c8 through real merges; production remains exact1bc.
  Do not edit this source during its build/audits.
- A fresh App1bc component passes loader/version and the existing52-check GUI
  regression. This is separate from the full combined build and new workflow.
  Read [the GUI proof](APP1BC-SOURCE8-GUI52-PASSED-20261115.md).

## Build results and running jobs: inspect before acting

Authoritative state is the current service/process and its output, not this
snapshot. Never restart a job merely because an observation handle times out.

1. Release owning build: ds-native90-release-build-20261117.service,
   MainPID1978514, InvocationIDf18eb32adee84544b99a1c6404d1d403.
   Plan: artifacts/native90-release-planning-20261115/
   release-090f00-20261117-audit/plan.json, SHAccf9931035481459d7f51ec03e829ee2572e424d381d2bbc69a29c1fe26e1661.
   Status is execution-native90-v1/status.json under that audit directory.
   Actual build and final audit terminate0 in799.133s:291 fresh compilations,
  745 verified independent whole objects,737 fresh links,3 generators,1776
   outputs and1036 dependency owners/434920 incidences with0errors/drift[].
   Status SHA3c4fd01b2a6702ee0069a744375b9d4a087ef4d8ed6a7faff829d22fe49be36c.
   The unit is independently captured as successful and stopped. No reused
   object is called newly compiled; old/new compiler records remain separate.
   Native90 and both whole-App public desktop64/syntheticXR260 matrices now
   pass; see NATIVE90-RELEASE-ACTUAL-20261117.md and
   WHOLE090F-PUBLIC-UI64-XR260-20261117.md for exact source roles.
2. Strict sanitizer build: ds-native90-sanitize-build-v2-20261117.service,
   InvocationIDf2bcbf89dda84a1dae2590e06d016dd4. Plan-v2 SHA
  62116c27b70b37459d186e037acc51984d34fac8262760e5f50351ac8c21f8d9.
   Status is artifacts/native90-sanitize-planning-20261115/
   sanitize-090f00-20261117-audit/execution-native90-v2/status.json.
   Prepare-only was actual0. First executor launch stopped before staging:
   its policy incorrectly rejected two existing optimized CPU/FMA test owners.
   The [v2 correction](SAN90-EXECUTOR-V2-20261117.md) preserves exactly those
   two O3/march=native owners and O1 for the other1034, with ASan/UBSan and
   frame-pointer instrumentation for ALL1036. Source/CMake/commands are unchanged;
   original failure and expected pins remain preserved. The v2 owning build
   now terminates0 in5645.490s with1036 owners/1776 outputs, zero drift and
   no survivors; read NATIVE90-SANITIZE-OWNING-20261117.md. Its separately
   reviewed strict native90 run also passes ALL90 suites plus version,
   workspace27/373 and raw90159, with final drift[]/owned[]/released lease.
   Actual unit2934532/Invocation7d8e087a completed0 and was stopped only after
   root captured terminal properties and journal. Read
   NATIVE90-SANITIZE-ACTUAL-20261117.md for the scoped runtime proof.


Builds use regular-user bounded units, two compiler jobs and8GiB aggregate RSS.
No system installation, physical-headset launch or session restart occurred.

## New prepared-source public workflow

Initial6c8 fixture/runtime/font complete TUs and three links pass. Native91 also
passes, but the new capsule producer fails before UI: it compares PaintedMesh
wrapper pointers across Scene::snapshot(), which intentionally detaches wrappers.
The original failure is preserved under
artifacts/artist-prepared-public-link-v3-20261115/native-public-v3-20261117-v1.

Test-only childf1e193c7563eacc4db1d40518235450975460b15 captures the pointer baseline
inside the detached scene before Bake, additionally retaining full encoded input
and original-scene checks. Production is unchanged. Its complete fixture TU and
fresh link pass; runtime/font/scenarios remain explicitly6c8. Native capsule/
font qualification is now actual0, with all47 commands,4 genuine cases,
32 positive phases,8 wrong-Original refusals and all bitmap checks passing.
Read [the native seal](ARTIST-PUBLIC-NATIVE-20261117.md) and actor artifact namespace
artifacts/artist-public-snapshot-link-20261117; do not blindly rerun old failures.

The earlier component App passes desktop64/Arc syntheticXR260 in its explicitly
recorded3d context. The fresh whole090f App now ALSO passes desktop64 and
Arc syntheticXR260, with ordinary loader/version0, all7 critical ELF roles,
actual Arc PCI03/renderD129/i915/liveFD, exact source/normal/Save/history/reopen
checks, unchanged time/frame bounds,0drift/noowned and stopped units. Read
[whole-App qualification](WHOLE090F-PUBLIC-UI64-XR260-20261117.md). The two
matrices are distinct, not relabeled. Both use fixturef1 and runtime/font/
scenario6c8; no whole-f1 owning build is claimed.

The fresh wholeApp also passes14 actual native Wayland/DomeD checks on Arc:
application GL and canonical compositor Vulkan, visible clay canvas, real
universal menu/text/modal commands, desktop-side action, Undo and normal close.
Read [native Wayland GPU proof](WHOLE090F-WAYLAND-GPU-ACTUAL-20261117.md).
The canonical compositoracc build is optimized and uses the current SDK/private
bitmap stack; Cargo may reuse valid dependency artifacts. Private Xvfb presents
the nested compositor, which has a diagnostic copy. This is not native DRM
scanout/zero-copy or physical-headset acceptance. Failed staging/display/observer
attempts remain preserved separately. The test-only observational EGL hook
queries the actual context and changes no application rendering.

Operational drivers are in artifacts/prepared-public-operational-20261117 and
artifacts/whole090f-wayland-20261117. Use the common graphics lease before ldd/
products/GL and release only after owned-process cleanup. The whole SAN runtime
is now qualified separately above. New advanced-family products still require
their own reviewed component gates.

Combined native90 runner is prepared in artifacts/native90-release-gates-20261117;
its exact driver SHA is55f06be3963aaff1726962b224d7a534015ea89b179cb13e9e935dfe4744f057.
It now passes ALL90 actual native suites plus app-version/workspace27/373/
raw90159, with full posthash0drift/noowned and successful stopped user unit.
Actual status SHA8f496335c8b92cedc8887a9a53112d61c798ab20ecb465b37dbc853ceeddaaf2.
The native run uses the genuine whole090f build. Whole-App GUI/XR and native
Wayland evidence are recorded separately above. Use the common lease
/tmp/dome-sculpt-arc-device.lock before ldd/products/GL and release only after
owned cleanup. Original87/SAN/runtime failures remain honest separate records.

A further coherent source branch work/authoring-public-integration-20261117
(commit5605520b) merges the test-onlyf1 correction
with090f without modifying the frozen source. Product/tests/build
files matchf1 exactly. No whole-build560 claim follows from this source merge.
The separate pushed admission draft637ea1f4 on
work/normal-runtime-admission-20261117 merges that source with main771031f8;
read NORMAL-RUNTIME-ADMISSION-DRAFT-20261117.md in that worktree. Its strict
sanitizer condition is now satisfied; main merges this history in the current
qualified admission. No whole-f1 owning build is claimed.

## Next production slice

Agent artist_normal_completion is implementing owning Rasterize/RetainSource
normal Bake-source adapters in a separate branch, preserving source domains,
PBR/MToon roles, frame/pose transport and exact normal-event/material pairing.
Checkpoint102f0af4 now passes all16 actual native combinations with249415
checks, plus prepared91 and artist139; read
[the component proof](ARTIST-ADVANCED-NATIVE-20261117.md). Its separate child
90fad800 bounds the full owning ancestor replay graph before deep validation;
fixtureaa8db16a passes17 native cases/263495 checks plus prepared91/artist139,
including chained edits, every observed traversal cancellation boundary, cycle
refusal and atomic worker/Save quota refusal. Read
[the sealed child proof](ARTIST-ADVANCED-CHAIN-NATIVE-20261117.md);
native success is not strict sanitizer, graphics
or whole-App admission. Genuine first-Bake fixture setup
and stale-event negatives were corrected without weakening source guards;
original failures remain preserved. Canonical/native32/independent writable-paint
domains remain explicit refusals pending their own actual adapters.

All six new strict component TUs and three fresh links pass in
artifacts/artist-advanced-normal-sanitize-20261117. The separate native-v2
runtime now also completes0: advanced17/263495, prepared91 and artist139, with
live ASan/UBSan witnesses for each ELF, leak detection, all17 exact ordered-save
pairs, drift[]/owned[]/released lease. Status SHA
f8e9c649ebf0a324be9e3ba7ef48693eb662cf3fedcdb4281d47f694733dad03.
Root captured actual unit3089942/f021bca5 success before stopping it; the scoped
proof is integrated in main0094ee24. Graphics/whole-App admission remains
outstanding for advanced90fad.

Public source checkpoint8bb4072d adds separate source-use and UV-creation
desktop/XR scenarios. Its two complete Release test TUs and three fresh links
passed. The first retained-source producer exposed four geometry-only calls to
context-free PaintedMesh encoding, which requires an owning sampling archive.
Fixture-only child27ff940b compares the actual geometry directly instead; the
original failures remain archived. Its fresh native TU/link and native-v3 gates
pass: advanced17/263495, seventeen ordered-save pairs, eight source-use and eight
UV-creation cases, 128 positive verifications, 32 expected negative refusals and
bitmap checks. Read [the native proof](ADVANCED-PUBLIC-NORMAL-NATIVE-V3-20261117.md).

All eight actual desktop source-use cases pass128 checks in sixteen fresh Apps,
including real source/normal controls, second Bake, Save, Undo/Redo and reopen.
The normal-error maximum is below .00510 against .035; wrong-domain/omitted/
swapped controls retain .05 sensitivity. Read
[the desktop matrix](ADVANCED-PUBLIC-SOURCE-GUI-MATRIX-20261117.md).
These are explicitly product90fad/fixture27ff/runtime-and-scenarios8bb, unchanged
090f UI/support and font6c8. They do not admit advanced production into main.
All eight source-use syntheticXR cases pass65 checks each on authenticated Arc,
total520. All eight separate desktop UV-creation cases pass112 checks. Read
[source-use XR](ADVANCED-PUBLIC-SOURCE-XR-MATRIX-20261118.md) and
[desktop UV creation](ADVANCED-PUBLIC-UV-GUI-MATRIX-20261118.md).
UV-creation XR first fails a test reporter/capture contract: intended pressure
was logged for unused float actions, and captures14/27 conflict with actual
render skip/tracking loss. Test-only childb7d6ff17 records returned pressure and
observes13/29 while preserving all action timings and original render/tracking
events. Its full runtime TU/fresh DSO and all-eight rerun now pass208 checks;
read [the faithful XR observation proof](ADVANCED-PUBLIC-UV-XR-MATRIX-V2-20261118.md).
Operational statuses remain under artifacts/advanced-public-normal-operational-20261117.
Each case retains the original bounds and common device lease. No
physical-headset result follows from synthetic XR.

The independent advanced renderer compiled and linked successfully, but its
first actual software run failed the negative-control contrast requirement.
Correct PBR RGB errors were0,1,0,1; omitted-normal differences3,4,1,6 never
reached7. A separate wrapper rejected a non-ELF deleted Mesa lp_dma_buf mapping.
Both failures and all images are preserved. A CPU forecast across68 views
calibrates all16 known PBR control colours within1 and finds interior PBR
witnesses. Authored MToon estimates remain insensitive and graphically
uncalibrated. Read [the actual contrast record](ADVANCED-NORMAL-CONTRAST-ACTUAL-20261117.md).
The declared display-only presentation and extra interior probes now compile
and link. Its source2c687a1a preserves genuine normal data/source ownership and
publishes through Document. The CPU forecast passes68 views and selects438
probes including all272 original probes, with predicted contrast in both roles.
The first corrected software GL fails a MToon material control:231,217,209
against196,207,197. Dropped generic-source colour metadata caused that mismatch;
the corrected ordinary control now agrees exactly, with the initial six views
passing. The next actual failure is real continuous normal-detail loss: source
RetainSource64 sampled through a compressed current atlas into target64 exceeds
.035 between texel centres, with original-probe error up to .21789. All original
low-resolution cases and probes are preserved; production fidelity checking and
an explicit higher-resolution retry workflow are in progress. Results remain under
artifacts/advanced-normal-display-owner-build-20261118/software-display-v1-20261118.
The agent is diagnosing actual material/normal transport; no advanced GL/Arc
success is claimed and no threshold or genuine normal data is weakened.

The next canonical normal-field source adapter is implemented separately at
85eafc42. Its changed private headers invalidate142 existing owners; the selected
headless slice is49 core plus4 fixtures. The first compile stopped after28
successful complete TUs on a missing HDR-authority include. The include-only
child preserves that failure; compile-v3 prepares25 remaining complete TUs and
authenticates the original28 through actual dependency files and byte-identical
child inputs. Compile-v3 completes0 with all25 fresh TUs and no drift; the193-member
core archive (49 refreshed owners,144 authenticated unchanged owners) and four
fresh fixture links also complete0. The first canonical runtime fails a test
assertion comparing a deliberately detached conversion paint wrapper with its
mutable original. Fixture-only childb11f9d42 preserves the failure and compares
actual geometry/maps/event/content and detached deformation instead. Its fresh
fixture compile/link pass. Native execution passes advanced17/263495, prepared91
and artist139 independently, then canonical32/525092 with32 ordered Save pairs.
Read [canonical qualification](ARTIST-CANONICAL-NORMAL-NATIVE-20261118.md).
The fresh product85/b11 strict sanitizer core/four-fixture gate also passes;
read [its exact scope](CANONICAL-NORMAL-STRICT-SANITIZE-20261118.md).
Other affected consumers, new-family graphics and broader public application
qualification remain pending. Do not reuse an
old renderer with the changed private packet layout. The32-case canonical
matrix does not implement the separate native32 writable-paint source family.
The latter, canonical writable-paint and joint source-colour/normal adapters now
qualify as SOURCE paths at product441d9985 with explicit fixtureaa8dfdbd/joint48d5dfa3.
All six Release suites pass: advanced263495/prepared91/artist139/canonical525092,
writable64/619634 and joint8/346305. All121 ordered Save pairs match exactly;
read [the component proof](ARTIST-WRITABLE-JOINT-NATIVE-20261118.md).
This adds immutable paint-attachment capture, source-menu eligibility, correct
independent PBR/material sampling and bounded colour/normal ancestor replay.
Original compile/link/fixture failures remain preserved. Fresh strict sanitizer441
now also passes all six suites/1754756 checks and121 exact ordered Save pairs;
read [the strict component proof](SOURCE441-STRICT-SANITIZE-20261118.md).
Matching new-packet graphics and the combined application's own native/sanitizer
qualification remain pending. Product85 results are not relabeled441 or combined.

## Coherent integration build

Isolated branch work/authoring-canonical-public-integration-20261118 is pushed at
8a8ecea1. It merges current main, canonical85eafc42, public fixture27ff940b and
independent graphics2c687a1a through real merges. All765 production/build/asset
files match85 exactly; selected test/runtime roles are checked explicitly in
its integration receipt. This is source composition, not main admission.

Fresh Release configuration and the whole production application target pass in
artifacts/authoring-canonical-public-release-20261118 under
Invocation7d8c6f8b6e694690b7970cc3e097a259:242 compiler owners,93903 dependency
incidences, all193 core members and260 build steps in670.415s. Ordinary loader
exact72 and version also pass. Terminal/journal captured before stopping the
completed unit. Read [fresh build proof](CANONICAL-PUBLIC-RELEASE-APP-20261118.md).
The fresh application additionally passes one source-use desktop16 and matching
Arc syntheticXR65 canary, explicitly App8a/product85 with older unchanged
fixture/runtime/font roles. Read
[the coherent App canary](CANONICAL-WHOLE-APP-SOURCE-CANARY-20261118.md).
No all-target/test or full new-family application matrix is claimed. Keep its
source worktree frozen, including tests and CMake files.

The newer real-merge draftab497007 on
work/authoring-combined-source-20261118 combines main4416, SOURCE441/proof9bba,
filtered-quality8eba and creator75/test3666. Its fresh whole App target now
passes244 actual compiler owners/195 core members and262 steps in1116.610s under
ds-authoring-combined-release-app-20261118.service, Main1327750,
Invocation3721533d8d604d8c8c8a0b36d3b30149. Actual dependencies, the complete
195-member archive and174 linker inputs are audited. Ordinary
loader72/version0 also pass. The completed unit/journal are captured and stopped;
read [the exact build scope](COMBINED-AUTHORING-APP-BUILD-20261118.md).
No native/GL/application workflow or all-target acceptance is claimed for this
union. Keep its source and artifact tree frozen.

The separate pushed successor63956adc on
work/authoring-combined-checked-source-20261118 real-merges the bounded2048
target-resize childea838 and creator reporter/wordingd566. Its CMake conflict
resolution retains current filtered and full legacy17 tests plus all canonical,
writable and joint targets. Fresh App/seven-fixture compilation now completes0
with251 actual compiler owners/195 core members,276 steps in551.078s under
ds-authoring-combined-checked-release-20261118.service, Main1549843,
Invocation5f3dc23a5a99429d80b4351fca6d9067. Actual dependency/archive/link audit
also passes, and the terminal unit/journal are captured before stopping.
Read [the exact selected scope](COMBINED-CHECKED-APP-FIXTURES-BUILD-20261118.md).
No whole-family native or graphical qualification follows from compilation.

Fresh strict ASan/UBSan full core plus4 fixtures completes0 from immutable b11/
product85 in artifacts/canonical-normal-headless-sanitize-20261118:193 fresh core
members/212 actual compiler owners,851.199s. Runtime Invocation96b5dcc1 also
passes advanced263495/prepared91/artist139/canonical525092 with49 exact ordered
pairs, live ASan/UBSan witnesses, leak detection, no address-space cap and empty
drift/survivor lists. Both root units were captured then stopped successfully;
do not restart them. The compressed scoped proof is committed above.

## Current resumed slices — 2026-11-18

The latest user resume supersedes the previous round's pause. The original full
production goal stays active and incomplete. Main still admits product1bc; the
new component code is on explicit branches until combined qualification.

- **Combined integration:** frozenf599 Release App+13-fixture build/audit passes
  259 owners/197 core members, but its selected runtime exposes a missing
  separately qualified sampler producer. Root had merged fixturec56 without
  productione006. The original13 positives and final MToon validation failure
  remain preserved. Corrected real-merge child05f5d1cc adds e006 and its original
  helper/regression, with exactly2 affected complete owners +new sampler TU,
  257 qualified unchanged owners and15 fresh links. Loader/version72 pass.
  All15 coherent Release cases now pass265913 checks, including full17-family
  filtered workflow646 under originalAS2/RSS3 and all creator/HDR/legacy checks.
  Read [corrected runtime proof](CURRENT-COMBINED-SAMPLER-RUNTIME-20261118.md)
  and [initial owning build](CURRENT-AUTHORING-COMPOSITION-20261118.md).
  Current05 strict15 cohort has terminated unsuccessfully under
  ds-authoring-current-sampler-sanitize-native-v2-20261118.service,
  Invocation695aa961493d49299c1c84da387f9870 (former Main252918). No AS caps,
  original RSS and900s case bounds. Sampler55 passes; advanced17 exceeds the
  original deadline at900.127s/-15 with peak owned RSS2625769472 below3GiB.
  Remaining cases do not run. Terminal/journal capture and process-identity
  cleanup are complete; the device lease is independently free. See
  native-selected-v2/terminal-closure.json in the strict artifact directory.
  Its first
  fast-sampler observer miss is preserved, corrected observer holds actual
  executable FD and captures maps before post-run hashing. Sampler55 has
  actual live ASan/UBSan witness; full strict acceptance remains pending.
  Public current App/desktop/XR and broader all-target admission remain pending.
- **Independent HDR GPU:** productc524/fixture212 now passes actual llvmpipe
  source64/current512 PBR and MToon, before/after editable physical paint.
  All original FLOAT coefficients survive GPU extraction exactly;4422 checks
  pass, PBR error1 and MToon0 under bound2. The same production's prior old
  FLOAT172040 and complete MToon81 product steps pass separately; their failed
  outer/new-fixture phases stay failed. Read
  [the scoped proof](HDR-INDEPENDENT-GPU-20261118.md). Exact product hardware
  now also passes on authenticated A770:105 reads share one EGL context,
  all4422 checks/coefficients and four RGB phases pass unchanged. Read
  [Arc proof](HDR-INDEPENDENT-GPU-ARC-20261118.md); root independently rehashes
  all34 actual archived authorities. Resource/fault stress and actual combined/
  public application qualification remain pending.
- **HDR/native export:** genuine1a joint51247409 and current controls752/24 now
  pass on current exportcore. SOURCE8 export60a1 implements the actual owning
  source corner/paint route while preserving the visible original source.
  Native export16400 checks and23707 independent probes pass (HDR2.513e-6 under
  unchanged2e-4), separate64/512 witnesses, Scene43 restore, byte-identical
  second-cycle/worker export and real forgery refusals. Three actual GLBs have
  Khronos0 errors. Expanded Release32822 and strict ASan/UBSan/leak80255
  checks now pass, including actual coordinate/source-owner forgery, hidden
  Scene43 restoration and legacy worker controls. Root rehashes all570 actual
  archived authorities; read [source export proof](RETAINED-HDR-SOURCE-EXPORT-20261118.md).
  Original guard failures remain preserved. No combined/public admission yet.
- **Filtered normals:** e006/c56 passes native17 current families plus historical
  corpus. CPU forecast68/37968 dense points now passes under unchanged bounds,
  and all68 software views/432 points/1796 PNGs pass3721 checks, max RGB1.
  Root independently rehashes every archived member and actual image authority.
  Read [forecast](ADVANCED-NORMAL-FORECAST-20261118.md) and
  [software graphics](ADVANCED-NORMAL-FILTERED-GRAPHICS-20261118.md).
  Arc's actual product and complete independent saved-evidence audit now pass,
  including actualA770/renderD129/i915/136boundaries and all images/numerics.
  The original outer renderer-prefix/parser failure remains preserved separately;
  the full scoped software/Arc proof is linked above. Root independently
  rehashes all1818 Arc archive members against actual authorities and audit0.
- **Creator:** b75/de60 actual native448 VRM +44 workspace +352 controls +1
  fixture checks pass, including six female/male styled/reopened/posed exports
  and six genuine refusals. All six exports pass official schemas and Khronos
  with0 errors (21 naming/existing skinned-node warnings each). An unrelated
  empty Scene object was incorrectly dereferenced by the fixture; corrected
  avatar-group scoping preserves unrelated objects. An outer metadata assertion
  expected fixture0 instead of actual1; original1 is retained and a separate
  read-only9518-pin review establishes the precise positive scope. Read
  [native VRM proof](CREATOR-NATIVE-VRM-20261118.md). Current strict and actual
  desktop/XR UI runtimes remain pending; do not borrow oldApp GUI results.

Two genuine production defects are now isolated from the qualified05 source:

- HDR source capture's whole-before-Scene retains unrelated2048 destination
  rasters (160MiB) against32MiB source quota. Exact joint2048 reproduces the
  before-raster refusal. Source/dependency-only context closure is implemented
  separately in6a7d55a8, compiling/qualifying without raising the quota; old
  Scene43 contexts and full outer project restoration must stay exact.
- Independent sculpt edits double-charge persistent shared storage across
  different COW roots. Same fullsize4-object probe fails f599 at the second
  edit and passes3aa7aea8 allfour within512MiB, final327634597 bytes versus
  initial228304414, preserving density/queries/affine transforms/guards. Cold
  reopen, recovery and original independent-authoring sustained benchmark remain
  pending before this isolated feature can be integrated.

Root rechecked canonical DomeD local and remote main onNov18: both remainacc2b70.
No system installation, physical-headset launch or active-session restart occurred.

## Canonical toolkit and full remaining goal

[Nov17 effective-closure checks](CURRENT-TOOLKIT-20261117.md) confirm canonical
DomeD local/remoteacc2b70 and both SDKs remain current. Use
artifacts/sdk-20261111/toolkit-{release,sanitize} and the explicit private bitmap
runtime /opt/fltk-vr/bdf-runtime. Updated unrelated /usr font-library copies are
outside the actual selected72-provider closure; do not rewrite old expectations
or rebuild solely because an unused historical/global manifest differs.

The [Nov18 progress assessment](PROGRESS-ASSESSMENT-20261118.md) describes
the earlier pause and the remaining production work at that checkpoint. This
Nov26 handoff reflects resumed work.
Complete production sculpt/local meshing/recovery, painting/texture sets/baking,
remesh/retopology/UV workflows, humanoid/rig/weight authoring, glTF/VRM coverage,
physical XR/input and accelerator execution remain required. Controller trays
and spatial node/history editing are additional required later phases.
ArmorPaint is assessed, not integrated as a hosted engine. Preserve the accepted
selection interface and CC0 VRoid-only creator bases. Passing subsets, build
completion or an integration checkpoint do not complete the goal.
