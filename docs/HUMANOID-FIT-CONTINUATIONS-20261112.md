# Anatomical hand and finger fitting — 2026-11-12

This private child of `4977fb32` corrects the geometry used by existing native
humanoid landmark fitting and automatic binding. It extends the existing
FLTK/XR worker commands, with no new frontend, public layout, Scene schema,
solver, default binding method or resource limit. Product changes are confined
to `src/avatar.cpp`, `src/rig-fit.cpp` and one private continuation helper.

The old fitter gave terminal hands and finger phalanges the body's upward
axis. These are actual binding segments and local pose frames, not merely guide
lines. In the bundled female/male VRoid models the resulting distal axes have
cosines between -0.040 and -0.065 against original fingertip end nodes; palm
axes have cosine about 0.124 against the middle-finger base direction.

The fitter now prefers a reviewed MiddleProximal landmark for the palm and a
reviewed Toes landmark for the foot. Terminal fingers continue their own
reviewed parent phalanx. A hand without MiddleProximal continues its incoming
forearm direction, preserving the explicit terminal length; it does not select
an arbitrary first finger. A foot without Toes retains the previous explicit
body-up convention. No missing landmark or source node is manufactured. Exact
head/joint positions, source geometry and the hierarchy remain authoritative.
The terminal length option applies only to an inferred terminal; an explicit
child landmark determines the actual segment endpoint and length.

The authoring adapter measures each inferred terminal's actual world axis under
the complete object/group affine transform. Thus the requested world length
remains correct for fingers as well as body-up bones under nonuniform scale,
shear and reflection. Existing landmark-to-landmark lengths stay unchanged.

The original multi-source surface-graph `automatic_bind` is unchanged. A native
reader consumes all original POSITION/NORMAL/index/node arrays in both bundled
CC0 assets: female 90,713 vertices / 14,804 triangles, male 90,958 / 15,176,
including unused vertices and all 54 original humanoid roles. Original artist
skin values are not weighting inputs. In 33–34 visible vertices around each
index/middle fingertip, mean distal support changes as follows (left/right are
nearly symmetric):

| Source / tip | Old segment | Corrected segment |
| --- | ---: | ---: |
| Female index | 0.349 | 0.785 |
| Female middle | 0.363 | 0.780 |
| Male index | 0.347 | 0.786 |
| Male middle | 0.362 | 0.780 |

These are measured support values, not a claim of anatomical ground-truth
weights. The final test uses the unchanged default binding work/distance/
influence settings. The earlier diagnostic uses a separate independently
reconstructed reference rig; its sources and results are retained.

The new native test independently checks all five terminal finger directions,
explicit palm/toe endpoints, missing-Middle/Toes behavior, a rotated body frame,
reflected/sheared placements, physical world lengths, positive locked weights,
normalization/influence limits, source/morph retention, cancellation, native
ordered Save/reopen and exact single-step Undo/Redo. Actual finger bends are
compared with Rodrigues rotation and weighted position/normal formulas. The
final reader retains actual NORMAL accessor values and applies an independent
cofactor/inverse-transpose transform; it does not substitute constant normals.
Existing imported-source and independent cgltf export regressions complement
these controls. Existing worker allocation-denial and executing Cancel/retry
controls are retained.

Qualification is an audited incremental native closure, not a fresh whole
application build. Release uses the completed current `4977fb32` 193-member core,
replacing two complete owning translation units and verifying the other 191
member payloads. The strict sanitizer route uses the completed fresh `5bb34971`
193-member core with byte-identical public headers/assets/third-party data,
recompiles the two current query owners plus the two fitting owners, and verifies
189 unchanged member payloads. Actual source/private dependency/flag/asset macro
proofs and fresh matching consumers are recorded in the validation receipt.
The old provider asset string literal is deliberately preserved and its actual
asset files are compared and pinned; unrelated `-D` flags are not normalized.
ASan/UBSan use leak detection 1, halt on error and no suppressions.

All failed and intermediate evidence remains separate. Initial new fixtures
failed for a missing type-definition include and overlapping synthetic UV
islands. An old controller fixture selected slider endpoint 3 (Surface Heat)
and then expected Transfer index 1; the same object fails identically against
the untouched `4977fb32` core. Its test-only correction selects index 1 using
actual control bounds and retains every native transfer/history assertion.
An export runner initially omitted its required output-directory argument;
the same ELF passes with the generated CTest command. Configure-only sanitizer
audits exposed absent-ELF CTest JSON command omission, an output-namespace typo,
and actual CTest name aliases; their raw failures are preserved. No original
status is relabelled by later successful runs.

This remains a bounded fitting-quality improvement. Automatic geometry-only
landmark recognition, general existing-rig mapping/rest transport, absent-Toes
foot inference, arbitrary bent-hand geometry, volume-aware production weights,
additional deformation-quality coverage and physical-controller acceptance
remain required by the full goal. No GPU, GUI, physical HMD, system installation,
session restart or production performance result is claimed here.

Both profiles pass all eleven selected native suites; the final new test passes725
checks in each. Existing worker tests pass2001 allocation-denial boundaries and51
cancellation boundaries in each. Release is explicitly a union of qualified
individual runs; the original failed cohort statuses remain intact. Strict SAN
passes all38 build/archive/link/runtime steps in one current closure. Read
[the validation receipt](validation/HUMANOID-FIT-CONTINUATIONS-20261112.json)
for exact scope, source/provider hashes and the fully decoded evidence archive.

The evidence can be verified without executing archived code or extracting files:
`python3 docs/validation/verify-humanoid-fit-evidence.py docs/validation/HUMANOID-FIT-CONTINUATIONS-20261112.json`.
