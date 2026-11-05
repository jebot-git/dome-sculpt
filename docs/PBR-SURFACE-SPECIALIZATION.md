# PBR surface specialization

Runtime79abe8e removes unnecessary typed MToon branches from actual PBR-only
frames before driver specialization. It retains the exact authored texture
samplers, UVs, filtering, PBR equations and native paint reconstruction. Mixed
or typed snapshots lazily use the complete original shader for the entire frame.
The private TextureState owns both programs, cleans up failed shader links and
abandons program names with the other lost-context resources. Public/private
headers, native formats and app/core ABI are unchanged.

The original frozen full689 source-paint-sampler test209 times out at180 seconds
with23 completed cases and no observed appearance discrepancy. Isolated timing
finds preparation below one millisecond, while cold shader specialization costs
seconds per comparison. On the same llvmpipe environment, six unchanged selected
comparisons improve from32.714s cold/4.578s warm to2.973s/0.717s. The unchanged full
72-case sampler matrix now passes cold in16.611s Release and21.964s sanitizer,
289checks each and maximum pixel difference0, with its original bound2 and180s
limit. This does not relabel the original timeout or promise universal timings.

Each profile additionally passes17 genuine PBR/typed/mixed transitions,81 existing
MToon/linear-target checks,38 sculpt-mask checks,7 accepted-selection rendering
checks and24 buffer-reuse checks:456 graphics checks per profile. A new transition
fixture initially copied a native PBR color override while comparing against an
unmodified typed source; the same original main renderer reproduces that failure.
The corrected fixture uses its actual white native-reference material. Original
negatives and the unchanged lighting bound remain in the receipt.

The complete changed render.cpp TU replaces only that member in a copied matching
owningab371 Release/sanitizer render archive; all other archive members are exact.
Every consumer TU is complete. The unchanged whole owning main application objects
are newly linked against that renderer and exact same core/support/SDK. Actual
native12+compact12, nested DomeD Wayland11 and actual-loader syntheticOpenXR30 pass
each profile, with exact inputs. The sanitizer XR client maps prove the private
owned EGL/Mesa implementation; there are no sanitizer suppressions or physical
headset/session changes. Compact bitmap controls were visually inspected.

Same Release binaries pass456 actual PCI03 Arc A770 graphics checks. All six
same-binary forced-software invocations refuse with status1. Actual render-node
file descriptors, PCI/vendor/device identity, driver maps and loaded-library
hashes are recorded. No compute acceleration qualification follows from GL tests.

Fresh canonical-toolkit configuration registers740 tests, including the new
surface-program-switch command; it is not740-test runtime or a fresh whole-source
application rebuild. Initial orchestration/fixture/configuration failures remain
recorded. Read validation/PBR-SURFACE-SPECIALIZATION.json for exact commands,
source/archive/SDK hashes, raw logs and limits. The full production goal, coherent
native36/37 authoring, remaining adapters, device/compute work and later controller
drawers remain incomplete.
