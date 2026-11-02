# Authored and core-PBR material preview

The native View > Shading menu now offers Authored material (the default), PBR,
and Toon preview. The shared XR root header opens View page26; its first three
sectors invoke the same commands. Existing per-object material-style commands
remain separate. This extends the existing application and its accepted
selection controls; it is not a new frontend or the later spatial-drawer work.

PBR selects the actual core-PBR material, alpha and normal/tangent role even
when the source also contains MToon. It does not rewrite has_mtoon, original
roles, material values, visibility, Scene bytes or authoring history. Authored
restores the original material policy. Preview choice is ephemeral and starts
with Authored after a project restart.

An independent view_epoch identifies the requested worker endpoint without
advancing the authoring revision. Accepted asset work and its dependent Save
therefore retain their original leases. Published surface ranges own the
rendering/picking policy until their immutable replacement arrives. Waits and
native strokes require the matching view endpoint; stale workers cannot publish
over it. Trigger pressure begun during replacement must physically release
before it can edit the new surface. Active strokes, object/workspace grabs and
owned continuous controls block view changes transactionally.

Geometry identity changes only when the effective PBR/MToon frame owner changes.
Exactly shared full frame owners retain their static GPU geometry; second-eye
and no-op frames reuse geometry and images. No new vertex attribute, production
readback or per-frame extraction is introduced. SurfaceRange, MeshSnapshot,
EditorState, MeshJob and NativeStroke layouts changed, so every consuming
core/render/native/XR target must be rebuilt coherently.

## Portable Source20 delivery

The portable child is based on exact projection649befe. It carries only this
preview feature, with no Source21 normal-coordinate field, UV control, sampling
pool or schema change. Full core/support/render/native/XR consumers were built
fresh. Its receipt is validation/PBR-PREVIEW-PORTABLE.json.

Eight coherent Release suites pass in140.51s: native/controller/material-query
116 (including44 persistent publication denials), actual female/male111 and18
independent GLB images with maximum byte error0, View26 actual-loader10,
selection native/loader11, and projection native/controller regressions. The
separate existing projection loader passes10 assertions in20.31s. Native FLTK
passes13 normal plus13 compact. Final actual PCI03 A770 passes116+111, all18
images error0; explicit UHD770 refuses, and before/after source/binaries match.
All20 emitted files pass Khronos with zero errors and342 unfiltered warnings.

Projection strokes freeze the same view policy for the active target and all
other-object visibility queries, including cached and fallback queries. Actual
projector placement blocks mode changes until its gesture ends. This child has
no new whole-core sanitizer claim: the final combined portable/projection/rig
source will be rebuilt and qualified by the integration owner. The prior owned
sanitizer receipt below is explicitly historical and does not cover a changed
parent or new projection layout.

## Historical Source21 qualification

The complete scoped receipt is validation/PBR-PREVIEW.json. On the frozen
Source21 UI parent3855882, coherent optimized native/core/render/XR builds and
fresh O1/g1 ASan+UBSan/leak-instrumented C/C++ core/support/renderer builds pass:

- Native/controller/query/GPU fixture92, including all44 persistent real preview
  admission/publication allocation denials, exact source/publication rollback
  and retry; gated stale worker completion, physical trigger rearm, a real
  accepted asset plus queued Save and its exact one-step Undo.
- Actual CC0 female and male nineteen-part avatars111: eighteen independent
  parsed-GLB images under original, anisotropic and rotated mixed poses, with
  morph weights0/.35/1. Optimized software and actual A770 images have maximum
  byte error0; the separately optimized sanitizer consumer has maximum error1.
  This uses the real public PBR command, not altered snapshot material flags.
- Fresh owned sanitizer suites3/3 in171.60s plus controller-controls1/1 in29.55s.
  Source libraries/headers are coherent; no sanitizer suppression is used.
- Actual native FLTK universal-menu workflows13 normal plus13 compact
  (640x360, bitmap16 at2x), with visible mode/radio changes, exact authored
  viewport restoration, modal disable/Ctrl+W, source-exact Save and restart.
- Actual OpenXR loader with synthetic controllers10, including the rendered
  root View header/joystick path, genuine PBR alpha difference and exact
  restored opaque-alpha counts in both eyes at identical tracking endpoints.
  Existing actual-loader selection11, native selection/controller tests and the
  unchanged authored MToon renderer regression pass (1/1 in35.89s).
- Actual PCI03 Intel Arc A770 runs both92 and111 fixtures; explicit UHD770 and
  llvmpipe strict-device negatives reject. Before/after source and binary hashes
  are identical. All20 emitted core-GLB files pass Khronos2.0.0-dev.3.10 with
  zero errors and342 unfiltered/unlimited warnings.

The first GUI screenshot used a fixed short delay and captured the old worker
endpoint; its negative is retained. The corrected test waits for actual viewport
pixels, without a product workaround. The initial XR comparison mixed generic
focus/cursor conditions; the final fixture pins equal tracking/focus and tightens
restoration to exact equality. The initial sanitizer fixture omitted the matching
nothrow allocator overload; the fixture is corrected, with the negative retained
and no mismatch suppression. These are separate from the final passing scope.

## Remaining required work

This checkpoint does not enable retained-normal-domain GLB/VRM export. The
separate exact-normal export prototype remains unadmitted. Its source-colour/
alpha preservation negative near filtered partial-Bake holes after UV resampling
is real and remains required work; view selection does not repair it. Independent
writable-paint sampling authority (Scene24) is being developed separately.
Scene21 normal source/UV/key/positive rebake adapters, genuine compact editable
UV workflows, broader format consumers and post-basic interactive controller
trays remain required. There was no physical headset launch, system install or
active-session restart.

The portable delivery applies only the preview delta onto the qualified
public/projection source. Its history does not import the Source21 ancestor
merely to obtain the view choice. Projection strokes must capture this preview policy
for both active-target and other-object visibility queries; rig-fit placing
must be treated as an active gesture at the eventual union. Final registry
union is page25 projection,26 View,27 rig fit (count28).
