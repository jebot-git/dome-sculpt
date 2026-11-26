# HDR paint bake across independent UV layouts

Release candidate `460a30ee` retains the original physical paint coefficients
when the target has an exactly corresponding affine UV0 layout. The worker
certifies all ordered mesh corners with exact binary32 arithmetic and retains
the existing supporting-face, projection-direction and chart overlap checks.
The source grid keeps its original sample locations. The target keeps its own
UV0, resolution and editable layers. PBR and MToon apply the certified transform
when reading the existing retained GPU texture.

Native Scene43 and editable GLB keep the existing identity record for coincident
coordinates. A mapped record carries version 2 and both affine rows. Native
Open recomputes the map from the actual geometry and frozen source. Editable
GLB verifies the emitted target UVs and the source FLOAT image and rejects a
rehashed coordinate substitution. Standard glTF continues to carry a sampled
fallback evaluated on the target grid.

- The actual Bake worker, ordered Save, reopen, Undo/Redo and GLB worker
  complete all 20 native controls on a mirrored and sheared target. Across
  3,969 independent source samples at each of three stages, the largest
  difference is 2.38419e-7. The identity-coordinate control differs by 3.24298.
- The original identity render case passes all 4,422 checks in software.
  The mapped renderer passes all 4,423 checks each in llvmpipe and Intel Arc
  A770. The source texture coefficients match exactly and the maximum image
  error is one 8-bit RGB level across PBR and MToon before and after target
  paint.
- The source-closure and historical test passes all 22 checks. The genuine
  older Scene43 file opens and saves byte for byte. Khronos glTF Validator
  reports zero errors on the new editable GLB and one warning about generated
  tangent space.
- The original identity export passes all 16,400 checks. Its direct,
  second-cycle and worker GLBs each match the earlier broad-regression
  build's file byte for byte.
- A fresh 233-action ASan/UBSan build passes all three selected cases with
  leak detection enabled. The native case repeats 20 controls and the two
  software renderer cases repeat 4,422 and 4,423 checks.

The two tests for HDR emission from independently sampled original PBR and
MToon images still reproduce their 0.015625 source-fidelity error. Their source
roles require separate continuous image and paint evaluation. The mapped
candidate covers certified affine layouts and a constant emission background.
The scoped optimized App builds with the canonical DomeD FLTK and bitmap
runtime. See [the validation receipt](validation/mapped-emission-release-20261126.json).
