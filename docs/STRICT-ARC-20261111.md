# Current strict Arc qualification — 2026-11-11

All five unchanged current GL consumers pass **1,727 checks per profile on the
actual Arc A770**, including clean strict ASan/UBSan/LSan exits for all five
positive sanitizer runs. Each profile also passes56 software/Arc image pairs
with maximum RGB difference2 against the fixed6 bound and exact alpha.
All56 corresponding Release/SAN Arc PNGs are byte-identical.

The two Release UHD rejection controls behave as required. Their sanitizer
counterparts also refuse the actual UHD770, but subsequently report **132,268
leaked bytes in1,142 allocations each** and abort. They are functional refusal
evidence, **not clean sanitizer cleanup passes**. This caveat remains open.

Product source is frozen `b8c469ce875eb2542c220300c3c63edbf1526570`, with312
current owned outputs per profile and the latest canonical DomeD `acc2b70a`
toolkit/bitmap runtime. No product, test consumer or driver was rebuilt or
patched by this qualification. The only compilation was the independently
owned observational tool described below. No installation or display/headset
session restart occurred.

| Actual consumer | Release | Sanitizer |
| --- | ---: | ---: |
| Original-field polygon appearance | 51 | 51 |
| Balanced same-object polygon/texture sets | 537 | 537 |
| Sculpt chunks | 72 | 72 |
| Direct upload | 73 | 73 |
| Sourceful same-object fields | 994 | 994 |

The batch took54.15s Release and400.66s sanitizer, including input hashing,
observation and both negative controls. Each consumer retained the original
180-second deadline. Observed process-tree RSS peaks were612,491,264B Release
and3,054,460,928B sanitizer, within the declared3GiB/4GiB instrumentation envelopes.
Product numerical, image and native resource limits were unchanged. These are
finite correctness scopes, not quiet performance or physical pacing results.

## Actual device and owning-input evidence

Every positive run authenticates the imported, unmodified `glGetString` result:
vendor Intel, renderer `Mesa Intel(R) Arc(tm) A770 Graphics (DG2)`, excluding
llvmpipe/softpipe. It queries the actual current EGL display/context and
`EGL_DEVICE_EXT`, then resolves the DRM node, character-device rdev and PCI
sysfs identity: `/dev/dri/renderD129`, `0000:03:00.0`, `8086:56a0`, `i915`.

The process itself observes open descriptors on that node; the external
coordinator separately records four matching live Arc FD observations per
positive consumer, including inode, rdev, PCI, vendor/device and driver. The
actual executable, EGL, Gallium and observational preload must all appear in
the process maps with exact inode and certified mount-device identity. This is
more than a requested device name or environment override.

Historical `graphics-v3.py` and `mount_identity.py` were restored byte-exact
from the preserved control archive. Their SHA256 values are respectively
`0606a923f5fbe1a4da5ffd973ae3b541d0bb343cd55614a33ee2f3edef9f6a7a` and
`1e2c6a322ae8797c78f9a2693d7e2e18aab9dd8d6472babbeb81334971904c81`.
These are archive-hash checks, not cryptographic signatures. Mount checks retain
the certified Btrfs UUID/source/subvolume proof, longest covering mount and
child/coordinator namespace equality; map-device and stat-device are checked
separately rather than incorrectly equated.

The old private preload no longer existed after environment cleanup. The new
tool changes only the forwarding seam of tracked `tests/retopo-marker-device.cpp`
to `RTLD_NEXT`, retaining its authentication checks and returning the actual GL
query value. Original tracked source SHA256 is
`9736c98298adb754c6bb43eaf195083c9b95758f56b660777229eb9c55ca21ee`;
derived source SHA256 is
`f13b90442862bdc64bd0e707a728305a68ecd45d726d54a09982b6c1bdae260e`.
Full derivation, source, compiled tools and mapped ELF fingerprints are retained.
The tool does not alter consumer GL or native document state.

The coordinator adapts the historical reader to current native/software status
schemas and actual SDK paths. It validates all22 native routes and both manual
workspace gates, all312 owning hashes, fresh current native inputs, software
consumer/log/map identities and canonical SDK hashes before launching. All
protected before/after hashes and image pins remain unchanged. Both profiles use
the actual current system Mesa26.2.5 EGL/Gallium providers; the private LLVM19
software XR route is not used for this hardware qualification.

## Negative cleanup caveat

Both original negative consumers actually query
`Mesa Intel(R) UHD Graphics 770 (ADL-S GT1)` and emit the required strict-Arc
refusal. Release exits1; sanitizer exits SIGABRT after LeakSanitizer reports
Mesa `driConcatConfigs`/`driCreateConfigs` and `drmGetDeviceNameFromFd2` allocations.
The negative path rejects before its EGLDevice/FD report; rapid negative exits
were not captured by the external FD sampler. Positive Arc FD authentication
must not be attributed to these negative controls.

The preserved coordinator's original negative oracle accepts a nonzero exit
with the actual UHD/refusal strings. Consequently its raw batch `exit:0` records
that original behavioral oracle and the positive/image gates, **not** a claim
that sanitizer negative teardown is clean. The scoped receipt explicitly marks
the sanitizer-negative cleanup failure and retains the reports unsuppressed.

Read-only audit finds no observer `exit`, `_Exit`, abort or terminate: its
refusal throws `std::runtime_error`, and both actual consumer main catch blocks
print `FAIL Actual renderer is not strict Intel Arc A770`. `NativeGraphics`
already exists when this call occurs, before framebuffer/renderer construction.
Its destructor unbinds its own current context, destroys its surface/context and
calls `eglTerminate`; it does not call `eglReleaseThread`. The observations do
not establish the leak's cause. An isolated, explicitly scoped lifecycle probe
is planned separately; no blind thread-release patch is justified because it
could unbind another caller's current context/API.

## Receipts and remaining scope

The compact receipt is
[validation/STRICT-ARC-20261111.json](validation/STRICT-ARC-20261111.json).
Persistent raw status, logs, source derivation, current mount certificates,
device reports, live FD observations, native/image outputs and before/after
manifests are under `artifacts/strict-arc-20261111/control/{release-v1,sanitize-v1}`.
`artifacts/strict-arc-20261111/qualification.json` annotates the raw-status scope.

Both batches released `/tmp/dome-sculpt-arc-device.lock` and all their owned
processes closed. No additional GPU runs were launched while root's full P1
workload held the lease. Native/GUI/reader/full-workload qualification,
physical-headset acceptance, main admission and the complete original goal
remain separate requirements.
