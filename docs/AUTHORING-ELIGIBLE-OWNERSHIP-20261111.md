# Current authoring ownership and capacity milestone

This docs-only seal starts from main `e2efd373`. It records the frozen private
candidate `e9931f72`, whose actual new query implementation is `2ef16028`
(product `38c7577d` plus small-behavior correction `ad73d6e9`). It changes no
product source, handoff, requirements, frozen source/provider, installation or
session. Main **product admission is still separate**; the full authoring goal
is not complete.

## What was actually built

Both Release and sanitizer owning closures finish with 65 current outputs. This
is an audited **incremental ownership closure**, not a new compilation starting
from zero objects. Each profile uses:

- Four qualified complete 2ef TUs: the two owning query TUs and the two matched
  native-query/capacity consumers.
- Seven complete 44be TUs at their actual recorded compiler `-o` paths, plus 296
  authenticated unchanged a904 objects: 307 required objects total.
- The qualified 192-member current core, with 190 unchanged member payloads
  independently hashed equal to the prior core and the two changed owning
  members matched to their qualified objects. All noncore archive members are
  independently matched to their actual object bytes.
- **63 fresh links and two byte-for-byte retained, already freshly qualified
  matched consumer ELFs**. The retained pairs require current planned link-argv
  equivalence; only output location and non-ELF linker dependency-file reporting
  differ. No old-core executable is substituted. Of the 65 real CMake targets,
  58 own the current core; seven standalone helpers keep their genuine original
  dependency shapes.

The new CMake plans contain the actual current CTest and DSO paths under
`authoring-eligible-owning-20261111/{release,sanitize}-v3/plan`. The 418 current
SDK pins, all public headers, complete relevant source/asset/third-party trees,
compiler flags and literal macros are checked. Historical qualification hashes
cover all 1654 old 44be and 1651 a904 relevant files, including symlink target
bytes; current clean Git alone is insufficient evidence. Each final profile
postchecks 7390 pins, its audit/self hash, core and 65 outputs without drift.

The source-asset macro deliberately retains the exact already-compiled literal
`artifacts/authoring-production-union-20261111/assets/humanoid`. All seven actual
asset files are pinned byte-identical to the current canonical assets. Other
`-D` tokens are never normalized. This preserves the real compiled behavior;
it is not an assertion that arbitrary embedded path literals are equivalent.

## Current runtime evidence

| Current e993 scope | Release | Sanitizer |
|---|---:|---:|
| Owning outputs / fresh links / retained matched ELFs | 65 / 63 / 2 | 65 / 63 / 2 |
| Version command | exit 0 | exit 0 |
| Native/software CTest suites | **56/56** | **56/56** |
| Chunk / placed workspace controls | **27 / 373** | **27 / 373** |
| Authenticated Arc original / separate large capacity checks | **64 / 63** | **64 / 63** |

The 56 suites comprise the actual two new query/callback suites and all 54
selected native/software suites. Original capacity/picking/image/Undo/Redo,
worker cancellation, source preservation, renderer recovery and upload/cache
controls remain. Every root gate step exits zero, with no external guard or
provider drift. Sanitizer uses `detect_leaks=1:halt_on_error=1:abort_on_error=1`,
strict UBSAN and no LSan suppressions.

All four hardware commands use the exact current-v3 capacity ELF and original
inputs/assertions: default `--hardware` and separate `--large-buffer`. Each has
an individually released common Arc lease. The unchanged-result observational
query authenticates real Intel Arc A770/DG2 GL4.6, current EGLDevice, PCI
0000:03:00.0 / 8086:56a0 / renderD129, live DRM FDs and actual mapped ELFs.
All 71/71 Release and 73/73 sanitizer mapped providers are first hashed from live
process maps and remain equal afterward. Exact baseline pixels, analytic
barycentrics, global IDs, direct selection, recovery and zero unchanged-eye
allocation assertions pass; no threshold or native source was changed.

Actual vertex stores include **48 bytes per vertex**: positions 12, normals 12,
UV0 8 and genuine worker canonical tangents 16. Material input pools are separate.
The original 4,800,012-vertex scene requests 230,400,576 VBO bytes; the independent
six-actor 9,600,024-vertex case requests **460,801,152 bytes (>256 MiB)**. The
indexed case keeps all 1,083,603 vertices and 6,480,000 indices, requesting
52,012,944 / 25,920,000 vertex/index bytes.

**Budget wording correction:** the capacity test's old-plus-new flat geometry
endpoint must remain below **512 MiB**. That is its stricter test envelope,
not the flat product limit. Actual `ContextStoreLedger::Limits.geometry_bytes`
default is **1 GiB**; `TextureState` overrides material fields only. The separate
`SculptGpuArena` hard maximum is **512 MiB**. Earlier sealed PACKED documentation
used the wrong flat-budget wording; the historical archive stays intact. No cap,
native input, assertion or result has changed.

All original guards remain: native default 180 s, capacity 240 s, selected batch
2400 s, per-process RSS 4 GiB Release / 8 GiB sanitizer. The packed query retains
512 MiB CPU retained / 768 MiB retained-plus-build limits, 128 parts, 262144
triangles per part, 4194304 BVH nodes and its explicitly charged 8-byte/node
interval sidecar. Standalone SurfaceQuery's structural 2M caps and legacy default
2M total closest budget remain. Filtered/mixed queries share 65536 total tests;
known partitioned object/material routing shares 262144 metadata tests. Small
delegated behavior remains unchanged. Original draw/material/UV/color/paint data
and global IDs stay authoritative; coarse eligibility never infers alpha or
animated UV and arbitrary filters still spend the original work budget.

These are **functional checks under permitted CPU contention**, not a quiet
performance result. No current sustained P1, physical headset, native Wayland
shell-global-menu acceptance or system install is established by this seal.

## Preserved historical scopes

The original v1 owning audit really exits 1 before linking: a compiler dependency
path such as `tests/../src` was compared lexically. V2 resolves filesystem paths
without normalizing compiler macros, and its Release audit really exits 0. V3
adds historical source authentication; both audits and their subsequent current
owning executions exit 0. Those records remain separate and unmodified.

The **old 44be** build driver really stops after 11 complete compiled TUs and 24
successful links because it mistakes a `-Wl,-soname,...so` token for a relative
filename. Its actual status remains 1. Each separately authenticated resume
retains those objects/24 links and freshly links the remaining 41, reaching 65
without recompiling. Its later **old native/software gates remain 54/56, exit 1**:
`document-render-capacity` and `document-render-large-buffer` fail. They are not
retagged as current e993 successes.

The independent same-public-TU small-query counterfactual likewise preserves all
three outcomes: old 44be exit 0, intermediate c84 actual bounds refusal exit 1,
fixed 2ef exit 0. Old and fixed four-call logs are byte-identical. This supports
preserving small 65000-triangle/eight-exclusion behavior while bounding the new
partitioned domain.

## Limited public reference and remaining admission

The separately verified **Release** public summary covers source/plain normal
GUI10 and real-loader XR39 each: 98 checks, both genuine producers, actual native
Save/Undo/Redo/reopen comparisons and bitmap-font reference. Its precise limits
remain: GUI uses private Xvfb/exported menu actions; its GLX canvas may be software
while the native EGL blur worker authenticates Arc. XR uses the real loader and
actual Arc stereo with synthetic poses/actions, not a physical HMD. Public Save
is the next actual action, not proof of a forced busy-worker race. Timings are
functional observations, not exclusive-system performance.

This seal does **not** admit the separate full public sanitizer aggregate, quiet
four-object sustained P1 or main product source. Those are separate parent-owned
milestones. The original complete layered painting/material/baking and genuine
source/projected/physical adapter objective remains required; this receipt does
not redefine that goal as a shader, query fix or build inventory.

The [machine receipt](validation/AUTHORING-ELIGIBLE-OWNERSHIP-20261111.json) and
hashed companion archive retain the self-contained text/log/script/receipt
records, exact source snapshots and raw positive/negative scopes. Every archive
member is fully decoded and SHA-256 checked. Extract it and run
`python3 verify-evidence.py` for an offline integrity/scope check; it launches no
application or GPU. Binary ELFs and native saved assets remain in their actual
artifact paths and are deliberately excluded.
