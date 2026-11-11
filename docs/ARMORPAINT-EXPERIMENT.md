# ArmorPaint painting reuse: bounded native GPU proof

The isolated [blur experiment](../experiments/armorpaint-blur/README.md) establishes
that selected ArmorPaint shader tools can run through Dome Sculpt's existing
OpenGL/EGL ownership. It does not select a new default painting backend or replace
the native FLTK/OpenXR interface, document, source/VRM model or history. The full
original Substance-like layered painting/material/baking objective remains active
and incomplete, alongside the other requirements in REQUIREMENTS.md.

ArmorPaint was inspected at exact upstream
[`629f8e77b3f19d533c60a1baa4fe9d4db9ee1163`](https://github.com/armory3d/armorpaint/tree/629f8e77b3f19d533c60a1baa4fe9d4db9ee1163),
dated 2026-11-11. This is current native C with the Iron engine and Kong shader
generation, not the historical Haxe version. The root zlib/libpng notice and
unchanged `paint/sources/render/make_blur.c` are retained in the experiment;
additional upstream dependencies/assets have their own notices and were not
wholesale imported. Exact hashes and purpose are in third_party/sources.json.

The proof preserves upstream's 15 horizontal plus 15 vertical samples, duplicated
center, exact weight function and coverage-associated RGB accumulation. The
GLSL adaptation explicitly adds independent FLOAT coverage, a separate R32F
destination mask, rectangular integer sampling with clamp and a documented
associated-space blend. That blend is an alteration, not a claim of identical
full upstream brush behavior. No original normal/material ID packing is adopted.
Runtime dependencies are libOpenGL/libEGL and the C++ runtime; there is no stock
ArmorPaint UI, window/event loop, Vulkan, GTK/Qt or X11/GLX dependency. Python is
build/test orchestration only. Root application build/install targets are unchanged.

## Evidence and its limits

The source milestone is built independently in both optimized Release and
ASan/UBSan configurations. Each runs software and device-authenticated Arc
controls, checks every resulting float component with an independent long-double
oracle, and compares cross-device and Release/SAN output. The current compact
receipt is [validation/ARMORPAINT-BLUR-20261111.json](validation/ARMORPAINT-BLUR-20261111.json).
It records the exact committed source, binary/generated/source hashes, original
profile outputs, exit statuses and sanitizer options. Builds and generated
shader/evidence files stay outside tracked source directories; generation is
offline, hash-checked and deterministic. Reproduction is in the experiment README.

Fresh source milestone `a5a6973e977c49fb9e35cfd73712e33e8e228607` passes all four
profiles: seven cases and31,521 assertions each. Both sanitized runs enable leak
detection and strict UBSan and exit0 without reports. Release/SAN outputs are
byte-identical per device;15,708 software/Arc float components differ by at most
`4.76837158203125e-7`. The source tree remains clean after configuration/build and
validation; generated fragment shaders match across both builds. These scoped
results do not establish production throughput or physical-headset usability.

Fixtures cover sub-UNORM8 FLOAT coverage, HDR radiance, independent nontrivial
masks, rectangular clamp edges, cross-only impulse, zero coverage/strength/mask,
unchanged input bytes and rejected aliases/invalid formats/domains/budgets. Wrong
texture targets are refused immediately after failed binding, before querying a
previous binding or writing output. Five test-only zero-object GL API failure
controls check partial resource cleanup. They do not force global driver OOM.

Arc qualification follows the current EGLDisplay to its EGLDevice, DRM render
node and sysfs PCI/vendor/device identity; a DRI_PRIME request or renderer string
alone is insufficient. Sanitized runs use `detect_leaks=1` and strict UBSan with
no suppressions or LSAN_OPTIONS. Earlier local artifact runs with leak detection
disabled were not leak qualification and remain historical local evidence.

This proof supplies textures directly from a test harness. It does not establish
real surface/controller projection, all14-channel mutation, native project round
trips, whole-history cancellation, source-aware normals/UVs, material graph
authoring, texture-set membership, baking or production/headset performance.
Readback occurs only in validation, never in the reusable blur adapter. Inputs
must already satisfy the documented finite/range/semantic contract.

## Why selective extraction, rather than whole-app embedding

Current upstream [`paint/project.c`](https://github.com/armory3d/armorpaint/blob/629f8e77b3f19d533c60a1baa4fe9d4db9ee1163/paint/project.c#L1-L39)
builds a native app over Iron. Linux selects Vulkan plus X11 in
[`base/project.c:41-58`](https://github.com/armory3d/armorpaint/blob/629f8e77b3f19d533c60a1baa4fe9d4db9ee1163/base/project.c#L41-L58),
and its enabled native dialogs introduce GTK at lines253-270. The internal
[`script_layer_*` API](https://github.com/armory3d/armorpaint/blob/629f8e77b3f19d533c60a1baa4fe9d4db9ee1163/paint/sources/minic_impl.c#L619-L660)
depends on global project/context, GPU begin/end, app layer admission and history.
The baker similarly depends on global scene targets and frame notifications in
`render_path_raytrace_bake.c:22-85`. These are useful source donors, not an assessed
ready multi-document engine library with a native FLTK/OpenXR host interface.

Upstream genuinely supports layers, groups, masks, float16/float32 textures,
hidden-layer persistence, UV1 and skin blobs. Those features are not absent.
Its standard paint output instead packs base RGB/opacity, tangent normal/material
ID, AO/roughness/metal/height and transient strength. See
[`make_paint.c:604-666`](https://github.com/armory3d/armorpaint/blob/629f8e77b3f19d533c60a1baa4fe9d4db9ee1163/paint/sources/render/make_paint.c#L604-L666).
Emission/subsurface use material-ID tags. Float texture support therefore does
not establish Dome's independent physical RGB radiance/coverage or typed original
PBR/MToon sampling/normal authorities. Stock `.arm` saves raw layer textures and
app mesh/node/skin/timeline data; it is not a lossless replacement for the native
document/source graph. No ready Vulkan/GL external-memory/OpenXR host path was
found in the inspected source. The extraction avoids needing that bridge.

## Next genuine hosted paint boundary

The following maps a proposed integration to the current native pipeline,
reviewed in resumed candidate `b8c469ce875eb2542c220300c3c63edbf1526570`.
It is an implementation boundary and admission plan, not an implemented adapter.

| Existing native authority | Required hosted-tool boundary |
| --- | --- |
| `PaintedMesh` sparse COW layers in `include/dome/paint.hpp`: all14 common components, optional independent PBR-color tiles, `PaintEmissionTile` with legacy/physical RGB and actual FLOAT coverage | Name the exact semantic plane and active object/set/layer. Supply immutable layer input plus original coverage and independent mask. Write a private changed-tile result; preserve every unrelated channel, layer ID/order, hidden/zero-opacity state and source owner. Explicit sRGB decode/encode is required for byte color; physical radiance/coverage must never pass through UNORM8. |
| `PaintOverlayFactory` / `PaintOverlayCompiler` in `src/paint-overlay-packet.hpp` and `src/render-paint-overlay.hpp` | Their five RGBA32F textures are premultiplied render contributions, not editable layer authority. Captured visible layers omit hidden/zero-opacity authoring data; original source and absolute background remain separate. The experiment expects unassociated RGB plus coverage. Passing a flattened preview or unpremultiplying tiny coverage as a substitute for layer data is not an integration. A new authenticated per-layer view or an explicitly associated-input kernel is needed. |
| `Document::begin_native_locked`, native worker and acceptance in `src/document-native.cpp` | Capture the existing generation/revision/object/edit-authority, source scene/paint identity, deformation, placement, texture-set lease and evaluated brush metric. Use a GL-owning execution queue/shared context with fences; the native asset worker currently has no arbitrary current GL context. Never block the XR frame on a whole-atlas upload/readback or wait for a background geometry job there. |
| Worker previews and final native acceptance, including `src/texture-set-stroke.cpp` lease authentication | Stage every allocation/result before publication. Recheck source generation, set/face membership, held placement, pose and gesture epoch before publishing immutable preview or acceptance. Cancellation/loss of tracking/focus/stale job discards private results. Preserve one complete gesture Undo/Redo and exact original native endpoints; release/sync GL objects on their owner thread. |
| Immutable CPU sparse tiles, native file/asset workers, ordered saves and source codecs | Current editable/history/save authority is not a GPU texture. Initial integration needs a bounded asynchronous dirty-tile GPU result transfer into a private COW draft, followed by existing typed validation/atomic acceptance, or a separately designed durable GPU-native authority with exact CPU consumers. Neither a whole-atlas per-frame readback nor a float PNG composite is a lossless history/save implementation. |
| `SourceMaterialBindings`, `PaintSamplingDomain`, original-field/normal owners and texture-set fields | Keep original typed roles, current writable coordinates, corner/face domains and tangent/normal frames distinct. Do not infer authority from byte-identical maps or a texture ID. Preserve source graphs, rigs, morphs and VRM metadata exactly. Initially admit only a proven plane/domain subset and retain explicit diagnostic guards elsewhere; validate broader adapters before expanding. |

A sensible first product slice is a real active-layer blur request on a plain
native writable field, using the existing visibility/material-aware evaluated
brush and independent destination mask, with per-channel dirty tiles and one
native transaction. It must demonstrate native Save/restart/Undo/Redo/Cancel,
stale/held/pose rejection, desktop and actual-controller workflow and independent
all-channel before/after checks. This is larger than the texture-only proof.

In particular, native color's authored alpha (`PaintPixel::data[3]`) and its
paint coverage (`data[9]`) are different values. The experiment's RGBA carries
RGB plus coverage only; a full color/opacity tool needs an additional typed alpha
plane or a separately specified treatment. It cannot reuse composite color alpha
as paint coverage. Normal/tangent values likewise cannot be treated as generic
RGB, and physical emission must retain its separate legacy/physical contributions.

The broader objective still requires clone/smudge, stencils/projection, robust
layer/group/mask/material authoring, texture sets, procedural material/brush
evaluation, baking/cage/error controls and useful XR tools. Each extracted family
needs its own explicit typed contract, provenance, independent oracle and device
qualification; the one blur proof does not complete those targets.

## Before increasing the experimental caps

Both the 512-per-dimension guard and default4MiB caller-texture accounting are
deliberate experiment limits. Full512² already needs9MiB and an explicitly larger
budget. Simply raising either value is not production admission. Three full
source/mask/output planes at36 bytes/texel require576MiB at4096² and2.25GiB at8192²,
before native originals, layer/history snapshots, preview cache, driver storage
or additional channels. Production needs:

- GL texture-size/format/framebuffer and same-device/context validation, checked
  overflow and combined old/new GPU/CPU/worker budgets before any mutation.
- Sparse dirty-tile execution, a seven-texel cross-kernel halo and real UV island,
  seam, face/material membership and padding policy; rectangular clamping alone
  cannot define painting across a mesh seam or adjacent atlas island.
- Halo/partial-transfer equivalence against an independent full-field reference,
  all14 and independent PBR/FLOAT/mask errors, hidden-layer/data conservation and
  defined empty/zero-coverage/color-space behavior at production resolutions.
- Asynchronous upload/fences/readback only where required for durable dirty
  tiles, bounded retained histories, cancellation/device-loss and allocation
  fault tests. No accepted partial result or silent fallback/quantization.
- Sustained multi-object/layer production workloads, GPU/CPU memory and frame
  pacing evidence on actual devices and usable physical-controller workflows.

Stock ArmorPaint code can reduce tool/shader implementation work, but does not
remove those native owning contracts or establish comparative performance.
