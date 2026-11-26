# Independent production sculpt: native workflow and sustained Arc milestone

Production source `8c3b9aea` fixes a genuine persistent-index working-budget
failure on the `6d3ca25e` sculpt component. It credits only newly allocated
shared index storage which has actually been deallocated. The original source
owner, live candidate paths, payloads and scratch remain charged. See the
[allocation contract](SCULPT-INDEX-ALLOCATION-LIFETIME.md).

The original independent production Arc workload passed all three repetitions:
900 sculpt strokes, 360 mask edits, 6,000 stereo view pairs, 360 held pairs,
42 independent geometry checkpoints, and three complete cancel/Undo/Redo,
ordered Save/reopen and real stale-generation barrier checks. The 7,690 raw
rows and full reports are retained. Runtime was 812.333 seconds; measured owned
RSS peaked at 4,787,830,784 bytes. Limits remain 5,400 seconds, 5 GiB owned RSS
and 2 GiB output. Actual output was 173,307,362 bytes. Initial and final device
witnesses authenticate Intel Arc A770 through `/dev/dri/renderD129`; all 74
provider inputs remained unchanged. The actual lease released naturally with
no owned survivors.

The failing predecessor is preserved separately: source `6d3ca25e` with the
corrected original mask fixture `87f6b41f` exited 1 after 314.438 seconds. Its
last completed row was checkpoint 12/sequence 2561, followed by a genuine
representative-index reservation refusal at the unchanged 646,373,634-byte
operation cap. Both runs use byte-identical fixed stroke/mask/view/held inputs,
SHA-256 `85e82de7824ea1d90ff79938d46a4e8dbfc2e3c81602b6cdc4d5dc428aa4419d`.
The accepted native project from the failing predecessor is also byte-identical
to all three corrected repetition projects (50,692,148 bytes each,
SHA-256 `db9c506daedc1b13992838200b95e9e4569e98155bce306142803aca6cb8edb8`).

Release also passes all 167 full native independent workflow checks in
64.180 seconds at 2,314,473,472 bytes owned RSS, within the original 900-second /
4 GiB bounds. Its accepted and durable recovery files retain the exact native
bytes of the earlier qualified workflow. Release and ASan/UBSan both pass the
26 real surface regressions and 92 Document pipeline checks, including the
measured local working peak, refusal with one byte less, cancellation and
continued validity of both prior immutable owners.

The full ASan/UBSan 167-check workflow is **not accepted**: its unchanged 4 GiB
RSS guard stopped it after 696.589 seconds at 4,307,349,504 bytes, after check
164 and before the final quota-refusal/cancellation endpoint. Leak detection,
ASan/UBSan halt behavior and all limits remained enabled. No sanitizer,
source/provider drift, DRM access or survivor diagnostic occurred. Earlier
strict failures and default-allocator Release memory refusals are preserved;
this milestone makes no universal default-allocator 4 GiB claim. A separate
compact allocator candidate is being qualified, without altering this source
or these results.

Both builds admit three complete changed core translation units and 194
qualified existing owners, verify all 197 actual archive payloads and four ELF
links, and retain actual dependency files and source bytes. This is a sculpt
component result on the stated source, rather than a whole-current-combined
application qualification. The GPU renders real offscreen stereo draws;
brushes and meshing remain the explicit CPU baseline. Physical headset pacing
and the remaining authoring goals are still open.

The [machine-readable record](validation/sculpt-index-lifetimes-20261126.json)
indexes the [evidence archive](validation/sculpt-index-lifetimes-20261126.tar.xz):
1,755 members, 10,726,152 bytes, SHA-256
`b649ca2be7dd656d1d1bce902594abd08619cdd6414df9c88e9cc98e83226b62`.
Each archived payload and its actual local authority was independently hashed
after archive creation. Full real native projects are included once per unique
payload; every actual native path remains listed in the record.
