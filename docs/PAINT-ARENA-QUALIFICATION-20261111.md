# Current painting and sculpt arena qualification — 2026-11-11

The composed painting and ordered sculpt arena source **27f01cad7910a9c6b6dd383a4422f3feffdbc61c** passed the independent provider audit and twelve actual Arc consumer runs described here. This receipt adds documentation only. It changes no product code, public layout, default or source guard.

## Exact composition

The immutable union is `artifacts/paint-arena-integration-20261111`; its outputs are `artifacts/paint-arena-build-20261111`. The baseline is current painting source **775f92b837da757aa5bfb8583e2f426685eade50**, with matching `artifacts/paint-main-build-20261111/{release,sanitize}` providers.

The audit independently rehashed 2,747 frozen union files and all 331 baseline providers per profile. All 102 public-header files and 714 unchanged product/header/asset/third-party files match the baseline. Four complete translation units are fresh: `render.cpp`, `sculpt-chunk-render.cpp`, `sculpt-gpu-arena.cpp` and `sculpt-gpu-arena-layout.cpp`. Eight compiler dependency scans with their actual profile flags identify 516 distinct files, including 118 project files; their final hashes are unchanged.

Each thin render archive contains exactly four members: the fresh union renderer and the baseline palette, palette-text and polygon-render objects. The old renderer member is absent. All nine actual link commands and direct provider hashes were inspected: the application, sustained consumer and seven tests use current union/current 775 project objects and archives. No old b8 project provider is used. This is an incremental matched-source composition, not a fresh whole-core build.

## Executed controls

Root's completed software gates remain existing evidence; they were not rerun here. The new Release and sanitizer Arc runs execute the same six current union ELFs and native fixture arguments:

| Consumer | Checks per profile |
| --- | ---: |
| Direct immutable uploads | 73 |
| Polygon surface | 51 |
| Ordered sculpt chunks | 98 |
| Mixed texture/polygon ranges | 537 |
| Source-bearing ranges | 994 |
| Arena allocation, cancellation and retirement | 405 |

The first five total **1,753** graphics checks per profile; the arena adds **405**. Every native process exits 0 under its original 180-second bound and separate 4 GiB validation RSS envelope. Sanitizer runs retain `detect_leaks=1`, ASan halt/abort and strict UBSan, with no suppressions or reported diagnostics. The existing software layout test's **2,097,166** controls remain separately attributed to Root's completed run.

Arena runs include the real 46-prefix calling-thread C++ allocation sweep, 100 cancellation boundaries, mask and growth allocation/upload/copy errors, retained-stage accounting, pending/drained retirement debt and atomic retry/publication. Both Arc runs report 277,860 accepted and 694,636 peak requested-store bytes. The production 512 MiB arena budget covers known requested stores, staging, reserves, fragmentation and unconfirmed retired stores. It does not measure opaque driver heap overhead or assert a whole-application physical VRAM bound. Injected API errors do not prove physical device loss or real device-memory exhaustion.

Each profile uses a separate exclusive `/tmp/dome-sculpt-arc-device.lock` lease. Release handle **36174**, observer PID **3290900**, and sanitizer handle **73957**, observer PID **3298906**, are terminal 0 with both leases released. No additional GPU work remains in this qualification.

## Actual device and images

A freshly built, project-ABI-independent observer forwards the actual GL result unchanged. Each native process authenticates `Mesa Intel(R) Arc(tm) A770 Graphics (DG2)`, Intel `0x8086`, device `0x56a0`, PCI **0000:03:00.0**, the actual EGL render node, live DRM file descriptors and i915. The process is briefly paused after its first authenticated context to record actual maps, inode/device and mount-namespace witnesses, then resumed. This observational pause is included in the deadline; the timings are not benchmarks. All 85 distinct actually mapped ELF files are rehashed after the twelve exits without drift. Current system Mesa/LLVM22 and the official current SDK are recorded; this scope does not borrow the historical patched-Mesa route.

Independent comparison finds **94 images and four native files byte-identical between Release and sanitizer**, separately for software and Arc. The original 56 software/Arc PNG comparisons per profile retain RGB limit 6 and exact alpha. Actual maximum RGB difference is **2**, with alpha difference **0**. No tolerance or numerical test body changes.

## Preserved failures and limits

Root's first software coordinator used an incorrect expected-count table: the actual direct consumer passed 73 checks and exited 0, while the wrapper exited 1. That original wrapper, log, ELF hash and failure remain preserved. The corrected v2 receipt explicitly reuses that completed native execution and maps polygon 51, mixed 537 and direct 73. It does not invent a rerun or relabel the original wrapper successful.

Standalone arena source, its earlier sanitizer 180-second miss and all original negatives remain in distinct seal **5796bee91aeb70260b481afbe646bb7ffb2f8e8d**. This receipt qualifies the new painting composition rather than relabeling standalone evidence. A first documentation-write attempt used an incorrect relative destination and created no file; correcting that path changed no runtime or validation result.

This scope adds no long P1 benchmark, application GUI/XR acceptance, physical-headset/session action, compute-kernel proof or general performance claim. Main admission is a separate coordinator decision.

## Persistent evidence

The compact receipt is [PAINT-ARENA-QUALIFICATION-20261111.json](validation/PAINT-ARENA-QUALIFICATION-20261111.json). Raw evidence is under `artifacts/paint-arena-qualification-20261111`.

`seal/current-union-evidence.tar.zst` contains 336 source/control/manifest/log/map/output members, independently decoded and checked against raw SHA-256 and PAX mode, UID, GID and nanosecond mtime. Its SHA-256 is `616c6f7a84d87e5037c4b0ad0f63f8ba1a17fafe8ca58e9870e73d0ba671b182`. Originals remain in place. The archive includes the audit, exact compiler dependency commands, forwarding observer source, strict Arc runner and native/image comparison script. It is an evidence copy, not a distribution of project libraries or executables.
