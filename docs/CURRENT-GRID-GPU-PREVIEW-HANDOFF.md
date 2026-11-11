# Private GPU preview handoff — paused

Parent finalized main independently at `3eb77a8a` and requested a pause after
this evidence milestone. Do not resume implementation, merge, install or start
another test until requested. No live build/test process or GPU lease remains.

Private worktree: `artifacts/plain-gpu-preview-20261111`.
Private remote branch: `work/plain-gpu-preview-20261111`.
Base: `775f92b837da757aa5bfb8583e2f426685eade50`.
Product source: `1452ddec1d1b298d2c1b4f5a30200d2492c0b9fd`.
Final tested consumer: `acff27fefa886add190f32be844fc4358a63cdbe`.
Later seal changes documentation/evidence only. Main was not modified.

The actual Document producer now selects an explicit render-only current-grid
packet before legacy image materialization. It retains genuine immutable
common/FLOAT COW layers, actual mesh/padding and material factors; actor wrappers
and existing texture-set proofs authenticate its render context. The context-owned
GPU cache resolves canonical legacy byte color/ORM/emission or genuine physical
FLOAT planes without a per-frame composite readback/reupload. Exact CPU alpha
sampling preserves material picking. Existing public Image APIs, source-color
preparation, default texture-set reference preparation and export remain real
image consumers. Render-only packets are refused as export/bake image authority.

The compiler retains requested-store retirement debt through nonblocking fence
completion, supports compact COW slot reuse and actual GPU buffer growth/copy,
and refuses device capability/resource failures explicitly. Failed staging keeps
the preceding resource. It does not flatten native authoring or alter history.

Fresh Release/SAN ownership includes core, renderer, native graphics, FLTK UI/XR
application and ten matched consumers. Both profiles passed the ten software
controls at 1452; the test-only extension passed 98,815 software and 98,818 paired
Arc assertions per profile. Strict ASan leak detection 1/UBSan, no suppressions.
All source drift lists are empty. Original build/include and GLSL failures remain
in the receipt. No actual private GUI/headset launch or system installation was
performed. These ten controls are a selected closure, not the full project suite.

Raw builds and receipts: `artifacts/plain-gpu-preview-build-20261111/`.
- `release/`, `sanitize/`: owning artifacts and compile databases.
- `receipts/qualified-1452ddec/`: original ten-case R/S and fresh SAN closure.
- `receipts/attempt0-6b09ec82/`, `attempt1-848780e6/`: original retained failures.
- `receipts/*-extended-tests.json`, `current-gpu-arc-*.json`: final test-only
  extension controls and actual runtime library/DRM identity.
- External runner scripts at the build root reproduce the exact commands;
  Arc runners take `/tmp/dome-sculpt-arc-device.lock` exclusively and cap 60s.

Tracked compact receipt:
[CURRENT-GRID-GPU-PREVIEW-20261111.json](validation/CURRENT-GRID-GPU-PREVIEW-20261111.json).
Architecture/limits:
[CURRENT-GRID-GPU-PREVIEW.md](CURRENT-GRID-GPU-PREVIEW.md).

When the goal resumes, review this private change against latest main in a new
integration candidate and rebuild its actual owning closure: `TextureMaps` gained
a typed pointer, so older core/renderer/module binaries cannot be borrowed as
ABI proof. Preserve the parent’s already-qualified arena, UI/font/checkbox and
other component work while composing. Then qualify the actual combined desktop
and XR callers; the older private application build is not that combined build.

Broader painting/material/baking objectives remain required. Source-backed,
projected, baked, polygon-authored and retained-domain fields keep their existing
paths; this implementation deliberately does not invent Source4/native-emission
identity or claim those domains were redirected to the new current-grid cache.
Larger backend/provider work and final combined user acceptance remain separate.
