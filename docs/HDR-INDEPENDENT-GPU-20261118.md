# Independent HDR source GPU sampling — 2026-11-18

Production `c52418bf`, exercised by fixture-only child `2122416b`, implements
independent retained-source sampling in the native PBR and MToon renderers.
This component passes software OpenGL; Arc, combined application/public UI and
physical headset qualification remain pending. It does not complete the goal.

The source's immutable seven-FLOAT records upload directly to a GPU buffer.
A GPU extraction pass produces an RGBA32F texture at the original64 resolution.
Current editable paint retains512 and compiles separately; the source background
is never resampled into those emission planes. Final PBR uses explicit FLOAT
bilinear sampling. MToon uses an eighth view in its existing texture-buffer pack,
keeping the sixteen-unit sampler contract. Both compose current paint afterward
and preserve resolved physical source emission when current material factors or
MToon strength change. Existing resource accounting includes the extra texture,
source upload, pack storage and temporary working resources.

An actual native Bake supplies the test's factory-owned source. Fixture-only
readback proves every source FLOAT coefficient survives GPU extraction exactly.
Independent sparse-layer decoding supplies the pixel reference, without the
production field sampler, projection or emission resolver. All four cases pass:
PBR and MToon, before and after painting a separate512 physical layer. PBR RGB
error is1 and MToon0 under bound2;4422 checks pass in6.7s on actual llvmpipe.
The same production separately passes the existing172040 FLOAT GPU checks and
81 complete MToon/linear-target checks. Their prior wrapper remains failed:
its later new fixture incorrectly authored an HDR value as a bounded MToon
factor. The corrected fixture uses normalized factor plus strength. No whole
failed run is relabeled as passed.

The first fixture compile used a nonexistent native roughness field. Its next
runtime lacked required genuine tangent corners. Both failures remain preserved;
the fixture now obtains actual Mikk corners and checks unambiguous quad transport.
No source or fidelity threshold was relaxed. GPU product sources are unchanged
by the final fixture-only correction.

Four renderer TUs were initially compiled completely; the manual FLOAT sampling
change recompiles render.cpp completely. Three unchanged renderer owners retain
their actual MD/object identities. Corrected fixture TUs and links are fresh.
All196 reused core archive members are independently checked against their
qualified payload hashes; they are not called newly compiled. Plans, actual
MDs, commands, CWDs, old failures and runtime receipts are in the scoped archive.
All runtime units have terminal/journal capture before stopping, empty drift and
survivor lists, and actual nonblocking lease release. No install or session
restart occurred.

See [machine receipt](../validation/hdr-independent-gpu-20261118/receipt.json).
