# Original-space imported VRM0 export checkpoint

The subsequent [mixed rigid-part component](VRM-RIGID-SOURCE.md) extends this
checkpoint with genuine unskinned parts, durable attachments and their native
pick/paint/held workflows. The original checkpoint scope below remains historical.

This private component extends the existing native exporter. The full production
goal remains active and incomplete. It introduces no public field, archive version,
runtime converter, or new frontend. Root owns integration and publication.

Imported VRM0 parts can now export through one authenticated original-node graph
with their current shared joint pose and morph weights. Each skin retains its own
authored inverse-bind accessor. The exporter remaps humanoid, expression,
first-person and spring/collider references to actual emitted source identities;
unchanged original node TRS, extras, image bytes and author/license data retain
their existing source-preserving paths. It does not run spring dynamics.

The native avatar resolver selects the entire imported source instance. The actual
female model therefore exports all nineteen Face and Body parts rather than only
the active seventeen-part Body leaf. The existing file worker, native chooser,
universal menu, explicit Cancel, history and ordered project Save remain in use.

## Coordinate and identity contract

`vrm-source-graph.hpp/.cpp` is private. Original model, asset, raw byte owner,
source-instance root and source node IDs authenticate every part. Names never
reconstruct identity. Parts must retain the exact imported joint parent/rest
matrix table and inverse mesh-world bind-space matrix. Every view of a shared
source joint must agree on its current local pose. Source graph traversal is
bounded to100,000 original nodes,200,000 emitted nodes and256 ancestry levels.

An unedited original local node retains its exact original JSON transform.
Current local edits and the common native placement prefix use the existing
validated TRS/factor-chain writer; a sheared inverse is not emitted as one glTF
matrix node. Rendered meshes are attached beneath their actual original mesh
node. This preserves mesh-node reflection/front-face and tangent handedness.
Skins share the original-node graph while keeping distinct inverse binds.

The exported advisory `skin.skeleton` references a valid common-root wrapper.
The raw VRoid0 advisory hint has a separately preserved validation error; native
source bytes are never rewritten to repair it. Unused former generated bind-space
nodes remain inert, with no skin or VRM reference to them. Validator information
about unused/default/empty nodes is retained rather than suppressed.

Unique authored bone names and humanoid-role changes are coalesced across skin
views using source node identities. Unchanged auto-generated import labels are
not treated as edits. Conflicting names, role assignments or poses refuse
atomically. Original per-role parameters remain intact; new optional roles use
explicit default values. The test adds a genuine source jaw joint to an external
fixture, then independently verifies authored head naming and optional jaw mapping.
This is not an anatomical auto-rig claim.

## Completed qualification

- Native source/IBM/world-corner tests:108,546 checks across both actual CC0
  female/male models, rest and current pose/morph, plus authored metadata and
  conflicting-edit refusal. Every original inverse-bind float is compared by
  bits; every emitted ordered posed corner is compared independently at2e-6m.
- Independent emitted-file renderer:37 checks/twelve images across both actual
  geometries, ordinary transforms, a reflected skinned mesh node and a sheared
  original TRS hierarchy, each at rest and in current anisotropic pose/morph.
  Pixel tolerance remains2. Software maximum is1; actual PCI03 Arc A770 maximum
  is0 in every image. The deliberate material subset exercises main/shade;
  it does not establish every MToon lighting, outline or physics property.
- Fresh own C/C++ core, renderer and support ASan/UBSan/leak build:three suites
  pass211.35s. The subsequent test-only joystick-Cancel extension is recompiled
  and rerun against those same coherent instrumented archives; see receipt.
- Actual native file-worker running/queued export cancellation preserves seeded
  outputs, removes temporaries, keeps the following ordered Save, supports retry,
  and preserves exactly one import Undo/Redo. A real joystick route selects Cancel.
- Actual FLTK chooser/universal-menu fixture:seven checks, including chooser
  ownership and Close, durable-temp Cancel while authoring remains available,
  unchanged saved project/Redo and retry to independently parsed nineteen-part VRM.
  This is Xvfb/native FLTK, not a physical headset or new DomeD compositor test.
- Existing input publication, generic import graphics, painted export,
  retained-frame VRM consumer and role/expression consumer:five suites pass38.93s.
- Seventeen emitted VRMs: pinned Khronos validator0 errors,340 warnings, no issue
  truncation/filtering; official pinned VRM0 metadata schemas0 errors. Raw input
  variants and their inherited advisory errors remain separate negative evidence.

The tracked `tests/vrm-source-export-menu.py` uses the test-only
`dome-sculpt-vrm-source-export-ui-test` and existing durable-write preload gate.
Set `DOME_SCULPT_TEST_BUILD`, `DOME_VRM_UI_SOURCE` and canonical toolkit/runtime
library paths; optional Python-Xlib/Pillow/DBus packages are test dependencies only.
Core/graphics tests register in CTest. Hardware execution additionally uses
`DRI_PRIME=pci-0000_03_00_0` and requires the captured Intel Arc A770 renderer.

## Required remaining work

This route is VRM0, not a general VRM1 writer. It requires native skin and complete
humanoid authority on every part; static/unskinned accessories must gain an exact
rigid source-node path rather than dummy skin. Independently edited per-part
placements, changed original joint rest axes/parents, new native source-less
bones, inconsistent skin views and nonlinear avatar fitting still refuse.
Conventional creator export remains a separate supported route.

Full VRM1 material/expression/first-person/look-at/constraint/physics transport,
runtime evaluation, broader source graph edits, KHR sampler-transform coverage
and rich persistent allocation-denial qualification remain required. This child
does not absorb the later Scene19 normal-UV/UV1 union; root must merge it into the
current candidate while preserving those newer source and material consumers.
No new physical test, system installation or active-session changes occurred.

Primary source conventions are the retained glTF2 node/skin contracts and pinned
official VRM specifications; schemas are inspected test data, not vendored runtime
code: <https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#skins>,
<https://github.com/vrm-c/vrm-specification/tree/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/0.0/schema>.
