# Native resolution integration checkpoint

Private frozen sourcef01e04f in artifacts/resample-portable-c0fa331 combines
selected-backend layer/mask interpolation with device-callable binary64 constants
and detached-grid/missing-Scene/repeated-job tests. Fresh optimized CPU and
OneAPI complete native FLTK/OpenXR application and selected test targets build
against the canonical eee23b7 toolkit/bitmap stack (DomeD d271cf6 inputs).
This is not an alltarget/all401-or404-suite build or test claim.

CPU6/6 passes4.87s; actualA770 OneAPI5/5 passes7.72s. The production sampler
has19,661CPU/19,662GPU checks, including independent map results across repeated
sessions and destruction of the original host grid before further sampling.
Actual Scene resolution with a missing selected GPU refuses with original
layers/mask/owner unchanged. Three repeated native Volume jobs match the exact
1,152,784-byte CPU result recorded in the preceding worker checkpoint.

The actual application passes17 loader resolution assertions with software GL,
then separately the same17 with simultaneous Arc GL rendering and OneAPI compute.
Every hardware mode independently reports Mesa Intel Arc A770. These are real
GL images through a synthetic OpenXR runtime, not physical headset evidence.
FLTK universal-menu workflow15 and compact640x360/double-bitmap-size4 checks
pass, covering Cancel-default, Ctrl+W/Escape/universal Close, accepted Apply,
Save/reopen/Undo/Redo, precise ray-budget status and unchanged original data.
Screenshots were visually inspected. GUI harness teardown is its inherited
termination path; this does not add a normal-App-close lifetime proof. Loader
modes close normally. No Wayland-compositor result is implied by private Xvfb.

All1,149 captured source/binary hashes match before/after the tests and both
loader passes. Controlled ldd using the test environment resolves current local
FLTK/bitmap/SYCL prefixes with no missing dependency. The separately preserved
ambient ldd lacks SDK activation; it is not the test process environment or a
successful deployment claim. Actual full GUI DSO maps were not captured.

Small complete-job timings are CPU79.24/107.03/77.96ms versus OneAPI147.29/
71.53/72.80ms over three runs. First-use GPU overhead remains visible; these
concurrent-user/build measurements do not establish a general performance claim.
Backward reconstruction and topology/surface quality remain CPU work.

Fresh whole-owned native C/C++core/support/render/frontend sanitizer build53391
is still live; no final native SAN pass is claimed. Separate original worker
c0fa331 has its completed5/5 core/support SAN proof. An additional native-CPU
optimization probe finds a one-ULP fused multiply/add difference in interpolation;
the same probe with contraction disabled passes4,380words. Separate childade92df
pins contraction for interpolation only and passes44,217words in Release and
SAN, with an actual unfixed production-sampler negative. Its fresh full native
builds are pending. Thusf01 is not the final portable runtime admission.
No main runtime merge, installation, physical launch or active-session restart.
