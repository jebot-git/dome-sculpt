# Polygon and prepared-color authority

This fixes integration boundaries between Scene12 polygon sources and color3
preparation. It extends the existing application and adds no public object
layout, archive version, replacement frontend or implicit source-data deletion.

A standalone color receipt has no polygon authority field. Its original paint
must therefore refuse PolygonAuthored policy, including PA hidden in a retained
before-binding UV origin. Cheap publication checks enforce that boundary too.
Standalone source-aware UV baking/adoption cannot borrow an unrelated current
polygon witness. Proper Scene12 current and retained PA paint continue to use
their own exact REST/index/UV authority.

The native **Prepare source colors** bake option has a representable positive
path: validate the target's own active polygon derivative, rebuild the **same**
mesh and UV grid using the real Strict raster validator, and retain the exact
original PA paint and its direct prior authority in an owning PolygonOrigin.
Only then create the color receipt from the proven Strict pre-color state. The
existing shared origin-capture implementation serves both topology conversion
and this bridge, so later placement provenance has one capture seam to extend.
Strict conversion, color preparation and bake publish in one Undo transaction.
It does not unwrap overlapping UVs; those fail with explicit unwrap guidance.

Every retained PolygonOrigin now pairs its current projected paint with its
matching preparation and image identity. Deep checks also verify that field
against its current mesh and appearance, instead of validating only the initial
receipt or normal frames. Missing bindings, unprepared paint, swapped accessors
and equal-valued but differently owned projected images are rejected. The
Scene12 aggregate preflight charges retained PBR paint tiles and baked PBR planes
alongside common channels before decoding or allocating them. Shared projection
images remain in the existing global image budget; no new ordinary-mesh cap is
introduced.

The new native fixture passes 40 checks: malformed/unbound/overlap refusal,
original/before-binding policy admission, retained source pairing, exact quota
boundaries, actual Document bake with non-flat normal data, one Undo/Redo, native
UV Scale/Apply, save/new Document reopen, GLB export/parse, other-hand painting of
the physically held prepared target, readonly ghost-source preservation, motion
bake gating and a second held save/export. Twenty existing CPU regression suites
pass with the amended source. Both actual GLBs pass Khronos Validator with zero
errors. They retain its existing generated-tangent-space warning for a native
normal-mapped mesh without an explicit TANGENT accessor; this is not a universal
normal-portability proof and no diagnostic was suppressed.

The same 40 checks pass ASan/UBSan/leak detection without suppressions. All own
core and bundled vendor code is instrumented: the six changed translation units
are linked before the parent's byte-frozen, fully instrumented `84d9fe6` archives.
The link map verifies that none of their old archive members was extracted.
Public headers/layouts match the frozen source. This qualified overlay is
separate from the later complete integration build and frontend/device runs.
See [the validation receipt](validation/POLYGON-COLOR-AUTHORITY.json).

For the final coherent frontend, the existing native test accepts
`DOME_SCULPT_BAKE_COLOR_POLYGON=1` to use this genuine PA fixture; combine it with
`DOME_SCULPT_BAKE_COLOR_COMPACT=1` for the double-size 640×360 workflow. This
checkpoint supplies that fixture mode, without claiming those new GUI or OneAPI
executions have already run.

Arbitrary raw COLOR_0 corner interpolation/provenance, prepared common/PBR layer
transport through topology changes and the broader original-frame normal-bake
route remain required. The Strict bridge proves the unchanged mesh/UV case;
guards are staging boundaries, not completion of those larger workflows. It
never fabricates CreatorAppearance, relabels an authored accessor as Identity,
samples a texture only at vertices or divides by zero color/alpha to recover an
unrepresented field.
