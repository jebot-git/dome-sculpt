# Editable atlas import and controller publication

Product0057749 combines public899fa6b with owned source UV1 sampling, explicit
editable texture atlas import, generated writable UVs for untextured materials
with unused degenerate source UVs, and generic UV-worker cancellation. Original
source bytes, ordered corner identities, material/frame/skin/morph state and
source-restoration history remain retained. Native and controller import options
make the resampling choice explicit. Default unsupported source-UV import
continues to refuse until the user selects preparation.

The combined Release run before the input repair passed282/283. Its remaining
shapekey menu-selection failure exposed two input issues. Transaction cleanup
could erase controller samples already submitted against the newly published
document epoch. It now removes obsolete epochs only, while keeping the overflow
cancellation reset for retained current input. A first released sample after a
retained-menu rearm now updates the radial highlight immediately; it cannot
activate a command or parameter gesture. No public data layout changed.

A deterministic test gates the real transaction after immutable publication,
submits two actual controller frames through the public API, and observes the
queued hover. Replacing only the complete transaction translation unit with the
old implementation reproduces the lost selection, with current fixture and
safe-hover implementation unchanged. A second route submits65 frames through
the actual64-frame bounded queue and verifies its overflow reset survives.
All15 native checks pass. The original shapekey fixture/assertion is unchanged;
both tests pass20 consecutive runs each.

After all Release consumers were relinked,36 selected Release suites and31
owned-core/support ASAN/UBSAN/leak suites pass. Four real OpenXR loader fixture
suites pass using synthetic tracking: general client, colored cuts, atlas import
and object selection. Actual native FLTK import/options/Save/Undo/Redo passes12
checks. These are separate from the earlier ec71855 Arc537, Wayland8 and
sanitizer13 proofs. The old282/283 run remains recorded as failed; a focused
repair does not rewrite it into an all-suite pass.

Read validation/ATLAS-CUT-IMPORT-INTEGRATION.json for exact source, hashes, reports
and scopes. The component receipts remain authoritative for original atlas
quality, both10,333-allocation sanitizer sweeps, cancellation, restoration and
independent GLB consumers. Atlas conversion is explicit resampling and does not
promise pixel identity at raw source edges.

The accepted physical headset UI remains unchanged as a baseline. No new
headset launch, system installation or session restart occurred. Generic VRM
import and Scene19 normal UV/key integration are a separate private build;
current-field target baking and the complete REQUIREMENTS.md matrix remain
required. This checkpoint does not complete the production goal.
