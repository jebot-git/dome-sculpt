# Retained writable paint material9 — private codec stage

The shared material codec now owns colour-entry4 and normal-entry4 through a
separate PaintSamplingArchive context, preserving all previous overloads. Without
new reachable sampling authority it emits the same older version/bytes even
when an unused context is supplied. Material9 cannot be decoded through an old
context overload or merely by copying a version tag.

Colour replay inherits the exact original projection, receipt, original source
and old UV recipe from its explicit earlier binding. A private factory then
proves the current topology/corner field and pooled sampling basis against the
complete before-state. It attaches that exact structural pool owner after real
replay; equal geometry/counts cannot fabricate source ownership. Normal-entry4
independently owns its metadata_sampling_domain and receipt2. Actual shared
beforePaint/binding aliases, colour fields, geometry basis and normal events
survive the paired material/paint round trip.

All new predecessor refs and topology dimensions are bounded before SurfaceMap
reconstruction. A paint-table binding ledger rejects conflicting immutable
geometry/material attachments instead of cloning away a declared shared owner.
Current Paint2 reconstruction can reuse the proven map through the worker-only
full REST/UV/flags/resolution/policy/domain check; actual paint values, layer
owners and cache are retained. Paint/Material cycles are not introduced.

Fresh coherent optimized core/support Release passes4/4 in4.12s: retained paired
factory/material tests, structural Paint2, existing normal-domain and cheap/deep
geometry validation. Tests include real native paired Bake and .7scale/90degree
UV, version/context refusal, exact shared aliases, unchanged unused-context
bytes, wrong before/source/domain/projection/receipt/geometry records, orphan
domain and preparation quotas. Wholly owned C/C++ O1 ASan/UBSan/leak qualification of exact sourcea52f62c
passes4/4 in58.78s. The expanded paired factory/material test has44 assertions. The old structural Paint2 and authority stages remain frozen
separately with their own sanitizer scopes.

This is not a Scene24 native/render/export claim. Scene root contexts, complete
retained origin/min-version/resource/reachability accounting and actual consumer
coordinates remain required before guard removal. Full current-layout texture
compilation and unsupported typed/extra-role/corner-normal transfers remain
required work. The original sharp partial-Bake graphical failure stays explicit.
