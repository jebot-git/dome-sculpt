# Independent current-grid preview review — 2026-11-11

This read-only source review covers private preview **f47348eb12b573799880f2bb8fb4946b4aa9731e** and its actual current painting/arena composition **ccb2b7b5803d92b1b3bf65c38f422366c9128b94**. It reports two concrete source defects and a shared-cache accounting gap. No new native test, build, GPU execution or admission is represented by this review. Root's separate fresh owning build and the repair owner's new child remain separate work.

## Findings requiring repair and actual caller evidence

### 1. Nonterminal renderer cleanup discards accepted preview resources and pending debt

In the actual composition, `src/render.cpp:793` calls `textures_->clear(true)` after any material-preparation exception. The final GL-error branch at line 1217 does the same. `TextureState::clear` at line 219 invokes `CurrentPaintPreviewCompiler::clear(true)`.

The latter (`src/render-current-paint-preview.hpp:244–246`) deletes live outputs, sparse inputs, views, all retired stores and the fence, then zeros requested bytes, retirement debt and epochs. There is no completed-fence condition. This happens even when `prepare` itself correctly refused an allocation because the latest fence is pending, or correctly retained debt after a fence creation/query failure.

Consequences follow directly from that call chain: a recoverable real renderer failure loses the preceding accepted resource, and a subsequent same-context retry receives false budget credit for stores still potentially referenced by queued GPU work. The separately retained sculpt arena ledger does not repair the material ledger. Direct compiler tests at `tests/current-paint-preview-gpu.cpp:78` fault `Compiler::prepare`; they do not exercise this outer renderer catch.

Terminal destruction/context abandonment can retain their distinct lifecycle contract. Normal recoverable draws need a staged accepted material/geometry boundary and retirement debt which survives the error path. Merely retaining the `current_paint` pointer while clearing its dependent material caches is insufficient.

### 2. Mutable wrapper transparency can contradict a genuine sealed packet

`CurrentGridPaintPreview::authenticate_range` (`src/current-paint-preview.cpp:43–66`) authenticates marker ownership, object, material factors, plain-field exclusions and ordered UV correspondence. It does not compare `TextureMaps::transparent` with the sealed packet's `transparent()`.

The renderer's automatic blend classification consumes the wrapper flag. A legal `make_shared<TextureMaps>(*range.textures)` copy can retain the genuine packet and change only `transparent`; an ordinary set-0 range still passes the shown packet checks. For a material with opaque background factor but translucent painted alpha, changing true to false can select the wrong blend/depth pass. No const-cast or mutation of a truly const object is needed. A genuine texture-set proof independently rejects a substituted maps wrapper, so the immediate gap is the ordinary single-field route.

Authenticate the derived flag or consume the sealed value throughout classification. Add a real renderer/query counterfactual using that legal wrapper copy and unchanged native input. This review establishes the missing predicate in source; it does not claim an executed graphical counterexample.

### 3. Other cache mutation and requested-store lifetimes require a common transaction

Several pre-existing material caches are destructive before the new frame is accepted:

- `render-source-pack.hpp:133–135`: even `preflight()` calls `sweep`, deleting accepted packs.
- `render-paint-overlay.hpp:131–134`: old outputs are deleted before new builds; failure removes new additions but does not restore old outputs.
- `render.cpp:585–606`: original images/samplers are pruned and new objects allocated before all downstream work succeeds.
- `polygon-surface-gpu.hpp:45–54`: old frame entries are erased before replacement uploads.

The current compiler commits at `render.cpp:626`, before polygon preparation, ready-binding map allocations and flat geometry uploads. Flat buffers are also overwritten after material preparation. Removing one catch does not make all of those resources transactional.

In addition, old polygon-frame bytes/image names are not included in the pre-current allocation allowance at lines 580–626; polygon aggregate admission occurs afterward. Thus a mixed scene can allocate new current-grid stores while earlier polygon stores remain live, before the later polygon check refuses the combined footprint. Old deleted original/overlay/pack stores likewise have no completion-backed debt in their existing counters. These are partly inherited cache contracts; they must not be presented as strict shared old+new/retirement accounting merely because the new leaf compiler has such a ledger.

A repair can stage an immutable accepted material bundle, preserve its actual resources until full frame success, account failed/staged and retired texture/buffer stores through a shared completion-backed ledger, and publish bindings with the existing arena `Pending` commit boundary. The existing arena ledger is useful as a pattern but owns buffers only; it is not already a generic texture-cache transaction helper.

## Boundaries traced without a blocking finding

The source deliberately admits only genuine plain writable strict-UV fields. Source maps, baked/projection/domain/attachment owners, appearance/material bindings, bake recipes and active polygon owners keep their existing adapters. Actor wrappers authenticate object, full bitwise compositing factors, geometry/padding strong ownership and immutable contribution ownership. Texture sets retain their independent field/range/logical-corner proof, which the renderer and material query authenticate before consuming the packet. Reflection uses the explicit packed winding map.

CPU visibility evaluates native alpha, applies byte quantization and bilinear filtering, and then keeps the existing alpha/cull/vertex-color rules. It does not synthesize a CPU RGB/ORM preview. Ordinary public `PaintedMesh`/Scene image APIs remain materialized; export refuses render-only packets, and baked payload validation rejects them. Those are sound inspected boundaries, not a claim that every caller has been newly tested.

The preview compiler itself stages new output resources, retains deleted-store debt until a nonblocking fence completes, checks actual texture-buffer/MRT capabilities, copies pool growth on the GPU and restores relevant resolve state. Its output arithmetic retains the explicit GLSL precise/byte-quantization contract. The source merge preserves the arena's actual ordering and independent geometry/mask owner rules; no public geometry flattening was introduced here.

The arena's 512 MiB requested-store limit and material texture limit (default 1 GiB) are distinct. Neither their sum nor driver-private heaps constitutes a proved whole-application physical VRAM budget.

## Required end-to-end repair gates

Use the actual composed `SurfaceRenderer`, rather than only the compiler leaf. Keep independent old-image comparisons and exact native state/history inputs. Exercise pending fences; create/query/allocation/upload errors; legal transparency-wrapper tampering; mixed original-image/overlay/source-pack/polygon/current fields; and a scene containing both current-grid mesh paint and arena sculpt chunks. Include failures after current resolve, during a later material component, during flat geometry upload and after draw submission.

After each rejected frame, verify retained accepted resource/command authority, correct requested-store debt, prior rendered pixels on retry, no native mutation, and bounded full success once the actual fence completes. Held/second-eye/canvas frames must continue to issue no geometry/mask/current-field uploads or blocking waits. No `glFinish`, CPU-composite retry or reduced numerical/render bounds should stand in for recovery.

A fresh coherent owning build is still required for the `TextureMaps` layout append and the final repaired composition. Earlier private preview counts and separate arena counts remain evidence for their original sources, not automatically this final caller transaction.

The machine-readable source pins and findings are in [CURRENT-GRID-PREVIEW-REVIEW-20261111.json](validation/CURRENT-GRID-PREVIEW-REVIEW-20261111.json). This document changes no source or main state.
