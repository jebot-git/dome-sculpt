# Radiance-preserving Bake integration candidate

This source candidate extends typed-field milestone `60ac081e`. It is private,
unadmitted and awaiting a completely fresh core/dependency build. None of the
previous standalone579 checks are evidence for these changed public types or
new adapters. No old object/archive ABI reuse is permitted.

The candidate adds an independent immutable `BakedTextures.emission_field`.
RGB8 emission is empty when that field is present. Other baked channels,
normal authorities and editable/hidden layers retain their owners. Old RGB8
base promotion retains its relative coefficients; independent FLOAT base
merges do not round through the common R8 coverage map. Read preflight counts
all FLOAT retained bytes and polls the existing native read cancellation scope.

Scene41 is an exact three-key `{format,version,payload}` envelope around one
actual prior Scene2–40 record. A private stack capability passes explicitly
through the owning decode contexts. The normal old scene capability predicates
remain intact. A valid new FLOAT field must actually be decoded before Scene41
can return. Empty/fake envelopes, stripped capability, direct nested41 records
and competing RGB8 authority refuse. Baked6 retains an explicit base1–5
interpretation, original colour/normal identity and exact FLOAT field bytes.

The Bake source sampler captures immutable actual paint packets, geometry,
original role coordinate/image/sampler owners, current material factors and
typed strength. Projection-hit sampling evaluates those owners, not RGB8
physical previews. New results hold resolved absolute radiance; projected and
padded hits own FLOAT coverage1, while misses/protected/outside retain zero
new coverage. Old nonphysical Bake output stays on the old path. Dense new
retained channels are preflighted against the unchanged128MiB paint budget.
Existing unsupported retained-native/frame, UV-transfer, source-normal and
polygon capability guards remain; this candidate does not claim those older
independent workflows have acquired new adapters.

Color preparation and canonical normal conversion use actual emission-free
other-channel images for their existing quality checks, retaining the real
FLOAT base and all editable/hidden emission in the native model. Deep source
and normal-field archive replay must prove those transitions. This is a source
implementation, not a claim that the earlier physical colour-preparation
limitation has already passed its actual regression gate.

The GPU compiler uploads owning seven-FLOAT bytes directly into its existing
coefficient buffer. No dense host repack, new sampler slot, product readback or
UNORM8 emission authority is introduced. Ledger forecasts include the exact
new input bytes and omit the removed RGB8 plane; old resource limits remain.
EXR interpretation2 uses actual lossless ZIP FLOAT32 channels for arbitrary
finite resolved radiance; interpretation1 retains its exact old metadata,
wire behavior and editable-paint bounds. Native GLB output retains its original
native owner and reports its explicitly measured ordinary fallback separately.

New independent planned controls are `baked-emission-native`,
`baked-emission-workflow`, `baked-emission-exr` and `baked-emission-gpu`, alongside
the original `baked-emission` field controls and relevant old regressions.
The workflow uses the real Document worker, ordered Save, Undo/Redo, fresh
reopen and actual EXR/GLB adapters. It does not manufacture UI success via a
callback/observer. The GPU oracle uses independent binary64 source/layer math
and fixture-only FLOAT reads. Software/hardware execution requires separate
coordination and explicit evidence. Full actual UI/XR owning consumers still
need fresh builds and public-workflow qualification after native closure.
