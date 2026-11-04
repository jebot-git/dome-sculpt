# Source4 import contract qualification

This isolated child of ce9c68a updates four native import regression suites after
the Source4 ownership audit. Runtime, public headers, schema, assets, dependencies,
quotas and numeric tolerances are unchanged. The unmodified counterfactual remains:
five of eight commands failed, including both atlas fault variants at their warm
assertion before allocation measurement. That was not a zero-allocation pass.

Source4 retains original logical UVs, texture roles/images, sampler/normal intent
and immutable source owners while giving paint a separate authenticated Strict
writable atlas. Empty PBR bundles are genuine owners without fabricated image
roles. MatCap remains view-dependent and does not invent a UV stream.

The tests require those authorities explicitly. Typed-only UV1 compares every
remapped coordinate with raw primitive IDs and an independent bilinear oracle.
Actual native painting, immediately ordered Save, restart and separate import/
paint Undo/Redo remain. MatCap V0/V1 additionally exercises native Import, queued
Save, restart, exact one-Undo/Redo and cgltf validation of ordinary GLB output.
That geometry/resource validation is not external MToon visual qualification.

Release passes all eight commands in 83.214 seconds with 1,733 captured inputs
unchanged. Each atlas transport, openat2 and forced ENOSYS fallback, executes
10,771 persistent allocation-denial boundaries and 128 cancellation boundaries.
They preserve the caller Scene, paint/COW identity and revision, original files,
descriptor count and complete retry result. Every original loop remains. Fault
injection covers direct `prepare_authoring_import`; separate native-worker
positive/history checks do not turn it into worker-thread allocation denial.

Fresh owned asserted ASan/UBSan core/render/support and seven consumer builds
pass in 710.392 seconds with 1,718 inputs exact. Six CTest commands pass, while
both exhaustive tests hit the inherited 1,500-second CTest observer. That
original eight-command run remains six passes/two timeouts, with 1,733 inputs
exact. The SAME unchanged binary subsequently completes both transports:
2,432.998 seconds for openat2 and 2,429.676 for fallback, each with all 10,771
allocation and 128 cancellation boundaries. All 1,735 inputs stay exact. Leak/UB
halt is enabled without suppression; neither child reports a sanitizer failure.
Four xatlas PackCharts diagnostics per transport under actual denial remain in
the logs. This is composed six-CTest plus two-standalone sanitizer evidence,
not a relabeled eight-CTest pass. External SDK/libraries are separately scoped.

Checks include default-material 560, typed UV1 41, MatCap 56, independent default
native/source/export graphics 66, source-aware import graphics 89 and editable
atlas graphics 207. Default native/export max pixel error is 0; incorrectly
selecting authored material zero gives 255. Typed correct max linear error is
3.11336e-8; substituting UV0 gives 0.266667. Existing bounds remain unchanged.
The atlas renderer distinguishes exact source preview from explicit finite
portable output; arbitrary painted minification is not claimed lossless.

Pinned Khronos validator 2.0.0-dev.3.10 with maxIssues 0 checks all 38 emitted
GLBs in each profile: zero errors, eight NODE_SKINNED_MESH_NON_ROOT warnings and
140 infos per profile. The 12 default, 24 atlas and two MatCap output pairs are
all byte-identical between Release and sanitizer.

The measured duration justifies a conditional 5,400-second CTest observer only
for the two explicit ASan/UBSan atlas fault tests, about 2.2 times the longest
measurement. Ordinary Release retains inherited 1,500 seconds. Fresh
configuration-only checks confirm both profiles, 594 registered tests and exact
sources. Executed C++ bytes match the final tests; only this CMake observer and
documentation follow the compiled proofs. No full fault retest under the new
CTest property is claimed.

The nine Release archives match the unmodified fresh diagnostic cohort. No
fresh application/UI, actual A770, full 594-regression or main admission is
claimed here. The separate application union and later Mirror qualification
remain independent. Original full-589 failures and every wider unfinished
production requirement remain explicit. No installation or physical session
changed. Exact commands, manifests and output digests are in
[the receipt](validation/SOURCE4-IMPORT-CONTRACT.json).

Fault-mechanism clarification in the later application union: the measured
boundaries override C++ operator new. They do not deny cgltf malloc/custom
allocators or qualify null-return/partial-parse cleanup. That separate actual
allocator robustness requirement remains open.
