# Portable device arithmetic for sparse resolution work

Private frozen source2d17cb1 in artifacts/resample-portable-c0fa331 fixes an
actual CUDA backend compilation failure from parentc0fa331. Dynamic device
lookup could not access the generated host constexpr reciprocal arrays. The
reproducible generator now emits host/device accessors containing the exact
pinned sixteen constants per table. Raw upstream sources/hashes stay unchanged;
no arithmetic precision, rounding or reference tolerance changes.

Full backend_gpu.cu translation-unit compilation passes CUDA13.4.59/sm86 and
HIP7.1.52802/gfx1030 at optimized C++20. These are compile-only object proofs,
not full application linking or positive NVIDIA/AMD hardware execution. Initial
CUDA diagnostic and old HIP success remain in the separate worker parent.
The SoftFloat license is also included in system-install packaging.

Fresh standalone Release2/2 and whole-header/test ASan+UBSan/leaks2/2 pass.
OneAPI4/4 passes on actual Intel Arc A770 with native_fp64=false:1,999,168 host
arithmetic results plus integer conversions,1,250,000 device arithmetic/integer
results×4transactions,4,380host interpolation words and889,116device sparse
results×4transactions. Device selection is level_zero:0 and printed device
identity is A770. All1,140 captured source/binary hashes match before/after.
Repeated generation matches exactly and pinned raw-source hashes are checked.

This run occurred while the user's VRChat process and other builds were active.
Its dense component timing is CPU197.37ms packed-hash versus GPU82.11–108.24ms
including pack/validation; it is not a controlled performance regression result
or a substitute for the earlier5ed benchmark. Original measured logs remain
unmodified. Whole-resolution worker integration and quality/history/Save,
full CUDA/HIP linking and positive vendor hardware remain separate requirements.
The child inherits unfinished actual-worker code; no main runtime admission,
installation, headset launch or active-session restart is implied.

Separate minimal native probes now link each actual backend object with its
vendor runtime. CUDA reports insufficient driver and HIP reports no ROCm-capable
device; both nonempty sparse dispatches refuse explicitly with every source
cell byte unchanged. This is missing-device evidence, not positive hardware.
The first manual HIP link invocation incorrectly treated the object as source;
a second omitted its runtime. Preserved diagnostics are separate from the final
successful explicit-language/runtime link and do not indicate a product defect.
