# HDR baking with an independent original image

Candidate `08936bd8` retains the original HDR emission image and the writable
source paint as two continuous inputs. It certifies each map from destination
UV0 against the source's actual ordered corners. PBR uses the transported
original-image UV set. MToon uses the logical authored set and its static
material transform. Each image retains its own dimensions, sampler and colour
encoding. The renderer filters the image and premultiplied paint separately
and applies paint coverage after sampling. The original texels and paint
coefficients share the existing retained GPU texture unit.

Native Scene43 and editable GLB preserve both coordinate maps and the sources
that own them. Replay recomputes the geometry proof and rejects coordinate
substitution. Standard glTF retains its measured display fallback. Native
replay recovers the original image and both editable paint grids.

- The two previously failing PBR and MToon source-role tests pass all 21 and
  62 controls. They cover actual Bake, ordered Save, native reopen and exact
  Undo/Redo. The missing logical UV1 cases still refuse atomically. The PBR
  oracle covers 2,048 samples with maximum absolute error 1.19209e-7.
- The separate image and paint GLB test passes all 23 checks. It verifies
  3,969 independent source samples at Bake, native reopen and GLB replay, with
  maximum error 2.38419e-7. A rehashed change to the recorded image map is
  refused. Khronos glTF Validator reports zero errors and one warning about
  generated tangent space.
- Intel Arc A770 and llvmpipe each pass three image-render cases with 4,425
  checks per case. The cases exercise linear, nearest, and rotated MToon
  sources in both target shaders. The maximum difference is one 8-bit RGB
  level and the extracted source coefficients match exactly. Software also
  passes the 4,422 and 4,423 original identity and affine checks.
- The ASan/UBSan core passes all five selected native cases. With the
  corrected shader, all five software render cases pass under ASan/UBSan
  with leak detection, including the odd-width mirrored edge.
- The ordinary UV1 worker passes 180,375 checks and historical archive replay
  passes 22. The original identity GLB export passes 16,400 checks. Its
  three files match the earlier broad-regression outputs byte for byte.

The first MToon fixture supplied a duplicate transform on the resolved role.
With the transform on the authored material, the CPU oracle agreed and the
shader showed a 54-level error at an odd-width mirrored edge. Using a
nonnegative remainder for wrapped texel addresses corrected that edge. The
initial logs, causal pixel observations and revised renderer are pinned in
[the validation receipt](validation/original-image-emission-release-20261126.json).

The certificate covers static images and exact affine layouts on the
supported coincident source and target geometry. General correspondences
remain open. Candidate `08936bd8` builds against the canonical DomeD FLTK and
bitmap runtime. A fresh optimized build of the broader software regression
cohort passed. CTest finished with 57 passes, 29 failures and zero skips in
231.98 seconds. All 29 failures belong to the earlier suite's 65 failed or
dependency-skipped tests. The current run clears 36 of those 65, and all
21 additional selected cases pass. Raw CTest and JUnit results are under
`artifacts/production-original-hdr-regressions-release-20261126/`. The complete
1,349-case suite remains to be run on this source.
