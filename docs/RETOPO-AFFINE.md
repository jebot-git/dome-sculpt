# Affine manual retopology — private component checkpoint

The enabled Retopology entry previously refused every exact affine source in
`Scene::world_transform`. This checkpoint uses the evaluated full affine source
in a world-metre workspace. Positions, inverse-transpose normals, reflected
winding, desktop brush/ray/normal queries and shared controller coordinates use
that same frame. Existing similarity-only workspaces retain their old path.
An additive `RetopoMesh::transform` method preserves stable IDs, allocation cursor,
pins, seams and materials atomically, reversing face loops for reflections.
There are no new object fields, native formats or persistent authority shortcuts.

Whole changed `retopo.cpp`, `retopo-authoring.cpp` and `document.cpp` compile and
link before immutable qualified main10944/c179 support. This is a matching
component proof, not a fresh application build. No library archive is rewritten.
Release and ASAN/UBSAN each pass394 new checks over rigid, nonuniform, sheared and
reflected native sources, both rest and independently calculated skin/morph
states. Genuine paint, viseme metadata, pose and placement remain on the hidden
source. Projection/ray/normal bounds, topology conversion cancellation, actual
native gestures, draft/global history, source invalidation, Apply, Save and
reopen are checked. Existing authoring62 and bridge/extrusion131 checks also pass
per profile:587 total each. Eight corresponding native files are byte-identical.
The new CMake target is `dome-sculpt-retopo-affine-test` / `retopology-affine`;
configuration/full-suite/frontend execution is not claimed.

The original current-main entry refusal is preserved. First new Release refused
a real zero-paint fixture whose inherited brush normal faced away; correcting
only that fixture kept its genuine paint assertion. The next run exposed a real
product gap: immediate `save_async` after accepted retopo input can precede the
input worker's AssetWork publication and still refuses. The positive Save test
uses an explicit test-only gate after **real unwrap** and before native scene
publication, proving accepted AssetWork ordering only. Immediate acceptance-to-Save
is still open and must be fixed independently. No product sleep or guard waiver
was added.

To stay below0.75GiB, sanitizer links the three complete test TUs into one core
consumer with named entry functions. Its first invocation passed394 new checks
and reached the legacy author's final assertions, then correctly diagnosed that
renaming `main` removed C++'s implicit `return0`. The failed exact executable and
source/wrapper provenance are losslessly archived with SHA/mode/mtime and restore
ledger. Adding explicit `return0` to that whole test TU changes no assertions;
the corrected composed invocation then passes all587 with no sanitizer report.
The failed invocation did not reach bridge cases and is never counted as passing.

Exact commands/hashes/failures are in `validation/RETOPO-AFFINE.json` and ignored
`artifacts/affine-proof`. Source fixtures here are native matrices/skin/morphs;
actual external avatar metadata graphs, additional production/fault fixtures and
fresh matching client/UI proof remain separate requirements. The full production
goal, adaptive remesh, CATS-style preparation and texture-set work remain open.
