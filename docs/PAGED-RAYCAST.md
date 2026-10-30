# Paged field picking

This change accelerates the existing trilinear field picker. It does not change
its positive tolerance (`min(0.12 * voxel_size, 0.1 * physical_band)`), cubic
critical-point/root solver, first-hit policy, or 20,000 candidate-cell budget.
It does not depend on a surface mesh or on signed-distance step estimates.

## Index and ownership

`src/ray-index.cpp` caches an immutable, path-copied 96-bit Patricia tree of
occupied 16³ sample pages. Each leaf records the exact minimum stored value;
branch nodes record minimum and conservative page bounds. A page's incident
cells occupy the closed lattice extent `[page * 16 - 1, page * 16 + 16]` on each
axis. Outward-rounded bounds include this halo before ray-box intersection.
Implicit samples equal the physical band, which is strictly above the picking
tolerance. Therefore only bounds whose complete stored minimum exceeds that
tolerance can be discarded. Strictly positive low samples remain candidates.

The sorted union of intersected page intervals feeds the original cell solver.
All retained intervals share the original 20,000-cell budget. Distant empty
space consumes no cell budget, while dense nonempty candidate geometry can
still exhaust it. The implementation gathers bounded intervals before solving;
it is not a front-to-back lazy hierarchy traversal.

Keys compare both pointer and shared ownership identity of the minimal immutable
`Volume::GeometryOwner`, plus voxel size and physical band. They retain no
mask, displacement layer, editable history, authoring settings, or full Volume.
`GeometryChange` authenticates the ancestor owner and supplies at most 128
single-page dirty groups across at most 64 events. Only these pages are reduced
and tree paths copied. Missing/expired ancestry or unknown replacement rebuilds
from page summaries. Removals delete leaves. Mask/control-only changes reuse the
same key; retained old owners remain valid. The added CowMap read methods do not
flatten storage, mutate pages, add fields, or assume cached mutable minima.

## Bounds, failure and cancellation

Index entries, nodes and reusable per-thread interval vectors share a tracked
64 MiB allocator, including a conservative allocation/control-block allowance.
The LRU holds at most 256 entries and conservatively accounts at most 128 MiB of
retained SDF roots; shared pages may be counted more than once. Cache bookkeeping
is separately bounded (256 weak refusal markers and entry pointers, plus at most
128 temporary dirty-page set nodes). Query
stacks and the ancestor-candidate array have fixed sizes. Cold construction is
limited to 262,144 pages, spatial traversal to 262,144 nodes, and interval
collection to 32,768 leaf intervals before merging. Cancellation is checked at
pages, ancestor searches, bounded traversal batches and cell batches.

Acceleration-only quota exhaustion falls back to the original bounded cell
traversal; it does not make an otherwise eligible scene unpickable. Weak-owner
negative entries prevent another failed cold build on that unchanged root.
Ray-specific workspace/traversal limits do not poison the owner index: later
short rays can continue to use it. O(64)
conservative root-byte/page-count proofs also avoid a full scan after small
edits when the complete source demonstrably cannot fit the index. A small edit
to a source that failed for uncertain transient resource pressure may need
another bounded build; no stronger negative-ancestry claim is made. A fallback
still has the original 20,000-cell limit and can consequently refuse a far fine
ray that a fitting index could accelerate.

Actual `bad_alloc` and cancellation propagate rather than being disguised as
quota fallback or a miss. Construction publishes only complete entries; failed
local updates leave the prior immutable index usable. A real interval allocation
failure also propagates and the next query can recover. A global cache mutex
protects publication/LRU only; cancellation callbacks run outside that mutex.
Concurrent immutable queries use separate per-thread interval buffers. Recursive
picking from a cancellation callback is explicitly refused before it can
invalidate an outer query's buffer. Callbacks may inspect state and statistics.

The existing three-argument `Volume::raycast` symbol remains. The additive
four-argument overload accepts `const std::function<bool()> &cancelled`.
When integrating with the newer native desktop stroke worker, its
`native_loop` call to `Volume::raycast(origin, direction, maximum)` should pass
its session cancellation predicate, including stale replacement and shutdown.
This private checkpoint is based on synchronous production `2135d8d`; it does
not edit or prove that newer worker route.

## Validation scope

The `paged-ray` fixture has 34 checks. Its reference is the original `2135d8d`
whole-cell solver copied into a separate test translation unit, with only its
test traversal allowance raised to two million cells. Analytic binary-valued
fixtures independently require the earliest of two roots separated by 0.015625
voxel, or an exact tangency, after a five-metre diagonal approach. Their bound
is 0.001 voxel, independent of camera-distance ULPs. A separate translated
feature-order assertion excludes a later candidate even where float world
coordinates have coarse ULPs. Further cases cover 10 µm/0.1 mm/coarse spacing,
negative/page-boundary coordinates, positive-only slabs, physical bands,
randomized oblique trilinear fields, zero directions, disjoint supports,
concurrent/warm queries, 96 actual gesture stamps, complete clearing and
repopulation, layers, undo/redo, skipped and expired lineage, cold/local
allocation failures, cancellation, and quota fallback. The measured cold and
local fault sequences each contain 16 allocation boundaries. Fifty warmed
queries over 8,192 occupied pages allocate nothing and do not reduce pages.
The existing sculpt-raycast suite also passes its 18 analytic/legacy checks.

Release tests rebuild the changed Volume/ray modules and tests, linking them
before the frozen `2135d8d` Release support archives. Link maps verify that the
old Volume/raycast archive members are not extracted. Sanitizer validation
rebuilds those same changed modules/tests with ASan/UBSan and links matching
frozen `2135d8d` whole-core-instrumented C/C++ support, with leak detection and
halt-on-error. This is focused 34 + 18 fixture coverage with reused instrumented
support, not a fresh full-suite/full-core build claim.

On an i7-12700 with GCC 16.2.1, `-O3 -DNDEBUG`, three complete 96 pick-and-stamp
trials at 0.1 mm spacing and 0.5 m camera distance measured reference
120.837/120.013/120.571 ms versus indexed 63.6981/63.2965/63.339 ms. Authoritative
field and layer samples match exactly; at most eight pages were reduced per
stamp in the independent oracle run. One separate 5 m / 10 µm comparison was
67.0309 ms for the lifted-budget test oracle versus 0.047765 ms indexed cold.
The unmodified production reference would reach its 20,000-cell limit on that
far case, so that latter number is not a successful original-production timing.
Measurements include local CPU work; no headset or frame-pacing claim is made.

The native FLTK resolution GUI uses a genuine dense low-corner corridor to
exercise real candidate-cell exhaustion. It clicks the canvas centre, excluding
the status row and hidden universal menu, rather than the window centre. A
Linux test-only `dome-sculpt-status-observer` module forwards the real FLTK
`copy_label` call and acknowledges the exact bounded-error substring (allowing a
worker prefix). The script awaits that status publication, changed status
pixels, no modal, exact source, and working Save. This closes an inherited
indirect fixture that previously checked only no modal/source equality after
an off-centre ray. Build the observer target before running the GUI script;
there is no product hook or installed module.
