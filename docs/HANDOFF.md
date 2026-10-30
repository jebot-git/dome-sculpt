# Paged native gestures, retained normal frames and CUDA qualification

Product source `233e586` combines the published paged storage milestone
`2a83eca`, retained frames `414799f`, existing-bake preview fix `5bd6e7f`, ordinary
renderer capacity fix `fbb90f0` and ordered desktop gesture worker `17e540d`.
The publication preserves all histories. Test-only `4943af9` acknowledges actual
XR menu images; `479a348` qualifies CUDA builds and missing-device failure.
All product C++ and public headers remain identical to `233e586`.

The full production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md remains
active and incomplete. Prior exact handoff/validation/agent notes are archived
with `-2a83eca`; earlier histories and the full requirement matrix remain.

## Delivered

- FLTK Canvas sends ordered rays to a dedicated seventh worker. Snapshot,
  evaluated visibility/UV picking, mutation and immutable preview preparation
  occur outside the UI/model mutex. Every delivered sample is retained up to
  the explicit4096-sample transaction budget; overflow cancels the whole stroke.
  Accepted release creates one Undo. Cancel, failure and stale replacement retain
  original authority. Save/Close/destruction drain accepted work in order.
- Painting on moved/skinned/morphed meshes maps displayed positions and normals
  back to original UV layers; rest geometry, source and deformation remain exact.
  Native-only publication faults restore history/content/source/preview and defer
  recovery until success. Generic Document transaction hardening remains separate.
- Scene9/material archive2 retains independent original PBR/MToon/canonical
  normal and tangent frames, stable morph target identities and source origins.
  Mesh workers and standard exports split/remap all related channels together.
  Independent GLB/VRM consumers cover the implemented subset. Legacy derivative
  data is not silently relabeled; live SourceUV editing and general canonical
  conversion remain incomplete, as detailed in NORMAL-FRAME-EXPORT-STATUS.md.
- Existing avatar bakes without captured canonical authority keep their posed
  Mikk preview basis; no persisted/export frame is invented. Bake/Clear/Undo
  invalidates cached packing correctly. The restored ordinary renderer supports
  valid multi-object aggregates above4.8M vertices/6M indices and256MiB uploads,
  retains per-frame quotas and checked address ranges, and recovers from actual
  failed driver uploads. Ordinary vertices remain56B; retained frame block40B
  is optional. Full source vertex color support remains separate pending work.
- CUDA13.4.1 is now a verified optional compiler/runtime build dependency, staged
  without system changes from five checksum-pinned NVIDIA archives. Full native
  FLTK/OpenXR and test targets compile with NVCC13.4.59/GCC16.2.1/C++20 and explicit
  compute86/sm86. No eligible GPU throws while retaining host samples; positive
  execution fails explicitly on this Intel-only host. NVIDIA numerical/device
  evidence remains required. Runtime authoring remains native C++.

## Current combined proof

CPU native103/103 in248.68s; CPU synthetic real-loader XR150+17 in298.26s.
OneAPI31 selected suites in14.52s, actualArc11 renderer fixtures, synthetic
resolution17 and mainXR150 pass. GUI216 assertions pass in21 final flows,
including realDomeD Wayland, native worker barriers/failure, accepted posed
painting, native VRM export, rig, retopo, bake, compact controls and Unicode.
The first posed-paint run exercised cancellation because of a harness setting;
it is retained separately and accepted painting was rerun correctly.
Whole C/C++ core/xatlas/Mikk/UI/render ASan/UBSan/leaks passes103/103 in598.19s
with explicit exit0. The earlier interrupted build wrapper and its no-test state
are retained separately; build completion alone was not counted as a pass.
See VALIDATION.md and the local artifact manifest for exact source/proof scopes.

## Continue the complete goal

1. Preserve the exact combined proof and every requirement still marked pending.
   Do not relaunch physical XR unrequested.
2. Integrate private static COLOR_0 checkpoint `d3e5e61` (product `be1858c`) and
   verify the new packed ABI with fresh consumers. Private root union `0c9a44e`
   includes this source plus the current lineages; its fresh CPU build is running.
   General colored-target bake policy/receipts and COLOR/UV morphs remain required.
3. Integrate generic Document admission/publication/history rollback hardening
   and conservative fine-resolution picking after their agents freeze proof.
   Wire the additive cancellable picking overload into the native stroke worker.
4. Finish every remaining production row: live source UV, interoperable normal
   preparation, full glTF/VRM import/export and native authoring, stronger weights,
   procedural/detail tools, incremental CPU/GPU work and required device evidence.

Canonical DomeD282467e/FLTK1.4.5+22 patches remains the tested toolkit. Use
explicit share/fltk and lib64, inspect actual linkage, build regular-user Release.
The physical8f4d67d WiVRn test remains stopped with basic functionality confirmed.
No system installation or active-session/display-manager restart occurred.
Preserve primary old frame/creator WIP: before the eventual fast-forward, record
exact bytes and retain an explicit-path stash for overlapping tracked/untracked
files. Never reapply obsolete prototypes over the integrated implementation.
