# Canonical bitmap SDK migration qualification

Runtime `592bc76b67f396b5fc594535722e6528740fc278` requires the canonical
DomeD `53b0fcaf533ad867300d65a4178285d0a2dbef58` Cairo cache/deferred-SHM
cleanup patch. Configure rejects the preceding bitmap SDK explicitly. The
24-patch FLTK source and all153 bitmap public headers remain unchanged.
No public class layout or scene archive changes in this migration.

Fresh optimized Release and whole-owned ASan/UBSan builds recreate the native
FLTK/OpenXR application, core, support, renderer and selected consumers. The
37 selected targets include34 test executables for35 registered commands.
Configure/build completes in540.67s Release and900.71s sanitizer, with all2032
captured inputs unchanged per profile. This is a fresh selected-target build,
not an all-target build. Owned sanitizer code includes matching instrumented
FLTK; the external bitmap libraries use their optimized canonical build.

All35 original registered commands pass in each profile, preserving their
commands, timeouts and properties. Wrapper durations are139.28s/588.36s and
all2112 captured inputs remain exact per profile. These cover source paint
capture/import/export/portable compilation, sampling/material consumers, HDR
package/codec/budgets, native recovery and file ordering. They are selected
regressions, not a complete production test suite.

The actual fresh application also passes8 native FLTK checks per profile on
private Xvfb, bus and HOME: universal-menu Open, owned chooser cancellation,
editable-package recovery as an unsaved document, Save As cancellation, exact
native Save, clean reopen and Close Window. Both sanitizer and Release exit0;
all1610 captured inputs remain exact per profile. No suppression is used.
The old-SDK configure control fails with the intended specific diagnostic.

[The machine-readable receipt](validation/NATIVE-PACKAGE-SDK.json) includes
original commands, per-stage results and immutable status digests. Raw logs and
before/after input maps remain under `artifacts/sdk-proof` in the qualification
worktree. The preceding Cairo32-byte failure and same-binary fixed-SDK control
remain documented in NATIVE-PACKAGE-OPEN.md and DomeD SHM-LIFETIME.md.

No new Wayland/compositor, physical headset, GPU/compute or full-suite claim is
made. The standalone-XR Mesa112-byte report remains open. Later generic append,
painted native-container producer and agent changes are outside this source.
The original full production objective remains active and incomplete. No system
installation or active session changed.

The follow-up provenance-only continuation refreshes `third_party/sources.json`
from the exact installed53b bitmap lock/manifest. Its four patch digests, all16
built-library hashes, all24 unchanged FLTK patches and unchanged builder digest
are verified against the canonical commit. FLTK retains its actual f797 source
commit with53b verification; bitmap provenance records53b. This corrects stale
manifest data and does not reinterpret the592 runtime proof as a new build.
