# Current native CUDA and ROCm build checkpoints

Current GPU-resolution sourceade92df (qualified41deb41, publishedfb75ea5) now
also compiles complete native FLTK/OpenXR applications and selected new sampler/
resolution/reference targets with checksum-pinned CUDA13.4.59/sm86. This is a
fresh current application build, not an alltarget/alltest or positive hardware
claim. Registered compute-missing-gpu +native-contract2/2 pass; direct nonempty
sampler/Scene missing-device checks retain exact original data/owners. All1,148
captured source/application/archive/test hashes remain unchanged.

The same source's first full ROCm build compiles but fails the native application
link: system GCC emits absolute C/C++ objects while the ROCm Clang link driver
defaults to PIE (R_X86_64_32/32S diagnostics). Source3fc5035 makes position-
independent project code the ROCm default before target creation, while retaining
explicit caller overrides. CPU/CUDA/OneAPI configuration defaults are unchanged.
The old failed link, exact flags and full invocation remain preserved.

Fresh complete native application and selected targets now compile/link with
HIP7.1.52802/gfx1030, system GCC16.2.1 C/C++ and ROCm Clang20. Native C/C++ app
objects show-fPIE, static libraries-fPIC and final executable is ELF DYN. The
native-CPU arithmetic contract1/1 passes; real HIP missing sampler/Scene calls
refuse explicitly without partial changes. All1,153 captured source/application/
archive/test hashes remain unchanged. These CPU contract tests do not execute
AMD arithmetic. No eligible AMD or NVIDIA device is present; neither backend's
positive numerical execution/performance/loss recovery is qualified here.

All builds use optimized Release and the current canonical DomeD FLTK/bitmap
prefixes as the regular user. No system dependency/library/toolkit install or
active-session restart. Read the receipt for source/binary/log/flag hashes and
precise target lists. Separate CUDA/HIP brush scalar-transfer/error atomicity
work is underway on an isolated descendant; this checkpoint retains published
brush behavior and does not qualify that later product change.
