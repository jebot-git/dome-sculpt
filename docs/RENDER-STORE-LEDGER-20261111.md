# Shared GL store ledger — 2026-11-11

Private helper source **23227f52946a6b38dd5cae169f729d28906af88a** adds typed, shared ownership and completion-backed requested-store accounting for the renderer recovery work. It descends from composition `ccb2b7b5`. Only `src/render-store-ledger.hpp` and `tests/render-store-ledger.cpp` are product/test additions; existing renderer/cache/CMake code is not changed by this milestone. The actual renderer transaction repair and final coherent application qualification remain separate.

## Ownership and admission

`dome::detail::ContextStoreLedger` is shared across a renderer's accepted and Pending frames. `Store` is a copyable lease exposing `name()`, `bytes()`, `kind()`, `domain()` and `group()`. `create` admits the prospective cost and allocates all owned CPU bookkeeping before `glGen*`; callers initialize the actual buffer/image afterward. No project archive or old public-layout provider is used by this standalone component.

Kinds are Buffer, Texture, Sampler and VertexArray. Material bytes, texture/view names and sampler names are shared across every material-cache group. Geometry Buffer/VAO stores use a separate byte domain. Default material and geometry byte allowances are each 1 GiB; caller-supplied limits remain authoritative. These do not replace the arena's separate 512 MiB limit or prove whole-application physical VRAM usage. Zero-byte TBO views still consume texture names; VAOs/samplers do not invent storage bytes.

Optional dependency leases retain a view's or VAO's actual backing stores. Internal raw-node reference counts avoid a ledger ownership cycle. The renderer must bind the genuine declared backing resources and stage a new view when its immutable backing changes. The helper does not intercept arbitrary external GL rebinding. All methods and lease copies/drops belong to the owning current-context thread.

CPU bookkeeping is independently bounded: fixed 4 KiB, conservative 512 bytes per group (including old-plus-new vector growth), and 1 KiB plus dependency pointers per stable store node; at most 1,024 groups, 65,536 stores and 64 dependencies per store. The default CPU allowance is 128 MiB. The earlier 128-byte group estimate is retained at initial source `ed560953`; it was tightened before final qualification without changing the API, private layout or GPU limits.

## Transaction and retirement boundary

The last lease drop marks the actual GL object retired without allocating, deleting the name, waiting or granting budget credit. Stable ledger-owned nodes keep names and charges until completion. Accepted and staged resources therefore share old/new/retired accounting, including failure before cache insertion.

Call `mark_used()` before actual uploads/copies/draws. It performs no allocation, fence or query, so unchanged/held views need no maintenance. After all fallible draw/resolve work, `seal(true)` inserts and flushes the latest fence before nonthrowing frame publication. CPU-only lease drops after that seal reuse its coverage until another `mark_used`. New use after an earlier seal requires a newer fence before credit.

`collect()` uses only zero-time fence queries. Actual fence creation/query and GL deletion errors preserve stores and debt and throw. `seal_noexcept()` is for rollback: it retains errors for a future `collect` rather than throwing through cleanup. `clear(true)` and `abandon()` are terminal; they disallow allocation afterward and keep outstanding CPU handles valid with name zero. Ordinary recovery must not replace or terminally clear the ledger.

## Executed helper qualification

All final tests freshly compile the one test TU and private helper with optimized regular-user Release or ASan/UBSan flags, linking only system EGL/GL/pthread. There are no borrowed project objects, libraries or frontend artifacts. Final software job **3356** is terminal 0:

- Release: **93** controls on actual llvmpipe.
- Sanitizer: **93**, with leak detection 1, strict ASan/UBSan and no suppressions or diagnostics.
- The same Release and sanitizer ELFs: **95** controls each on actual PCI **0000:03:00.0**, Intel Arc A770 `8086:56a0`. The two extra checks arise from three hardware identity predicates replacing one software-provider predicate.

Final Arc runner PIDs **365517/365576** are terminal 0; both common-lock leases are released. Inline actual EGLDevice/node/sysfs/renderer checks and a forwarding observer independently authenticate the live context, DRM FDs and mapped ELF identities. All 43 Release / 45 sanitizer actual mapped ELF hashes remain unchanged. A deterministic pre-assertion observation pause is inside the original 180-second bound. No timings are offered as performance evidence.

Controls use actual GL buffers, data, GPU copies, TBO views, VAOs, RGBA image stores, samplers and fences. Validation readback verifies retained bytes and a real copy's changed value. They exercise shared-group old/new quotas, image/sampler retirement-name quotas, backing-store dependencies, presealed publication and later-use coverage, terminal handles, and foreign-ledger rejection. Both measured persistent helper allocation prefixes (dependency-vector and stable-node allocation) fail before actual GL generation and preserve accepted data. This is scoped to owned pre-GL helper bookkeeping, not exhaustive driver or application allocation coverage.

Fence creation/query and deletion errors come from genuinely invalid driver API arguments; retry uses the actual valid APIs. One timeout control deliberately returns a test override to verify that the ledger cannot credit a pending result. Actual fence draining, resource/data verification and the error controls remain real GL. A validation-only `glFinish` establishes completion before the deletion-error test; the product helper contains no `glFinish`, blocking wait or CPU result fallback. No physical memory exhaustion or device-loss execution is claimed.

## Preserved failures

The first Release consumer aborted during LLVM initialization before a renderer report. Its test allocator forwarded sub-pointer alignments directly to `posix_memalign`; normalizing to at least `sizeof(void*)` fixed that test-only setup defect. The original ELF, raw source and same-binary diagnostic repeat are retained; it is not a passing run or a host-capacity failure.

The first Arc native passed all 95 checks and exited 0, but its external coordinator missed the very short live-process map window and exited 1. That wrapper failure remains intact. A new observer pauses deterministically after authenticating the actual context; the same native ELFs are rerun for complete positive scopes. The final 512-byte group charge then receives fresh software and Arc runs; earlier source/results remain separate.

## Integration and evidence

The integration owner registers `dome-sculpt-render-store-ledger-test` with `PkgConfig::PAINTGPU`, private include `src`, wrappers `glGenBuffers`, `glDeleteBuffers`, `glFenceSync`, `glClientWaitSync`, a 180-second test ceiling and software-GL environment. This branch does not claim that registration or the renderer recovery test already ran.

All material caches, temporary stores and flat geometry must actually use the shared ledger and accepted/Pending boundary. Keeping only the current compiler's pointer cannot fix destructive source-pack preflight or other cache pruning. Required caller-level mixed-cache failures, native/source retention, original image comparisons, arena composition, GUI/XR and final owning builds remain the repair/integration owner's responsibility.

Raw evidence is in `artifacts/render-store-ledger-20261111/artifacts/ledger-proof`. The compact receipt is [RENDER-STORE-LEDGER-20261111.json](validation/RENDER-STORE-LEDGER-20261111.json). `seal-final/render-store-ledger-final-evidence.tar.zst` contains 93 source/control/ELF/log/map members, fully decoded and checked against SHA-256 and PAX mode, UID, GID and nanosecond mtime. Its SHA-256 is `5a738727638b3a8b65dd96b663779ae828fcfc1d3c16d7fb0e49ee88bb9ebf0c`; all originals and the earlier evidence archive remain intact.
