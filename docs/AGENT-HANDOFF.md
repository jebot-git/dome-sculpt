# Resumption entry point — combined plane-cut authoring

Read [HANDOFF.md](HANDOFF.md), [../AGENTS.md](../AGENTS.md),
[VALIDATION.md](VALIDATION.md), [PLANE-CUTS.md](PLANE-CUTS.md) and
[REQUIREMENTS.md](REQUIREMENTS.md). The full production objective remains active
and incomplete. The current stage is integration plus concrete cut cancellation
and real-loader workflow coverage, not completion of Divine Cut or the suite.

Repository /home/blux/de-xr/dome-sculpt, branch work/plane-cut-integration.
Application merge 2ec8a5a preserves both de3f433 (plane source 5b0f9f5) and
production main 2c89e8d (source 79fe64a). Fetch/status/collaboration inspection must
precede overlapping work; do not reset or force-push newer remote history.
The parent is not a repository. No agents were spawned for this stage.

FINAL FETCH: newer main 9a0cfc4 / source 20c132d adds sculpt masks/symmetry and is
not integrated here. This verified stage is published on its feature branch;
main is preserved. Reconcile that upstream FIRST, before group-cut extensions.
Read its current handoff/SCULPT-MASKS and preserve both histories. New Volume v3 /
Brush/Sample layouts require fresh coherent consumers; reconcile Document mask
controls, immutable mesh mask/cache fields, shader/cap attributes and XR scripts.
Masks page 15 no longer is just reserved upstream; Cut page 17 remains separate. Keep this
branch's sixth worker and accepted cut cancellation/final-save contracts.

Skin stays on XR page14, Masks reserves15, Retopo uses16 and Plane cut uses17.
Preserve the combined input bounds, page routing, slider row counts and renderer
labels. Retopo New/Resume/Mode closes the menu to expose pressure; cut authoring
uses controls/hand placement while pressure strokes are disabled. From hand
keeps the confirming command_device, including left ownership with both tracked.

The cut worker captures the displayed immutable mesh and BakeSource, guarded by
session/generation/revision/object IDs. Preview requests coalesce; preview_request
must match current request before Apply. Closing restores the original unclipped
snapshot and drops obsolete caches outside model/input locks. Clipping uses the
selected object's document-space position before the canvas transform; cap range
UINT64_MAX is independent. Preserve main's GPU-cached retopology overlay and
source material binding/normal-frame behavior while extending this path.

Mapped BakeOptions validate source triangle/corner barycentrics, reconstruction
and atlas ordering. Empty maps retain ray/cage baking. Apply uses FileJob's appended
cut_session and one guarded global transaction, retaining the exact hidden source.
CutCancel during accepted publication now requests asset-owner cancellation;
acknowledgement retains the draft/preview for retry. A further Cancel closes it.
Do not reset the session or discard the preview while publication still owns it.
wait_asset covers accepted cut work. Standalone --save drains accepted work, then
discards remaining unconfirmed retopo/cut drafts before saving committed data.

All six workers, including cut, use the predicate-mutex shutdown handshake.
The deterministic worker-shutdown fixture pauses actual wait registration; the
new cut-cancellation-boundaries fixture pauses a real prior save's fsync before
queued publication, exercises desktop and joystick Cancel, checks exact source/
preview retention and retries with one global Undo. Do not replace these barriers
with arbitrary sleeps or weaken the source/geometry assertions.

Combined Scene v8 and complete legacy v7 decoding remain intact. Cut groups/outputs
use existing records; no new durable recipe/version was invented. Preserve
immutable materials/origins and the live SourceUV normal-frame guards. Legacy
UvDerivative normals cannot be relabelled original-frame data. Further frame
transport requires explicit versioned records, native/runtime/export agreement
and independent consumer evidence; read NORMAL-FRAME-TRANSPORT.md.

Use build-cut-integration and the toolkit-authoring/lib prefix for this source;
the old build/ contains the earlier feature-stage executable. GUI scripts were
copied byte-for-byte to artifacts/cut-integration-ui/tests with its build symlink
pointing to build-cut-integration. build-sanitize was rebuilt coherently for the
combined headers/core. VALIDATION.md records current results and exact limits;
older -2c89e8d and -de3f433 handoffs/validation are historical archives.

Next cut work: repeated group/multi-material cuts, not another kernel-only or
active-object UI implementation. Current separate shell/cap outputs are editable,
but open-shell closed-cap mode rejects and open trim leaves siblings untouched.
General Booleans, stronger contours, source compaction/GPU preview caching,
durable recipes, quality controls, full material/skin/morph transfer and draft
recovery remain incomplete. Continue every other REQUIREMENTS row, comparing
against integrated main before reimplementing its skin/UV/material/Unicode work.
Preserve C++20/FLTK/EGL/OpenXR, canonical BDF Fontconfig/FreeType helpers and DomeD
universal menus, modal history/cancellation and controller sliders/wheels.

Verified at 2ec8a5a:69 non-XR Release suites, full127-check real-loader XR,
9 whole-core sanitizer suites,103 native GUI checks and actual Intel ADL-N
MToon81 plus full spatial-render checks all pass. All stage handles are terminal;
no compiler/test/app jobs remained in the final inventory. No system install,
session restart, physical XR/input-device or new compute execution was performed.
The full-loader40-second client timeout is unchanged; isolated cold graphics
startup diagnostics and exact scopes are retained in VALIDATION.md.
