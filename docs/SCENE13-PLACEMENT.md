# Scene 13 immutable source placement

This private checkpoint persists general imported placement without approximating
it as a uniform CanvasTransform. It does not enable general affine import in the
application yet. Existing import guards remain until rendering, interaction,
held previews and export use the same full-matrix contract.

Group local placement is `E_native * TRS_native * A_original`; object local
placement is `E_native * TRS_native`. Original source-node matrices, parsed model,
encoded model bytes and decoded images remain shared immutable authority. Native
edits are independently reversible validated affine prefixes. A detached polygon
origin captures the full root-to-leaf group path plus object delta. Group IDs and
source instance IDs in that capsule are historical provenance, not live lookups.
Restore-original continues to keep current placement, as in Scene 12.

Scene 13 adds the deduplicated `affine_state` version 1 table. It bounds edits and
node placements to 4096 each, capsules to 512 and copied groups to 65536 (128 per
path). Native matrices, finite/invertible edit authority, source model/node refs,
UTF-8 names, complete paths and reachable records are checked. Model/image/raw
budgets share the existing Scene archives, including retained-only capsules.
Polygon source archive version 2 adds capsule references to origins; legacy
version 1 remains unchanged. Polygon version 3 is reserved for Scene 14 current
corner colors. No older Scene accepts the new placement fields.

`Scene::world_matrix`, `Scene::parent_matrix` and `ObjectSurface::world_matrix`
are the full-matrix interfaces. Legacy `world_transform` rejects affine holders
rather than returning an approximate similarity. Source conversion and the strict
polygon/color bridge share the existing complete origin-capture implementation.
Removing groups while keeping objects preserves complete world placement through
an affine prefix, staged before mutation.

Fresh regular-user Release proof: five selected suites pass (source placement,
source placement allocation, model archive, Scene compatibility and polygon
archive). The placement fixture reports 14807 assertions, with 1506 encode +1673
decode allocation boundaries for current affine sources and 2146 encode +2063
decode for retained-only sources. Denial remains active during unwinding; exact
source state and retries are compared after each failure. Malformed refs,
singular/projective edits, orphan records, quotas, old versions and captured-path
errors are rejected. Exact CBOR restart and shared model/edit aliases are covered.
Logs: `artifacts/test-scene13-fault2.log` and CTest LastTest.log. A separate fresh
whole-own-core/support sanitizer build is in progress at this checkpoint; no
sanitizer completion or device/frontend support is claimed here.

Remaining application work includes effective-matrix mesh packing and normal/TBN
policy, reflection/culling, evaluated painting and brush metric, desktop/XR picks,
held world-delta conjugation, exact source hierarchy GLB/VRM transport and real
import/frontend qualification. Arbitrary-affine sculpt resolution/remesh metrics
must be represented explicitly or refused. Full import also retains its separate
PBR UV1, absent material, untextured overlapping UV, FBX skin/morph and animation
diagnostic requirements. Background loading currently uses inherited cancellation
boundaries; this codec checkpoint does not claim newly interruptible JSON decode.
