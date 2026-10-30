# JSON3.12 with allocation-free destruction

`nlohmann/json.hpp` and `json_fwd.hpp` originate from the upstream JSON for Modern
C++ v3.12.0 single_include directory, commit
`55f93686c01528224f448c19128836e7df245f72` (annotated tag object
`65ee68451d8eb2b5f3a30b410476ab83deb3289b`). License: MIT, retained verbatim. The later upstream helper copyright/license
is separately preserved in `LICENSE.MIT.backport`.

Only `json.hpp` is patched. The original destructor's temporary heap vector can
throw `bad_alloc` from a `noexcept` destructor, terminating valid native Scene
serialization or retained-source loading. Dome Sculpt's exact negative cases
are preserved in the preceding Scene12 checkpoint. This copy backports the
upstream allocation-free link-reversal destruction helpers from:

https://github.com/nlohmann/json/blob/d20c4dcd894728cd6a25e8e96533adf85fd17223/include/nlohmann/json.hpp

The backport uses that helper's direct iterative walk. It also treats a typed
null container child as an empty/unowned value:3.12 constructors can leave that
state while initializer-list allocation failure unwinds, whereas the later
upstream construction path avoids it. The same null-owner guard protects container
freeing; real retained-material decode allocation83 is the regression fixture. It deliberately omits the
newer develop branch's optional bounded-recursion fast path and unrelated
parser/error/API changes, so no new `recursion_depth_limit` or `owns_memory`
detail API is required. Object traversal picks the same stored child for link
reversal, removes it only after its value is scalar/empty, and preserves correct
allocator-based key/value/container destruction. No new JSON data members,
namespace/version tags, serialization rules or forward declarations are added.
The16 scalar comparison wrappers additionally receive the upstream conditional
`is_nothrow_constructible` exception specification: allocating string conversion
can propagate `bad_alloc`; arithmetic/null construction remains nonthrowing.
Comparison bodies and ordinary results are unchanged.
The existing primitive/string/binary cleanup remains intact. Full and forward
headers are supplied together by the CMake `dome-sculpt-json` interface target.
Every core, polygon and transitive consumer must be rebuilt coherently.

The exact sequential diffs are retained in
`patches/allocation-free-destruction.patch` (unmodified3.12.0 to lifetime fix) and
`patches/scalar-comparison-exceptions.patch` (lifetime fix to conditional scalar
comparison contracts), both based on the pinned upstream develop source. The full upstream copyright/SPDX
notice remains at the beginning of the header; the helper block identifies its
upstream source commit. `third_party/sources.json` records original and patched
hashes. Do not replace this with unpinned system headers or a different-version
system forward header.

Qualification is recorded separately in the checkpoint manifest. Required gates
include permanent allocation denial for wide/deep/map/ordered/binary/discarded
values, allocator ownership, exact legacy CBOR, coherent native archives, and the
original whole-scene encode and retained-source allocation-boundary reproducers.
A standalone successful destructor probe alone does not complete those gates.

The additional local `patches/binary-factory-exceptions.patch` applies after both
backports. All four `basic_json::binary` factories allocate their payload while
the local result still has its original null tag, committing the binary tag only
after success. Otherwise a failed allocation destroys a fully constructed
binary-tagged result with a null payload and asserts when invariants are enabled.
The exact native Scene14 `PaintedMesh::encode` negative stack is retained. This
local ordering correction changes no successful values, bytes, API or layout;
normal and diagnostic standalone tests force assertions on and inject every
measured factory allocation. It does not claim strong replacement semantics for
all external JSON constructors or universal JSON out-of-memory safety.

`patches/null-promotion-exceptions.patch` applies next. Nine existing member
null-to-array/object promotions (indexing, push, emplacement and update) allocate
the container before committing its new tag. A failed creation preserves null;
later insertion failures may retain valid intermediate containers as before.
This fixes the demonstrated nested MToon extension failure without disabling
invariants or claiming strong transaction semantics for every JSON mutator.
Normal/diagnostic tests force assertions, cover every site plus nested chains,
check valid owners and existing values, and retry after every measured denial.
External constructor replacement paths remain outside this correction.
