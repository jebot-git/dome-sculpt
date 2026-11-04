# Canonical conversion: real Clear and rebake counterexamples

The private Scene31 conversion is **not ready for main admission**. Actual
`Document::clear_bake()` produces different images when called before versus
after conversion: maximum channel errors are 7 and 74 in the fixed-camera
96px variants, above the unchanged bound of four. Both resulting objects have
no baked payload; only the converted object incorrectly retains the resolved
normal as an intrinsic projection.

This is not only a Clear command bug. A real partial rebake from a cropped
source with the same original normal direction also changes the image by seven.
Missed regions must preserve the previous baked detail. The old conversion
empties the baked normal maps and installs their fused result as intrinsic
detail, so subsequent bake merging uses the wrong field and coverage.

The read-only native probe retains both failing runs, actual worker calls,
images, complete compile/link commands, exact archive and SDK hashes, and four
input Scene hashes matched against the sealed owning-conversion receipt.
See [the receipt](validation/CANONICAL-NORMAL-CLEAR-AUDIT.json). These are real
negative controls, not positive qualification or a sanitizer/device claim.

The required repair separates original intrinsic normals, durable attribute
provenance and the active baked-normal field. Normal coverage and filtering
must be independent of material coverage. Clearing drops the active field while
preserving original material/source/history, and partial baking combines with
the actual previous normal field. A Clear-only reset, fabricated neutral
intrinsic plane, tolerance increase or permanent command refusal is insufficient.
Scene33/material15 are reserved for this layered representation; Scene32 and
material14 remain reserved for the separate native-paint work. No new format or
layout is implemented in this audit commit, and existing Scene31 files remain.
