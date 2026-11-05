# Affine polygon and retopology composition

This private source composition starts from published main1a70f2a and preserves
all admitted retopology, ordered Save/Cancel, marker-renderer and avatar92 changes.
It adds qualified polygon1e8c84d without changing Scene/EditorState/public data
layouts. The current plan is recorded in
`validation/POLYGON-RETOPO-AFFINE-PLAN.json`; this document does not qualify an
integrated runtime.

The complete merged `document.cpp` must compile afresh. Its constructor and
destructor instantiate BOTH actual private deque element types. Reuse of either
parent's old Document.cpp object is forbidden. The actual own-header probe gives
Document6968/8, EditorState1896/8 and Scene72/8 with unchanged inspected outer
member offsets; the real private payloads remain PolygonJob232/Accept360 and
RetopoJob72/Acceptance112. Only Doc.cpp/DocPolygon/DocRetopo have payload-symbol
references across99 actual existing R/S object/archive inputs. The reused whole
DocPolygon and DocRetopo definitions exactly match the respective qualified
payloads; the other whole provider bodies and used inline preview helpers are
byte-identical to their qualified parents.

First Release work is specifically limited to one fresh complete Document.cpp,
one fresh combined native consumer, and one ordinary app relink, using main5 and
polygon6 complete provider objects before immutable matching C179 archives. A
link trace must reject every replaced old archive member, including any old weak
constructor. No source extraction, archive rewrite/copy, or full build is planned.
The cumulative approved source+Release cap is0.2GiB and actual free must remain
at least20GiB under prospective link guards. Sanitizer, new GUI and hardware
cohorts remain held until actual first-phase growth and results are reviewed.

The combined native fixture exercises both operation orders, real source pose,
paint and affine placement, held Cancel, mode ownership, Apply/immediate Save,
Undo/Redo, and an actual retopology→polygon edit→source-Restore roundtrip. Restore
is a separate real history operation; it never erases the conversion entry.
Later GUI/device qualification must retain both viewport paths and current
retopology marker semantics. No remesh source or protected root sweep is changed.

Scoped runtime closure is now recorded in `validation/POLYGON-RETOPO-AFFINE.json`:
R/S248 native assertions each,16 byte-identical native pairs, and77 real ordinary
application checks in each normal/compact/profile case. Both mode orders and
retopology→polygon/source-Restore run within each actual desktop process.
The unchanged ordinary Release app also completes genuine affine posed/painted
real-loader retopology and polygon/Restore workflows on observed PCI03 A770;
software/UHD qualification controls refuse the hardware claim while the ordinary
application retains its correct software support. Actual source/binary/ELF maps
remain unchanged, and the lease is released. Original observer/setup failures
remain explicit. This is not main admission: published9dd worker composition and
its current-body closure are the next required stage.
