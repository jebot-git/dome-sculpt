# Private painted VRM1 authoring package

This component adds an explicit Scene-owner route for painted VRM1 while keeping
ordinary Avatar/GLB export guards unchanged. It is based on public `a2e2780` and
the sealed `8ecc412` native-package attachment seam. It is not a full VRM1 or
application admission. The machine receipt is
[VRM1-PAINTED-AUTHORING.json](validation/VRM1-PAINTED-AUTHORING.json).

## Contract and implementation

`export_vrm_authoring(NativeAuthoringExport, path, VrmExportOptions[, SourceUvBakeOptions])`
requires an immutable native Scene and explicit portable-fallback permission.
The capture derives its original surfaces internally. Only its private sealed
compilation can enter the painted V1 writer; caller-supplied surfaces or a bool
cannot authenticate a rewrite. Existing option layouts and native Scene schemas
are unchanged. The new `EditableAvatar256/512/1024` enum values select this route
through the real Document FileJob. They export the selected avatar's declared
subset and required dependencies, preserving unrelated objects in the project.
The direct API can explicitly preserve the whole Scene, including hidden data.

The compiler samples genuine typed V1 role/normal frames through the existing
bounded source-UV worker and evaluates immutable sparse paint contribution
planes directly. It does not substitute a finite CorePBR material for MToon.
Base colour, aliased shade, and legacy emission use independent linear
contribution/coverage algebra. The original shade alias is authenticated before
resampling creates separate image owners. Standard PNG encoding is explicit,
with measured linear quantization errors in the export diagnostics. Original
metadata, source images, skins, morphs and source-node identities remain owned.
The final V1 node/material/morph/metadata remaps finish before the native Scene
is attached and authenticated against the emitted JSON/BIN/resources.

The native package uses the existing version1 authoring container and explicit
non-lossless portable-view declaration. No fake EXR/HDR resources or VRM0 tags
are introduced. Exact native restore and standard-view fidelity are separate
claims. Core occlusion/MR fallback fields do not become MToon shading roles.

## Closed qualification

Both optimized Release and asserted ASan/UBSan/leak profiles pass **762 data and
worker checks plus22 image checks**, in66.963s and148.464s respectively. Each
cohort hashes2296 source/SDK/support inputs before and after with no change.
These are matched component consumers: complete native-authoring, V1 writer and
Document replacements plus the two new compiler TUs, linked to copied immutable
current-cage/FBX support. They are not fresh whole-app builds. No public data
fields change; the Document header appends enum values only.

The actual native worker imports an owning Source4 avatar, paints colour and
legacy emission, queues Save, exports the selected avatar, opens the package as
an unsaved native project, and produces an identical second export. A real
fsync gate pauses after temporary file output and before publication. Shared
Cancel reaches both running and queued exports; prior files, following full
native Save and retry remain exact. Independent parsed geometry verifies two
real skin bind spaces, the rigid child, morphs and all ordered corners within
6.144e-8m against a fixed2e-6m bound. Whole-scene API recovery is exact, including
its hidden original object. Subset worker recovery is compared to its declared
native dependency closure, not mislabeled as the whole project.

The independent parsed V1 image consumer evaluates actual skins and morph
normals with separate matrix/accessor math and pinned MToon1 base/shade/emission
GLSL. Three weights0/.35/1 have1312–1412 visible pixels, maximum error1 against the
unchanged4 bound, exact recovered-native pixels0, and real morph sensitivity.
This nominal image fixture has constant original role images plus nonconstant
paint; it does not qualify arbitrary minification or every MToon role. The
other metadata fixture retains genuine MatCap data but has no MatCap image
comparison claim. Actual strict PCI03 Intel Arc A770 passes the same22 checks
and three images in2.376s; forced software `--hardware` refuses.1538 input hashes
are exact. This is GL/rendering, not GPU baking or physical HMD proof.

The three unchanged source-export, worker and replay consumers also pass in
both profiles (16.267s/44.944s) against the updated core. Configuration registers
611 tests and a real painted-model fixture dependency; no new611 full run is
claimed. The optional image target is conditional on the renderer target.

Every25 emitted containers across Release/SAN/hardware have **0 Khronos errors,
226 unfiltered warnings and400 infos**, validator2.0.0-dev.3.10/maxIssues0. All31
actual GLB files, including6 original fixture controls, pass236 pinned official
VRM1/MToon/spring/constraint schema instances with0 errors. The unchanged
original controls retain24 Khronos empty-children errors; the complete validator
cohorts correctly return1 for those inputs. They are not attributed to outputs.
Two non-container Cancel sentinels are explicitly excluded. All15 R/S path
pairs (14 containers and the sentinel) are byte-identical.

## Preserved negatives and remaining work

The old actual painted-Avatar worker counterfactual remains in the earlier
`vrm1-painted-d639` proof. Current ordinary writer refusal remains checked. This
component preserves its initial test compile error, a consumer restriction on
unused PBR occlusion, and the RGB223 comparison against the wrong native PBR
view. The final test explicitly requests `authored_shading()` and asserts the
state, snapshot and every range are authored; export data and bounds were not
altered to repair that test mistake.

Animated colour/UV expressions on rewritten roles still refuse atomically,
including through the actual worker and following Save. Physical/HDR typed
emission, incompatible prepared normal/colour interpretations and unsupported
REST/physics/constraint transforms retain their real guards. The unchanged
source graph and spring/constraint topology are preserved; no simulation is
claimed. Native menu exposure, a fresh coherent application/UI qualification,
painted licensed external VRM1 examples, new-domain persistent allocation-fault
sweeps, all-role images and broader animation,
physics, HDR and sampling interoperability remain required. Scene30 recipe and
other private lanes are not imported here. No system installation, session or
headset action occurred; the GPU lease was released.
