# Dome Sculpt current handoff — 2026-11-26

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
actual Arc XR accept plus ordinary fresh-App reopen now pass19 checks; cancel,
focus and desktop remain separate gates. The prior-authored stress fixture
moves the head joint5m, so its cropped screenshot does not establish ordinary
default-base presentation quality.

Genuine2048 HDR now reaches a further128MiB paint-channel allocation refusal
after the provenance fix. A new isolated87729adb worker avoids the discarded
112MiB field using a certified actual retained source domain; build/audit passes,
end-to-end qualification remains pending. Sculpt's new workflow first exposes
an invalid mask-brush fixture, then an inappropriate initial mesh wait after
file-only recovery catalogue refresh. Correct those fixtures without weakening
product guards; full cold-open/recovery and sustained acceptance remain pending.

Inspect live units before taking action. All product/loader jobs take the common
nonblocking lease, with no idle reservation. Root's opt-in timing fixture91a90a67
compiles/links against the exact strict05 core without changing product, data,
checks or resource limits; the full-resolution runtime diagnosis is pending.
Full scope in REQUIREMENTS is unchanged.

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

The [current progress assessment](PROGRESS-ASSESSMENT-20261118.md) describes
remaining production work. The latest user instruction requests a pause after
this round. Preserve all incomplete requirements for the next explicit resume.
Complete production sculpt/local meshing/recovery, painting/texture sets/baking,
remesh/retopology/UV workflows, humanoid/rig/weight authoring, glTF/VRM coverage,
physical XR/input and accelerator execution remain required. Controller trays
and spatial node/history editing are additional required later phases.
ArmorPaint is assessed, not integrated as a hosted engine. Preserve the accepted
selection interface and CC0 VRoid-only creator bases. Passing subsets, build
completion or an integration checkpoint do not complete the goal.
