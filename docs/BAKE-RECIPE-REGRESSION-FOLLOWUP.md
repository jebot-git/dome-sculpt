# Private bake-recipe regression follow-up

This private checkpoint repairs real workflow fixtures for durable bake recipes.
It preserves the full production objective. It does not admit Scene30 recipes to
main or declare the running whole-application regression passed.

The original fresh `176dd50` 612-command Release run closed with 578 passed,
31 failed, one timeout and two dependency skips in 6212.67 seconds; all 2748
recorded source/artifact/SDK/script inputs were exact. Most failures were old
outer-Scene capability assertions or an XR fixture that assumed the saved source
would be silently rebound. The repairs retain the actual recipe and independently
construct complete no-recipe Scene controls, preserving inner UV, normal, colour,
rig, key, history and old-reader capability checks. XR now explicitly chooses its
live source through the existing controls. No runtime guard or quality bound was
relaxed.

Four initial native commands pass Release/sanitizer (92.01/581.57 seconds), then
eight workflow commands pass both profiles (140.23/1066.63 seconds). Projection
and UV real-loader cases pass both profiles; the complete existing client suite
also passes Release. The last sixteen full native workflow commands pass Release
in 1228.04 seconds. Both previously skipped Mirror writer/renderer pairs now pass
all four commands per profile (72.13/428.16 seconds). Their immutable input audits
are exact. Last-sixteen sanitizer work is still running; this manual component
run does not establish the registered 600-second normal-colour-UV time bound.

Independent Khronos 2.0.0-dev.3.10 validation, with unlimited issue reporting,
checks 252 actual emitted Release GLBs: zero errors, 271 warnings and 148 infos.
Five original input controls are separate, retaining three errors and every
warning/info/hint. Two `cancel.glb` files contain the exact original-output
sentinel asserted by the C++ cancellation tests; they are deliberately not glTF.
The first aggregate validator produced no final receipt. The isolated validator
then stopped at this invalid sentinel. Both negatives remain; the final audited
continuation verifies every file hash, retains all completed reports, validates
the remaining emitted models and checks both sentinel byte strings against the
original C++ producer. No invalid exported model was excluded.

The original sampler timeout is retained. The same frozen executable passes its
full 72 cases/289 checks with an initially empty, default-enabled private Mesa
cache under the unchanged 180-second CTest bound (166.90 seconds). A separate
cache-disabled run reproduces the timeout (180.13 seconds). This diagnoses the
extra harness override; it changes neither product shaders nor test coverage.

A fresh optimized coherent application/core/render/support ALL build on `2bf564c`
passes all 1212 tasks in 2068.27 seconds. After that build closed, test-only
`d23100c` repairs rebuilt 46 tasks; all twelve application/archive hashes remain
unchanged. The complete 615-command Release regression is running with normal,
initially empty private Mesa caching, unchanged tests/timeouts and no pruning.
Its result remains pending. Product source, headers, assets, dependencies and
CMake are byte-identical between these two commits.

The full receipt, immutable hashes, exact terminal scopes, unfiltered emitted
validator messages and pending-process observations are in
[validation/BAKE-RECIPE-REGRESSION-FOLLOWUP.json](validation/BAKE-RECIPE-REGRESSION-FOLLOWUP.json).
No hardware/headset/system/session action is part of this checkpoint. Full
coherent regression, appropriate sanitizer/application/compositor/device gates,
all other production requirements and the later controller drawers remain.
