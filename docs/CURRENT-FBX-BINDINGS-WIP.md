# Qualified current authoring / named FBX UV bindings

Frozen8a89787 in artifacts/fbx-bindings-current-132caef merges qualified adapter4
823fe61 with public132caef. Runtime import-native.cpp is byte-identical to
b603e0b; no public ABI or Scene version changes. Existing image projection,
PBR, fitting, Save/navigation and input repairs remain. Full production is
incomplete; Source21+ and post-basic controller drawers remain separate.

New real native37-check named-UV pixel proof independently resolves the raw
FBX's shared material at UV1/UV0, checks visible wrong-coordinate and wrong-TBN
negatives, native explicit512atlas adoption, current CorePBR/authored restore,
Save/restart and actual parsed GLB export. Both software and strict PCI03
Intel Arc A770 give maxpixelerror1, against unchanged4-bound. All tracked
source plus relevant binary/archive hashes before/after Arc are exact.

New75-case matrix covers5genuine upstream FBX inputs across all5unit choices
and3axis interpretations. Independent UFbx evaluation stays in raw input units
and converts fully evaluated skin/morph world corners afterwards, avoiding the
production geometry/local/IBM scaling algorithm. Actual native objects retain
the choices; Save/restart and independent emitted GLB skin/morph evaluation
pass37097 assertions/5130sourcecorners. These are default source poses; prior
edited-pose proof remains separately scoped.

Fresh coherent Release core/render9990 and whole-owned C/C++core/support/render/
frontend80065 are terminal0 onbad7009. Initial numericalUV passes but the new
graphics fixture crashes in its independent consumer: canonical FBX is legally
nonindexed, while the old helper dereferences p.indices. Exact helper-SAN null78
and old binary/log/JUnit remain. Test-only8a89787 handles sequential corners;
no production repair was needed. Refined Release40156/SAN71723 and selected
Release frontend50123 builds pass. Core/support/render and SAN App hashes stay
exactly those from the initial coherent build. Current registered16/16 pass
8.35s Release and16/16 pass18.45s ownedSAN/leaks; separate initial4/4 pass1.23s.

Actual native Maya skin14 and Blender morph14 pass in BOTH Release and ownedSAN,
all four Apps closing normally. These genuine UI fixtures choose Millimetres;
they are not a named-UV chooser proof. Current93positive emitted GLBs pass
pinned Khronos/maxIssues0 with0errors/57unfilteredwarnings.

CPU alltargets90629 and pinned DPC++7.2 OneAPI alltargets14599 are terminal0;
CPU registers398 cases and OneAPI399 (extra explicitmissingGPU). Current-source
actual Arc OneAPI15/15 pass13.87s:184analytic endpoints,224CPU-reference batches/
445088samples,maxerror8.9407e-08. ExplicitmissingGPU refuses with host samples
unchanged. Current sycl-ls confirms level_zero:0A770/:1UHD; GL stayed software
for this compute scope, separate from actualArc37 above. No full398/399-suite
claim. All recorded96prior hashes were rechecked intact. Source/header/test
inputs remain frozen for final main merge/equality and exact receipt. Read
validation/CURRENT-FBX-BINDINGS-WIP.json for exact scopes/hashes. No physical
test, installation or active-session restart occurred.
