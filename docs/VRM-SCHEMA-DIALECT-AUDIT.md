# Official VRM schema dialect audit

The older normal-expression validation forced a Draft4/RefResolver recipe across
mixed schema resources. On the real MatCap material in its exported Normal file,
that recipe accepts both a missing texture index and a string texture coordinate:
it skips constraints alongside the core2020 textureInfo reference. The original
legacy results and actual diagnostic controls remain recorded.

The optional test utility tests/vrm-official-texture-schemas.py dispatches each
resource reference to its declared Draft4 or2020 validator (undeclared VRM schemas
retain Draft4). It requires jsonschema4.26.0 and an explicit pinned --spec root;
unpinned/network references fail closed. It records consumed schema hashes and
checks their stability. Schema and positive-instance bytes are not rewritten.
Real missing-index/wrong-coordinate controls must reject; Shift-bearing datasets
also check the actual declared Draft4 scale constraint. No Python runtime is added.

All24 unchanged Normal source/worker/GUI files from d6d40ca/seal4c9f321 pass all186
extension-schema instances using the corrected dispatcher. Both actual MatCap
negative controls now reject. Input COW copies match every original file SHA.
No malformed positive output was found or repaired. Original Khronos0errors and
all110 warnings/344 infos/112 hints remain separate unchanged evidence.

The first wrapper failed before copying or validating because a fresh worktree
lacked its ignored artifacts parent; that exit1 is retained. The explicit corrected
invocation creates parents and validates the original data. This audit corrects
standards evidence; it does not admit private37/Normal, add a native feature or
complete the original production/device/accelerator goal. Machine evidence is in
validation/VRM-SCHEMA-DIALECT-AUDIT.json.
