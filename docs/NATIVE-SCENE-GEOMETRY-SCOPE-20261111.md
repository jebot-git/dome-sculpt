# Scoped native geometry sharing: independent qualification

Product `9c521366` passes matched native controls and a fresh Document reopen of
the actual production four-object file in Release and strict SAN. Both decoded
and published geometry have exactly two owners: edited object1 remains distinct,
and unchanged objects2–4 share genuine COW storage. Both a held decoded Scene and
the current Document encode equal the entire independently decoded native CBOR
input, including source, layers, masks, materials and placement. All four actual
objects publish with current generation/revision/view and no error.

| Fresh native consumer | Controls | Release seconds | SAN seconds |
|---|---:|---:|---:|
| Chunk pipeline, including native read/share |92|0.56|4.34|
| Local surface |77|0.64|6.19|
| Surface chunks |22|0.11|0.70|
| Small chunk workspace |27|0.24|2.41|
| Small placed workspace |373|0.31|3.43|
| Persisted scope-lifetime test |17|0.019|0.166|
| Production reopen and entire native wire equality |—|6.51|67.78|

The new test-only child `af10a07d` adds
`tests/native-scene-geometry-intern.cpp` and target
`dome-sculpt-native-scene-geometry-intern-test`. It uses actual Scene/Volume reads
to check independent file scopes, a malformed second object after valid Volume
decoding, standalone reads after failure, nested normal/exception restoration and
simultaneous thread isolation. It introduces no product observer. The existing
pipeline checks decoded layer/mask independence, real edit divergence, exact
whole-mesher/affine corners, and actual Document Cancel/Apply/Save/Undo/Redo.

Source review confirms that a sample-count/context hash only filters candidates;
complete coordinate and float-bit equality precedes sharing. Voxel/band bits are
part of the context. Only fully validated evaluated `cells_` storage is joined;
layers, masks, settings and edit history remain separate. The stacked thread-local
scope is published only after reserve succeeds, restores the previous pointer
on destruction and cannot be copied. Reserve-failure safety is source-reviewed,
not an injected allocation-failure test. The optional8m-visit/256-entry bounds and
all existing native validation remain; exhaustion cannot fabricate an alias.

All public and existing private headers are byte-exact with b6. Two complete new
core TUs, Scene and Volume, replace their objects in the audited b6 thin core:
189 exact f473 owning objects, one previously qualified b6 sculpt-scene-chunks
object, and those two new objects. Six existing test TUs, the persisted scope TU
and the production probe TU are also compiled whole and freshly linked. This is
focused current-TextureMaps-ABI composition, not a whole fresh core/application
build. No 775/27f core or executable supplies pass evidence. Every source/provider/
fresh-output drift list is empty. Native pipeline input and accepted Save files
are byte-exact across profiles; accepted SHA-256 remains
`627cf8b51a7167c1209d69bb2b94c039f557625f33beb178831738798352cd0c`.

The original production reopen failure is preserved: b6 loaded four independent
geometry owners, failed `Sculpt manifest retained byte limit exceeded`, and ended
after122.98 seconds with no external guard. The original4-object workload passing
did not qualify this separate file-open path. The unused diagnostic attempt0,
whose temporary-Scene range lifetime was caught before execution, also remains.

An earlier e3 recipe was rejected before any job: its friend-only public Volume
class token change would violate ODR when borrowing b6 objects, despite unchanged
layout. No e3 compiler/link/native result is claimed. The new9c source restores
byte-identical public headers and replaces both owning TUs instead.

One further failure remains visible. The first9c Release run passed all six small
suites and published the production scene with two owners, then the added verifier
incorrectly parsed the CBOR native file as text JSON. Its parse_error101/byte0xac
and a subsequent UTF8 log-decoding error leave the original coordinator status1.
That status, source, ELF and raw log remain unchanged. A separate fresh verifier
TU/link corrects only the input format to `json::from_cbor` and passes0 against the
same core and complete equality oracle. The Release proof therefore references
both raw scopes explicitly; it does not rewrite the first status as successful.
SAN's complete corrected run is terminal0.

The actual Document publication wait remains120 seconds and occurs before the
expensive additional encoding checks. Product geometry quotas remain512MiB
retained/1GiB working. Distinct host instrumentation envelopes are4GiB for Release
and small controls, and a preapproved8GiB for the full49MB SAN diagnostic. Measured
full-probe peaks are2,172,915,712 and6,026,510,336 bytes respectively; neither guard
fires. These timings include verification and do not qualify loading latency or
performance. Strict ASan leak detection1 and UBSan, no suppressions, report no
error. No DRI/GPU context or lease is used, and no job remains live.

The companion JSON pins exact sources, headers, archives/members, commands,
providers, native maps/logs, the full input and all failures under
`artifacts/native-volume-scope-qualification-20261111`. Product and root/main are
untouched by this test/evidence branch. Responsive read cancellation, the coherent
final owning application, actual desktop/XR callers, installation and the full
production objective remain separate required work.
