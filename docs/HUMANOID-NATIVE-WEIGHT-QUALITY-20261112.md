# Genuine source-authoring weight quality comparison

All three native methods complete on both CC0 VRoid bases, retaining all19
source parts and a54-bone fitted rig. Before fitting, the fixture physically
removes source skin/JOINTS/WEIGHTS accessors and their binary payload. Artist
values never enter any solver. Independent comparison uses the accepted
`CreatorAppearance` node/mesh/primitive/accessor-vertex identities; no guessed
joint names, nearest vertices or opaque source metadata substitute for them.

| Native method | Female mean shared influence mass | Male mean shared influence mass | Female dominant-joint agreement | Male dominant-joint agreement |
| --- | ---: | ---: | ---: | ---: |
| Legacy automatic | 47.73% | 46.38% | 55.29% | 52.99% |
| Surface Heat | 83.66% | 81.84% | 84.25% | 82.24% |
| Anatomical source | 76.33% | 75.17% | 77.81% | 80.40% |

Surface Heat's head-dominant regions retain98.54%/97.62% mean shared mass;
Anatomical retains97.91%/98.45%. Thus the weak standalone legacy result must not
be used to describe these newer source-aware workflows. Female1110 reference
vertices containing positive unmapped controls remain excluded and reported;
8741 female and9878 male visible native vertices enter the mapped comparison.
The three runs also verify the exact original unweighted native Scene remains
unchanged. Artist quantization normalization affects the reference only.

These numbers describe agreement with these artists, not a universal visual
quality threshold. Native Surface Heat's weaker agreement includes hips, chest
and palms. Hierarchical bone motion can make different distributions visually
similar; independent posed-position, normal and silhouette comparisons must
establish which differences actually require solver changes. No default,
product solver, format, original asset or frontend is changed by this audit.

Actual separate native measurement TU compile/link/run exit0 with zero input
drift, using the fresh cc5e6aed owning core193. All six native method calls
complete without refusal. The full per-role breakdown, original source inputs,
method timings and hashes are in
[the receipt](validation/HUMANOID-NATIVE-WEIGHT-QUALITY-20261112.json).
Source9560ac1d is on `work/humanoid-native-weight-reference-fix-20261112`.
The first fixture's duplicate-entry-point compilation failure and all raw
artifacts remain preserved. This is measurement evidence, not complete robust
automatic weights, graphical/physical workflow or whole-goal acceptance.
