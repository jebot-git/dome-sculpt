# Source8 parameter ownership preservation — 2026-11-12

Product repair `3f9c03c8`, strengthened native fixture
`87095a5b492f68774f9019ef6ed926af97b11f5b`, extends the genuine Source8 visible
component `f67825ce` and strong negative-control fixture `a5cf47a2`. This is a
scoped component result. Combined application, public desktop/XR, hardware,
physical-headset and performance qualification remain separate requirements.

## Defect and ownership repair

`SourcePaintAccess::parameters` previously prepared a new original/current
sampling proof and copied sparse layers, but omitted the accepted `baked_`
owner. Source8 newly made such a current bake valid, exposing silent loss of
RGB8 colour/normal and immutable FLOAT coefficients during source parameter
edits. The same new regression linked against the old F678 core exits 1 at the
exact baked-owner preservation assertion. Its raw log is retained.

The repaired factory authenticates the existing source transport and forecasts
coexisting current geometry, coordinates, raster and layer slots before deriving
replacement owners. It prepares a new before/current trace with the requested
original role affines, then pairs that fresh unpublished proof with the exact
existing immutable baked shared owner. It does not merge or copy pixels and does
not move current canonical normals into the original role's UV frame. Sparse
paint and hidden physical layer owners remain shared and exact. Canonical normal,
component colour, native-domain and prepared-frame restrictions remain enforced.
Final cumulative storage and cancellation checks precede publication.

Only `src/paint.cpp` changes product code. All 103 public headers, private
headers, shaders, packet ABI, Scene42/Paint8 wire and capabilities are unchanged.
The earlier exact291 HDR overflow helpers remain intact; the later root full-FLOAT
target integration is not qualified by this moderate-radiance component fixture.

## Actual native evidence

Both optimized Release and strict ASan/UBSan runs pass all five scopes with zero
input/source drift: 424 actual visible Source8 controls, 180373 Source8 worker
checks, 27 source controls, 16417 original-worker checks and 32794 public-fixture
producer checks. Software GL uses actual production SurfaceRenderer on llvmpipe;
this producer is not execution of the public desktop/XR applications. Sanitizer
runs enable leak detection, halt on error and use no suppressions or preload.

The eight visible cases cover PBR/MToon1, opaque/blend and quarter/sub-UNORM
coverage, with varied per-corner geometry/normal and independent skewed UV1 versus
current inset UV0. Each proves:

- Exact baked RGB8 normal/colour/FLOAT owner, current mesh/UV0, original UV0/UV1
  trace, original images and hidden editable physical tile retention.
- All 20 real factory cancellation boundaries refuse atomically. A measured
  924408-byte prospective budget succeeds and 924407 bytes refuses without
  changing the source/current input. These numbers describe this fixture only.
- Actual Document parameter worker, immediately ordered Save, reopen, Undo,
  Redo, Clear and Clear Undo preserve exact native scenes and rendered images.
- Fully computed late publication cancellation rejects the dependent Save and
  retains the prior file, source/current/baked owner and Undo/Redo states. An
  actual Save retry reopens exactly.
- Rotating only the original PBR normal affine by 90 degrees changes the real
  image by 19/23/14/17 RGB levels across the four PBR cases. A separately derived
  point oracle changes only the original derivative basis, retaining the current
  Mikk contribution. The typed cases have no original typed normal role and
  correctly remain identical. All 32 additional oracle points have maxRGB error
  at most 1, within the unchanged bound 3.

Original and rotated no-baked-detail controls explicitly fail the same bound 3
and have a gap of at least 7 at one or more points in every case. The strongest
original gaps are 82/89/16/17/64/69/12/13; the rotated gaps are
72/78/16/17/56/60/12/13. Correct-oracle maxRGB error is at most 1.

## Owning compilation and preserved evidence

`artifacts/source8-visible-preview-proof-20261112/sanitize-v1` independently built
the complete affected SAN closure from authenticated immutable5bb/current4977
source bodies: 27 full core TUs, all four renderer TUs and two fixture TUs; all
166 reused core members and public headers were checked. The parameter repair
replaces one whole paint TU in that qualified cohort and freshly links consumers;
192 other core members remain byte exact. Release uses the corresponding owned
F678 affected cohort, one whole paint TU and fresh consumer links. The final
stronger fixture adds one freshly compiled test TU per profile. No provider was
modified and no private-header-dependent stale object was reused.

Final receipt map:
`artifacts/source8-parameter-proof-20261112/handoff-final.json`
(SHA256 `aca99fa846424efdd644f4e7552070e31645d6fe3117acec573ab7eb09b41098`).
Build/native statuses are `release-v1`, `native-release-v1`, `sanitize-v1`,
`native-sanitize-v1`, `release-v3`, `native-release-v3`, `sanitize-v3` and
`native-sanitize-v3` under that directory. All are terminal 0 with zero drift.

Owned core archive SHA256:

- Release: `d2121b9b49f9e8549a1e3c30248bdb37ea2be84ec496bdb590c756dfff6e585a`
- SAN: `91d84fcbe0166fe7aa885a015ff668bf0ff235d949f8a789fb89b589ca57423b`

All older component sources and failures remain unchanged, including frozen
4977/5bb/63/3e/F678/A5, the original raw UV1 failures and all visible-component
failures. The test-only964 source had a missing `<fstream>` compile error;
`release-v2` preserves it, and the immutable870 child adds that include. The
old-core preservation failure remains in both native-v1/v3 receipt scopes. There
are no remaining live compiler/runtime handles owned by this component milestone.
