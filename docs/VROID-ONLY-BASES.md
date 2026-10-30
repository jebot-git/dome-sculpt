# VRoid-only humanoid bases

New humanoid creation uses only the verified historical CC0 VRoid beta female
and male models. The FLTK creator offers Female (CC0) and Male (CC0), under
VRoid base. The XR avatar page offers the same two models; its former MakeHuman
sector now centers the selected part. The old command enum value is reserved
and disabled, avoiding a numeric command renumbering.

The 3.3 MB embedded MakeHuman mesh/target/skeleton/weight array file and its
offline converter are removed. Historical license, credit and hash records have
moved from installed creator assets to docs/references/makehuman. The original
shared native groom implementation remains useful for the VRoid workflow.
Only the two unchanged verified CC0 VRoid model files are staged as model assets.

Legacy HumanoidControls schema1 and schema2 records still decode their exact
saved values. Existing projects retain their own geometry, paint, rig, morphs,
groups and transforms; no model is replaced automatically. Attempting to create
a new model from a removed MakeHuman recipe fails before publishing a Scene
change. Opening Create humanoid from a legacy group starts a new female VRoid
draft, visibly selectable as male, with Keep existing objects checked.

## Qualification

Base: `83723fe0e2709bd65a09f64ad98ba99dd08259b2`. Fresh regular-user optimized
Release build includes CPU core, FLTK UI and OpenXR. Native UI runs use the
DomeD toolkit snapshot `282467e` (FLTK 1.4.5 plus22) and isolated Xvfb/DBus;
no active desktop session or headset is restarted.

Six targeted native suites pass in 19.14 seconds: humanoid creator, grouped
creation/controller authoring, original VRoid adapter, VRoid groom, conformal
UV solver and UV initialization. They cover both actual CC0 bases, all three
style presets, source identities, source/native Scene restart, controller female
and male creation, exact undo, cancellation and removal guards. A self-contained
pinned two-triangle winding fixture replaces the removed MakeHuman mesh-based
foldover test, retaining the rejection assertion without bundling another mesh.

An actual pre-removal five-part MakeHuman project was generated externally using
the frozen old d3e5e61 build. It has its original163-joint rig, anisotropic hips
and .35 facial morph. The new build loads, re-saves, restarts and exports it.
The pre/post GLBs are byte-identical SHA256
`6d2e3dc3c30da0bcdce3c7be97e19b0f133ba9f0de66a116c7b3869e7a4ab3ed`;
both pass Khronos validation with zero errors and warnings. This legacy model is
an ignored validation artifact, not a bundled creator/default/test mesh.

Actual native creator GUI runs pass11 checks at ordinary size and13 at640x360
with2x Unifont. Each starts from that saved legacy project, selects female and
male through the real FLTK choice, creates nineteen source parts, saves and
checks the old object payloads/recipe remain exact, then checks exact undo/redo.
Both choices fit the large-font dialog. No hardware XR or new sanitizer-suite
claim is made for this focused removal; the controller workflow uses native
synthetic input and the UI uses software OpenGL.

A regular-user staged install contains only base-female.vrm and base-male.vrm.
The core symbol audit has no removed MakeHuman generator or mesh arrays.
The source/binary/asset/test manifest is validation/VROID-ONLY-CHECKPOINT.json.
No system installation, push or shared primary index change was performed.

Reproduce focused native tests with `DOME_LEGACY_HUMANOID_SCENE` pointing at an
existing external native MakeHuman project to include the actual-project check;
the default test also exercises metadata compatibility without such a fixture.
Run the native menu fixture with DOME_MENU_CREATOR_ONLY=1 and optionally
DOME_MENU_CREATOR_COMPACT=1; DOME_CREATOR_LEGACY_SOURCE supplies the external
legacy project, otherwise a mesh-free legacy metadata project is generated.

General colored-target bake receipts/UV adoption and COLOR/UV morph channels
remain separate required work, preserved in their parked private prototype.
