# Cancellable exports and polygon/color authoring

The complete production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md
remains active. All additional polygon/holding/VRoid-only/FBX-OBJ requirements
remain required. This extends the existing native C++20 FLTK/OpenXR application.

## Current integration

Product source `4cf3032` adds explicit queued/running GLB and VRM cancellation
through File / Cancel background work and actual controller Cancel work. Its
atomic commit arbitration preserves old output, ordered Save, document
replacement and accepted-export close/drain semantics. Internal asset
cancellation remains separate. Fresh Release14, whole-core/support SAN14 and
actual FLTK chooser/Cancel/retry7 pass. Read EXPORT-CANCELLATION.md and its receipt
for exact source/binary scope; do not relabel the preceding full-suite runs.

Product source `27365c9` combines published `7152f5e`, the complete polygon
lineage through `e8994a1`, qualified color preparation `8031163`/`950f56b`, native
context-capability corrections and polygon/color authority checkpoint `a5ef50f8`.
Read POLYGON-COLOR-INTEGRATION.md and its validation receipt for final source,
actual build/test scopes and remaining requirements. Older component receipts
are historical evidence, not automatically final-union proof.

The application now provides the shared native/controller polygon workspace,
stable N-gons, transactional worker acceptance, Scene12 source witnesses and
explicit restoration. Color preparation/baking preserves dual PBR/common planes
and complete original sources. A real same-mesh Strict conversion allows
polygon targets to prepare colors without inventing authority. Native compact
controls, local/global history, accepted Save ordering and held-target painting
are covered by actual workflow checks. Every consumer must be rebuilt together;
the polygon and color integration changes public headers/layouts from7152.

Import affine/source poses, UV1 sampling, native FBX deformation/materials,
COLOR/UV morphs, prepared-color topology transport and broader glTF/VRM remain
incomplete. The complete matrix retains sculpt/paint/remesh, humanoid/rig/weight/
shapekey/device requirements. Accepted snapshot exports retain close/drain
semantics alongside explicit cancellation. Do not wire shutdown or ordinary
internal authoring cancellation into accepted output.

## Active delegated work

- merge_contracts: full Scene12 polygon workflow is integrated. Private frozen
  `df3d3b2` adds actual raw COLOR_0 corner provenance and bounded topology
  transport. Its new consumer branch adds transient shared RGBA data and actual
  render/export/picking fixtures. No Scene14 persistent current-color authoring
  is enabled yet, and old source/prepared guards remain. Finish consumers,
  persistence/history and actual frontend workflows before removing those guards.
- merge_palette: `a5ef50f8` is integrated. New normal-bake work investigates a
  separately versioned Scene15 interpretation for generic native and retained
  artist frames, curved/mixed-skin/morph transport and fractional coverage.
  Foundational math/schema validation is not independent pixel parity.
- merge_docs: private affine math/hierarchy, interaction, common source ownership
  and bounded export factorization are validated prerequisites. Scene13 source
  placement and native affine edit prefixes are still being implemented. Full
  affine imports remain guarded until codec/effective-matrix/render/pick/paint/
  held/export consumers are connected. Preserve source local chains and exact
  normal/reflection behavior rather than projecting shear to TRS.

Scene13/14/15 reservations are coordinated feature minima, not already supported
archive versions. Preserve recursive source-feature checks and every existing
canonical/source-color authority boundary when merging them.

Only the old8f4d67d rudimentary milestone has physical headset confirmation. The
user ended that test. Synthetic/runtime/GPU fixtures are separate evidence; no
new HMD launch, system installation or active-session restart occurred. DomeD
main was rechecked at3024fb6; current FLTK1.4.5+23/native BDF/PCF inputs remain in
TOOLKIT.md. Native/Wayland tests explicitly use the private **lib** prefixes.

The primary old frame WIP remains stashc46d1d907a9ca0cdb56cda17e54ab20284939fc1
plus byte-verified backups. Preserve all private work and unrelated repositories.
Commit/push authorized milestones and rebuild coherently after layout changes.
Do not mark the full goal complete from a subset of passing checks.
