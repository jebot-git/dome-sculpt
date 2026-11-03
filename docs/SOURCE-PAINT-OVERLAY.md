# Private original-source PBR paint and native-worker integration

This checkpoint extends SOURCE-PAINT-OWNERSHIP.md with live sparse GPU preview,
material-aware picking and actual native Document painting for the qualified
untyped PBR subset. It is **private and incomplete**. Source28 native import,
standard export, typed MToon/original-UV, compound normal/bake/rig/key workflows,
new HDR controls and full production admission remain required. The public
application's separate540-command pass does not qualify this branch.

## Composition and ownership

The common immutable PaintOverlayPacket captures visible COW tile owners,
aliased validated SurfaceMap padding and the exact optional source-map owner.
The compiler emits five RGBA32F contribution planes: colour, ORM, relative
emission, coverage and absolute emission. Raw source images, per-role samplers,
affine UVs and original coordinate carriers remain independently sampled by
SurfaceRenderer. Live preview constructs no dense CPU colour/float image and
performs no CPU pixel readback. Test readbacks are independent oracles.

Native PBR combines the original field times uncovered weight with premultiplied
paint. Physical emission is added after relative emission; explicit absolute
background zero overrides legacy fallback. Core unlit still uses base colour
only. Four sparse point evaluations with the same padding and linear footprint
supply alpha picking without a dense image. Every material/packet binding still
requires actual source-owner identity; equal values are not a substitute.

The first actual Document test found that render-vertex packing legitimately
creates new UV owners. PaintOverlayFactory::pack_source_maps now verifies all
image/control-block identities, role/filter/wrap/affine fields and every bitwise
coordinate against the actual appended source-vertex map before accepting the
packed metadata. This is a render-only expansion, not new Scene/paint authority.
Packed metadata forwards layers/padding/baked data to one strong immutable
contribution owner. The compiler and global resource forecast canonicalize that
owner, reusing all five GPU planes across coordinate expansion without copying
layer vectors. There is no Scene or Material back-edge.

## Persistence and build scope

Source28 and physical Scene27 capabilities are composed explicitly. The original
SourcePaintDecodeContext source-only overload still refuses physical tiles;
the physical-only overload still refuses Source4. Their owning Scene reader can
provide both capabilities. Current and retained material/shape/rig owners round
trip combined source/physical paint. Quota accounting includes physical tiles,
retained source data and render-packing correspondence; prior retained budgets
and exact rollback remain enforced.

The graph base is sealed e8e54fe. Initial HDR runtime/packet/compiler inputs were
copied read-only from its agent child with an exact1495-input snapshot manifest.
This predates that child's later06816fe prospective-planner/empty-packet/
retention/float-sourcepack checkpoint. **Those later changes are not integrated
here** and must be reconciled before broader admission.

Fresh optimized and whole-owned ASan/UBSan/leak builds compile this branch's own
core, all linked support, renderer, native FLTK/OpenXR app and selected consumers.
No earlier source-paint component archives are reused across the appended
PaintLayer/PaintedMesh, TextureMaps, material-binding or typed-context ABI.
The first13db builds reached a missing test declaration include after building
the app; that failure is preserved. Subsequent coherent continuations correct
policy inspectors, packing ownership and contribution caching. Final runtime is
b986faa; d43af79 adds strict hardware test identity, and7749d4f adds visible
physical-emission assertions. The merged public0b5ada0 changes documentation
only. Read the JSON receipt for every source boundary and result.

## Actual evidence

- Release16/16 and owned sanitizer16/16 selected consumers pass on final7749d4f;
  all1527 source inputs and26 selected binaries/archives remain exact. These are
  selected tests, not the full registered suite or new clean all-target builds.
- Actual SurfaceRenderer44 checks cover six lit/unlit ×Opaque/Mask/Blend frames
  against the established constant-source composite, max difference1code;
  picking, forged-owner refusal, hiding, restart, shared GPU quota, combined
  source/physical Scene and retained shape/rig persistence. Final PBR also shows
  physical green.005 beside red100, no invented blue, unlit ignoring physical
  emission, actual absolute background and meaningful explicit zero.
- Actual native Document19 checks cover mesh publication, input sampling,
  immediately queued Save, exact reopen, global Undo/Redo, Cancel, fresh-document
  render equality, authenticated packing, foreign image/view/coordinate refusal,
  cancellation and actual five-plane cache reuse.
- Actual allocated paint activates the overlay in72 per-role filter/minification,
  magnification, alpha, wrapping, affine UV and2/128-pixel source cases. All289
  assertions pass with max image difference0. The zero emission paint leaves
  the source emission0 while changing real coverage; this isolates original
  sampling. The44-check coloured/ORM/emission cases separately prove changed
  paint is visible. This does not claim independent exported-image fidelity.
- Strict actual Intel Arc A770/PCI03 passes44+19+72 cases. All three explicit
  software `--hardware` controls refuse. The final44 test is rerun after its
  test-only extension; Native19/Sampler72 and all ten linked archives are
  byte-identical to their preceding positive hardware cohort.
- Source ownership26, Scene74, resource24, all643 actual persistent allocation
  boundaries and nine preparation cancellation points remain passing in both
  profiles. Separate legacy/physical data and shader consumers also pass.

Preserved negatives include the missing test include; two auto-merged Scene
inspectors using the older envelope reader; a draft fixture using frame.mesh
instead of the actual frame.surface; and the real native packed-source identity
mismatch. No assertion, pixel bound or resource ceiling was relaxed. All final
finite jobs are terminal, with no sanitizer reports or input drift.

## Remaining required work

1. Reconcile06816fe compiler prospective allocation planning, metadata-only empty
   packets, retention accounting and optional typed float source transport.
2. Preserve original logical UV0/UV1 for all typed MToon roles independently of
   writable paint UV0; support untextured/MatCap-only original capture without
   fake images. Current typed and unsupported compound guards remain explicit.
3. Connect actual source-aware native import/atlas/adoption, repeated UV/bake,
   colour/normal/rig/key authoring and Source28 standard export. The existing
   native import option still uses its earlier resampling route.
4. Finish HDR authoring controls/interchange, native FLTK/Wayland/controller and
   synthetic/physical XR validation, full regressions and fresh vendor closure.
5. Complete every other original requirement and post-basic controller drawers.

No system installation, physical headset test or active session restart occurred.
