# Actual Wayland and synthetic XR conversion controls

The existing conversion command now has actual native nested-DomeD Wayland and
OpenXR-loader control evidence: 11 Wayland checks and 30 new stereo XR checks
each Release and ASan/UBSan. The latter uses the real application, EGL rendering,
swapchains and input handling with synthetic tracking/controller data. It is
not a physical-headset test.

Wayland checks include the real universal menu, ordered native Save,
compositor-delivered Undo/Redo and normal CloseWindow. The nested compositor
uses its matching Release SDK; the sanitizer client uses its own SDK. An
initial attempt incorrectly gave the unsanitized Rust compositor sanitizer
FLTK libraries; that setup failure is retained separately.

XR checks cover Accept, Cancel, focus loss with a held trigger and a converted
single-object scene whose Bake source no longer exists. Native results are
compared to a separately generated public-worker result, not another execution
of the synthetic UI route. Both eyes have visible BDF **Convert normal detail**
labels, balanced acquire/wait/release and actual rendered content. The same
runtime passes the existing recipe consumer's 22 checks each profile.

Twelve new and four legacy native Scene pairs are byte-identical across
profiles. Eight pre-input stereo PPM pairs are exact. Later asynchronous-worker
UI frames are not claimed byte-identical. The registered CMake graph contains
621 tests including the new loader test; this is configure-only evidence,
separate from the still-running frozen 620-test coherent regression.

The sanitizer OpenXR run uses the already documented private patched Mesa
driver and matching EGL, with no leak suppression or system installation.
Complete commands, source/SDK/binary hashes, logs, pair reports and setup
negative are in [the receipt](validation/CANONICAL-NORMAL-WAYLAND-XR.json).

These controls do not establish semantic correctness of every later operation.
The separately retained Clear Bake and partial-rebake counterexamples still
block Scene31 conversion admission to main. The full production goal, including
later controller drawers, remains incomplete. No headset launch or active
session restart occurred.
