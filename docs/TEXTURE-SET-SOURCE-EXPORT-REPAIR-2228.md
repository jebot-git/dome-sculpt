# Native source-export integration repair

The frozen 2228 whole Release build completed, but its native matrix passed
21/22: source export refused a genuine KeepPaint binding. Native KeepPaint
intentionally publishes sampling_reparameterized=true. The new paint-only
polygon capture adapter had incorrectly rejected that required flag.

The repair requires true on that authenticated native before-binding. The
current polygon domain3 binding still requires false. Actual owner/control
block, current attachment, original basis/corners, typed/frame exclusions,
quotas and normal/source guards remain unchanged. The fixture now verifies
true-before/false-current and rejects an otherwise genuine cleared flag.

The completed path exposed two fixture defects: retaining an object reference
across create_texture_set's atomic Scene replacement, and passing JSON values
to array at()'s generic object-key overload. The fixture now retains the stable
object ID and obtains actual size_t array indices. Every buffer/component/count,
independent N/T/image/UV/material and cgltf check remains intact.

Focused optimized Release proof uses one whole affected owner TU, eight whole
current test TUs, a fresh thin core archive that excludes the old owner member,
and eight new executables. Exact current 2228 headers/libraries/SDK are pinned;
no layout changed or original provider/index was overwritten. All eight CTest
routes pass: surfaces, native authoring, ordinary export, polygon native owning,
mixed polygon/sets, field source, retained history and sourceful export.
A separate explicit source-export run passes 655 controls. Each run writes five
real GLBs covering native/prepared shear/reflection and smooth N-only output;
all are independently cgltf parsed/validated, with five exact output pairs.
Scene39/40 ownership/history/Restore and independent current/original UV checks
pass. Varying-normal and FLOAT/HDR export refusals remain explicit.

All 6079 final pins remain unchanged, including the original 312 whole-provider
outputs. Raw current component allocation is 171503616 bytes; with 64MiB serial
prospective reserve it remains below 256MiB. The first map-size resource refusal
and subsequent fixture lifetime/JSON-index failures remain retained. Seven new
large linker maps were losslessly compressed with actual original-path
restore/hash/mode/owner/nsmtime/xattr verification; frozen baseline artifacts
were untouched. See validation/TEXTURE-SET-SOURCE-EXPORT-REPAIR-2228.json.

This is component qualification on exact unchanged2228 providers with one
whole implementation replacement, not fresh whole-application qualification.
The parent will merge current main documentation and build all 32 targets afresh
from this source. Combined native/graphics/GUI/loader, sanitizer/hardware and
main admission remain separate requirements. No broader test or installation
was performed after this closure.
