# Scalar comparison and direct import memory qualification

This continuation uses the exact flat-normal-enabled ec537a84 source. It adopts
only the pinned JSON dependency, CMake interface and standalone lifetime tests
from the earlier6ce11bc checkpoint. No PolygonAuthority/Scene12 or PolygonAuthored
policy implementation is introduced here.

## Scalar comparison contracts

JSON3.12's scalar comparison wrappers were unconditionally `noexcept`, but a
C-string operand constructs an allocating temporary JSON value. The frozen
lifetime-only6ce11bc reproducer still terminates with SIGABRT under permanent
`operator new` denial, even with a surrounding `bad_alloc` catch.

All16 existing scalar wrappers now use the exact upstream conditional
`is_nothrow_constructible` exception contracts from the pinned develop source.
Friend declarations use the dependent operand type to defer traits until the
JSON type is complete. Comparison bodies and ordinary values are unchanged;
allocating string conversion propagates failure, while arithmetic/bool/null
construction keeps its existing nonthrowing guarantee. This is a library fix,
not another list of special-case repository string checks.

The additional patch is separate from the allocation-free destruction patch;
applying both to the pinned original produces the shipped header exactly. Full
and forward headers retain the same3.12 namespace/version/layout and are
provided together to every consumer. The exception contract correction is
intentional and requires rebuilding all consumers.

C++20, C++17 and legacy-discarded-comparison executables check both operand
orders for equality/ordering under permanent denial. Explicit C++20 spaceship
operators are tested too. They pass64 assertions, catching46 allocation failures;
numeric, boolean and null comparisons make no allocation attempts. The operand
values remain unchanged. The original failing executable/log remains preserved.

## Whole authoring import

The test calls the real `prepare_authoring_import` entry point, not an archive
helper or publication-only substitute. It starts with an existing painted mesh,
nonempty hierarchy and immutable COW geometry owner. Each measured allocation
fails persistently through unwinding. Every failure must preserve the complete
original Scene, painted pixels, pointer/revision/geometry owner and file bytes;
an unarmed retry must equal the complete expected imported Scene.

| Source | Persistent allocation boundaries | Cancellation callbacks |
| --- | ---: | ---: |
| Creased GLB without NORMAL, genuinely generated flat corners |2009 |57 |
| External textured glTF JSON +binary resource, decoded PNG and raw receipt |2171 |58 |
| Authored inverse-bind skinned GLB with PNG and P/N/T morphs |2728 |83 |

That is6908 allocation sites and198 cancellation sites. The external case also
checks the original JSON and referenced buffer independently, including their
exact immutable receipt data. The flat case proves the source has no authored
NORMAL and the native derivative has real expanded flat corners; it uses the
current implementation, not the former blanket refusal. The skinned case proves
actual skin, morph and explicit inverse-bind state are present.

All six focused native suites pass. Full CPU native Release passes139/139
in148.88s. Fresh whole C/C++ core and bundled Mikk/xatlas/ufbx ASan/UBSan/leak
qualification passes20/20 in93.25s, including the same complete allocation/
cancellation/comparison cases. The final manifest records exact source, binary
and log hashes, flags and the matching-header audit.
These injected failures are C++ allocation failures; no positive claim is made
about uninstrumented system-library malloc failures or unrelated GPU work.

The full goal remains active. General affine import, broader native-format skin/
morph authoring, the separate polygon policy/context workflow and other remaining
features are not completed by this memory/error-handling milestone.

## Separate descriptor-ownership followup

The subsequent OS-resource audit found a real failure outside ASan's heap scope:
`make_unique<State>(dup(directory.value))` evaluates `dup` before allocating its
owner. Failing that allocation leaks a directory descriptor. The actual wrapper
probe at allocation43 observed4→5 open descriptors and exited42, while ordinary
heap/source tests still passed. That negative evidence is retained. The original 1651b37 checkpoint did not claim descriptor OOM ownership was
complete. The separately qualified fix and real descriptor/fallback regression
are recorded in [IMPORT-FD-OWNERSHIP.md](IMPORT-FD-OWNERSHIP.md).
