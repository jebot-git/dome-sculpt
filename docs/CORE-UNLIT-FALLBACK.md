# Core unlit fallback conformance

This private component on `13f7e57` fixes a concrete core-glTF rendering defect:
`KHR_materials_unlit` previously added fallback emission to the base color. The
native shader now evaluates the base color term alone. Factor, decoded texture,
linear vertex color, alpha modes, sidedness and editor overlays keep their
existing paths. Typed VRM0/VRM1 MToon dispatch still runs earlier and retains its
separate emission semantics. Authored fallback fields and image pixels remain
in the source and export; no material data is erased.

The [pinned Khronos specification](https://github.com/KhronosGroup/glTF/blob/3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded/extensions/2.0/Khronos/KHR_materials_unlit/README.md)
defines the base-color-only rule. Three parsed comparison shaders contained the
same mistake and are corrected. A new independent CPU double-precision product
checks the native shader, rather than treating agreement between those shaders
as proof. The unchanged old renderer fails this oracle in both profiles; both
old role/prepared readers separately fail their new analytic controls.

Affected optimized Release4 and owned-sanitizer4 each pass252 checks: core unlit28,
existing MToon81, emission43 and source-color100. Both profiles retain all1,498
captured inputs exactly and have no sanitizer report. The two additional
role/prepared-reader fixtures pass8 checks in each profile, with1,509 inputs
unchanged. Their generated tangent accessor is an explicitly authored flat-quad
fixture; the original native-export output remains untouched. These are matched
component consumers: `render.cpp` was rebuilt against exact parent headers and
eight ledger-verified copied archives per profile, with the old archive member
removed. They are not a new full application/all-target or full-regression run.

The original frozen28-check fixture also passes on actual PCI03 Intel Arc A770
in3.961s, with1,498 inputs unchanged and native/parsed image difference0 at the
unchanged tolerance2. No hardware run is claimed for the additional reader-only
fixture. All61 actual positive emitted GLBs pass pinned Khronos
`2.0.0-dev.3.10`, unfiltered `maxIssues:0`:0 errors,54 warnings,4 infos.

The first fixed fixture reached the analytic/alpha/source-preservation controls
but changed an already-retained vertex-color snapshot. That comparison failure
is preserved; the harness was corrected to publish a new immutable snapshot.
Neither the product owner/cache behavior nor the image tolerance changed.
Initial argument-type and missing parent sanitizer-object setup failures are
also retained. See [the receipt](validation/CORE-UNLIT-FALLBACK.json) for source,
binary, archive and evidence hashes and exact scopes.

HDR Scene27 authoring is separate and remains unfinished. This component does
not change layouts, native schemas, import admission or physical exposure. The
inherited standalone-XR Mesa112-byte sanitizer report remains open. No physical
headset, user desktop, system installation or session action occurred; the GPU
lease has been released.
