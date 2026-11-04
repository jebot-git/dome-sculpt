# Vertex Mirror source graphics and native UV/key workflows

Test-onlybd24/bd6 extends9ae/af79 without product/layout/archive changes. The
extra-UV renderer explicitly selects the real VertexPositions method used by the
native UI. The commutation consumer now positively exercises two genuinely split
current seam vertices with distinct edits, Save/restart and exact Undo/Redo,
rather than expecting the new supported operation to refuse. Complete paint
comparisons use their actual owning image/normal/paint archive context.

Two complete consumers pass each Release, ASan/UBSan/leak and strict Intel Arc
A770:395 extra-UV checks and104 key/UV commutation checks. They exercise logical
UV2–7, original/generated frames, independent exported consumers, multiple poses
and simultaneous key weights, actual paint after both operation orders, distinct
seam ownership, late cancellation, ordered Save and retry. Original finite pixel
bounds and wrong-coordinate sensitivity controls stay unchanged.

Original bd24 wrappers remain failed: Release173.747s and sanitizer2096.969s
reached the real independent seam successfully but their paint comparator omitted
owning sampling archives. Corrected complete bd6 consumer passes179.852s Release
and2110.348s sanitizer,2234 inputs unchanged each. The preceding extra-UV395
phases in both original wrappers actually pass and are composed explicitly here;
the failed wrappers are not relabeled successful.

The exact two Release executables pass strict A770 and two actual software
`--hardware` refusals:134.595s,3948 inputs unchanged. All312 actual source/export
GLBs (86 extra-UV +18 commutation each profile) pass pinned Khronos/maxIssues0
with0errors,684warnings,2490infos and432hints retained. Of208 cross-profile
comparisons172 are whole-file exact; the other36 have exact binary payloads and
only a process-derived source-node name differs. Nothing is normalized.

This is scoped graphics/native component evidence on immutable matching af79 core
and c092 renderer/support. It does not qualify a fresh application, full suite,
current27SDK UI, physical headset, new XR method/plane controls or system install.
The complete [receipt](validation/VERTEX-MIRROR-SOURCE-GRAPHICS.json) retains
actual commands, warnings, source boundaries and original negatives.
