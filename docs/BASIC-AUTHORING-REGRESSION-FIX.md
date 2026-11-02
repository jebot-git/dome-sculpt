# Isolated combined-authoring regressions

Frozen9684486 in artifacts/basic-authoring-regressions-25fe208 is a child of the
untouched25fe candidate. It fixes a real input regression: material-view epoch
fencing accidentally prevented direct voxel sculpt/mask strokes before the
first display mesh existed. Only mesh sampling needs that fence. The actual
first preview worker is now held deterministically in the strengthened mask
fixture; this exact fixture fails against the original frozen archive, then
passes the corrected coherent builds. Existing PBR stale-endpoint and held
pressure/release negatives remain passing.

The other two changes update obsolete fixture assumptions: shared pages now
number28/max27 with actual projection/PBR/rig actions, and the loader's paint
script traverses the new projection submenu before Colour Presets. Its actual
blue-pixel assertion is retained. No persistent Scene schema or public ABI
changes are introduced.

Fresh optimized Release71240 and whole-owned C/C++core/support/render/frontend
SAN71315 builds are terminal0. Targeted5/5 pass0.75s Release and5/5 pass3.67s
SAN with leak detection enabled, covering import/controller, real native
strokes, pending-first-mesh masks, mask transactions and PBR endpoint guards.

Initial full-loader22752 has59passing checks then fails because the targeted
build omitted the bake-controls helper. Its terminal8/log/JUnit/output remain
preserved; coherent helper build75542 is terminal0. The unchanged full fixture
retry48146 is live. Inspect its exact output before restarting; no corrected
loader-pass claim yet. Original full39538150 remains live on untouched25fe
with three observed failures. These split results are not a single395greenrun.
Read validation/BASIC-AUTHORING-REGRESSION-FIX.json for exact hashes/scopes.

The full production goal remains active. Controller spatial drawers stay
required after the basic targets. No new physical test, system installation or
active desktop/session restart occurred.
