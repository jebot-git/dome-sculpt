# Exception-safe JSON binary factories

Assertion-enabled exhaustive Scene14 serialization exposed an independent
JSON3.12 defect. `PaintedMesh::encode` calls `json::binary`; the factory creates
an already-constructed null result, changes its tag to binary, then allocates
its value. If allocation fails, the result destructor asserts because the binary
payload is null. The retained GDB trace reaches the const-container factory from
`Scene::encode`. This differs from destruction of a partially constructed JSON
object and from the earlier allocation-free destructor backport.

The local patch changes all four factory overloads to assign the payload while
the result is still null, then commit its binary tag. API, layout, successful
values, subtype and serialization stay unchanged. The sequential patch applies
after the two earlier pinned JSON patches and reproduces the complete vendored
header byte-for-byte. Source, patch and license hashes remain recorded in
`third_party/sources.json`.

The standalone test links only the JSON interface and forces assertions on even
in optimized Release. Both normal and JSON_DIAGNOSTICS modes inject all six
measured permanent allocation denials across const/move and subtype/non-subtype
factories. Each failure must actually occur and propagate bad_alloc; const
inputs stay exact, every retry succeeds, and exact bytes, subtype and independent
CBOR expectations remain checked. Both Release2/2 and ASan/UBSan/leaks2/2 pass.
The same unmodified-header negative probe aborts with SIGABRT at its first
factory refusal. No assertion or allocation boundary is suppressed.

The preceding Scene14 sanitizer run passed15/16; this binary factory refusal
was its one failure. That source/stack/negative receipt remains retained. Fresh
whole-core/support C/C++ and Scene14 regression qualification after this fix is
recorded separately; this standalone dependency test does not qualify the whole
application or claim every JSON operation has strong allocation-failure
semantics. In particular, external_constructor replacement paths are a separate
contract and are not changed by this four-factory correction.
