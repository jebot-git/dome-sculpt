# Immutable packed geometry and scalar GPU buffers

Renderer milestone `6117b6c` starts at `9a0cfc4`. The private combined tree now
also contains authenticated Document producer `8869707`; its contract and CPU
proof are in [AUTHENTICATED-LOCAL-MESHING.md](AUTHENTICATED-LOCAL-MESHING.md).
The evidence below belongs to the renderer milestone before combination; actual
Document-to-renderer traffic and combined GUI/XR/GPU proof follow separately.
Empty tokens preserve conservative existing behavior. Incremental tile rendering
is not complete.

The opaque shared token must strongly retain every packed geometry dependency:
ordered visible parts, generation, immutable mesh/COW field ownership and voxel/
physical band, current deformation/pose/morph data, complete world transforms,
source appearances/material-frame bindings and corner expansion/UV1 layout.
A token is reusable only when those dependencies are unchanged. It is not a
revision number or raw allocation address. Pointer AND shared ownership equality
are required; generation and packed-count consistency are checked independently.
There is no full-mesh fingerprint scan.

The renderer separates the static position/normal/UV0/UV1/tangent stream and
indices from weight (attribute4) and sculpt protection (attribute6) scalar buffers.
One mask-only update transfers4 bytes per vertex rather than resending geometry
and indices. Nonempty weight and mask streams together transfer8 bytes per vertex.
Scalar arrays are still validated and uploaded on each changed snapshot; unchanged
nonempty scalar arrays are not independently memoized. With no scalar overlays,
material-factor and texture changes need no vertex/index upload. UV1 reconstruction
and tangent validation are skipped only under the trusted geometry contract.
Canvas navigation and a second stereo draw use uniforms and existing buffers.

Empty overlays disable their arrays and release old stores. Disabled-attribute
constants are set every draw because those constants are context state, not VAO
state. This prevents interleaved renderers from leaking a heatmap or mask into an
ordinary material. Geometry/count/scalar errors invalidate the GPU cache so the
next valid draw reconstructs it. Persistent scene data remains immutable. Buffer
statistics are CPU counters, not GPU readbacks.

## Evidence

Fresh coherent Release core/render consumers were built with the appended snapshot
layout. The new fixture passes24 assertions on llvmpipe and actual Intel Arc A770.
It wraps the real `glBufferData` symbol to measure actual transfer calls/bytes,
then checks rendered pixels independently. Coverage includes scalar-only transfer,
weight priority, material/texture changes, navigation, changed geometry/generation,
equal raw addresses with different control blocks, invalid counts/scalars,
recovery, legacy fallback, empty scenes and retained authored UV1 across MToon
factor edits.

Four software suites pass in6.27s: buffer reuse24, existing mask/mirror38,
MToon75 and material/picking25. The same four fixtures pass on actual Arc hardware.
Changed renderer.cpp plus the entire new test is compiled with ASan/UBSan and
leak detection against matching coherent Release support archives;24 assertions
pass with no reports. This is changed-code instrumentation, not whole-core
sanitizer coverage. The separate authenticated producer branch owns whole-core
and actual Document identity tests.

Logs in the parent artifacts directory:
- build-gpu-buffer-reuse.log /build-gpu-buffer-reuse-uv1.log
- tests-gpu-buffer-reuse-regressions-software.log
- tests-gpu-buffer-reuse-{uv1,masks,mtoon,visibility}-arc.log
- build-gpu-buffer-reuse-changed-sanitize.log
- tests-gpu-buffer-reuse-changed-sanitize.log

No full-workflow/device/frame-rate or complete local-meshing claim follows.
Document still flattens/copies complete packed arrays; geometric edits still
replace the whole geometry buffer. Authenticated local extraction is integrated in the private CPU adapter. Bounded
tile assembly/seam ownership and incremental per-tile GPU updates remain required.

## Combined application and plane cuts

Application source `cf89a81`, followed by test-only `91ceb04`, preserves remote
`a8af3f8` plane cuts and the authenticated producer. A cap preview owns a distinct
immutable geometry identity because appended cap vertices/indices differ from
the original source. Each changed capped plane gets a new owner; both eyes of
one immutable preview reuse it. A cap-free preview retains the source identity
and changes clipping uniforms. Cancel restores the original snapshot and key.

The actual Document-to-GL fixture now passes22 software assertions: original
mask/material/paint/geometry/transform cases plus cap ownership, original mask
and cap sentinel preservation, second-eye reuse, changed-cap uploads, cap-free
uniform-only changes and exact cancellation pixels. The fixture deliberately
uses planes clear of source vertices; vertex-touching contours remain explicitly
unsupported by the cut kernel. Combined hardware, full native/GUI/XR and whole-
core sanitizer results are recorded separately in VALIDATION.md after completion.

The pre-cut `42b2ff3` source passed78 native suites, CPU and actual-Arc OneAPI
140-check synthetic XR runs,15 Arc native suites,15 actual Document/GL and24
buffer assertions, plus native menu/compositor workflows. Its immutable evidence
is in artifacts/local-gpu-final-manifest-42b2ff3.json. These are prior pinned
results, not substitutes for the combined cut checks.
