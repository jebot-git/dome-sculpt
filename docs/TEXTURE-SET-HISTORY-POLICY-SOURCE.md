# Enclosing polygon policy for shared collection decoding

Source-only follow-up to42cd1025. V2 now forwards the actual enclosing holder's
polygon capability from decode_holder through collection_at into each additional
paint reader. Previously only primary decoding received it, so legitimate plain
polygon-authored additional Paint7 grids refused. Default current-only v1 bytes
and its existing policy path are unchanged. Sourceful graph guards remain intact.

The regression now constructs the actual case through a strict native mesh,
prepare_polygon_workspace_topology, adopt_polygon_edit and create_texture_set on
one actual logical polygon face. It requires a genuine polygon-authored Paint7
payload, current/retained reads with exact aliases and logical membership, v1
roundtrip, and transactional refusal when the actual enclosing polygon is absent.
It creates no fake polygon authority and invokes no source snapshot helper.

Syntax only: whole private archive TU plus complete regression TU pass0 with407
inputs and no drift in5.03seconds. Child allocated3,579,904 bytes under8MiB.
No runtime assertions, ELF/object/archive/link, installation or qualified tree
edits occurred. Ignored receipts: artifacts/policy-syntax-v1. Root's fresh coherent
cohort must execute this fixture before a behavior qualification can be claimed.
