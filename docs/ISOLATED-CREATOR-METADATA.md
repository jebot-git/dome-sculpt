# Pinned creator metadata snapshot comparison

The generic held-object guard rejected even a source-assisted all-locked no-op:
`Scene::snapshot` deliberately deep-copies `HumanoidControls`, whereas the guard
compared the independent copies' addresses. The real native failure retained its
exact Scene and held authority and reported `Held-object isolation preserves
all group authority`.

The private guard now compares the complete retained creator metadata: presence,
baseline string, all 17 scalar float bits and all 12 RGBA channel bits. It uses
`bit_cast<uint32_t>` per float, without structure padding, tolerances, JSON
allocation or rebinding a shared mutable pointer. Group placement/affine source
owners and every other group/object COW check remain unchanged. This has no
public layout or Scene codec change.

The independent regression uses only existing Scene/Document APIs. Its encoded
member sweep covers 21 fields and 30 individual values, plus signed-zero and
presence negatives. Real pinned worker callbacks that change the creator group,
an unrelated group's controls, or another object's humanoid roles all refuse.
Their dependent Saves fail, prior project bytes and complete Scene remain exact,
and no history, revision or held authority changes.

Release proof: the 43-check metadata regression and the separately scoped real
source-assisted controller workflow's 21 checks pass (2/2, 19.09 seconds). All
1,624 captured source, binary and configuration inputs are unchanged. These
binaries came from the coherent private source-assisted workflow tree; this is
not a fresh guard-only build, sanitizer or main integration claim. The metadata
regression does not depend on the new weight method or its public layouts.

Retained negatives include the original 5/6 native run, its diagnostic (Scene
byte-equal, held object unchanged), and the independent original-helper binary
that fails before any native mutation. See the JSON receipt for exact files and
hashes. The broader automatic-weight method remains private and unqualified for
main; this portable repair does not complete that goal.

A separate immutable1f tree now completes the fresh O1 assertions-enabled
wholly owned C/C++ core/support sanitizer build and43-check regression:1/1
passes in55.78 seconds with ASan/UBSan/leaks enabled. All1,608 source inputs
and the recorded binary/configuration inputs remain exact. This qualifies the
portable guard's existing native APIs, not the new weight solver or frontend.
