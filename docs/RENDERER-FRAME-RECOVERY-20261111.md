# Accepted-frame GPU resource recovery — 2026-11-11

Product source `2a98ea09`, test child `f21cb91c`, private branch
`work/preview-renderer-recovery-20261111`. This is a focused source/runtime
milestone, not main admission, an installed build, headset acceptance or full
painting/material/baking completion. Root owns the fresh whole-product build.

The renderer now stages material metadata, source images/samplers, source packs,
current native fields, sparse/FLOAT overlays, polygon frames, flat geometry and
sculpt arena acceptance as one Pending frame. Accepted owners and bindings remain
usable after an allocation/API/fence/resolve rejection. Material admission runs
before geometry allocation. Publication follows a successful latest-use fence
and destination resolve; deferred resolve retains the transaction and rejects an
overlapping draw. Failed destination writes can change framebuffer pixels: the
transaction protects accepted renderer/native/cache state, not framebuffer
rollback. A subsequent accepted draw restores the verified picture.

One context-owned typed-store ledger keeps requested bytes, image/sampler names,
immutable TBO/VAO dependencies and retirement debt across cache eviction and failed
staging. Last-lease drops do not delete or credit stores. Zero-time collection
credits only completed latest-use fences; fence/query/deletion errors preserve
debt. Only terminal renderer/context release or abandon clears the shared ledger.
Driver-private heap, programs/FBO metadata and the independently bounded linear
viewport cache are outside this requested-store accounting claim. Material and
flat-geometry quotas remain separate; the qualified sculpt arena retains its own
ledger. No per-view wait, glFinish, CPU readback or silent CPU rendering fallback
was added. Immutable mip versions protect accepted mipless images during upgrade.

Private raw-resource helpers mark acquisition and expose `mark_used()` for
retained references; callers must mark before each later submission and cannot
retain invalidated references across pruning prepare/retain/clear. Polygon draw
marks actual use. Empty polygon selection retires leases without terminalizing a
reusable helper. A legal mutable TextureMaps copy changing the sealed current
field's derived transparency is now rejected.

The real caller test uses actual Document/native producers together: plain layered
fields, sub-UNORM8/FLOAT source/MToon emission, polygon frames and changed sculpt
arena geometry. It injects real GL-invalid sizes/enums/fence/query calls plus a
bounded native allocation refusal, retains actual accepted sampled GL names,
checks unchanged counters/pixels without rebuilding, retries against a fresh
renderer, and verifies native Scene immutability. Conservative pending-fence
shims are identified as such; these are not physical OOM/device-loss tests.
Deferred overlap, unchanged/scalar reuse, final force seal/resolve, immutable mip
upgrade, actual shader sampling before retirement, unresolved destruction and
real context-loss abandonment are covered.

Focused Release/SAN own whole render/current-authority/consumer TUs use isolated
archive copies with all 191 other core members authenticated unchanged against
f473's existing ABI. Public headers and shaders did not change. This does not
substitute for the parent's new whole owning closure. Recovery passes R656/S656
software and strict Arc R659/S659, with unsuppressed ASAN/LSAN1/UBSAN. Arc identity
pairs actual GL with EGLDevice/renderD129/PCI03:00/8086:56a0, live DRM FDs and
mapped-provider hashes. Software is actual system Mesa/LLVM22, not a patched19
runtime. No further hardware jobs remain.

Traffic fixtures observe neutral COPY_WRITE staging without counting native paint
DYNAMIC/null inputs as geometry. Direct source-array byte comparisons include real
index/scalar payloads. Source-color/extra-UV footprints use present immutable
payload arrays rather than obsolete padded defaults; all original independent
image/native/visibility/stereo checks remain. The actual document gate now checks
indexed chunk fingerprints, required scalar uploads, caps/unmasked GL defaults,
local geometry immutability, uniform authored/held motion, actual GPU current-field
publication and exact paint Undo/Redo. It passes R31/S31. Earlier failures are kept
with accurate artifact-retention limits in the receipt.

**Remaining real capacity blocker:** default original3×1.6M vertices and new
separate6×1.6M/>256MiB command both fail inside Document before rendering. The
unconditional global MaterialSurfaceQuery calls standalone SurfaceQuery, whose
structural2M vertex/triangle limit rejects the aggregate. The original6.48M index
case would encounter the same triangle-domain problem. Both R/S failures are real,
not deadlines or the preselected RSS guards (R4GiB/S8GiB,240s). The default4.8M
input/pixel/indexed checks remain; its true compact payload is153.6MB. The separate
`document-render-large-buffer --large-buffer` test preserves actual >256MiB intent
with307.2MB, and a bounded old/new geometry endpoint below512MiB. Neither is claimed
passing. Fix the query composition with bounded genuine partitions/global IDs,
shared work budgets and original unused-vertex validation; do not lift standalone
caps, disable picking or alter native draw data. That product work starts on a new
branch, leaving this source/proof checkpoint intact.

Original full layered painting/material/baking and projected/source/physical
adapters remain required. A next genuine painting gap is radiance-preserving bake:
the current bake result/base lacks immutable FLOAT emission seed/codec/adapter
support, and existing guards must remain until that typed path exists.

Compact machine receipt and raw text/build/runtime/orchestration evidence are in
[the validation receipt](validation/RENDERER-FRAME-RECOVERY-20261111.json) and its
hashed companion archive. Generated ELFs remain in the local artifact tree;
the archive deliberately contains receipts/logs/scripts rather than binary builds.
