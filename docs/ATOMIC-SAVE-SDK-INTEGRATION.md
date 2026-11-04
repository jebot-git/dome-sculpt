# Atomic save and final canonical SDK integration

Published main1fc20c4 adds portable atomic-write resource ordering
and requires the final canonical27/poll FLTK SDK. It contains no Source4/HDR,
private authoring/weights or new Scene-schema admission. The production goal
remains active and incomplete. Main was fast-forwarded from0b5ada0 and the
remote integration head was independently verified; no local commits were discarded.

The runtime repair is c13909f, carried by qualified8d20d8c: prepare paths and
open the parent directory before the output commit callback/rename. An
allocation failure cannot occur later merely while constructing the rename or
parent path after replacement. Directory ownership remains RAII, cancellation
and fsync/close errors retain explicit semantics. Read ATOMIC-WRITE-ALLOCATION.md
for the original allocation counterexample and narrow regression.

The final SDK source is DomeD6fa2624; its qualification341152b is published on
canonical remote main. It includes responsive compact bitmap picker25, actual
Enter callback reason26 and exact X11 clipboard ownership/reply cleanup27,
with upstream FLTK_USE_POLL=ON. All165 public include files and previous exported
symbols are retained. The private generated configuration, complete patch and
builder digests and actual imported libraries are bound by runtime-profile.json.
Desktop CMake checks the profile belongs to the selected prefix and the actual
linked bytes. The old24 SDK specifically refuses at its missing viewport patch.

Final immutable prefixes are:

- `/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-release`
- `/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-sanitize`

The corrected bitmap prefix remains
`/home/blux/de-xr/DomeD/artifacts/cairo-shm-drain-f797/artifacts/bitmap-runtime`,
actual source53b0fca. Its public153 headers and actual binaries are unchanged.
No existing prefix or active desktop was replaced.

This integration explicitly reuses immutable qualified8d objects/core/support
and relinks the native app and export helper through new CMake graphs against
the final SDK. The optimized parent is its audited four-member IO core with
unchanged matching392-build app/support objects. The sanitizer parent is its
fresh whole-owned asserted ASan/UBSan app/core/render/support build (209 tasks,
748.38 seconds). Only the previously absent sanitizer projection test consumer
is newly compiled against that exact copied core. There is no new whole-app
compile or full-suite claim. Configures and relinks finish0 in36.77 seconds;
product source/API bytes equal the8d parent, and copied input hashes stay exact.

Twenty core commands per profile retain their parent proof after actual binary,
fixture and dependency audit. Their ELF dependencies do not link FLTK or Cairo.
Release qualification composes18 initial Passed results and the corrected
required-fixture2 Passed results; the original missing-fixture2NotRun is kept.
The sanitizer runs one complete20/20 command in135.50 seconds. No unchanged core
proof is relabeled a fresh SDK-dependent execution.

Actual native UI qualification on the relinked current app passes:

- Normal and640×360/font32 X11, Release and sanitizer: eight checks per run,
  32 total. Real universal menus open the owned chooser, Cancel it, pause an
  accepted export at the durable write gate, preserve authoring/queued Save,
  Cancel the running export, retry to independently parsed GLB and Close cleanly.
- Native Wayland Release and sanitizer: nine named checks per run,18 total.
  The actual image chooser Cancel/Return, detached load, posed projection paint,
  ordered Save, exact Undo/Redo and normal teardown remain asserted.

The versioned export harness waits for native filename input readiness and
uses actual SelectAll/Paste/Copy. Clipboard ACK must come from the real app's
X-client base and exactly equal the full destination; Paste cannot accept the
chooser. Actual Return follows proven input focus, with no filename/OK pixel
guess. Runtime process maps verify final SDK library paths/digests. The four
final X11 ledgers preserve2070/2071 inputs each and both Wayland ledgers likewise
remain exact. No assertions, per-operation deadlines or sanitizer checks were
relaxed. The Wayland filename/isolation changes are the narrow qualified062 test
delta, not an unadmitted producer/runtime import.

The first sanitizer harness safely refused before app launch because the
compiler's libasan.so is a linker script. Those two exact negative ledgers and
logs remain. The final harness resolves the actual ELF from the executable's
ldd dependency, places it before the test interposer and passes clean exit.
No suppression is used. Earlier chooser readiness/preload negatives and all
canonical25/26/select-based failures remain preserved separately.

Read [validation/ATOMIC-SAVE-SDK-INTEGRATION.json](validation/ATOMIC-SAVE-SDK-INTEGRATION.json)
for commands, exact binary/source/SDK maps, source equality, copied support,
parent receipts and negative controls. All UI used private signed-RPM Xvfb,
private buses/HOME and a nested canonical software compositor. No GPU, physical
headset, system installation or session restart occurred. Earlier540 CPU/vendor
proofs retain their original runtime/SDK scopes; full production completion is
not claimed by this IO/picker milestone.
