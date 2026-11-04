# Current source-aware extra-UV and lossless native workflows

Test-only206034a qualifies the actual source-aware architecture on unchanged
9ae/af79 runtime. The importer retains original role/UV2 arrays while a separate
owned strict atlas supports editing. The tests now require authentic current and
original UV2 maps, valid immutable source-sampling attachment and unchanged
original bytes, rather than expecting flattened/discarded source arrays.
Source-paint capability requires Scene28; Scene29 specifically requires a genuine
explicit Mirror derivation. Native decode/restart still proves full equality.
The lossless tamper test now rejects an equal-valued foreign empty role owner;
the actual adoption guard already requires the captured strong owner.

Three complete workflows pass each Release and ASan/UBSan/leak: extra-UV native
import54, generated UV2 tangents26 and lossless native/controller editing49.
Both actual nine-state pose/morph/GLB consumers keep their original four-level
pixel bound and wrong-UV sensitivity. Save/restart, exact Undo/Redo, native paint,
mid-import cancellation/retry, physical quota and source/tamper protections stay
active. Release takes80.568s; sanitizer486.660s;2238 captured inputs are unchanged
each. Complete changed test TUs use immutable matching af79 core plus own c092
renderer/support; this is not a fresh application/full-suite qualification.

The exact two Release renderer binaries also pass strict Intel Arc A770 and
actually reject llvmpipe when hardware is required:2.528s,2242 inputs unchanged.
All89 actual source/export GLBs pass pinned Khronos with0errors,92warnings,
1277infos and30hints retained. Of59 cross-profile comparisons58 are whole-file
byte-identical; the remaining lossless export has the exact same binary payload
and differs only in its process-derived source node name. Nothing is normalized.

Original unchanged three-test failures and the first incorrect universal29
migration are preserved separately, including exact after-input audits. This
milestone does not relabel the original c092589-suite result, add product support
by weakening guards, qualify all glTF/VRM cases, or imply current-SDK application,
physical-headset or system-install evidence. Read the complete
[receipt](validation/EXTRA-UV-CURRENT-SOURCE.json).
