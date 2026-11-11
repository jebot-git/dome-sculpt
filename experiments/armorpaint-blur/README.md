# ArmorPaint blur extraction experiment

This opt-in, UI-free experiment adapts one texture-space blur kernel from
ArmorPaint's current native C source. It demonstrates reuse inside Dome Sculpt's
existing OpenGL/EGL resource ownership. It is excluded from the application's
root build and install targets, and is not a complete painting backend.

See [NOTICE.txt](NOTICE.txt) for exact source, license and alterations, and
[the integration assessment](../../docs/ARMORPAINT-EXPERIMENT.md) for the boundary
to native paint/history and the remaining production requirements.

From the repository root, using a regular user and existing C++20, CMake/Ninja,
Python3 and EGL/OpenGL development packages:

```sh
cmake -S experiments/armorpaint-blur -B build-armorpaint-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-armorpaint-release --parallel 2
cmake -S experiments/armorpaint-blur -B build-armorpaint-sanitize -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPROOF_SANITIZE=ON
cmake --build build-armorpaint-sanitize --parallel 2
python3 experiments/armorpaint-blur/validate.py \
  --release-build build-armorpaint-release \
  --sanitize-build build-armorpaint-sanitize \
  --output-dir build-armorpaint-evidence
```

Use `--profile software` for a machine without the documented Intel Arc device.
The `arc` control requires current EGLDisplay → EGLDevice → DRM node
`/dev/dri/renderD129` → sysfs PCI `0000:03:00.0`, vendor/device `8086:56a0`, plus
the Arc A770 renderer. It fails if that identity is unavailable; an environment
request or renderer label alone cannot satisfy the hardware control. Software
requires llvmpipe. Device-specific identity is deliberate validation policy.

Generation is offline and happens only in the binary directory during the build.
The complete vendored source hash is checked before extracting the original
weight function. Configuration and generation do not modify tracked files.
The unchanged repository `NativeGraphics` EGL owner is linked by the consumer.
The reusable static library has no own window, UI, event loop or pixel readback.
Python is test/build orchestration only.

Each profile exercises seven fixtures, independently checking every float
component against a long-double oracle, HDR and tiny coverage, nontrivial masks,
rectangular clamp boundaries, a cross-only impulse, untouched source/mask data,
host render-state restoration and rejected invalid texture/alias/budget inputs.
Five test-only GL API allocation failures verify partial object cleanup. Both
sanitized runs enable `detect_leaks=1` and strict UBSan, without suppressions.
The script records original outputs/exit codes, source/binary/generated hashes,
device identities and software/Arc and Release/SAN comparisons in its output
directory. A compact committed receipt is linked from the assessment.

Inputs are linear unassociated RGB plus independent FLOAT coverage; output is
associated RGB plus FLOAT coverage. A separate R32F mask gates the blur. These
are explicit experimental plane semantics, not proof that arbitrary native paint
planes or source/normal/VRM data can be replaced by this texture. Finite data and
unit-interval mask/coverage are caller preconditions. The adapter requires a
clean GL error state and valid current context. It caps each dimension at 512
and defaults to a 4 MiB caller-texture budget; full 512² needs an explicit larger
budget. The production-sized native pipeline remains unmodified and unfinished.
