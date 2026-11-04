# Original source UVs require a real writable domain

Private test-only correction after the coherent602 Release suite completed
601Pass/1Fail. Test275 reached12 genuine mirrored/linked-paint/projection/history
positives, then expected a stroke on an entirely zero-area UV mesh to silently
produce no data. The current native guard intentionally refuses that transaction
before editing starts. Production code and its guard/limits remain unchanged.

The complete corrected test passes68 assertions in both Release and owned
ASAN/UBSAN/leak checking. It verifies exact refusal, no transaction/allocation
and unchanged original UV/image state. Actual prepare_source_paint creates a
strict independent writable quad while retaining all original degenerate UVs,
geometry correspondence and the same immutable source image. A real changed
colour stroke, exact pixel Undo/Redo and actual native Save/restart follow. The
reopened Source4 owner retains its original sampling mesh and genuine shared
image identity; the original capture remains unchanged. Native R/S files are
byte-identical.

All prior candidate-budget/cancellation, linked/projection, ordinary Strict and
actual19+19 CC0 avatar tails remain: female14,804 and male15,176 triangles,
28 zero-area faces and36,738 linked texels each, complete one-model/25-image
native source restart. No skipped assets or synthetic authority is substituted.

Scope: whole changed consumer TU against immutable coherent d639 Release and
B6 whole-owned sanitizer archives; actual24 consumed project headers are
byte-identical to B6. No stale-layout pairing, application rebuild, device,
physical headset or system/session claim. Build/run total33.069s; all recorded
inputs are unchanged. Read [the receipt](validation/SOURCE-UV-EDITABLE-ADMISSION.json).
This targeted correction composes with the other601 unchanged coherent commands;
it does not relabel the original failed602 invocation as a single green run.
Every unfinished original/additional production requirement remains active.
