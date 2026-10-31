# Controller selection on the viable authoring candidate

The complete production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md
remains incomplete and is **paused at the user's request**. All additional polygon/holding/VRoid-only/FBX-OBJ requirements
remain required. This extends the existing native C++20 FLTK/OpenXR application.

## Current integration

The focused selection request is implemented by `3cb4517` and `f3f9199` on
the user-passed candidate. The first XR menu page has a Selection toggle;
either controller can touch or point and trigger to select, and either grip
modifies membership. Selected objects remain solid; unselected visible objects
remain at 18% opacity. The XR hierarchy synchronizes groups, active/multiselected
objects, collapse, paging and visibility undo. All controller events retain
release/focus guards and editing authority. Closed hierarchies are not rebuilt;
group memberships use bounded traversal. Read XR-OBJECT-SELECTION.md and
validation/XR-OBJECT-SELECTION.json.

The coherent optimized Release passes all 250 configured CPU suites in 269.88s.
Nine selected own-core/render/support ASan/UBSan/leak suites pass in 1.21s;
native selection has 182 checks including 140 allocation-failure prefixes.
Actual native FLTK/DBusMenu workflows pass 12 normal + 12 compact checks.
Actual OpenXR loader/GL fixtures pass 11 software + 11 Arc checks, and seven
Arc renderer checks verify uniform-only selection/ghost presentation without
geometry readbacks. Tracking in these fixtures is synthetic. This selection
change has not received a new physical headset test. No system installation or
session restart occurred. The broader production goal remains paused; all
agents are idle. Resume development only when the user requests it.

The user passed the physical headset test of combined candidate
`993e8af7ebc247e3239df3a1acd27fc47fb8e999` on 2026-10-30 and designated it a
viable candidate for further implementation. The test ended gracefully and its
project was saved. The optimized candidate passed all247 configured Release
suites and24 selected sanitizer suites before launch. Preserve the local
`work/production-authoring-c1` branch, binary and saved session as the physical
baseline. This milestone publishes its combined sources with the qualified
selection extension. Read HEADSET-CANDIDATE-993e8af.md and its validation receipt
for the earlier test's exact scope. The component notes below are historical;
their narrower proofs do not replace the current integration receipt.

Product sources21e140e +c166f3b (primary0a19281 +5916801) repair pinned JSON
binary factories and nine null-to-container promotions that could terminate
while unwinding allocation failure. Both real assertion negatives and dependency
patches are retained. The coherent optimized primary CPU build passes207/207
in393.73s; four forced-assert normal/diagnostic standalone ASan/UBSan/leak suites
pass in0.03s. This is not whole-current-core sanitizer or OS-wide OOM proof.
The Release is staged at artifacts/stage-json-5916801 under configured/usr/local;
no system install or session restart occurred. See JSON-BINARY-FACTORIES.md,
JSON-NULL-PROMOTIONS.md and validation/JSON-ALLOCATION-INTEGRATION.json.

Product sources b395ff3 +5ffbfea (primary283d701 +2b89aeb) recover input/file
worker error publication during persistent allocation denial. Normal failures
publish their status before completing a file promise; under memory pressure,
the original exception still reaches the future and bounded notices retry on
the same worker. Original input/file/export SIGABRT negatives are retained.
Corrected own-core/support sanitizer14/14 passes63.36s; primary coherent Release
14/14 passes22.11s and actual FLTK export Cancel/retry7 passes. The optimized
frontend is staged using its configured /usr/local prefix; no system install or
session change occurred. Read WORKER-PUBLICATION.md and
validation/WORKER-PUBLICATION-INTEGRATION.json for scope. Native gesture, mesh,
UV and retopology worker allocation-failure paths remain separate work.

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

## Historical delegated work before the combined candidate

- merge_contracts: full Scene12 polygon workflow is integrated. Private frozen
  consumer14ca381 is qualified; private Scene14 product0c890a8 implements durable
  current corner RGBA, source-retaining posed conversion, history/save/reopen,
  native painting/picking and actual FLTK/XR flows. Initial SAN15/16 exposed a
  pinned JSON binary-factory exception-safety assertion; dependency-only fix
  21e140e and final coherent qualification remain separate from this milestone.
- merge_palette: integrated a5ef50f8 is followed by frozen helperca67572 and a
  connected private Scene15 normal-preparation branch. Actual native/controller,
  FLTK, GLB/VRM software/A770 and selected sanitizer proof exists; final revised
  normal-strength/disable/UV guards and OneAPI qualification are ongoing.
  General source-normal adapters and UV adoption remain required.
- merge_docs: Scene13 codecad9, GLB89f31e3 and application9491b50/cbf6890 are
  qualified private dependencies. Held checkpoint298b6a8 has actual native,
  FLTK and software/A770 motion proof. General affine import admission and XR
  painting remain guarded while final consumers are connected. Preserve exact
  local affine chains and normal/reflection behavior.
- Root: private Scene16 key toolsfb29fde +receipt34d39c8 have actual kernel23,
  controller46, GUI7+9 and persistent-denial2493 proof, plus fresh whole-core/
  support sanitizer9/9 in78.33s. A separate selected-key freehand branch now
  connects the ordered native worker to desktop/controller strokes; it remains
  private and needs its own GUI/graphical/sanitizer and joint-integration proof.

Scene13/14/15/16 are combined in the published candidate lineage. Preserve
recursive source-feature checks, all retained-source edges, aggregate budgets
and normal/color authority. The unfinished import, prepared-normal/shape and
current-color cut/bake adapters remain separate private work with guards intact.

Both old8f4d67d and candidate993e8af have user-confirmed physical headset tests.
Candidate993e8af is viable for continued implementation; this does not complete
the production requirements. Synthetic/runtime/GPU fixtures remain separate
evidence. No system installation or active-session restart occurred. DomeD
main was rechecked at3024fb6; current FLTK1.4.5+23/native BDF/PCF inputs remain in
TOOLKIT.md. Native/Wayland tests explicitly use the private **lib** prefixes.

The primary old frame WIP remains stashc46d1d907a9ca0cdb56cda17e54ab20284939fc1
plus byte-verified backups. Preserve all private work and unrelated repositories.
Commit/push authorized milestones and rebuild coherently after layout changes.
Do not mark the full goal complete from a subset of passing checks.
