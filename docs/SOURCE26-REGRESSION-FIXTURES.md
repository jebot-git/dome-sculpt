# Source26 regression fixture corrections

Four historical tests need fixture/expectation changes when original indexed
coordinate ownership is admitted. No production code, parser, public header or
CMake input changes in this checkpoint. The original frozen cf5fa43 full479 run
continues unchanged and preserves all its failures; these corrected executions
are separate evidence.

The glTF indexed attributes must begin at0 and be consecutive (the primary
[meshes specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview)).
Missing-coordinate atlas/cancellation fixtures now remove all UV attributes.
The typed-only shade-UV1 fixture restores the actual original UV0 accessor and
bytes, retaining that genuine stream without adding a PBR image. A precise
UV1-without-UV0 negative remains; no padding or parser relaxation occurs.

The generic import fixture replaces its obsolete blanket UV1 refusal with actual
owned UV1/flat generated-frame checks, restart, atomic native paint refusal, and
explicit editable-atlas painting admission. It retains all old missing-normal,
source/morph remap, cancellation and worker/history checks. The unlit source's
attribute assertions do not claim new normal-map pixel sensitivity.

The OBJ worker fixture records the actual adapter4 diagnostic, `Native textured
primitive has no UV coordinates`, while retaining exact whole-scene refusal,
valid textured retry, raw OBJ/MTL/PNG ownership and exact Undo/Redo checks.

All four corrected TUs pass against frozen cf5 Release and wholly-owned
ASan/UBSan/leak core/support archives, with byte-identical public headers and
unchanged production sources. No new whole-suite or hardware claim is made.
