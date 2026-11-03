# Coherent VRM1/display integration

Runtime/CMake head `fc56265411f80e6d4a4e1a2c40e309c23a1d5b1e` composes the
qualified first VRM1 component `a21843b` with cleanup base `69ca191` and published
qualification metadata `431a187`. Root display/EGL ownership, native SDK guards,
source provenance and all unrelated commits are preserved. The only further
CMake change is test-only `a5770ff`: canonical JSON forward declarations for the
standalone sparse-quality fixture. No runtime/header/API/source-code change was
made after the frozen `f99ac01` build/gate cohort.

The [machine receipt](validation/VRM1-COHERENT-INTEGRATION.json) records exact
commands, sources, outputs and profiles. Published main `13f7e57` now contains this qualified integration, verified against
the remote after its fast-forward from `431a187`. This does not complete full
VRM1 support or the production goal.
The earlier [component receipt](VRM1-SOURCE-EXPORT.md) retains its own parent and
SDK/proof scopes and is not substituted for this fresh build.

## Fresh coherent builds and affected checks

| Scope | Exact result |
| --- | --- |
| Optimized CPU ALL-target build |Pass1588.99s;535 registered commands built,416 ELF/static outputs,1,474 tracked inputs unchanged; not an all535-test pass |
| Whole-owned sanitizer native app/core/render/support +17 affected targets |Pass642.15s; UI/OpenXR ON, O1/assertions, C/C++ ASan+UBSan, no old archive reuse |
| Six additional sanitizer legacy/import consumers |Pass53.86s, same1474 source inputs |
| Affected optimized CTest |28/28,100.77s;1,518 inputs exact |
| Affected whole-owned sanitizer CTest |18/18,202.54s;1,508 inputs exact, no ASan/UBSan/leak report |
| Separate whole-owned sanitizer graphics |V1 image16 +actual private-X11 desktop context18,9.50s/19.94s;1,487 inputs exact, no suppression/leak report |
| Actual native FLTK V1 export menus |7 normal +7 compact,11.34s/11.07s; real chooser Close, durable-temp Cancel, following Save/Redo, exact project/retry |
| Actual current DomeD Wayland |11 native menu/navigation/gracefulClose checks,20.89s; private compositor/server, no physical session |
| Official V1 independent image consumer |16 software +16 actual A770,5.16s/3.57s; all source/current full-image errors1 at unchanged tolerance2, genuine time/PBR sensitivity |
| Actual export→fresh native reimport→export/restart |3 Release +3 owned-SAN cases: multi-skin/rigid, current kinematic pose, official static source;1,499 inputs exact |
| Pinned interoperability |15 retained positive outputs: Khronos0errors/100unfilteredwarnings,84 official VRM1/MToon/spring/constraint instances0errors |

The affected suites include legacy creator/source-model replay, raw metadata
and optional-tag behavior, default material, original source chains, affine
skin/export, normal/PBR/MToon consumers, V0 source export, typed UV1/matcap,
V1 source/worker cancellation and malformed retained-source replay. Full posed
skin N/T images and V1 runtime constraint/physics behavior remain outside scope.

All private frontend runs preserve source/history/cancellation assertions and
use canonical matching cleanup toolkit/bitmap libraries. Actual A770 selection
is explicit `DRI_PRIME=pci-0000_03_00_0`, vendor Intel, renderer
`Mesa Intel(R) Arc(tm) A770 Graphics (DG2)`; the lease was released. The image
consumer uses genuine pinned licensed official sample data, not a VRM0 relabel.
Creation bases remain the existing verified CC0 VRoid female/male assets.

## Header and provenance closure

Public parsed DTO changes require this rebuild: ImportedMaterial appends exact
per-role affine texture-transform/emissive-strength fields; ImportedAsset adds
an advisory version derived from validated actual immutable metadata. No Scene,
Document or normal-state layout or serialized trust/schema field was added.

The dependency audit found one standalone quality fixture using only system
`json_fwd.hpp`/ABI macros through Volume.hpp. It did not include full system JSON,
link core, or exchange JSON values with runtime. The private interface link now
uses the canonical local forward header. Its unchanged original numeric oracle
passes; the all-build check is a no-op, and **all416 output hashes remain exactly
identical**. Afterward all561 Release/191 sanitizer JSON forward/full-header
consumers are local;315 Release/77 sanitizer consumers of the new import DTO
were compiled coherently. No production JSON defect is inferred from that
standalone declaration-path correction.

Fresh configurations explicitly clear `*DOME_BITMAP_RUNTIME*`, `*NATIVEGL*` and
`*RENDERFONTS*`. They select the current canonical toolkit/toolkit-sanitize and
matching patched bitmap prefix under `domed-cairo-cleanup-b2f23ce/artifacts`.
NativeGL FreeType/HarfBuzz resolve to that prefix, preserving the root's24th
FLTK patch and Cairo manifest guards. Published431 changes only documentation/
qualification/source-used prose relative to69ca; after its semantic merge,
all runtime/API and416 output hashes remain exact.

## Remaining goal and source scopes

Full V1 painted common planes, prepared colour/normal interpretation and UV
expression transport, native HDR emission, initial out-of-unit source UV
capture/adoption, broader fitted/affine metadata, dynamic constraints/springs,
and actual synthetic-loader V1 export remain required. Unrepresentable cases
retain explicit guards. Root's separate initial-source-UV candidate is not
merged or claimed by these results. A future HDR child must not mutate this
frozen qualification tree.

The known standalone-XR112-byte Mesa sanitizer report remains separate/open;
the new desktop18 checks pass cleanly without suppression. These fixtures use
private Xvfb/D-Bus or nested native Wayland and fail-closed XR environment. No
physical HMD, system install, active desktop or display-manager action occurred.
All own proof jobs are terminal and no device lease remains.

## Publication

The publication preserves every integration/local commit and leaves runtime,
headers, CMake, tests and assets byte-identical to qualified `fc56265`. The
publication follow-up changes only these status notes and the receipt.
Fresh OneAPI/CUDA/ROCm ALL-target ABI builds now pass separately on immutable
`fc56265` in `artifacts/vrm1-vendors-fc562`, in 2041.135/2050.339/2030.903 seconds
respectively. Each profile preserves all 1,476 tracked source inputs. Read
[the build-only receipt](validation/VRM1-VENDOR-BUILDS.json) for exact commands,
logs and manifests. This adds no fresh vendor runtime or positive AMD/NVIDIA
hardware claim; earlier backend runtime proofs retain their previous scopes.
No system install or session
change accompanied publication.
