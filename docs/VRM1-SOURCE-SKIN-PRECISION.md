# Private source skin world precision checkpoint

Runtime `28f27b362fd9755dea78a65f865ff64bf9099733`, on sealed native-expression
checkpoint `6a9252f7f7e902422a3484ae7e253108410510e9`. This is a private component
qualification. It is not current-main, a fresh whole application build, an
all-628 regression, a new UI qualification, or full VRM1 completion.

The preserved shifted-eye counterexample is resolved in the actual native
worker: 24 independent image pairs across poses 0/2, weights 0/.35/1,
Authored/CorePBR and two eyes have maximum difference 1 at the unchanged bound
4. Camera, source bytes, materials, samplers and independent consumer inputs
are unchanged. Minimum coverage is 3,660 pixels. Wrong source UV, moving paint,
and tinting fixed paint still produce differences 78/65/79. No-op and second-eye
draws upload no new geometry/images. Cancel, explicit Apply, immediately queued
Save and fresh native restart retain their exact image/Scene predicates.

The original 7>4 images and all earlier controls remain in
`artifacts/vrm1-expression-preview-1853cc3/artifacts/preview-proof`. In particular,
substitution of native-rounded positions into the independent consumer remains
a diagnostic only. It is not used by this positive. The generic-double and
raw-stored-world inverse probes also remain separate unsuccessful diagnostics.

## Cause and authority

The imported skeleton represents source skinning in mesh-local space using a
float inverse of the original mesh-node world matrix. The old native worker
rounded the locally skinned position before separately applying placement. An
extra inverse/forward residual moved an actual source UV from .3125 to
.312499910593: a NEAREST sampler chose texel 39 rather than 40. This was a real
pre-existing native/source-consumer discontinuity, not proof that either image
could simply be relabelled correct.

[glTF skinning](https://github.com/KhronosGroup/glTF/blob/3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded/specification/2.0/Specification.adoc#skinning)
uses joint global transforms and IBMs, excluding the original skinned mesh-node
transform. The new private evaluator derives that cancellation from actual
immutable model/raw-byte owners, source instance and complete original
placement ancestry, source joint IDs, original or retained first-rig rest
witnesses, and the authenticated imported inverse relationship. Numerical
matrix equality or bone names alone do not authorize it. Current per-part pose
and explicit IBMs remain authoritative. It composes the source hierarchy,
current placement and skin product before the final float position boundary.
It does not rewrite native Scene data or generic Skeleton matrix semantics.

The real mesh worker and native paint/pick/evaluated geometry use the same
positions, including shape-query refresh. Existing normal/frame policy is
unchanged. Each skin uses its own actual pose; no first-skin pose is substituted.
The precision route requires the authenticated complete source-placement chain;
other source/placement representations are not claimed as a universal adapter.
No raw JSON, file read, GPU geometry readback or UI-lock parsing was added.

## Bounds and failures

Within the existing maximum of 256 bones, ancestry membership now uses a local
256-bit set. All range, cycle, self-parent and name checks remain. A genuine
256-node ordered or reversed chain uses 256 allocations rather than the old
33,152; all 529 endpoint, bind, persistence and invalid-input checks pass in
Release and owned SAN. This changes validation work, not accepted matrix data.
The source evaluator reuses its already authenticated immutable explicit IBMs
within that operation. There is no broad/global source cache.

Scratch forecasts cover REST/morph/position/matrix storage and complete raw
source-node parent/rest/world tables, including nodes outside mesh ancestry.
An actual imported source retaining 6,000 unused nodes passes at the default
budget. The initial candidate wrongly admitted that metadata at 256 KiB; the
current candidate prospectively refuses the same budget without changing the
Scene. Old and new default positions are bit-exact. No Scene/resource cap rose.
Borrowed equal-address owners, mixed instances, incomplete ancestry and
unwitnessed source reparenting remain real negative controls. Independently
edited generic space matrices retain generic native authority.

The new normal test retains every one of 908 cancellation boundaries and the
actual native paint/history/Save tail. Its explicit `--faults` companion retains
every persistent C++ allocation prefix. Current actual count is 2,208, reduced
from 66,187 through validation work, not sampling the fault domain. Both profiles
pass all prefixes, retry, exact Scene and owner predicates (5,360 checks).
The original unoptimized Release sweep also finished all 66,187 prefixes,
908 cancellations and 133,316 checks in 1,390.08 s. The SAME original SAN sweep
(handle 64560) remains separately live at this receipt. Its partial positives
are not a pass, a restart, a timeout or an optimized-scope substitution.

Initial max-ancestry fixture compile failures (missing full JSON include) and
ignored real-source probe setup failures are preserved. They were test include
corrections; assertions and runtime precision were not relaxed.

## Qualification

Each profile rebuilt complete `rig.cpp`, `document.cpp`, `document-native.cpp`
TUs and complete new consumers against exact immutable owned 6a core/render/
support archives. All 95 public headers match 6a. This is an explicit matched
component, not a fresh whole build or an old-header/new-archive mix.

- Release/SAN current finite component: 109.58/491.76 s, all inputs unchanged.
- Each profile: rendering 103, ordinary ownership/native 944, ancestry 529,
  exhaustive failure/native 5,360; all pass.
- Seven existing affected registered consumers each profile pass, 4.15/31.27 s.
- Whole rig/deformation/skin consumers and actual female/male source-rest graphs
  (1,672/1,368 checks) pass each profile, 32.17/73.33 s.
- Both actual CC0 VRoid bases: all 19 parts, actual shared-pose worker,
  queued Save, restart and exact Undo/Redo pass in Release/SAN. All 44,412 female
  and 45,528 male corners retain exact restarted positions (24 checks/profile).
- Final strict PCI03 A770: 104 checks (103 plus identity), 24 pairs max1,
  1.506 s. The same binary forced to llvmpipe refuses `--hardware`. Source,
  support, binary and mapped driver/SDK libraries are hashed. Lease released.
- CMake configure-only graph: 628 registered tests, including four genuine new
  registrations. No all-628 execution/build claim. No physical HMD/compute claim.

The [machine receipt](validation/VRM1-SOURCE-SKIN-PRECISION.json) records exact
commands, timings, inputs, archive/object/binary hashes, negative scopes and the
original live-SAN snapshot. Canonical toolkit/bitmap prefixes and old proof
trees remain immutable. No system install/session change occurred.

Dynamic expression-preserving painted export and prepared/normal-texture/
plain-PBR/absolute-background expression adapters remain required. Existing
strict guards remain. Current-main/Scene31/32/33/native-float semantic integration
needs its own coherent ABI qualification. Every other unfinished original-goal
requirement remains active.
