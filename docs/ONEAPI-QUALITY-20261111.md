# Fresh OneAPI aggregate quality component — 2026-11-11

The unchanged current aggregate-quality component builds and passes on the actual Intel Arc A770 from source **27f01cad7910a9c6b6dd383a4422f3feffdbc61c**. This milestone adds only documentation. It is a fresh standalone backend qualification, not a full OneAPI application/core build or a new performance claim.

## Fresh source and toolchain

The isolated branch/worktree is `work/oneapi-quality-20261111` / `artifacts/oneapi-quality-20261111`; outputs are `artifacts/oneapi-quality-build-20261111`. All three project translation units are freshly compiled: `tests/resample-quality.cpp`, `src/sculpt-storage.cpp` and `src/backend_sycl.cpp`. The target links no project archive and contains no CPU backend implementation. Actual compilation/link commands, three object hashes, ELF hash and 733 compiler dependency hashes are retained. Backend/kernel/public-header/test bytes match current painting source 775f92b8; no old project ELF, object or receipt substitutes for execution.

The compiler is the existing user-owned **DPC++ 7.2.0 / Clang 23.1.1**, commit `5680988bb38bd45f13081394fba43588c5f5e395`. Configuration requests `ONEAPI`, Release, testing enabled and UI/XR disabled. The original CMake compile/link SYCL check succeeds. Actual flags retain `-O3 -DNDEBUG`, `-fsycl` and `-ffp-contract=off`. Configuration took 9.132 seconds and the serial `--parallel 1` build 10.585 seconds; these are observed durations, not quiet-host benchmarks. Build handle **64008**, PID **3372935**, is terminal 0.

The original SDK remains unchanged. Runtime selection explicitly uses the previously documented separate `artifacts/oneapi/compat/libumf.so.1`, whose `libm` dependency adjustment and original/modified hashes match `artifacts/oneapi/provenance.json`. No software-Mesa/LLVM19 route, installation, sudo or session action is involved. An unconfigured `ldd` inspection initially reported `libsycl.so.9` unavailable; actual configure/build/run commands use the recorded explicit SDK library path. A later read-only ledger lookup used an incorrect relative path and was corrected to the absolute artifact path; neither diagnostic changed or reran a native gate.

## Actual original controls

Under one short exclusive `/tmp/dome-sculpt-arc-device.lock` lease, the freshly built target runs the existing controls without modifying arguments other than the original mode options:

| Mode | Actual result |
| --- | --- |
| `--trace` | Correct exact maximum; six real kernels, three bounded chunks |
| ordinary numeric mode | **94** exact aggregate sparse-quality checks; **3,200** real kernel launches |
| `--missing`, with `ONEAPI_DEVICE_SELECTOR=opencl:cpu` | Explicit unavailable-GPU refusal; **zero** device allocations and kernel launches |

The numeric controls compare the GPU maximum with an independently evaluated native paged-map reconstruction and the integer-binary64 point oracle. They cover three spacing/background combinations, workgroup/chunk boundaries and final tails, exact source/query preservation, invalid expected words/coordinates, cancellation after actual work, refusal to reuse the cancelled GPU session, valid retry, signed zero, the minimum binary64 subnormal and a one-ULP difference near unity. Actual A770 reports `nativeFP64=0`; the tested implementation uses its existing integer binary64 kernel.

Original admission remains unchanged: at most 4,000,000 source entries, 16,000,000 queries, 128 hash probes, chunk capacity no greater than 65,536 and reduction width 128. The independent native interpolation keeps the original contraction policy and exact-word comparisons. This leaf does not claim exhaustive allocation/device-loss faults or full Volume/Document transaction qualification.

The original 300-second test ceiling is retained, with a separate 4 GiB external validation RSS envelope. Actual observed native durations are 0.425 / 1.157 / 0.064 seconds and peak observed RSS is below 95 MB. Observational tracing and identity pauses are included; these are not performance measurements. Runtime handle **94091**, PID **3426422**, is terminal 0, all original-source/toolchain/object/ELF/dependency hashes remain unchanged, and the Arc lease is released.

## Actual selected device and transfers

The qualification observer uses the installed official Level Zero loader tracing API (`zelTracerCreate` and epilogue callbacks). It does not modify handles, arguments, data or return values. It queries the genuine `ze_device_handle_t` used to create each production command list and authenticates the device associated with every actual kernel-list handle: Intel `0x8086`, device `0x56a0`, **PCI 0000:03:00.0**, Arc A770. This is not inferred from device enumeration, environment variables or an open DRM FD. The actual backend uses immediate lists: the trace case records three created lists and six launches; numeric records nine lists and 3,200 launches. No ordinary command-queue creation call occurs on this route.

At the first actual kernel, process maps are captured and the native process briefly paused to certify mapped ELF inode/device/mount identity. All **34** distinct actual mapped ELF files are rehashed after exit without drift, including current `libsycl`, the Level Zero v2 adapter, system loader/Intel GPU driver, IGC and the explicit UMF compatibility library. The observer initializes the driver for tracing but creates no context, queue, list, buffer or kernel itself.

Independent parsing of real UR completion records, rather than the fixture's expectation message, verifies:

- One 48-byte sparse-grid upload.
- Query uploads of **3,048, 3,048 and 72 bytes**.
- Exactly three **8-byte** scalar readbacks, with no per-query result download.
- Six successful `urEnqueueKernelLaunchWithArgsExp` calls, two distinct kernels and one actual UR queue.

That is **6,216 bytes uploaded and 24 bytes read back**. Trace-mode device allocations total 5,644 requested bytes; this is a measured small fixture, not a physical-device-memory limit. The missing-device path runs without the observer, throws `No device of requested type 'info::device_type::gpu' available.`, and has no device allocation or kernel completion record. No CPU result is substituted.

## Evidence and boundary

The compact receipt is [ONEAPI-QUALITY-20261111.json](validation/ONEAPI-QUALITY-20261111.json). Full persistent build, source, compiler, dependency, runtime, UR and actual-device records are in `artifacts/oneapi-quality-build-20261111`.

`seal/oneapi-quality-evidence.tar.zst` contains 76 evidence/source/build members (40,296,437 raw bytes), all decoded and verified against raw SHA-256 and PAX mode, UID, GID and nanosecond mtime. The 739,799-byte archive SHA-256 is `7444bf4168190a58ae67cd46bd16b3260a47acb4b6aaf2d31c31b4628652976d`. Original files remain intact. It includes the unchanged actual project inputs, fresh executable/objects, observer source and scripts; external SDK/system dependencies retain explicit hashes and paths.

No `--bench` workload, long P1, sanitizer build, full OneAPI frontend, current Document/Save/Undo proof, NVIDIA/AMD execution, physical HMD or general accelerator completion is claimed. Those remain separate integration and full-goal requirements.
