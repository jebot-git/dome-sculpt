# CUDA/HIP scalar transfer integration

Mergea3c52b08625c401b64ce5c2e12552df3cc3ae36c preserves current main4fbc72d
ROCm default-PIC repair while integrating qualified runtime53a26f0/finaldabd865.
Source/header/test/asset/tool/packaging match the component; CMake differs only
by the independent six-line PIC default. Both merged CUDA-default and HIP-default
module/probe configurations compile/link and pass actual unavailable-device
checks. The component also has full native application builds with explicitPIC=ON.

Read GPU-BRUSH-TRANSFERS.md/.json for exact130Release/130SAN vendor-API-shim
scope, real SDK no-device status and byte-preservation checks. Source/Brush/Sample
math/layouts are unchanged. New private host staging and constinput→float kernel
reduce readback to4bytes/sample, bound device chunks and keep partial failed
readbacks away from caller memory.31 recorded component artifacts were rechecked
intact before integration. Component hashes are sealing-time; no before/after
binary proof is invented. Shim tests do not establish GPU arithmetic or actual
vendor device-loss recovery, and reduced traffic is not a throughput measurement.

Positive NVIDIA/AMD numerics, real recovery/performance, remaining eligible
workloads and the complete production requirements remain open. No Scene/schema
change, Source24/25/26 admission, system install, physical test or desktop restart.
