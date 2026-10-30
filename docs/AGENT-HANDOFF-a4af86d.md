# Production continuation after local meshing/GPU/plane-cut integration

The original full production goal remains active: read REQUIREMENTS.md and
HANDOFF-466ebd1.md, then current HANDOFF.md and VALIDATION.md. Combined application
sourcecf89a81 preserves origin a8af3f8 and local42b2ff3. Tests91ceb04/8b8dc60 do not
change product code. Do not replace or narrow the objective to this checkpoint.

Canonical toolkit is DomeD282467e/FLTK1.4.5+22 patches, built locally under
artifacts/toolkit-282467e with **lib64**. Use explicit FLTK_DIR per ../AGENTS.md.
Read parent /home/blux/de-xr/DESIGN_CONVENTIONS. No system installation/session
restart and no physical headset launch are authorized by these automated checks.

Keep immutable typed SDF ownership, actual dirty-sample ancestry, safe cache
fallbacks, exact Cancel/Undo, source/pose/material-frame identity, ordered file
jobs and paint/source preservation. Never infer reusable GPU geometry from a
revision/raw address/count alone. Cap previews change geometry independently of
the source token; shader-only clipping retains it. Preserve attributes4(weights),
6(mask); future frame attributes7/8/9 must share the static layout correctly and
retain cut_position/clip uniforms. SIX worker handshakes remain mandatory.

Masks15, Skin14, Retopo16, Cut17 are independent canonical joystick pages.
Resolution controls are being reconciled privately; avoid wholesale Document
replacement, lost cut exclusions, altered source availability or lost exact
uniform mask interpolation. Scene8/Volume2/3 remains this source's format scope.
Do not silently import private Scene9/Volume4 APIs without coherent consumers.

Active delegated branches (inspect live state before any restart):
- resolution-cut-authoring-cf89: native/XR resolution, physical band/Volume4,
  cancellable progress/asset lease, exact history/source preservation, DDA status.
- paged-sculpt-2c160cf: adaptive16³ pages/persistent shards and paged before-history;
  exact replay, quotas/fault/lifetime/performance validation pending.
- frame-runtime-prototype: raw normal frames, UV adoption and independent parsed
  GLB/VRM consumers; multi-primitive expression remap fix and representability work.

The shared primary has preserved frame/creator WIP. No reset/force-push/reapply or
staging unrelated paths. Commit/push authorized verified milestones to main,
retaining remote advances. Keep logs/manifests tied to exact source; archives from
other environments and older scopes are historical, not new combined proof.
Full-array assembly/uploads and CPU mutation under the desktop model lock remain
production bottlenecks; no sustained frame-rate claim follows from these tests.
