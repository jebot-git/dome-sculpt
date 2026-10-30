# Valid JSON container promotion on allocation refusal

After the binary factory correction, the exhaustive assertion-enabled native
Scene14 allocation probe reached a second failure. Retained material decode
constructs a default MToon material and its nested extension object. Nonconst
JSON indexing changed a completed null child to object before allocating its
map. Allocation refusal left a typed-null child; destruction of its parent map
then asserted. The exact GDB source chain and the 106.21s negative run remain
retained separately from the earlier binary factory failure.

This local patch reorders the existing payload/tag assignments in nine member
null promotions: numeric/string/transparent-key indexing, const/move/object-pair
push, array/object emplacement and object update. Allocation must complete
before the new container type is published. Successful values, API, layout and
bytes are unchanged. The sequential pinned-header patch reproduces the complete
header byte-for-byte and its hashes are recorded with the other JSON patches.

Both normal and JSON_DIAGNOSTICS standalone tests force assertions even in
optimized Release. All 42 real permanent allocation boundaries per mode pass in
Release2/2 and ASan/UBSan/leaks2/2. They exercise all nine sites and actual nested
object/array chains, require each bad_alloc to occur and propagate, check valid
heap owners, exact existing values, serialization and deterministic retry. The
exact same test against the preceding header aborts with SIGABRT in every one
of the 11 diagnostic cases. The default qualification runs all cases.

This is the basic valid-state guarantee for container promotion. Later insertion
can leave valid intermediate containers; these tests do not invent a blanket
strong transaction guarantee for mutable JSON. External constructor replacement
is unchanged. Complete Scene/core and frontend qualification after this header
change is a separate gate, not implied by these standalone tests.
