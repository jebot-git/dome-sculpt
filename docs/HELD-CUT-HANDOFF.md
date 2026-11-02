# Held plane-cut publication

The native cut worker can publish an authenticated shell/cap while a source is
pinned with **Hold selected object** and **Ghost other objects**. The shell
becomes held and selected. Only it is editable; visible caps and pre-existing
neighbors remain read-only ghosts. The original stays hidden with its existing
authoring, placement, paint, current-color, rig, shape and source owners intact.
No stored lock/material flags are changed to implement isolation.

A private `CutHandoff` is minted only after the actual `publish_cut` helper
returns. It seals the complete immutable output and names its source, shell,
optional cap and group. Before publication the file worker checks append-only
output IDs, the source's visibility-only transition, every protected object and
group, and the exact output COW/immutable owners against the seal. The public
`edit_scene` path still rejects structural additions under isolation. Expensive
preparation and validation occur outside the model lock.

The existing staged transaction moves scene, history, selection, held authority
and epoch together. A cut-specific history endpoint follows Undo/Redo only when
the currently held endpoint matches that cut. Explicit unhold does not acquire
a new hold during later history navigation. Accepted-cut Save keeps its exact
completed scene; cancellation, an explicit-unhold race, or a changed document
fails dependent Save rather than writing a stale fallback.

Selected-key sculpt mode is explicitly incompatible with plane cutting. The
menu and direct API both require finishing that mode. An explicit saved morph
weight of one remains a valid authoritative cut source.

This checkpoint supports pinned holding. Moving a grip and cutting in a single
combined pose/geometry history transaction is still required. The existing
physical-grab finishing behavior is not claimed as that combined workflow.
Current-field target baking, UV1 compositing, prepared-color/normal topology
transfer and unsupported artist-frame/MToon cuts remain guarded.

## Validation

All builds use regular-user Release or a fresh ASan/UBSan own-core/support build
and the canonical private toolkit/bitmap runtime verified against DomeD d271cf6.
The prior complete preparation/publication fault sweeps remain separate frozen
proof at `9b823f5`; this checkpoint does not relabel selected faults as exhaustive.

- 157 actual native/controller colored-cut checks, including cap/no-cap,
  protected objects/groups, arbitrary-callback refusal, Cancel, immediate Save,
  one Undo/Redo, explicit-unhold history and cancellation race.
- Actual FLTK/DBusMenu holding/cut workflow: 16 checks. Selected-key mode:
  15 checks; compact held/cap controls: 7 checks at 640x360 and 2x BDF.
  Live clipping, modal cancellation, fixed actions/scrolling and saved geometry
  inspected.
- The unchanged cut/source transport is independently requalified through
  32 rendering/export cases and 257 checks on both software GL and explicit
  Intel Arc A770. Complete-image error is at most one; exact restored source
  layers/UV/corner colors and project restart are checked.
- Held publication persistent allocation denial: first32 and last32 of 10089
  measured real post-helper/precommit boundaries. Exact Scene/history/redo,
  content identities, lease, held/selection/epoch, mesh/source cache, original
  exception, existing output file and successful one-Undo retry all checked.
  Immutable helper preparation is witnessed and replayed as documented in
  `CORNER-CUT-ORDERING.md`; this is publication fault coverage, not another
  complete preparation sweep.
- Fresh own-core/support ASan/UBSan/leak checks: four cut/holding suites pass
  (31.80 seconds), all 25 generic transaction suites pass (80.94 seconds), and
  first16/last16 held publication boundaries pass with unchanged rollback oracles.
- Real OpenXR loader, synthetic joystick tracking and software GL: 21 checks
  over accept, unconfirmed and Holding-page-prefix cases. Actual radial menu,
  large slider, accepted shell/cap/source archive, precise saved half-space and
  balanced stereo shutdown all pass. The Holding preamble uses released-neutral
  rearming before hover/press; the cut fixture fixes its menu anchor independently
  of frame-number hand jitter. Neither the geometry nor source assertions were
  weakened during fixture correction.
- Combined regular Release scope: 30/30 cut/held/fault/transaction suites pass
  (18.86 seconds).

Evidence is under this private tree's `artifacts/tests-held-cut-*`,
`tests-menu-held-cut-first.log`, and `tests-menu-key-cut-first.log`. No physical
headset, system installation, restart, push or whole-goal completion is claimed.
