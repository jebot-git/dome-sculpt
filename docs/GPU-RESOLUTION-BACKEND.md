# Selected-backend forward sculpt resampling

Qualified runtime/test sourceade92df in artifacts/resample-contract-f01e04f
implements real forward interpolation of independent sculpt layers and sparse
masks on the selected GPU backend. CPU keeps the existing native-double map
algorithm. GPU sessions upload each immutable source field once, carry bounded
coordinate-only batches and return eight-byte numerical values. No CPU
interpolation precedes GPU queries, and missing/failed devices never substitute
CPU work. The candidate stays detached through unchanged reconstruction, surface,
normal and topology quality checks and source-generation publication guards.
There is no Scene/Volume archive or public-layout change. Reverse reconstruction
and geometry/quality remain CPU work; full accelerator requirements stay open.

The A770 has no native FP64. Pinned BSD3 SoftFloat3e supplies integer binary64
with numeric nearest-even results; raw source/license and reproducible generator
are retained. Host constexpr reciprocal tables were changed into device-callable
constant accessors after an actual CUDA compile failure. Native CPU/FMA flags
also exposed a one-ULP interpolation difference: an actual production-sampler
negative fails, while explicit contraction-off passes44,217words in Release and
whole-module ASan/UBSan/leaks. CPU sampler code is separate so brush flags are
unchanged. The actual source-file properties are checked in both native builds.

Fresh complete native FLTK/OpenXR CPU and OneAPI applications and selected tests
build on the current canonical DomeD toolkit/bitmap prefixes. Selected CPU7/7
passes3.97s; A770 OneAPI6/6 passes2.81s. Fresh whole-owned C/C++core/support/render/
frontend sanitizer build completes; selected6/6 ASan+UBSan/leaks pass20.33s.
The first registered SAN6/6 pass22.71s had a malformed post-manifest from a
script escaping error, so it is preserved separately. The corrected samebinary
batch has1,149 source/binary hashes equal before/after. No single12-suite SAN
claim, external-driver instrumentation or SAN application-UI execution claim.

Actual native/controller tests retain exact no-op, layer/mask state, cancellation,
later injected backend failure, history, stale sources, ordered Save/reopen and
retry. Repeated-session/destroyed-host-grid tests pass19,661CPU/19,662GPU checks.
Three actual Volume jobs match1,152,784 native serialized bytes across CPU/GPU,
including hidden-layer data and mask exceptions. Missing-GPU Scene operations
retain exact original owner/content. The CPU allocation fixture checks91 sampled
transactional boundaries, not exhaustive GPU or whole-worker allocation coverage.

Actual A770 compute and A770 GL simultaneously pass17 synthetic-runtime loader
checks: accepted/cancel/noop/focus/unconfirmed modes, stereo images and balanced
ownership. Every mode independently reports Intel Arc A770. Native universal-menu
workflow15 and compact double-bitmap-size4 pass, with screenshots and actual
loader library-initialization paths retained. GUI harness uses private Xvfb and
its inherited termination teardown; loader modes close normally. This is not
Wayland or physical headset acceptance. All1,152 captured source/application/
archive/test hashes match across the device/GUI batch. Canonical FLTK, bitmap and
SYCL load paths appear in the actual GUI process loader trace.

Current CUDA13.4.59/sm86 and HIP7.1.52802/gfx1030 backend translation units compile
and minimal vendor-runtime probes link. Missing devices explicitly refuse with
source cells unchanged. Full CUDA/HIP native application builds and positive
NVIDIA/AMD hardware are not claimed by these object/module checks.

Small full-job timings are CPU71.78/68.91/70.84ms and OneAPI130.09/66.86/64.65ms.
First-use overhead remains; concurrent user/build activity and this small sample
do not prove general speedup. The earlier dense interpolation-only benefit is
separate. Sustained device performance/loss recovery and other eligible GPU work
remain required. A private regular-user CPU stage verifies identical executable
text/rodata, seven humanoid asset files and the SoftFloat license. CMake changes
installed RPATH, so whole-ELF byte equality is not claimed. No system installation,
physical launch, toolkit fork/suppression or active-session restart occurred.
