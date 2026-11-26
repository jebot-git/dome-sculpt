# Canonical source fixture detached-history correction

The first genuine canonical fixture runtime stopped before source capture. It
required `origin.before_paint == live_input`, while the real factory intentionally
snapshots the mutable PaintedMesh wrapper and copies deformation before conversion.
The original native-v1 failure, input pins and logs remain unchanged.

This test-only child asserts the detached wrapper and full exact paint content,
shared immutable mesh/source-map/baked-event owners, exact field-origin identity,
and detached deformation with complete equal native encoded contents. The product,
canonical cap/header owners, raw packet/frame/event certificates and all numerical
bounds are unchanged.

The entire fixture was reviewed across conversion, field replacement, UV/key
adoption, Scene snapshots, Document.replace, worker adoption, Save/reopen and
UndoRedo. Exact pointer comparisons remain only within a single Scene or between
factory-guaranteed immutable geometry/maps/event/field owners. Cross-snapshot and
persistence checks use exact complete native contents. In particular field.prior,
field.origin, source-owner retention during Bake and shared Clear surface/padding
certificates remain exact; their guards are not replaced by byte equality.

Native17/prepared91/artist139 will be collected independently even if the new
canonical matrix fails. This source checkpoint is not a runtime pass.
