# Pinned optional CUDA development toolkit

The CUDA build uses the existing native CUDA/HIP brush source and shared
arithmetic. This qualification uses NVIDIA CUDA13.4 Update1, NVCC13.4.59,
CUDART13.4.49, CRT13.4.59, NVVM13.4.59 and CCCL13.3.4.2.1. The official release
manifest and exact five archive URLs/sizes/SHA256 values are pinned in
[CUDA-13.4.1-TOOLCHAIN.json](validation/CUDA-13.4.1-TOOLCHAIN.json).

NVIDIA documents GCC16 and C++20 support for this toolkit in its
[Linux installation guide](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/).
The tested host compiler is GCC16.2.1. No unsupported-compiler override is used.
The NVIDIA redistributable components contain their own licenses; staging keeps
them under `licenses/`. No NVIDIA source or binary is checked into this repo.

Optional development preparation requires Python3.12+; the application runtime
and authoring workflows remain native C++. The helper verifies every archive's
size and SHA256 before extraction and publishes only the complete staged tree.
It refuses an existing destination, leaves system files untouched, and includes
no GPU driver or driver installation. Run as the regular build user:

```sh
python3 tools/fetch-cuda-toolchain.py \
  --destination "$PWD/artifacts/cuda-13.4.1"
cmake -S . -B build-cuda -DCMAKE_BUILD_TYPE=Release \
  -DDOME_COMPUTE_BACKEND=CUDA \
  -DCMAKE_CUDA_COMPILER="$PWD/artifacts/cuda-13.4.1/bin/nvcc" \
  -DCUDAToolkit_ROOT="$PWD/artifacts/cuda-13.4.1" \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DFLTK_DIR="$PWD/artifacts/toolkit-282467e/share/fltk"
cmake --build build-cuda --parallel 2
ctest --test-dir build-cuda --output-on-failure -R '^compute-missing-gpu$'
```

Architecture86 is this compile qualification target, with compute86 PTX and
sm86 machine code. Choose architecture(s) suitable for the actual deployment
GPU; this is not a claim that one binary targets every NVIDIA architecture.
CUDA C++20 is explicitly required. The positive compute oracle reports the
selected real device and compares against separately compiled CPU reference
arithmetic when an eligible device is available.

`compute-missing-gpu` explicitly masks CUDA devices and probes the CUDA runtime
for no visible device or an insufficient driver. It then invokes the actual
nonempty brush dispatch and requires an exception with byte-identical original
host samples. An unrelated probe error or silent CPU success fails the test.
This supplements the existing OneAPI no-fallback check. Empty dispatch alone is
not evidence of usable GPU execution.

The toolkit reproduction was staged twice from the same verified archives;
all2436 resulting file hashes match. The full optimized native FLTK/OpenXR application and all test targets build.
The missing-GPU CTest passes; the positive oracle exits1 with the explicit
insufficient-driver diagnostic on this host. Core compilation uses
`-O3 -DNDEBUG -std=c++20` and exactly one CUDA apply_brush/backend implementation.
Compiler/link/source hashes and detailed results are retained in the local
qualification manifest referenced by the milestone validation notes. This Intel-only host cannot
prove CUDA kernel results, device performance or loss recovery. Those remain
required, as do eligible accelerated workloads beyond the existing brush kernel.
