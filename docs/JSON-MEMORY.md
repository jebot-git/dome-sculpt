# Reliable native JSON ownership and destruction

The previous Scene12 checkpoint exposed two actual process-terminating allocation
failures: the old system JSON3.12 destructor allocated a temporary vector while
cleaning initializer-list values. Its `noexcept` context aborted ordinary Scene
encoding and retained source-material decoding instead of propagating failure.
Those original probes/stacks remain in the frozen Scene12 worktree.

This change supplies matching JSON3.12.0 full and forward single headers through
one transitive CMake interface target. It backports upstream allocation-free
link-reversal destruction from the exact commit recorded in
`third_party/nlohmann/README.md`. JSON namespace/version tags, object layout,
serialization rules and public API remain3.12; unrelated develop-branch parser
changes are not imported. Every consumer must be rebuilt coherently.

The backport uses the iterative walk directly. It borrows already-owned child
slots as parent links while destroying the tree, so no heap stack or recursion
per JSON nesting level is needed. Keys, strings, binary values and containers
still use their matching original allocator. A small compatibility adaptation
recognizes null container children during3.12 constructor-failure cleanup. The
first plain upstream backport exposed that old-constructor state at real retained
material decode allocation83; its failure and null-pointer stack are retained,
and the complete boundary probe now covers its recovery.

Both the original3.12 and later helper MIT notices are retained. The exact local
diff is checked by applying it to the pinned original and comparing the result
byte-for-byte. Source and patched hashes are in `third_party/sources.json`.

## Evidence

The preceding6ce11bc native CPU Release checkpoint passes146/146 in165.45s
with UI and OpenXR disabled; those historical results remain frozen separately.
Within them, eight whole-scene failure fixtures inject every measured allocation,
check unchanged live source/input, and retry:

| Native path | Injected boundaries |
| --- | ---: |
| Plain retained-DAG decode |914 |
| Plain retained-DAG encode |1784 |
| Source-map/material decode |1108 |
| Source-map/material encode |2450 |
| Authored-bind/PNG/PNT-morph original GLB decode |2937 |
| Authored-bind/PNG/PNT-morph original GLB encode |4034 |
| Typed MToon material decode |2975 |
| Typed MToon material encode |4304 |

The total is20506 boundaries. The original `--encode` and `--source` process
abort reproducers are passing gates now; none of their failing allocation sites
was suppressed. The final qualification commands explicitly unset the optional
diagnostic boundary-selection environment variable.

Normal and `JSON_DIAGNOSTICS` lifetime suites each pass14 assertions, including
permanent allocation denial while destroying100000 alternating array/map levels,
100000 ordered-map/array levels, wide maps, strings, binary subtype, discarded and
primitive values. A counting allocator verifies all heap-backed object keys and
value/container blocks are freed exactly once. Independent fixed CBOR bytes and
UTF-8/ordered-object expectations are checked.

Separately compiled producers and consumers against the frozen old/system core
and the new vendored core produce identical Scene5, authored-source Scene11 and
retained-source Scene12 files. Own restart and both cross-reader/save directions
also produce the same bytes. The files are586,18333 and11965 bytes respectively;
the report records their exact hashes. This tests actual native CBOR and source
archives, including authored inverse binds and immutable original GLB data.

Fresh whole C/C++ core and bundled Mikk/xatlas/ufbx ASan/UBSan/leak
qualification passes20/20 in110.31s, including the same20506 injected boundaries.
Exact source/binary/log hashes, compiler flags and include-dependency audit are
in the final private checkpoint manifest. System libraries are not instrumented. No new GPU, GUI, compositor or physical
headset result is implied by these CPU archive/lifetime checks.

The full production objective remains active. This fixes the demonstrated JSON
ownership/destruction failure paths; it does not make every JSON operation
allocation-free or complete unrelated import, polygon, colored-bake or rendering
requirements. The approved PolygonAuthored paint policy is separate work and is
not mixed into this dependency qualification.

The fault phases above target native Scene encoding and decoding after source
preparation. Direct `prepare_authoring_import` allocation faults and generic scalar comparison
contracts on the newer flat-normal-enabled base are qualified separately in
IMPORT-MEMORY.md. This preceding destructor-only checkpoint did not claim those
then-remaining comparison overloads were safe. The earlier repository format/policy checks already
use nonallocating checked string references, and the original failure evidence
remains unchanged in the preceding frozen worktree.
