# Standalone XR: Mesa executable-pool lifetime

The installed Mesa26.2.5 software driver reports112 bytes in two allocations
when the standalone OpenXR application closes. This milestone supplies an
optional source patch for that dependency. Dome Sculpt's application/core,
public headers, formats and default driver selection are unchanged. No graphics
library was installed or active session restarted.

## Root cause and candidate change

The two allocations are the56-byte sentinel and remaining free block created
by `u_mmInit` for POSIX `rtasm_exec_malloc`. All generated functions have been
freed, but the process-global allocator keeps its metadata and10MiB executable
mapping until the driver unloads. The installed libgallium build ID is
`72630dfb6c6fe4925ad2fe6ca0e447cf5972a64e`; actual disassembly and the matching
source map the leak to this allocator. A test-only `eglReleaseThread` call after
termination does not remove it and is not added to the application.

The patch retains the existing mutex and live-function ownership. After a
successful free, the coalesced single free block proves the entire pool is
unused; only then are the metadata and mapping released. A subsequent request
initializes a fresh pool under the same mutex. Failed initialization and failed
first allocation also release partial ownership. Zero, oversized and
alignment-overflow requests refuse before acquiring a pool. Windows and other
non-POSIX allocator implementations are unchanged.

The patch does not change executable-page permissions or code generation. It
may add mmap/munmap calls when sequential workloads completely drain the pool;
no performance improvement or cross-driver/platform qualification is claimed.

## Controlled evidence

The source archive is checksum-pinned to the value in the official
[Mesa26.2.5 release notes](https://docs.mesa3d.org/relnotes/26.2.5.html):
`794b3d1729075dae7ebd47c7283b7ca0cdb5dec668f7f11cc0a5d82f6138a0a5`.
No full source archive or driver binary is bundled in this repository.

An optimized, private unpatched llvmpipe driver and matching EGL were built as
the regular user using LLVM19. The exact unchanged owned sanitizer app/runtime/
project reproduces the same112-byte leak with that driver actually mapped.
This distinguishes the allocator change from an LLVM-version change relative
to the installed LLVM22 driver. The original attempt with only
`LIBGL_DRIVERS_PATH` silently used system libgallium; its failed identity check
is retained. Positive checks use an explicit private EGL vendor JSON and
private driver library path, with `/proc/self/maps` verification.

A complete changed rtasm TU was compiled and relinked through a full auxiliary
archive with the exact own unpatched driver support and byte-identical EGL.
All other upstream source files match. This is a changed-TU qualification over
an owned full parent build, not a fresh whole patched-driver build.

The same app, runtime and input project then exit0 with the patched driver and
no sanitizer report. All five real OpenXR loader/EGL/OpenGL Mirror cases pass
40 checks under the owned sanitizer app: all three methods, Cancel and focus
loss, balanced real stereo swapchains, saved native output, independent
rest-position mirroring and complete original/checkpoint Restore. These are
synthetic poses/actions and software rendering, not a headset or hardware test.

The native allocator test compiles the actual complete upstream u_mm, futex and
rtasm source TUs. Link wrappers observe real calloc/free/mmap/mmap64/munmap
ownership and inject metadata/mapping failure. Patched Release and ASAN+UBSAN
each pass241 assertions and2000 concurrent cycles. Tests include partial
initialization, failed split while another function is live, capacity/overflow,
reinitialization, alignment, actual generated-code execution, and preservation
of live allocations across other threads' frees. The original source fails the
same test with exactly two metadata allocations and one mapping retained.
Initial test-observer missing-futex and mmap64 coverage errors are retained;
no production change was made to hide either test error.

Read validation/MESA-XR-RTASM-LIFETIME.json for exact build/component scope,
source/binary identities, commands, terminal results and preserved failures.

## Reproduce the allocator gate

`third_party/mesa/SOURCES.json` pins the source, headers and patch. Apply the
patch only to that verified source, configure a normal matching Mesa build to
obtain `compile_commands.json`, then run:

```sh
python3 tools/test-mesa-rtasm.py --source /path/to/patched/mesa-26.2.5 \
  --build /path/to/matching-mesa-build --output /new/path/pool-release
python3 tools/test-mesa-rtasm.py --source /path/to/patched/mesa-26.2.5 \
  --build /path/to/matching-mesa-build --output /new/path/pool-sanitize --sanitizers
```

With the original pinned source, `--original` instead requires the exact
retained-pool counterexample. The optional tool downloads, patches and installs
nothing. It uses the matching build's compiler flags and requires a new output
directory, retaining commands/logs/input hashes and terminal status.
Qualification here is Linux x86_64 with GNU link wrapping. The generated-code
execution portion is compiled only for x86; other platforms require their own
validation before deployment.

The installed Mesa stack still has the original report until a verified driver
patch is installed through the system's package workflow. This repository does
not enable a private driver globally or replace system EGL. Separate Wayland
backend reports, full application/device/performance coverage and every
unfinished Dome Sculpt requirement remain required.
