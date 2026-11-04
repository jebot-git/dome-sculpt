# Native and controller HDR emission controls

Private runtime `c93cb87` adds actual brush units, linear RGB and current material
radiance controls on qualified physical-producer base `1f857b6`. The original
source RGB factor and emissive strength are displayed separately, read only.
Material values and physical brush values span exact zero through 10000; an
explicit switch to relative units asks to normalize values above one. Small
values such as 0.002 remain actual floats. Brush settings are ephemeral; material
edits retain the leased object transaction and one Undo action. An already active
absolute emission background is edited in its own field without rewriting the
original material/reference. Creating or clearing that separate override remains
future UI work.

Desktop Object/material and Brush dialogs share these units with XR page 30
(Controls→Emission/HDR, explicit commands 16384+). Existing command identities,
Export 28 and reserved Bind 29 stay intact. The old object-properties wrapper is
retained. This isolated registry has disabled page 29; a combined bind workflow
must compose its actual controls and tests rather than overwrite that page.
Counted registry users were rebuilt together, including the complete app/core/
render/XR consumers. No stored schema or public data member was appended.

Physical quick actions exposed a genuine persistent-page ownership bug; the
Document now retains page 30 consistently with the input owner. The final bitmap
counterfactual also proves an old stale physical-units checkmark. The palette
cache now includes units and control interpretation bounds/components; it still
uploads only when its actual signature changes.

## Closed evidence

The final affected suite passes 8/8 Release and 8/8 whole-owned ASan/UBSan/leak
profiles. The base rebuilt all owned C/C++ core/support/render/native app code;
the final continuation rebuilds changed palette code and all affected consumers.
This is selected qualification, not a full all-target/CTest claim. The controller
fixture executes 31 actual input-thread checks: low/high RGB rays, zero, explicit
normalization, Cancel, focus/tracking loss, carried-pressure Close, material
Undo/Redo, native float stroke/ordered Save, existing absolute-background edits
and exact editable-project recovery.

The actual final FLTK app passes 34 normal-layout GUI checks per profile:
Import chooser→default Cancel→real 64-pixel atlas import→material 8→24/Undo→Physical
brush 12/0.002/3→pointer stroke→editable 512-pixel GLB→Open→native Save. Raw bytes,
factor/strength and the native radiance product 8/0.016/4 remain genuine. Native
float paint is unclamped and complete recovery equals that run's native project.
The 640×360/font 32 compact layout passes 29 checks per profile for material/brush/
paint/export/recovery, using actual focused keyboard traversal of scrolling
fields; it does not claim compact import. Universal clipboard selection,
application ownership and copied-back text acknowledge each input before commit.
Private Xvfb, D-Bus, HOME and software GL isolate every run. No physical session,
installation or hardware qualification is claimed by these GUI checks.

All four final GUI GLBs have 0 Khronos errors and 0 warnings, with no filters or
issue cap. Six HDR page/control/source-factor bitmap captures are identical
between profiles and visually reviewed. Input manifests contain 1693 exact files
per final GUI run; all before/after drift lists are empty. The receipt records
commands, source/archive/binary hashes, exact terminal results and old negatives:
[EMISSION-CONTROLS.json](validation/EMISSION-CONTROLS.json).

## Reproduction and remaining work

Use the immutable final poll27 toolkit prefixes and canonical 53b bitmap prefix
recorded in the receipt/TOOLKIT.md. Generate a raw fixture with the matching
`dome-sculpt-raw-hdr-authoring-test` in a private output directory; it produces
`raw0.glb` and `saved0-0.dsculpt`. Run `tests/emission-controls-menu.py` using
DOME_SOURCE_AUTHORING_BUILD, DOME_SOURCE_AUTHORING_INSPECT,
DOME_SOURCE_AUTHORING_FIXTURE and a fresh DOME_SOURCE_AUTHORING_OUT. Set
DOME_SOURCE_AUTHORING_COMPACT=0 and DOME_EMISSION_IMPORT=1 for the normal full
import path; use 1 and 0 respectively for the compact control path. The matching
inspector is a separate complete executable from the same core, never an old
archive paired with new headers. Keep the original failed diagnostics rather
than interpreting stale clipboard/input selection as a runtime clamp.

This component remains private and awaits coherent coordinator integration.
Preserving-only/nonprepared raw HDR, retained/native-only physical compiler
domains, general UV/refinement/interoperability and later spatial drawers stay
required. The finite PBR view is explicitly approximate; exact native recovery
does not make it lossless HDR interchange. Existing strict HDR/painted export
guards and authentic package paths retain their prior contract.
