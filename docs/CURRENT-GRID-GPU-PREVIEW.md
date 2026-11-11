# Current-grid GPU render preview

Implementation branch from `775f92b8`. This is an in-progress product change;
source compilation, fresh owning builds and runtime qualification are separate
steps. It does not complete the full painting/material/baking objective.

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
