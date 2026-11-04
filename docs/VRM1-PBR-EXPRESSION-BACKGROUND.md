# Private VRM1 PBR expression background

This descendant of `2e81e4f` fixes the original-source CorePBR view of the
implemented static VRM1 color/UV expression operation. Its native Material stays
the paint comparison baseline; actual V1 typed color, alpha and emission belong
to the original background term. Color paint is neither tinted nor moved by a
source expression. Legacy emission receives authored strength; absolute physical
paint and an explicit absolute background, including zero, keep their units.
The immutable material visibility query resolves the same background alpha as
the renderer. Automatic blending uses that resolved factor too.

The resolver is limited to the actual original V1 source/material identity and
source-only maps, including genuinely owning Source4 packets. Prepared color,
normal and key-derived material routes remain outside this adapter. It adds no
public layout, schema, persistent field or input identifier. It is view data,
not an export/source-authentication shortcut. The authored-MToon branch remains
unchanged. Before combining with the separate native-float packet lineage,
explicitly exclude its `native_geometry` interpretation and preserve both private
`draw_range` parameters; this base has no such packet method or new header.

Two genuine counterexamples are retained. The old matched renderer/query gets
46 of 72 MASK visibility decisions wrong when real paint is hidden, because it
uses the native comparison alpha instead of the current typed source alpha.
The final independent pixel consumer and valid strength-8 source also fail
against the old sealed core/render: the first pair differs by 74 levels. The
final runtime fixes both; no guard, quality bound or source input is weakened.

The valid fixture imports an actual two-skin/rigid original graph with morphs,
COLOR_0, UV0/UV1 and distinct base/shade/emission samplers. It creates real native
color/emission strokes. Only the active mesh is visible for image comparisons;
all other parts/raw metadata remain in the full native Scene. Three expression
weights and Opaque/MASK/Blend modes give nine native/reference pairs. Genuine
sparse physical paint adds three more: mixed legacy/physical, absolute-zero
background and nonzero absolute background. The physical stroke includes .005,
.5 and 100 without converting the owning field to PNG. Absolute backgrounds are
authored after the static operation; this does not prove static authoring over
an already absolute-background input.

The independent GL consumer parses original source geometry, morphs, skin and
COLOR_0, evaluates background color/emission targets from original expression
JSON, uses actual source images/samplers, and reconstructs sparse paint from
persisted common/physical tiles with independent triangle coverage and padding.
It does not call the production background resolver, compositor, overlay compiler
or paint point sampler. The PBR presentation is evaluated in its own shader.
This current-view test deliberately consumes the persisted current role UV
matrices: it proves their CorePBR interpretation, not an independent expression
matrix derivation. The preceding authored-view 2e81 proof independently evaluates
original KHR/expression factors on the GPU and remains green here.

Release and owned ASan/UBSan/leak consumer cohorts each pass eight commands:
fixture 7 checks; 72 alpha samples; PBR renderer 97 checks; unchanged MToon 81,
source overlay 44, visibility 40, unlit 28 and authored-expression 94. All twelve
PBR pairs have max error 1 at the unchanged bound 4 and coverage 2118..3777.
No-op redraws upload no geometry/images. All twelve native Save/restart images
are exact. Strong wrong-source-UV, moved-paint, tint, sampler and independent
legacy/physical strength controls differ by 4..105. In particular, omitting
legacy strength differs by 75; incorrectly scaling physical paint differs by
105. The genuine .005/100 fields and explicit-zero authority persist natively.

The complete current renderer and material-visibility TUs replaced their whole
members in immutable matching 2e archive copies. Whole new/affected test TUs
were compiled. The other current-layout support archives and public headers
stay exact. This is an affected-component proof, not a fresh whole-app or GUI
build. Release handle 79563 ends 0 in 156.684s; sanitizer 53216 ends 0 in 187.742s;
1839 recorded inputs per profile are unchanged. All 101 paired output files are
byte-identical: 24 PBR PNGs, 12 PBR native scenes, 54 unchanged authored PNGs,
9 authored native scenes and 2 fixture/source files. The actual three new
registered commands pass in each profile using immutable binary copies and
only relocating the generated CTest build directory. Configuration exposes 620
tests; no full-620 regression is claimed.

Strict PCI03 A770 handle 19089 passes 99 checks (97 plus explicit flag/device
checks); forced llvmpipe refuses 1 before rendering. Source/binary/support 1842
inputs are exact. Actual 69 mapped ELF files are unchanged. The raw mapped-file
ledger also captures the Mesa shader-cache index changing naturally; it is
recorded separately as non-code data, not hidden or treated as a library drift.
The wrapper ends 0 in 11.468s; the GPU lease was released. This is actual GL
rendering, not GPU baking/compute, physical XR or UI qualification.

The actual valid source in both profiles has zero Khronos errors (five retained
skin/rigid warnings), `maxIssues:0` and unfiltered messages. Twelve official
extension-schema instances have zero errors. These are source-input checks;
this milestone emits no new portable VRM expression export.

Earlier controls remain separate: compile-only test API/include typos; a mixed
physical case masking a weak legacy-strength sensitivity (1 level); and the
preceding strength-1.3 preliminary pixel run. The final strength-8 fixture
isolates each units control without changing a tolerance or production input
parser. All finite jobs are closed and no device lease remains.

Interactive expression state/shared controls, normal/prepared/derived/native
float and plain-CorePBR authoring adapters, source-aware finite export background
compilation and expression-preserving painted export still require their real
implementations. Static baking and this view correction do not replace those
requirements. Main admission and coherent future unions remain parent-owned.
