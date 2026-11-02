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

The follow-up factory also independently verifies the normal node's ordered
current REST/UV/layout and exact actual paint event before issuing a preview
proof. A staged further colour/paint derivative can still carry the previous
normal projection; that intermediate must not acquire a current normal proof.
This worker check remains separate from the colour/domain geometry witness.

The Scene/native worker transports this proof through read-only
`ObjectSurface` and `SurfaceRange` fields. Rendering and material picking
use cheap validation only when the exact map/binding proof and the independent
normal metadata domain match the range's genuine paint geometry attachment.
The existing ordered corner stream/winding authentication remains mandatory.
Missing proofs keep the old deep route; mismatched proofs fail. Reusing the
Painter cache preserves image owners across pose-only geometry updates.

The initial `8858f42` qualification below is historical. Final strengthened
runtime `3a55fa3` qualification is recorded after it. Fresh coherent Release core/support/render,
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
pixels. Final results and source/binary hashes are recorded in
`validation/NORMAL-PREVIEW-SEAL-FINAL.json`; main integration remains separate.

The complete 414-check image/export regression now has a measured 2400 s
registered budget. The previous source `44380ef` instrumented run timed out
at 1200 s after 45/54 complete image groups, with no sanitizer report. That
original result remains a timeout, not a complete green suite.

The test-only follow-up `normal-preview-tamper` separates node-address rejection
from scalar/owner rejection. It creates an actual mutable `make_shared<T>`
sampler, populates its public fields from the genuine prepared fixture, and
runs the real independent replay factory to mint a fresh seal. The test keeps
that legal mutable alias and a similarly mutable recipe while publishing const
views to the preview factory. It varies receipt fields and owner associations at the
same address, requires both seal/preview rejection, restores the exact fields,
then verifies retry and the unchanged original Scene. It never writes through
a cast to an object allocated as `const T` and does not mutate immutable
geometry/vector contents. Its 34 Release checks passed against the exact
completed `3a55fa3` headers/core/support/render; the separately compiled owned
C/C++ core/support/render O1 ASan+UBSan/leak overlay also passed all 34,
with no report. There is no additional product or ABI change.

## Final strengthened component qualification

Runtime `3a55fa3` has finished all owned finite gates. Release ran focused
2/2 (seal23/current5) in 12.04 s and six regressions in 172.04 s, including
414 complete image/export checks. The whole owned C/C++ core/support/render
O1 ASan+UBSan/leak run passed **8/8 in 2141.26 s**, including full414 at
1284.29 s, native63 at 348.84 s, seal23 at 131.84 s and current-layout5 at
84.98 s. There was no report. The earlier `8858f42` parent separately passed
7/7 in 2010.81 s; its missing factory-current association remains fixed only
in the strengthened runtime, not retroactively in that source.

Actual PCI03 Intel Arc A770 passed seal23 plus full414. Every one of the 54
native, 54 original-basis GLB and 54 newly emitted current-project GLB image
groups had maximum error **0**, with the original limits 2/3 unchanged. The
new geometry draw measured 228.177 ms legacy versus 1.69056 ms sealed on
this fixture; software measured 344.427 versus 0.561166 ms. These are fixture
measurements, not full-scene or physical headset frame-rate promises. First
geometry, stereo/no-op and actual pose had zero deep material/normal geometry
replay calls; pose retained image owners/uploads. Legacy fallback and all
forged-owner/ordered-stream negatives remain. All 761 captured source/header/
test/CMake/core/render/frontend/runtime hashes stayed unchanged; 128 output
hashes are retained.

The same frozen frontend/helper/runtime passed actual FLTK/X11 normal10 plus
compact10, and 20 actual-loader checks with synthetic controller tracking and
software stereo images. Canonical staged toolkit/bitmap runtime paths were
explicitly supplied and verified with the same environment. Normal and compact
captures were inspected. No new actual DomeD Wayland or physical HMD claim
is made. The legally mutable same-address unit overlay separately passed34
in both Release and wholly owned instrumented core/support/render.

This is a qualified active-UV0, untyped component. Palette's separate new
current-main/Source26 union must add and prove the detached factory's active
UV1/extra-coordinate refusal; frontend eligibility alone is insufficient.
Source25/rest, Source26 consumers and the latest root changes require coherent
semantic union and fresh composed checks. Typed/artist/conflicting weld/
corner-normal22/UV1+ transfers remain required guarded work. Opt-in Keep paint
does not complete default-lossless eligibility, compact/current-layout
compilation, exhaustive native admission allocation evidence, conformal
unwrap/stitch or actual native post-UV stroke/export workflow. The full goal
remains active.
