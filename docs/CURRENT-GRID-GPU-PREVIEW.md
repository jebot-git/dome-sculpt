# Current-grid GPU render preview

Private implementation branch `work/plain-gpu-preview-20261111` from `775f92b8`.
Product source `1452ddec` passed fresh owning Release/SAN builds and the selected
controls below; extended consumer `acff27fe` changes tests and link wrappers only.
It remains unmerged and uninstalled. It does not complete the full
painting/material/baking objective.

The actual Document mesh producer requests a private render-only interpretation
before it calls the old image compositor. The packet retains the genuine plain
writable mesh, padding, visible-layer COW common/FLOAT tiles and material factors.
A separate actor wrapper authenticates identity, factors, exact ordered UVs and
texture-set range authority. It is neither original Source4 nor native-emission
sampling authority. It never becomes editable data or a serialized format.

Public `PaintedMesh::preview/composite`, public Scene preview/export surfaces,
source-color preparation and default texture-set range preparation retain real
CPU images. Render packets are explicitly refused as export/bake image authority.
Source-backed, projected, baked, polygon-authored and retained-domain objects
remain on their existing paths. Extending their GPU coverage remains required.

The renderer uploads native common values in 16 bytes per texel of each changed
32x32 tile (the native payload has 14 bytes per texel), and separate genuine emission values in
seven FLOAT components. Unchanged COW tile owners and actual padding owners reuse
GPU input slots. Buffer growth copies existing stores on the GPU. A per-field
bounded integer directory references sparse slots. Hidden layers are absent from
the render interpretation and remain untouched in native storage/history.

The resolve produces the same byte-quantized sRGB color/emission and linear ORM
textures as the legacy image route, with its clamp/linear, nonmip sampler contract.
Physical emission instead retains independent legacy radiance, coverage and
physical radiance FLOAT textures alongside the legacy color/ORM byte textures.
Authored alpha and brush coverage remain distinct. CPU material picking samples
only exact native alpha; it does not rebuild a shadow CPU RGB/PBR composite.

A GL context owns its cache. There is no worker-context texture handoff, no
GPU-to-CPU preview readback, no per-view wait and no CPU retry on GPU failure.
Unchanged actor/held/view frames reuse the canonical immutable contribution key.
GPU precision/capability and declared byte/image/scratch limits refuse explicitly.

Requested-store accounting includes live outputs, compact sparse input pools,
padding, directories and both generations during replacement. Retired stores
remain actual owned GL objects and charged debt until the latest zero-time fence
query confirms completion. A single nonblocking flush seals work; there is no
`glFinish`. Fence/query errors retain debt and the preceding published resource.
Driver heap overhead is outside this requested-store accounting claim.

Tests added:

- `current-paint-preview-native`: actual worker selection before materialization,
  independent old-image alpha, forged/stale owners/material/UV refusal, distinct
  simultaneous material cache keys, real image API contracts, multi-resolution
  texture sets and actual native stroke/Cancel/Undo/Redo/ordered Save/reopen.
- `current-paint-preview-gpu`: every byte output against legacy CPU images,
  independent FLOAT emission arithmetic including subUNORM8 coverage, actual
  renderer comparisons, sparse dirty uploads, unchanged zero uploads, no product
  readback/wait/finish, pending retirement admission, real GL API rejection and
  prior-resource preservation. GPU reads are validation only.

Fresh core, render, native graphics, UI/XR application and matched consumers are
required because `TextureMaps` gains a typed pointer. Reusing an older core ELF
or archive does not qualify this source or its public byte ABI. No installation
or physical-headset acceptance is implied by these tests.


## Qualification and pause boundary

Both profiles passed the same ten selected software controls at product source
`1452ddec`: current native authority/history (8,401 assertions), current GPU
preview (49,569), existing compositor-native/render/export, texture-set
surfaces/native, old sparse overlay, source overlay, MToon renderer (81), and
native GPU blur software controls (139,378). Sanitizers used leak detection and
strict UBSan without suppressions. The software provider was actual system Mesa
26.2.5 / LLVM 22.1.8; it was not a patched LLVM19 overlay.

The final test-only extension passed 98,815 assertions on software GL and 98,818
on authenticated Arc A770 in each Release/SAN profile. The three extra hardware
checks pair actual current EGLDevice `/dev/dri/renderD129`, PCI `0000:03:00.0`
`8086:56a0`, and GL renderer identity; recorded live DRM FDs contain only 129.
Actual hardware times were 0.52s Release and 1.92s SAN. These are bounded correctness
controls, not a frame-pacing benchmark or a headset acceptance test.

The extension uses five genuine duplicate actors in one native Scene. It records
actual GL common-buffer sizes 68,608→134,144 bytes (capacity 4→8), one GPU copy of
68,608 bytes, and exactly one 16,384-byte new native slot upload. It then changes
four actual materials to force fresh resolves from every copied old slot, with
zero old-slot CPU reuploads and exact legacy byte outputs on all five fields.
Bare HDR background and hidden-FLOAT legacy-quantization cases also pass.

The full fresh SAN owning build compiled 277 steps with UI/XR enabled. The fresh
Release closure includes an original retained consumer include-path failure,
then successful corrected ownership and shader-consumer refreshes. The first
actual shader failure (integer returns in float GLSL functions) is retained and
fixed at `1452ddec`. Every recorded source manifest has no drift. Exact source,
binary/provider hashes, commands, prior failures and per-profile receipts are in
[the qualification record](validation/CURRENT-GRID-GPU-PREVIEW-20261111.json).

The parent has finalized main independently and paused the goal. This private
preview milestone is sealed for later review/integration; no jobs or hardware
leases remain, and no installation or session launch was performed. Resume notes
are in [the private handoff](CURRENT-GRID-GPU-PREVIEW-HANDOFF.md).
