# Active plain-paint compositing work

This is private work, not full qualification. Base ede6686 plus merged main30991dd,
worktree artifacts/gpu-workload-ede6686, branch agent/gpu-workload-ede6686. The
finished reverse-quality component remains frozen1be8ab6; root published30991dd.

The regular-user optimized CPU cost audit measured2048² plain composites at
641–697ms,1024² at159–173ms. The selected new route handles plain writable paint,
all14 byte channels, layer visibility/opacity, masks and exact padding. It copies
immutable sparse tiles/padding into private bounded packets; selected-backend
kernels compute complete16-byte output records in65536-pixel chunks. No image,
cache or model memory aliases readback storage. Errors never retry on CPU.

Eligibility is grid512..2048 with contributing visible tiles, at most128M
pixel-layer visits, and no source maps/bake/color projection/normal projection
or independent PBR tile owner. Unsupported owners deliberately keep the old CPU
path with no new refusal. There is no archive or class-field change. Old1-arg
preview/composite ABI remains; additive cancellation overloads reach Scene,
native stroke and Bake workers. New backend TUs preserve existing brush/quality
source and compiler flags byte-exact against30991dd.

Private shared blend uses explicit float expression boundaries; unit/sRGB byte
decode tables and exact float→sRGB-byte threshold boundaries come from the real
native CPU implementation. No device pow approximation or normal/color receipt
borrowing. The initial Arc primitive passes756624 exact tests including sparse
and redirected padding, multiple layers, every channel, malformed packets,
final cancellation and chunk tails. CPU primitive756623 passes. All743 original
source/binary hashes stayed unchanged through that first batch87738.

Initial independent generated-source comparison exposed tiny pre-existing
GCC/Clang stamp-input differences: one stored tile byte at512 and three at1024,
all by1. The resulting emission/coverage bytes differ by1. Retain these logs;
no tolerance changed and no runtime math fix was made. The OneAPI core forced
through its OLD CPU composite matches its actualGPU10maps exactly. Decoding the
EXACT same serialized GCC Paint input on both CPU and GPU reproduces all10
frozen-ede maps byte-for-byte (26,214,400bytes), twice; eight source/core/tool/
input hashes are unchanged. See oracle-* and composite-*-before/after artifacts.
The initially hidden seed layer/incorrect normals were fixture errors; failures
remain logged. Seed mutation is now explicitly required.

First process512 GPU composite179.8ms includes runtime/JIT setup; later5128ms,
102433–47ms and2048150–153ms include all packing/upload/readback/image/cache work.
These uncontrolled observations establish substantial progress, not guaranteed
VR frame time or universal speedups. Packed-CPU cost comparison and transfer
trace remain to add.

Current native/cache fixture33 CPU HOST-CONTROL-FLOW checks pass. The CPU build
uses a test-only backend-name linker seam to enter production aggregate staging
with the native CPU implementation; it is not GPU/SDK evidence. It exercises
legacy-image equality, cache reuse, completed-backend and late image-validation
failure, unchanged held maps/source/cache, cancellation/retry, actual posed
skin+morph native paint/Save/Undo/Redo/cancel/stale generation. Matching OneAPI
fixture is compiled; its actual Arc execution is pending root lease.

Remaining required gates: owned core/support ASan/UBSan/leaks, actual allocation
prefixes/cache failure proof, CUDA/HIP compile+real missing probes (no hardware),
actual Arc native33, independent GL/export consumers and native/controller/GUI/XR
workflow evidence. No HMD, install, push/main integration or full-production
completion is implied. Parent owns integration; work stays isolated.
