# Ordered sculpt GPU arena — 2026-11-11

Private source `ad091d89e877d299fc227af8eaaa9257ce1c764f` batches existing sculpt cell runs without changing global cell/triangle order. The final component passes **Release and sanitizer1753 renderer checks plus405 arena-resource checks on softwareGL and authenticated Intel Arc A770**, with all original1739 controls retained. [The validation record](validation/SCULPT-RUN-ARENA-20261111.json) pins actual commands, source/ELF hashes, dependency audits, logs, device observations and failures. Main admission and sustained P1 performance remain separate reviews.

## Behavior and ownership

The renderer stores positions, normals, indices and scalar masks in a bounded GPU arena. Each chunk keeps its original immutable local CPU arrays and local indices. Dirty geometry uploads directly; retained geometry moves only through measured GPU-to-GPU copies when capacity grows. No whole CPU interleaved or flattened geometry copy is built. Unused capacity,25% growth reserve and fragmentation stay charged.

Per-object command arrays retain exact source cell order and merge only adjacent contiguous runs of the same geometry owner. The source raw-run count stays separate from the coalesced draw count, so CPU capacity forecasts are conservative. Commands cache by actual immutable geometry owner and survive mask-only changes and live held/canvas motion. The two-object fixture has1324 raw runs; its independent741-rule coalescer yields580 logical draws. The actual renderer issues **two multi-draw calls containing those same580 draws**, plus the separate packed-object draw. This is dispatch-count evidence, not a measured frame-rate improvement.

Mask attribute6 is always enabled in arena VAOs. Unmasked vertices contain actual-1 values, uploaded from a fixed1024-float block. This avoids relying on a current attribute value after a masked draw made it unspecified. Packed VAO restoration continues to set its disabled attribute constants explicitly. Original opaque/Blend/Mask, reflection/shear, coincident-object masks, ghost ordering, grouped local placement, second-eye, held/canvas, scalar-only updates, real sculpt geometry edits, retry and pruning controls pass against independent whole-mesh references.

The arena stages new buffers, VAO, scalar owner and commands before acceptance. Writes into shared stores use only currently free holes; retired accepted ranges become reusable only in the next allocator. A failed stage keeps the old occupied ranges and immutable owners. Commit follows successful rendering. Native Scene values remain byte-identical through tested failures/cancellation; position/normal/source/paint/normal-field authorities and existing complex-owner eligibility are unchanged. This introduces no public header, Scene/schema, UI or default changes.

The sculpt vertex shader has no `gl_VertexID` dependency. Fullscreen uses are separate. The polygon `gl_PrimitiveID` path remains disabled for these existing eligible plain sculpt objects; no complex field/domain admission is granted. The implementation uses GL3.3-compatible APIs. Actual tests used contemporary Mesa contexts, not a3.3-only hardware device. Khronos specifies multi-draw as the equivalent ordered sequence of base-vertex draws: [multi-draw reference](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/glMultiDrawElementsBaseVertex.xml), [buffer-copy reference](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/glCopyBufferSubData.xml).

## Requested-store budget and retirement

The hard **512MiB per-arena requested-store budget** includes all accepted/staged buffer capacities, fragmentation, reserve, copies and deleted-but-unconfirmed stores. Driver opaque heaps/pools/implicit renaming and existing flat/texture/FBO budgets are separate; this does not measure total physical VRAM.

Deleting a buffer name can leave queued draws/copies using its store. A fixed scalar ledger therefore retains those bytes as retirement debt. After all last buffer/VAO leases are released, one latest GL3.2 fence covers all prior deletions, followed by a nonblocking flush. Only an actual signaled-fence observation releases the charge. Dirty allocation admission polls with timeout0. Fence creation/query errors keep debt charged and are explicitly reported before new admission. No production blocking wait or `glFinish` is inserted into an unchanged, held or canvas view. `abandon(false)` is terminal, matching the existing app contract. Fence semantics are documented by [Khronos](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/glFenceSync.xml); timeout0 polling follows [glClientWaitSync](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/glClientWaitSync.xml).

CPU scene-metadata reservation is bounded at128MiB, with a fixed1KiB charge even for empty operations, conservative packet/object overhead and actual raw command-vector capacities. Shared old/new forecasts remain conservative. Existing immutable CPU geometry/input arrays and driver allocations are separate. GPU vertex/index/count and arithmetic-overflow guards remain explicit.

The real fixture has277,860B accepted stores and694,636B observed requested-store peak. Its scalar store is22,980B. A deliberately tightened300,840B budget accepts one new scalar store, keeps it charged after deletion while completion is withheld, then refuses another allocation before GL entry. The completion-withholding control is explicitly an API-query shim, not an assertion that physical GPU work stayed busy for that duration. A test-only `glFinish`, followed by the real signaled fence, proves drain/retry. Production never performs that blocking test drain.

## Matched source and executions

The parent is7415d581; immutable complete providers are currentb8c469ce. All101 public headers match those providers byte-for-byte. The two complete affected TUs (`render.cpp` and the arena fixture) were freshly compiled per profile; the unchanged whole chunk fixture was reused only after exact source and actual dependency closure checks. All eight complete consumer/app/benchmark links per profile place the new renderer before unchanged archives. Every map excludes the actual absolute old renderer thin-archive member. Both312-file provider manifests were independently rehashed with no drift. Archives and shared builds were never rewritten or copied. This is a matched component cohort, not a fresh ALL build.

| Route, each profile | Actual scope | Result |
| --- | --- | --- |
| SoftwareGL, five original consumers |51+537+98+73+994=1753 checks | PASS |
| Additional software arena process |405 resource/transaction checks | PASS |
| Separate software chunk replay |98 checks | PASS; separate execution |
| Strict PCI03 Arc, five consumers+arena |1753+405 checks | PASS |
| Same chunk+arena ELFs forced to software with strictArc observer |Two real exit1 refusals | Expected refusal; no numerical credit |

The earlier pure allocator R/S tests each executed2,097,166 independent bitmap/transaction/overflow controls. Their exact pure source/header remains unchanged; those executions are reused evidence, not relabelled new current runs.

All final actual cases passed the enforced original180s external deadline and4GiB external validation RSS guard. Project flags remain optimized Release or `-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer` sanitizer configuration. `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1`; `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; `LSAN_OPTIONS` unset; no suppressions. Successful final logs contain no sanitizer diagnostics.

The arena fixture remeasures46 persistent calling-thread C++ allocation prefixes of the mask-only stage and100 actual preparation-cancellation polls. It injects one-shot genuine GL API rejection at mask allocation/upload/copy boundaries1/16/1 and growth boundaries4/80/112. These are invalid-size/enum/flags controls, not physical device-loss or driver-OOM evidence. Accepted state is checked at each boundary, with actual subsequent retry/publication; mask GL rejection cases retry individually. Growth copies, pending/debt/refusal/drain, fence errors, empty-CPU-budget refusal and source Scene bytes are checked. No new Document/worker mutation API is claimed.

Software uses the retained patched Mesa26.2.5 and privately recovered signed LLVM19 package. Arc uses actual system Mesa and PCI0000:03:00.0,8086:56a0/i915. The unchanged observational preload forwards real GL strings and checks current EGL device/DRM node. Each child has actual open DRM-FD and live mount/inode/ELF certification. A brief pre-assertion test-process pause allows identity capture; it is included in deadlines and prevents no numerical assertion. Profiles take separate shared-flock leases, both released. No physical headset/session or compute-kernel proof is claimed.

R/S software outputs match byte-for-byte across94 images and four native projects; the same98 pairs match between R/S Arc. Each profile additionally passes the original56 software/device PNG comparisons with RGB limit6 and exact alpha; observed RGB maximum2. Nonexclusive observed timings are recorded, not quiet-host performance claims.

## Retained failures and limits of the milestone

The complete original attempts remain on disk and in the compact control seal. Early namespace syntax, build-receipt lookup, duplicate-renderer app-link and worktree-checkout setup errors are retained; affected steps were corrected only after termination, without overwriting failed outputs. The first sanitizer arena fixture aborted in EGL/LLVM setup because its TEST allocator omitted nothrow forms; the complete family and an independent self-check fixed it without disabling any sanitizer option.

The original198 sanitizer sourceful consumer completed its semantic checks in183.40s under a900s coordinator envelope. It **failed the original180s timing gate**. That scope stays failed; later final-source180s runs are separate successes. The a861 source audit also identified premature retirement credit; no physical VRAM overflow was asserted. fbb introduced the fence-backed charge, then ad091 added fixed empty metadata overhead. Earlier source and evidence are preserved rather than reclassified as final qualification.

The independent final audit is exit0, with source/header/provider/output drift empty. The persistent control archive is `artifacts/sculpt-run-retirement-20261111/artifacts/arena-proof/seal/control-evidence.tar.zst`, SHA256 `d0dd998fb18994420ac9b1df4f14b202c21a0d4a7e8e9cc707aa56aa27b899d1`. Its476 metadata/control/log/depfile/map-witness members (16,264,151 raw bytes) have byte-SHA and PAX mode/UID/GID/ns-mtime verification. All original files remain. The67 large raw link maps remain in place with a full inventory; no project object/archive/ELF/native/image originals were deleted. Inspect the archive using `zstd -dc --memory=512MB <archive> | tar -tvf -`.

Root review, current-main/painting composition and the original sustained P1 old/new candidate comparison remain pending. Physical loss/OOM/hotplug/headset behavior, broader authoring requirements and generic retirement integration for paint-preview pools retain their own required qualification. No FPS gain, default promotion or full-goal completion is claimed here.
