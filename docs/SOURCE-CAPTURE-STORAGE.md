# Read-only source capture storage

Private runtime `eb4ce01` extends `8725c1b`. Original-coordinate source captures
now finish validation before allocating dense writable texel and padding grids.
The capture branch never creates those grids. Small tile slots remain so layer
metadata, empty pixel queries and legacy serialization keep their existing
contracts. Strict and ordinary source-authored writable surfaces still build
their actual raster. No public layout, file format, source image, sampler or
paint admission changes.

The allocation oracle refuses every allocation of 1 MiB or more while exercising
raw capture at 64, 512 and 2048 pixels, all three wrapping modes, preview,
snapshot, evaluated geometry, unchanged UV cloning, extra layer metadata and
native encode/decode. It also verifies forbidden paint and forged persisted
tiles still refuse. The largest allocation is 98,304 bytes; all 19 actual
construction allocation-denial prefixes and complete retries pass. A real
2048-pixel writable control correctly exceeds the oracle's bound. The unchanged
old product fails the same source-capture test with `std::bad_alloc`.

Release and ASan/UBSan each pass 139 new storage checks and six affected consumer
runs: initial capture/native worker/history/Save 123, independent sampler 22,692
texels, UV1 40, source UV bake 98 including both real 19-part CC0 bases, source UV
adoption and ordinary paint. All recorded inputs remain exact. No sanitizer or
leak report, suppression or resource/quality-bound relaxation was used.

These are fresh changed-paint and consumer TUs against matching immutable owned
archives. Release uses the parent's coherent complete CPU build. Sanitizer uses
the wholly instrumented f99 support with the matching bounded-UV source/import/
Document objects. This is not a fresh whole application or full regression run.
The [receipt](validation/SOURCE-CAPTURE-STORAGE.json) records commands, original
negative, input manifests and log hashes. The initial missing full-JSON test
include is preserved in the local setup log and was corrected before testing.

The initial source-UV candidate remains separate from public main. Its masked
source-to-atlas edge failure, initial untextured/MatCap-only authority and broader
sampling/LOD work remain required. This allocation fix does not qualify image
fidelity, HDR, VRM physics, physical devices or the full production workflow.
No system installation, physical headset or active session action occurred.
