# Current inherited-role sanitizer readers — 2026-11-11

All **33 actual reader cases passed**, with **two independently verified current native fixture endpoints reused**: 35 logical cases, 11 fresh whole test translation units, 11 fresh qualified reader ELFs, and 11 software-graphics executions. Strict ASan/UBSan and leak detection stayed enabled, without suppressions. This seals reader qualification; main product admission remains a separate review.

The source is `b8c469ce875eb2542c220300c3c63edbf1526570` in `artifacts/resume-integration-20261111`. No source, headers, tests, runtime, defaults, layouts, or schema were changed. The private evidence root is `artifacts/readers-sanitize-20261111`. [The compact validation record](validation/READERS-SAN-20261111.json) pins every reader source/object/ELF, actual case argv, deadlines, log hashes, SDK and graphics runtime, retained failures, and audit artifacts. Paths in that record are relative to `/home/blux/de-xr/dome-sculpt` unless absolute.

## Source and provider closure

The historical Release reader controls were recovered from `artifacts/integration-03f8-release-seal/control-evidence.tar.zst`, SHA256 `71b0474956b522f68e6ef64b7835311c0b3e4defcc57b30295fdfb448cef3d34`. The old `/tmp` reader/build/runtime paths and old source worktree were absent. Only control metadata was restored; no old project ELF, object, archive, fixture, or native output was substituted.

The historical source was `03f8edf15b265932cab5e1ddbd22d5878f1d8c26`. Current `src`, `include`, `tests`, and `CMakeLists.txt` match its reader semantics. The production/manifest differences are `third_party/desktop/bdf_font.hpp` and `third_party/sources.json`; the current official SDK and runtime are separately pinned. Historical Release35 results remain historical, including their observed image maximum1. This is a new current sanitizer qualification, not a new current Release35 or cross-profile comparison.

Current read-only providers are the fresh sanitizer core/render/support closure in `artifacts/build-20261111/sanitize`. The parent’s current 32-target/native cohort is terminal0; its 312 owned outputs were independently rehashed during the first reader case and at the final audit, with no drift. The first independent312 audit occurred during that case, not before launch. The preparation pinned 2,066 unique source/provider/SDK/config inputs, including216 archive/member providers. Actual compiler depfiles verify the current project include closure.

Each of the following test TUs was freshly compiled; all11 qualified ELFs were privately linked against current matched providers. No shared build was mutated, project support copied, historical project object reused, test body replaced, or wrapper substituted. Current CMake recipes were read directly; changes were restricted to private output/depfile paths and extra link-map output paths.

| Fresh target suffix | Whole source TU |
| --- | --- |
| vrm1-expression-targets-render | `tests/vrm1-expression-targets-render.cpp` |
| vrm1-mask-support-faults | `tests/vrm1-mask-support-faults.cpp` |
| vrm1-normal-authoring-workflow | `tests/vrm1-normal-authoring-workflow.cpp` |
| vrm1-normal-faults | `tests/vrm1-normal-faults.cpp` |
| vrm1-normal-pristine-render | `tests/vrm1-normal-pristine-render.cpp` |
| vrm1-rim-faults | `tests/vrm1-rim-faults.cpp` |
| vrm1-rim-mask-authoring-workflow | `tests/vrm1-rim-mask-authoring-workflow.cpp` |
| vrm1-rim-mask-pristine-render | `tests/vrm1-rim-mask-pristine-render.cpp` |
| vrm1-shift-default-pristine-render | `tests/vrm1-shift-default-pristine-render.cpp` |
| vrm1-shift-faults | `tests/vrm1-shift-faults.cpp` |
| vrm1-shift-pristine-render | `tests/vrm1-shift-pristine-render.cpp` |

Compile flags retain `-O1 -g1 -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -std=gnu++20`; link flags retain `-fsanitize=address,undefined -Wl,--gc-sections,--compress-debug-sections=zstd`. Compilation used at most2 jobs; links and runtime cases were serial. Ten qualified ELFs are in `build/bin`. The eleventh, `dome-sculpt-vrm1-shift-pristine-render-test`, is the successful new-path final link in `build-resume`; the old mode0600 partial ELF remains preserved and was never made executable.

## Actual execution and reuse

The two reused endpoints are `vrm1-background-fixture` and `vrm1-targets-uniform-roles-uv-authoring`, from the current parent sanitizer-native22 run (native stage118.84s, exit0). Their source/owned outputs, status, actual native command/log SHA, fixture owners and outputs were verified. Reuse does not claim a second execution here.

The33 direct cases comprise2 actual successes in `execution-patched-v2`,9 more in `execution-patched-v3`, and22 in `execution-patched-v4`. Completed cases were not restarted for observation timeouts. Read-only normal-model inputs remain in `runtime-patched-v2/vrm1-normal-authoring`; shift-model inputs remain in `runtime-patched-v3/vrm1-shift-authoring`; subsequent outputs use v4. Logical fixture dependencies and all semantic argv were compared against the recovered plan. These namespaces prevent shared-build writes; they do not replace any oracle or producer.

| Actual case | Original deadline (s) | Actual duration (s) | Execution namespace |
| --- | ---: | ---: | --- |
| vrm1-normal-authoring | 900 | 24.86 | v2 |
| vrm1-normal-render | 900 | 17.56 | v2 |
| vrm1-normal-pristine-render | 900 | 11.48 | v3 |
| vrm1-normal-faults | 1800 | 408.31 | v3 |
| vrm1-normal-native-workflow | 900 | 11.30 | v3 |
| vrm1-normal-generated-identity-guard | 900 | 3.32 | v3 |
| vrm1-normal-varying-uv-guard | 900 | 15.20 | v3 |
| vrm1-normal-zero-uv-guard | 900 | 9.32 | v3 |
| vrm1-normal-generated-transformed-import-guard | 900 | 0.74 | v3 |
| vrm1-shift-authoring | 900 | 43.88 | v3 |
| vrm1-shift-render | 900 | 26.06 | v3 |
| vrm1-shift-pristine-render | 900 | 28.45 | v4 |
| vrm1-shift-faults | 1800 | 782.55 | v4 |
| vrm1-shift-native-workflow | 900 | 5.05 | v4 |
| vrm1-shift-varying-uv-guard | 900 | 2.92 | v4 |
| vrm1-shift-zero-uv-guard | 900 | 3.27 | v4 |
| vrm1-shift-invariant-authoring | 900 | 35.42 | v4 |
| vrm1-shift-invariant-render | 900 | 34.95 | v4 |
| vrm1-shift-mask-zero-rim-authoring | 900 | 27.32 | v4 |
| vrm1-shift-mask-zero-rim-render | 900 | 20.47 | v4 |
| vrm1-shift-mask-authoring | 900 | 28.84 | v4 |
| vrm1-shift-mask-render | 900 | 12.38 | v4 |
| vrm1-shift-defaults-authoring | 900 | 22.95 | v4 |
| vrm1-shift-defaults-render | 900 | 13.86 | v4 |
| vrm1-shift-defaults-pristine | 900 | 10.48 | v4 |
| vrm1-rim-mask-authoring | 900 | 14.37 | v4 |
| vrm1-rim-mask-crossing-refusal | 900 | 1.64 | v4 |
| vrm1-rim-mask-render | 900 | 10.85 | v4 |
| vrm1-rim-mask-pristine-render | 900 | 7.07 | v4 |
| vrm1-rim-mask-faults | 1800 | 894.01 | v4 |
| vrm1-rim-mask-coefficients | 900 | 677.66 | v4 |
| vrm1-rim-mask-native-workflow | 900 | 5.29 | v4 |
| vrm1-targets-faults | 1800 | 403.16 | v4 |

Every successful case stayed within its unchanged original deadline. The fixture deadline is300s, ordinary cases900s, full allocation/cancel fault consumers1800s, and the Rim coefficient consumer900s. The target fault case ran its full controls without `--preflight-only`.

Five full factory consumers printed **12,060 actual checks**:

| Case | Allocation prefixes | Cancellation points | Printed checks |
| --- | ---: | ---: | ---: |
| Normal faults | 1994 | 654 | 2664 |
| Shift faults | 2066 | 654 | 2736 |
| Rim MASK faults | 1045 | 1340 | 2401 |
| Rim coefficients | 1045 | 1340 | 2397 |
| Targets faults | 940 | 909 | 1862 |

Their unchanged measured working/operation guards are also preserved: Normal/Shift2,888,896B and8,317,056 operations; MASK/Rim2,890,944B and4,763,408 operations; Targets2,888,768B and8,415,312 operations. Full raw printed boundary, retry, cancellation, ownership, target and footprint results are pinned in the JSON and archive.

## Sanitizer, SDK and graphics runtime

Every successful runtime used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; `LSAN_OPTIONS` was unset. No suppression or sanitizer-disable route was added. Successful logs contain no ASan/UBSan/LSan diagnostics. The project core/render/support, new test TUs and sanitizer toolkit are instrumented; the current official optimized system bitmap and other external libraries are not claimed instrumented.

The SDK is `artifacts/sdk-20261111`, from DomeD `acc2b70ae61dedec2a23727f67c60c0303d0f76f`. Sanitizer profile SHA256 is `cb0834a42140b5b80cd079d93c601718ad1ce6f02cbd5cd6cc91a24720bb4a3d`. Its verified manifest pins headers, toolkit libraries, bitmap dependencies and build configuration.

All11 graphics cases reported **Mesa llvmpipe (LLVM19.1.7,256bits)** and recorded actual recovered EGL/driver mappings. The runtime is the retained patched Mesa26.2.5 EGL and gallium driver under `artifacts/mesa-xr-leak-26.2.5/build-patched-v2`, with the signed Fedora `llvm19-libs-19.1.7-18.fc44.x86_64.rpm` privately extracted under `artifacts/xr-recovery-20261111/llvm19-package`. The package SHA256 is `0449fcdf119a271d785727e95d6d87cdae75ce35dc1c375ae26bcd51113b3b52`; OpenPGP header-signature and header/payload digest verification is retained. No system install occurred. Exact current EGL, gallium and LLVM hashes and map witnesses are in the validation JSON. Mapped paths plus on-disk hashes are not claimed mount-certified file identity or hardware proof.

Runtime configuration used the private LLVM19 directory, patched driver and current sanitizer toolkit in `LD_LIBRARY_PATH`; an explicit vendor JSON selected patched EGL. `LIBGL_ALWAYS_SOFTWARE=1`, `MESA_LOADER_DRIVER_OVERRIDE=llvmpipe`, `GALLIUM_DRIVER=llvmpipe`, `LP_NUM_THREADS=2`, and `MESA_SHADER_CACHE_DISABLE=1`. Desktop/preload/DRI-prime environment leakage was removed; `XR_RUNTIME_JSON` pointed to a nonexistent fail-closed private path. No hardware, physical headset, OpenXR session, or desktop session action was used. A system-Mesa runner was prepared but never executed in this reader lane; the other agent’s system-Mesa teardown failure is not relabelled as our reader observation.

The **actual current maximum `UV_TARGET_IMAGE` byte error is2**, within the **unchanged assertion bound4**. Historical Release’s observed maximum1 remains distinct; current SDK/font/runtime differences prevent claiming that old observation is the current result. All numerical checks and negative admission/owner controls stayed intact.

## Preserved unsuccessful attempts and resource scope

1. The original **8GiB private allocated-output envelope** stopped the eleventh link after all11 compiles and ten links succeeded. Measured allocated output8,078,376,960B plus536,870,912B prospective reserve exceeded8GiB. Large raw link maps, the partial ELF and status remain intact. Only the final link was repeated at new paths under the approved10GiB envelope; final build output measured8,931,852,288B. This was an external artifact guard, not ENOSPC or a product compile failure.
2. The original **3GiB external sanitizer process-tree RSS envelope** stopped Normal pristine at3,229,728,768B. Its preceding two genuine successes were retained. An approved4GiB instrumentation envelope allowed the remaining cases to finish; maximum successful RSS was3,254,837,248B in Shift defaults pristine. Product workspace/work/quality limits and assertions were unchanged. The original failed scope is retained, not retrospectively called passing.
3. A setup-only Python loop initialization error created private empty directories and stopped before any native case. Its script and failure record remain; zero native executions are credited.
4. The v3 runner’s final target binding referenced the retained nonexecutable partial ELF, causing `PermissionError(13)`. **No native process started for that row. Its raw exit0 was inherited from the previous child and is explicitly not success.** The separate audit records this. The corrected runner reset its child before `Popen` and bound every target to the qualified final ELF manifest; the actual final Shift pristine case subsequently passed in28.45s. No source, binary body, oracle, bound or timeout was altered.

At final runtime closure private output measured8,980,860,928B, including48,603,136B of runtime/log artifacts, before the small audit/seal metadata. The approved final envelope is10GiB, with a20GiB workspace-free floor and512MiB prospective serial-link reserve. Successful case durations sum to3614.97s; this is not total profile/build elapsed time. Host contention was substantial (observed peak load273.52 on20 logical CPUs), so these are nonexclusive timings, not sustained performance evidence.

## Independent audit and persistent seal

`control/final-audit.py` completed exit0. It independently checked all33 unique actual children, original case order/semantic arguments/deadlines, final target-ELF bindings, all11 graphics maps/reports, successful log hashes, all2,066 initial input pins, retained build failures/partial outputs, all312 current native-owned outputs, new reader outputs and source HEAD. Source/provider/output drift is empty. All native/build/audit jobs are terminal.

`seal/control-evidence.tar.zst` SHA256 is `2ad50e96af0a6c3838017df7778183a21b5fd648249af1d3ce97680e8f645611`. It contains161 closed metadata/control/log/map-witness members (5,356,072 raw bytes), verified byte-for-byte and with PAX mode/UID/GID/ns-mtime metadata. All original files remain. Large raw link maps stay in place with complete inventories; project objects, archives, ELFs, native scenes, image outputs, extracted packages and private HOME were not put into the compact control archive or removed.

The control archive can be listed with `zstd -dc --memory=512MB <archive> | tar -tvf -`; extract to a new empty directory for inspection. `seal/verification.json`, the member inventory and full large-link-map inventory pin the retained collection. The validation JSON separately pins the final audit and all primary statuses, commands and output manifests. No new current Release35, fresh ALL build, hardware qualification, main admission or full-goal completion is claimed.
