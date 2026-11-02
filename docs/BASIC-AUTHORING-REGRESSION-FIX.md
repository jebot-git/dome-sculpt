# Isolated combined-authoring regressions

Product9684486 plus test-onlycc3bf97 in artifacts/basic-authoring-regressions-25fe208 is a child of the
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
preserved; coherent helper build75542 is terminal0. Second full fixture48146 passes138checks then fails only Blue: the retimed
stroke landed on deliberate tracking-loss frame22. Test-onlycc3bf97 moves that
press to frame24 after recovered neutral input, preserving the original dropout
and Blue assertion. Its isolated real-loader paint has48blue texels (>20).
Final full fixture30315 passes1/1 in159.41s, all150 actual-loader/syntheticXR
assertions. Original full39538150 on untouched25fe is terminal8:392passes and
three failures in4541.67s. These split results are not a single395greenrun.
Read validation/BASIC-AUTHORING-REGRESSION-FIX.json for exact hashes/scopes.

The full production goal remains active. Controller spatial drawers stay
required after the basic targets. No new physical test, system installation or
active desktop/session restart occurred.

The final test-only runtime libraries were rebuilt separately in both flags;
all native/core/frontend binaries and inputs remain exactly those from the
qualified5/5 runs. No new whole-OneAPI or physical-headset claim is made here.
The parent retains its exact combined Arc/OneAPI/frontend/format evidence;
this correction changes direct voxel input fencing and fixture assumptions.
Inherited XR/Wayland Mesa lifetime allocations remain separate and unresolved.
