# HDR export cache isolation

Follow-up to the first GLB checkpoint ccf096e. An actual counterfactual on that
qualified core found an uncovered cache side effect: a physical-painted mesh with
zero Material.emissive correctly refused its legacy byte composite before
emission_export_surfaces(), then returned the temporary LDR map afterward.
Snapshot layer vectors were detached, but composite cache/content identity still
belonged to the original. Native Scene serialization remained equal, so the
original source-data equality checks did not detect this visual-cache defect.
The old before-refuses/after-accepts probe is preserved.

The private export-detach helper now lives beside the complete Paint cache type
in paint.cpp. It allocates a new CompositeCache and obtains a new global content
token before copying/altering temporary physical tiles. Original geometry,
layers, normal/source owners, token and cache remain unchanged. The exporter
calls the helper through emission-paint-access.hpp. No public header, layout or
Scene/archive version changes from ccf are involved.

Actual complete new consumer32 controls pass in Release and ASan/UBSan/leaks,
including the before-refuses/after-must-refuse control and all prior package
source/hidden/zero/mixed-LDR/subset/second-cycle checks. Complete paint and
emission-export TUs are rebuilt against matching byte-verified public headers;
old archive members are excluded from immutable qualified ccf core/support
copies. This is a precisely scoped two-TU consumer overlay, not a full fresh
app/core rebuild. The Release/SAN runs take0.2462/0.8523s after34.87/62.30s
compilation. All1511 source inputs and replacement artifacts remain exact.
All3 emitted GLBs per profile are byte-identical to the ccf validator cohort,
which has0errors/0warnings/9infos. No new device or external-viewer claim.

An initial overlay setup compile-negative mixed identical headers reached by
relative and -I paths. The orchestration now uses one own byte-verified include
tree; no source/API workaround or assertion weakening was applied. Root must
integrate this cache correction with ccf before publishing its new exporter.
Typed VRM, actual HDR file workers, alternate/retained source coordinates and
remaining HDR authoring still require their actual continuation. See the JSON
receipt for exact source/object/library/header and link-map scopes.
