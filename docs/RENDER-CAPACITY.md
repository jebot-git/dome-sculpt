# Ordinary scene rendering capacity

Frame checkpoint `c0d7874` inherited aggregate limits introduced with the frame
transport work: Document packing and texture preparation rejected more than two
million vertices or six million indices, and all surface buffers shared a new
256 MiB limit. These were unrelated to ordinary multi-object scenes. The older
local mesher supports eight million vertices and 48 million indices.

The ordinary aggregate gates are removed. Retained-frame/layout/archive quotas
remain unchanged. Document packing checks container arithmetic and the actual
`uint32_t` index address range; rendering retains the `GLsizei` draw-count limit.
Each vertex/frame, index, weight and mask buffer is checked against the actual
signed `GLsizeiptr` range with overflow-safe multiplication/addition. Independent
buffers do not share an invented fixed GPU budget.

Driver allocation/upload failures are checked immediately, reported with the GL
error code, drained from the error queue and invalidate the geometry cache. The
same valid immutable source can be retried. These changes do not discard source
meshes or alter SDF/retained-frame flags.

## Focused regression

`document-render-capacity` is a separate, heavy CTest target with an isolated
home, a 120-second bounded Document wait and a 240-second process limit. It
forwards and counts real `glBufferData` calls and renders real 32×32 GL images.

* Three independent valid 64-resolution PaintMeshes each contain 1,600,004
  vertices. The first four form the visible quad; the remaining finite vertices
  are legal unused source data. The actual Document publishes **4,800,012**
  ordinary vertices and uploads a **268,800,672-byte** vertex buffer, exceeding
  the removed 256 MiB limit. All source objects remain intact and carry no new
  SDF or retained-frame state.
* Three independently valid 600×600 painted UV grids publish **1,083,603**
  vertices and **6,480,000** indices. The driver receives the complete
  **25,920,000-byte** index buffer.
* Both cases reproduce the small three-object scene's exact pixels. Their
  second-eye draws issue zero geometry/scalar uploads.
* The wrapper forwards one deliberately invalid negative-size buffer upload to
  the real driver. `GL_INVALID_VALUE` (1281) is reported without publishing a
  successful upload; the cache invalidates, the error queue is clean and retry
  of the same small immutable scene restores the exact baseline. No actual
  device-memory exhaustion is induced.
* Arithmetic boundary tests accept the exact `GLsizeiptr` limit and reject
  multiplication or shared-buffer block overflow without huge allocations.

All **19** checks pass in optimized software GL and actual Mesa Intel Arc A770
GL. The same 19 pass ASan/UBSan/leak checking. This is focused changed-code
instrumentation of Document, renderer and fixture, linked with coherent frozen
`c0d7874` Release support archives; it is **not** a whole-core sanitizer build.
Link maps verify that the changed objects replace the original Document/render
archive members. Public headers/layouts match the frozen support snapshot.

Before the fix, the matching c0 headers/libraries reject the actual large
Document with `Packed surface preview exceeds native renderer geometry limits`;
the preserved pre-fix executable/log records that exact failure. Existing
software buffer reuse **24**, Document buffer authoring **22**, and retained
normal-frame rendering **47** checks also pass with the fixed objects. CMake
configuration verifies the independent target, timeout and software environment.

Logs, binary/source/support hashes, link maps and pre-fix provenance are recorded
in the local `artifacts/frame-capacity-manifest-<checkpoint>.json`. No source,
build or proof in the frozen paging `2135d8d` worktree or the parent production
tree was changed. No push, installation or physical HMD launch was performed.
