# Holding and shared import integration

This extends the existing native C++20 FLTK/OpenXR editor. The complete production
objective and every unfinished row in REQUIREMENTS.md remain required. The
preceding published integration is recorded in PRODUCTION-INTEGRATION.md.

## Behavior

- Objects have a native individual visibility list. Explicit object-grab mode
  supports desktop dragging and XR one/two-hand holding; ordinary grips retain
  workspace navigation by default. Ghost isolation makes other objects translucent
  and restricts mutations to the held object without rewriting user materials,
  locks, visibility or export data.
- Holding uses per-object GPU transform deltas over immutable packed geometry.
  Picking follows the same current transforms. Authority epochs, protected COW
  checks and source identities reject stale edits. The other hand can sculpt,
  paint or edit weights on the held object. Accepted local weight work finishes
  before the delayed transform commit; world-distance asset work requires motion
  to stop. A retained read-only ghost can still serve as a bake source.
- FLTK and spatial Import/options pages share format, units, axes, normals,
  materials and paint settings. The native OBJ/FBX adapter canonicalizes geometry
  and node translations into metres before local-coordinate limits are checked.
  Original format bytes and captured resources remain separate and unchanged.
- The import browser performs bounded directory work on its worker. Creation
  captures edit authority before the native chooser and options; Apply rechecks
  it. Active editors, moving grabs and pinned isolation cannot admit conflicting
  creation. Browse/configure/select/Cancel publication is transactional: request
  cancellation advances only after successful commit, and failed publication
  removes its staged job. An 81-boundary allocation fixture observes even transient
  request changes, plus actual stale-chooser and active-UV-editor refusal cases.
- Spatial BDF text explicitly sets HarfBuzz's 26.6 pixel scale. This preserves
  glyph metrics with the current native BDF-only fork and upstream hb-ft. All
  fallback faces must also be native, nonscalable BDF. Exact 1x/2x bitmap pixels,
  advances, Unicode fallback, bidi, grapheme clipping and palette rendering are
  tested with both runtime variants.

The integration unions the existing procedural tools, projection-policy flags,
robust Mikk/atlas fixes, source receipts and authored glTF binds. Spatial pages
18/19/20/21/22/23 remain procedural/polygon/holding/import/polygon/import-options;
the shared admission bound is page23. No polygon editor is claimed by reserving
its pages.

## Qualification and limits

Exact final build, source, runtime and test receipts are in
validation/HOLDING-IMPORT-INTEGRATION.json. All public-header consumers are built
coherently; old agent archives are not linked into this candidate.

Final code69bd085 passes CPU151/151 and actual Arc OneAPI152/152 native suites,
44 selected whole-own C/C++ ASan/UBSan/leak suites, all113 native FLTK assertions,
and current DomeD Wayland/universal-menu8/8. All four real-loader synthetic XR
suites pass on both builds (150+17+11+11 assertions). The BDF1x/2x Unicode/palette
regression passes with stock and native-BDF HarfBuzz, under sanitizers, and on
actual Arc graphics. Eleven direct Arc model/holding/consumer fixtures passed
at parent9c74319; their implementation is unchanged by the font-only followup.
These are scoped checks, rather than all-suite sanitizer or physical proof.

Canonical DomeD is 6feb62b936a9d8d2a2e6a893aebcbda9b724fe23. Its FLTK builder and
all22 patches are byte-identical to 282467e, so the optimized prepared FLTK1.4.5
artifact remains current. The new compositor and native BDF-only HarfBuzz/Pango
runtime are independently built and checked. The private Wayland harness accepts
an explicit test library path and records both live processes' library maps;
inherited overrides remain cleared by default. See TOOLKIT.md.

The synthetic OpenXR fixtures exercise real loader/session/image ownership and
existing workflows with software GL eye rendering; OneAPI compute executes on
Arc. They are not a physical-device test or a new complete real-loader holding/
import authoring scenario. Native/controller and direct GPU fixtures separately
cover those additions. Only the old8f4d67d rudimentary milestone has physical
headset confirmation. No system installation or active-session restart occurs.

Full FBX skins/blends/materials and affine/source poses, glTF missing-normal
adoption and other unsupported interoperability cases remain required. Full
Wings3D-style polygon workflow/persistence and general colored/deforming baking
are separate private work. The installed Level Zero driver's permanent global
heap-blackout failure remains documented in PAGED-VOLUME-ALLOCATION-VALIDATION.md.
None of these limits is a declaration that the full production goal is complete.
