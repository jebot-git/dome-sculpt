# Reproducible native anatomical CTest inputs

Seventeen new registered endpoints consume only repository assets and generated
build-directory files. No prior worktree,765 headers/archives, downloaded model,
sealed Scene, magic target ID or artificial CreatorAppearance is a prerequisite.
The original controlled-data executables remain usable directly.

`dome-sculpt-anatomical-fixture-test <repository base.vrm> <output>` reads the pinned
CC0 female/male assets in assets/humanoid/vroid. Its bounded GLB transformer omits
skin attributes, inverse-bind accessors and their physical BIN ranges, removes
skin/animation bindings, and retains exact geometry/morph/image bytes and the
complete original node graph. Retained/removed interval overlap refuses. Aggregate
output BIN is bounded64MiB; JSON/table/source bounds are explicit. The resulting
file is a genuinely unskinned fixture variant, not relabeled original artist skin.
Actual import proves zero skins/influences. The new source archive is produced by
prepare_authoring_import, with its authentic node/mesh/primitive owners.

Authored original humanoid-node positions provide explicit fitting landmarks.
The fixture chooses its actual largest imported mesh as a reference, then calls
author_humanoid_fit with bind=false. It discovers every target ID from real import.
The owning anatomical factory creates an expected native field in a separate path;
actual public Document generation is tested afterward. No original artist weight
values enter construction/energy. Only a separate post-solve quality process opens
the original artist skin, after the public field exists on disk.

CTest fixture dependencies are:

- anatomical-{female,male}-fixture: generate unskinned.glb,
  fitted-unskinned.dsculpt and bound.dsculpt below anatomical-native/{base};
- anatomical-{female,male}-public: actual public19/locks/Undo/Save/Restore, producing
  {base}-public/full.dsculpt from the generated input and owning expected field;
- anatomical-{female,male}-worker: actual helper Cancel/stale/accepted Save/controller;
- anatomical-{female,male}-linked and -pose-faults: consume the actual public Save;
- anatomical-{female,male}-{linked,controller}-export: independent current GLB
  consumers of actual linked/controller saved outputs;
- anatomical-{female,male}-quality: original fixed95 global pose/part cases each,
  using original artist data only as an after-solve oracle;
- anatomical-caller-faults: actual sampled admission failures from the female input.

CMake build dependencies include the real producer chain. CTest FIXTURES_REQUIRED
and FIXTURES_SETUP make selected execution produce all necessary data. The normal
full-build workflow can run:

```
cmake --build <build> --parallel 2
ctest --test-dir <build> -R '^anatomical-(female|male|caller)' --output-on-failure
```

No display, physical OpenXR session or GPU is needed; each registered command has
a fail-closed nonexistent XR runtime and software graphics environment. Current
outer budgets are600s producer/caller/fault/quality,900s public/worker/linked and
300s independent export. These are test allowances, not solver limits or latency
claims. The previous500M/256MiB workspace/512MiB total admission stay unchanged.

Scoped component verification freshly compiles the new producer and existing
independent quality consumer against own current44 headers and exact sealed aca
support; all inherited production/header/asset bytes are independently equal.
Fresh whole-project or all17 CTest execution is not claimed. The actual new-source
R producer/public/quality chains pass for both assets,111.504s including compilation.
Own-SAN producer+quality pass248.648s; public SAN is the prior current44 milestone,
not rerun on this variant in this phase. All manifests remain exact; generated
static GLB, fitted Scene and bound Scene pairs are byte-identical across profiles.
Female reports work444,333,739/workspace123,881,355/total483,384,934 bytes; male
408,496,469/130,904,447/487,988,554. Both satisfy the unchanged method budgets.
Independent190 new-source global cases have zero failures, exact original REST
correspondence and pose transport maxima3.54333e-7/3.76578e-7m against unchanged3e-5m.
No sanitizer diagnostics/suppressions or device/system/session actions.

See validation/ANATOMICAL-CTEST-FIXTURES.json. This test patch needs the preceding
aca9dd2 caller fixture target. Root owns the next fresh coherent build/CTest
execution. Local facial/stationary/stress cohorts, broad helper/publication faults,
current UI/loader and general edited/prepared/relative-affine/default binding remain
separate requirements. Registration does not complete those domains or the goal.
