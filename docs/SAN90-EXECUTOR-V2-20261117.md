# Strict sanitizer executor: retain optimized CPU contract fixtures

The first actual owning launch exited1 before creating an execution directory,
staging an object or compiling a translation unit. Its executor incorrectly
rejected every `-O3` token. The frozen CMake source intentionally adds `-O3
-march=native` to the two owners of `dome-sculpt-resample-native-contract-test`:
`tests/resample-native-contract.cpp` and `src/sparse-resample-cpu.cpp`. Those
optimized CPU/FMA fixtures retain their sanitizer and frame-pointer flags.
The original failure is preserved as exit1, not relabeled.

Additive executor v2 fixes the assertion. It permits precisely those two target
keys with their exact source paths and ordered `-O1`, `-O3`, `-march=native`
options. All other1034 owners require only `-O1`. All1036 owners still require
`-g1`, `-DNDEBUG`, address+undefined sanitizers and frame pointers. A read-only
validation of the actual complete plan passes. No fixture is weakened and no
source, CMake rule, compile/link command or compiler option changes.

V2 has a separate `execution-native90-v2` namespace. Its SHA256 is
`f5df4e63948c161482e4846003c16a15d4cfb5c45ebb9510947c215b9ff1703f`.
Separate `plan-v2.json`, SHA256
`62116c27b70b37459d186e037acc51984d34fac8262760e5f50351ac8c21f8d9`,
retains every old pin unchanged, adds the v2 executor pin, and updates only
execution-template metadata. Original plan24db, executor7362 and HOLD0be8 remain
untouched. This preparation does not launch another owning service; root must
bind the exact v2 plan/helper/limits in a new execution review first.

[Receipt and compact exact plan delta](validation/SAN90-EXECUTOR-V2-20261117.json)
include the original unit/journal, actual-plan flag validation, full executor
and47-line diff. Unit`ds-native90-sanitize-build-20261117.service`,
MainPID2106798/Invocation`4819ffcdc22b4b30895fbbf9e89770f2`, remains an actual
failed pre-staging run. No product test, install or session action follows.
