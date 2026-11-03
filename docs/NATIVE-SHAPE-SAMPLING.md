# Native retained-paint shape previews

Freehand shape replacement after retained Paint24 UV editing rebuilt its visibility query from an incomplete `SurfaceRange`. The first query owned the paint attachment, normal proof and logical-corner coordinates. The refresh lost them and correctly failed the visibility guard. Production fix `e7a3c7c6931057a799dbdfcdfeb75b5583487d48` captures the full initial range and reuses its immutable sampling owners and coordinate streams while replacing only evaluated geometry. Changed triangle correspondence remains refused.

The native query preserves logical triangle order: `place_mesh` transforms positions/normals without reversing indices. Its range therefore records `paint_winding_reversed=false` and retains the existing reflection/culling authority. Packed render geometry separately owns its genuine EBO permutation. Actual alpha-mask picking checks visible hits and transparent holes in both representations; a fabricated corner permutation is still rejected.

## Completed evidence

The native regression creates a real partial Bake, applies a fractional `.7` UV scale and 90-degree turn through KeepPaint, then creates and selects a new shape target. Ordinary and reflected/sheared placements each exercise direct native and `SpatialFrame` controller input. All four cases require three acknowledged, geometry-changing preview prefixes, retained query attachment/proof/corner streams, exact source owners during acceptance, ordered Save/reopen, one global Undo/Redo, and changed-preview Cancel or focus loss.

- Old production negative: the first refreshed sample fails with `Visibility paint range omitted or fabricated its own sampling attachment`.
- Fixed optimized matched overlay: all 84 checks pass.
- Whole-owned C/C++ core/render/support O1 ASan/UBSan/leak overlay: all 84 checks pass, with no sanitizer diagnostic. Its final success line was written 2961.46 seconds after native-process start; exit was observed by 2981.23 seconds. The wrapper did not record an exact elapsed duration.
- Independent actual FLTK final harness: 35 ordinary and 37 compact checks pass on the unchanged supplied app/inspector. It explicitly selects the existing target through its visible native dropdown, proves unchanged Apply adds no Undo entry, requires only that target to change, and preserves source, paint, skin, pose, Undo/Redo, Escape cancellation, Save/reopen, graceful Close and Ctrl+W modal cancellation. See private GUI checkpoint `fbd3c32eca2df9734d6b2c26fbcfbd0ac68400f1` and `validation/RETAINED-KEY-UI.json`.

The original sanitizer fixture's 20-second yielding observer failed before its first preview acknowledgement, without a sanitizer diagnostic. That result remains recorded. A separate frozen source changed only the observer to 180 seconds with 1ms polling and explicit progress logging. It retained every correctness assertion and completed all 84 checks. The committed test is byte-identical to that completed source. No production timeout or tolerance changes.

The total CTest budget stays 1800 seconds for uninstrumented builds. Explicit address/undefined-sanitizer compiler flags select 6000 seconds, justified by the measured deep source/quality/Save/history replay. Fresh configuration-only checks verified both generated registrations.

## Build and scope

This component used matched archive overlays, not a fresh whole-project or frontend compilation. Exact immutable 9050 core/render/support archives were copied locally. Their old native-worker member was removed from a local core copy and replaced by this tree's rebuilt native TU; own test/private/public headers match that source. Dependency files do not read mutable master sources. The supplied optimized app similarly combines exact staged frontend/render/support inputs with the one rebuilt worker TU and unchanged inspector. No header, class layout, public API or schema changed.

The final before/after audit has 147 checks and zero mismatches, covering frozen headers, staged app/archive inputs, dependency paths and source/binary hashes. Parent master rebuilds do not mutate those copies. Exact commands, hashes, environments, original negative results and process timing are in [the receipt](validation/NATIVE-SHAPE-SAMPLING.json); raw local evidence remains under `artifacts/native-shape-sampling-9050/artifacts/`.

A test-only stamp wrapper crosses the production 30ms preview throttle to force every sample through refresh; this is correctness evidence, not a UI latency benchmark. No new GPU/dispatch, physical-headset, allocation-fault or broad whole-project qualification is claimed. Existing independent-normal, colour, seam and unsupported topology guards remain intact. The broader authoring goal is still active.
