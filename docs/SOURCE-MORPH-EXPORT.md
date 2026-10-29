# Shared source morph export

The private `SourceMorphTransport` is shared by ordinary GLB and VRM0 export.
Retained source NORMAL/TANGENT morphs no longer have a VRM-only serialization
path. The GLB builder emits both channels alongside native editable POSITION
morphs, using explicit source target/vertex identities and coordinate basis.

A positive uniform scale/translation with the recorded proper rotation preserves
normal and tangent directions. Base tangent XYZ and morph direction deltas rotate;
base tangent W stays unchanged. Original-space VRM output explicitly selects the
already-restored frame, so the source basis is not applied twice. Tangent UV edits,
nonuniform tangent changes, missing identity maps and unknown legacy basis reject
rather than silently losing attributes. Native POSITION-only morphs retain their
previous export semantics.

Unchanged source morphs preserve authored direction deltas through reordered or
compacted native vertices. Changed source NORMAL deltas require an explicit
recomputation policy. That policy computes area-weighted normals from the actual
edited target geometry, using the same convention as native MorphSet evaluation.
It visits only that target's geometry rather than copying/scanning all unrelated
targets. Tangent recomputation is not implemented and always rejects when needed.

The source helper enforces a configurable visit budget (64 million by default,
256 million maximum), cancellation before returning channel arrays, finite
payloads, bounded native deformation dimensions and unique/in-range target
identities. Source assets and authoring state remain immutable. The caller owns
the ObjectSurface for the helper's lifetime; returned arrays own their data.

## Additive public GLB API

`dome/gltf-export.hpp` adds a three-argument overload without changing existing
callers:

```cpp
dome::GltfExportOptions options;
options.recompute_changed_normals = true;
options.cancelled = cancelled;
auto report = dome::export_glb(objects, path, options);
```

The existing two-argument void API stays strict. The report includes mesh count,
recomputed normal-target count and diagnostics. The opt-in policy authorizes
NORMAL recomputation only; it never silently drops or recomputes TANGENT data.
The output byte budget includes JSON/BIN padding and container headers. The final
cancellation check precedes the atomic write. This API supports background worker
status/error handling without introducing another user confirmation flow.

The shared VRM path opts into its reported NORMAL policy after restoring source
coordinates and validating source-local metadata. Ordinary GLB retains native
coordinates. Both paths call the same transport and serialization implementation.
Full editable native morph-normal/tangent authoring and renderer transport remain
separate work; preserved source channels are not a new editable authoring model.

The implementation is original native GPL-3.0-or-later code, reusing the existing
project's geometric normal convention. Functional attribute requirements were
checked against the official [glTF 2.0 morph target specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_morph_targets),
which identifies its published source revision as
`8e798b02d254cea97659a333cfcb20875b62bdd4`. No upstream source, schema, frontend or
runtime dependency was incorporated for this helper.

Focused validation passes 29 native and 29 ASan/UBSan/leak assertions, including
independent cgltf reads of both target direction channels/base tangent sign,
compacted vertex ordering, explicit output frame, normal recomputation, finite
payloads, visit budget, exact output-byte accounting and late cancellation.
The existing general cgltf interoperability suite passes with the coherent
coverage-enabled core. Both actual avatar VRM fixtures retain their 138 assertions
through the shared transport. Logs are `artifacts/source-morph.log`,
`artifacts/source-morph-sanitizers.log`, `artifacts/gltf-source-morph.log` and
`artifacts/vrm-shared-morph.log`. The focused sanitizer instruments the new helper,
exporter and tests, with the coherent Release core dependency.
