# Independent normal metadata preview proof

This private follow-up to `09989a5` removes redundant deep normal replay from
Scene24 rendering. It is not an additional serialized authority and does not
complete the remaining typed, welded, extra-UV, compact-layout or topology
adapters.

The original native material probe measured five cheap validations at an
average 0.0152376 ms and five deep validations at 86.344 ms. Repeating deep
plane/geometry replay when a pose creates a new GPU geometry snapshot causes
a visible VR hitch. Full replay still belongs to worker construction and
owning archive decode.

`SourceNormalSampling` now has a private runtime seal minted only after its
own full normal recipe, source/layout, before-state, metadata event and
resource receipt pass deep validation. It keeps strong immutable source
owners, records all receipt scalars, and binds to the actual sampling node
address/control block. A copied node cannot reuse the seal. Typed sampler
state retains its historical deep path. No serialized key, archive version,
colour receipt or PaintSamplingDomain grants this normal seal.

`NormalPreviewProof` is a separate private-constructor proof for the actual
immutable map returned by `PaintedMesh::preview`. Its worker-only factory
authenticates the normal projection, exact resolved/intrinsic planes and
normal-specific bake event against that independently sealed normal source.
It pins the actual binding, recipe, maps, projection and baked event, and
captures all normal-recipe owner/scalar associations. It owns neither its
parent object nor a back-reference cycle. It does not seal editable paint
values or unrelated material style.

The Scene/native worker transports this proof through read-only
`ObjectSurface` and `SurfaceRange` fields. Rendering and material picking
use cheap validation only when the exact map/binding proof and the independent
normal metadata domain match the range's genuine paint geometry attachment.
The existing ordered corner stream/winding authentication remains mandatory.
Missing proofs keep the old deep route; mismatched proofs fail. Reusing the
Painter cache preserves image owners across pose-only geometry updates.

Qualification is in progress. Fresh coherent Release core/support/render,
native frontend and six selected fixture targets compiled. Three native
regressions passed in 49.61 s: normal validation geometry, Scene24 colour/paint
sampling and actual Document/controller authoring. The corrected software
normal-seal fixture passed all 23 checks. It measured 305.525 ms for the legacy
geometry refresh versus 0.654723 ms for the sealed refresh, with identical
pixels and zero deep material/normal geometry calls on first geometry,
stereo/no-op and actual worker pose refresh. Pose preserved image uploads;
owning restart reminted the seal without archive-byte changes; independently
parsed current-project GLB agreed within the existing limit of 3.

The first run (`14321`) failed a test baseline after intentional forged frames
cleared the renderer texture cache. A premature retry (`62190`) still ran the
old binary; its log is retained and has no final proof status. After the
fixture build ended, the valid frame was restored before measuring pose reuse
and exact final fixture `69454` passed. No production change was needed for
that correction.

The focused fixture counts deep material and
normal geometry validations on the render thread, compares sealed and legacy
images/timing, exercises copied/changed owner negatives, Save/reload reminting,
actual pose refresh, texture owner reuse and independent current-project GLB
pixels. Final results and source/binary hashes must be recorded before this
private checkpoint is offered for integration.

The complete 414-check image/export regression now has a measured 2400 s
registered budget. The previous source `44380ef` instrumented run timed out
at 1200 s after 45/54 complete image groups, with no sanitizer report. That
original result remains a timeout, not a complete green suite.
