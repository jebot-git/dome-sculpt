# Persistent sculpt index working allocation lifetime

The geometry chunk compiler counts the original immutable input owner and all
newly allocated index nodes against the same working-byte limit. A persistent
AVL edit may replace its own unpublished path more than once. Once the final
shared allocation of such a path is actually deallocated, its former storage
is no longer simultaneously owned working memory.

`Work::NodeAllocator` is private to this compiler and supplied exclusively to
synchronous `std::allocate_shared` calls in `PersistentSculptIndex::node`.
Its `allocate` operation runs while the enclosing `Work` is alive. No allocator
is published as a general allocation interface or reused to allocate after the
factory returns. The allocator stored in the shared control block can outlive
`Work`; its former `Work*` is then an opaque pointer value. Deallocation never
dereferences that pointer. It credits only a weak numeric release account,
after the underlying allocation has actually been freed. Expiration of that
account makes later destruction of a published owner a no-op for accounting.

Each operation creates its own release account. Consequently, nodes retained
from an earlier operation cannot credit the current operation's input charge.
Neither equal node values nor equal subtrees authorize credit. Unpublished
nodes still referenced by a temporary, rotation, candidate root, or weak shared
control block remain charged until that allocation is deallocated. Charge
settlement runs before the next reservation and at cancellation checkpoints.

The requested node plus 128-byte allowance is unchanged. The allocator checks
that the implementation's complete shared allocation fits within that
allowance while leaving 64 bytes for allocation overhead and the amortized
numeric account. The account also receives its own prospective charge while
its operation is alive. Unsupported shared-allocation layouts are refused.
Input owners, new surface/tile payloads and dependency sets keep their existing
conservative charges. Lexical scratch keeps its existing scope-based charge.
No quota, work-step limit, data set or cancellation check is increased.

The real surface regression repeats a local edit at its measured allocation
peak, rejects the same candidate with one byte less, and verifies that both
prior immutable results survive a cancelled/denied update. The original full
independent production Arc workload remains the acceptance gate for the
reported sustained-workflow failure; a build alone does not close that gate.
