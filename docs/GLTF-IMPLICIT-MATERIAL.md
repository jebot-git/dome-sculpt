# Source-preserving implicit glTF materials

Native import now accepts a primitive with no `material` property. It uses
the parser's existing default slot0, with `CreatorAppearance.material=0` and
`source_material=-1`. The latter is an authenticated absence marker, not an
authored material index. Material/MToon JSON strings stay empty. No original
JSON, encoded resource, image, attribute, hierarchy or instance is synthesized
or discarded. Current native material/paint edits remain separate from the
immutable original default factors.

The witness is the exact captured primitive without a material declaration.
An unrelated authored material0 must never supply that primitive's appearance.
Native archive validation authenticates both the parsed primitive identity and
the raw absence, and verifies the imported default factors. An explicitly
authored empty material0 keeps identity1/0 and its original `{}` JSON; it retains
the historical schema when no newer feature is present. Unsigned, floating,
other negative and forged indices are rejected. The
[glTF default-material rules](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#default-material)
and [PBR schema](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/schema/material.pbrMetallicRoughness.schema.json)
define the independently tested defaults.

Scene17 is required whenever this meaningful identity appears, including only
in retained UV, color/normal preparation, polygon or shapekey histories. Every
older Scene version rejects it before appearance reconstruction. New metadata
walks have the existing256/512 archive table bounds. The immutable first-source
aliases, raw materials and exact source/paint/frame/deformation state survive
CBOR and disk restart. Scene18 remains separately reserved for owned UV1 data;
the later semantic union must preserve both feature minima.

Final qualification on published base78a1034:

- 548 actual witness/import/archive/minimum-version and retained-origin checks.
- 27 actual native/controller import, current-world pick/paint, source lease,
  isolation, Cancel, one Undo/Redo and ordered Save/reopen checks.
- 66 independent source/default/native and actual-worker GLB export assertions:
  all24 image comparisons error0, unchanged tolerance2. Six deliberately wrong
  authored-material0 comparisons visibly differ, including error255.
- Actual FLTK/DBusMenu static12 and authored-skin12 import flows, with real
  chooser ownership, default Cancel, exact source absence and universal history.
- Final optimized Release12/12 in2.26s; fresh fully instrumented own C/C++ core,
  renderer/support and final rebuilt fixtures SAN12/12 in18.99s, leaks enabled.
  External Mesa, FLTK, image/system and LAPACK libraries are uninstrumented.
- Actual Intel Arc A770, explicit PCI03 selector, repeats all66 assertions and
  all24 image comparisons error0. No physical headset/session changes.
- Pinned Khronos validator2.0.0-dev.3.10 accepts all24 original and emitted GLB
  files with0errors. The24 warnings concern preserved skinned-node hierarchy
  or ignored local skin transforms. Native source and output accessors are also
  independently parsed and validated with cgltf.

Validation exposed test metadata omissions rather than suppressing them. The
unused authored material retains its actual extension declaration, and the
shared fixture now declares exact bounds for its four POSITION morph deltas.
The first official report's12 missing-bound errors is retained separately from
the final0-error report. The earlier reflected export255 led to the independent
published skinned-surface hierarchy fix; this milestone depends on it. Exact
attempts, source and log hashes are in validation/GLTF-IMPLICIT-MATERIAL.json.

This does not complete full glTF/VRM support. Native UV1 import, plain unmapped
overlapping-source-UV authoring, wider extensions, VRM and FBX deformation
coverage remain required. Existing guards for those routes stay. There is no
new archive payload for them, artificial source image or silent UV flattening.
No system installation, push or active-session restart was performed here.
