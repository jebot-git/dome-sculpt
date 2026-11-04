# Private VRM1 dynamic paint layers

Runtime `8fce073d8c19ec3d6de0640037042e1dddf2239a`, based on sealed `ed56df4`, adds a bounded expression-preserving painted VRM1 route. It is private and does not complete the VRM1 objective or qualify a latest-root application union. The exact commands, hashes, negative results and support provenance are in [the receipt](validation/VRM1-DYNAMIC-PAINT-LAYERS.json).

The old actual EditableAvatar256 worker refused a genuinely nonuniform, twice-painted source with UV expressions. Counter44785 passed 14 checks of that refusal and its atomic rollback; commit `5e7d46d` preserves its test source. The current actual worker exports that same valid source, retaining its two different skins, rigid part, original hierarchy, images, samplers, expressions, morphs and native paint/history.

The lower primitive keeps the original material, texture roles and actual UV0/UV1. A derived transparent primitive shares the exact lower positions, normals, indices, skin attributes and morph accessors. It supplies straight paint colour and coverage through a genuine unused texture-coordinate set. Original opaque extras remain on their original primitive/material, rather than becoming duplicated editor identities on the derived contribution. Base-colour expression binds affect the lower material; background shade/emission and texture binds are forwarded to the additional material. The final graph is authenticated by the unchanged native authoring package finisher.

This first route requires every UV-expression bind on the source material to share one finite nonzero scale/offset target. The added paint texture starts at that target, so its expression delta is zero, while original background roles still change. Inverse-target writable coordinates keep the paint in its native domain. The compiler checks every preset and custom bind, including inactive expressions. Different or zero targets and numerically unstable inverse carriers refuse. This follows the pinned [VRM1 expression rules](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/expressions.md). It uses no vertex-colour carrier: [MToon1 ignores vertex colours](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md).

Native contribution filtering is premultiplied, whereas a standard PNG paint texture is straight colour plus alpha. The asset worker bounds that reconstruction over every rectangle in the merged native/output texel-centre grid. A biquadratic Bernstein control hull bounds the product of filtered colour and coverage against the native bilinear premultiplied field. This is a continuous field error bound with a numerical margin, not a claim of certified GPU arithmetic. Actual independent rendered pixels remain the admission oracle. Expanded geometry, coordinates, native/decoded float fields, PNG staging and proof work are forecast before allocation; original limits remain unchanged. These bounded export arrays are neither preview CPU rasterization nor GPU readback.

## Closed evidence

| Scope | Release | Owned ASAN/UBSAN/leaks |
|---|---:|---:|
| Actual native worker, all 256/512/1024 policies, Undo/Redo, running and queued Cancel, ordered Save, Open, retry and second cycle | 44 | 44 |
| Independently parsed cgltf/PNG MToon1 consumer, poses 0/2, weights 0/.35/1, both preserved cameras, all three resolutions | 112 | 112 |
| Factory allocation/cancellation/target/refusal/retry checks | 1,556 | 1,556 |
| Current legacy static-expression / painted-authoring / painted-image consumers | 81 / 762 / 22 | 81 / 762 / 22 |

The current 36 paired views use the original cameras, samplers and material values at an unchanged four-level bound. Maximum difference is one, coverage is at least 3,660, and deliberately moving or tinting paint produces differences of 114 and 84. All 72 PNG pairs and 20 generated-container pairs are byte-identical between profiles. Graphics focus the painted source part; the actual file/native payload retains all three exported parts and dependencies. No constraint or spring simulation is claimed.

Final strict PCI03 Arc handle16005 closed0 in 3.860s: observed Intel Arc A770, 113 checks and 36 pairs pass, with unchanged source/binary/archive/input hashes and captured actual mappings. The same binary under actual llvmpipe refuses `--hardware` with exit1. The lease is released. This proves GL rendering, not compute, physical XR, or a new application UI cohort.

The factory proof covers every observed persistent C++ `new` denial (905) and every cancellation boundary (647). It also refuses actual zero-scale, conflicting-target and huge inverse-carrier cases, then retries exactly. The 256-output continuous premultiplied RGB/coverage bounds are 0.00290593 / 0.00343137; forecast scratch is 2,888,768 bytes and counted sampling work is 8,317,008. This does not replace other importer/codec allocator scopes.

All six complete affected production TUs compiled in each profile, using immutable own ed56 support. All common texel-helper production consumers were rebuilt. The final complete writer replacement removes only copied extras from the derived primitive. Current whole consumer TUs were compiled and linked against those coherent archives; 95 public headers are byte-identical to ed56. This is an explicitly matched component cohort, not a fresh whole application build. Configuration registers 631 tests and genuine fixture dependencies; that count is not an executed-suite claim.

## Preserved failures and conformance boundaries

The original affected 13-command graph had ten passes, two obsolete error-substring failures and one dependent not-run in each profile. Both actual MASK/MatCap inputs still refused correctly. Their tests now prove the real MASK material, the precise unsupported-domain refusal, and exact file/Scene rollback; all prior history, source, worker and image assertions remain. The corrected three commands pass in each profile. Other compile/setup and subset-oracle failures are preserved in the receipt. The Avatar worker intentionally retains the exported avatar subset, excluding the unrelated hidden default SDF; the full caller stays exact.

All 154 official VRM1 schema instances in the generated 20-container cohort pass. Unfiltered Khronos validation found 20 errors in five inherited raw source/normal fixtures: each has four empty `children` arrays. That combined validator invocation remains exit1. No raw source was rewritten to conceal it. Every one of the 15 actual native writer outputs separately validates with zero errors, 114 warnings and 248 infos. The two preserved previous-file sentinels are not containers. Reports retain every raw-input error, warning, info and hint. Fixture conformance/strict malformed-source admission remains a separate open requirement.

The SAME original unoptimized source-world SAN64560 sweep remains live and unfinished at this seal; its receipt snapshot is not a completed prefix proof. It has not been restarted, stopped, extended, sampled or replaced by this module's factory proof. The previously qualified ed56 optimized and Release scopes remain separate.

## Still required

This route currently admits colour paint on opaque, outline-free, nonanimated typed MToon1 with the proven separate role domain. MASK/Blend, true shared base/shade interpretation, normal/MatCap/rim/shift/UV-mask textures, incompatible UV targets, prepared/key owners, independent emission/physical paint and absolute backgrounds still need genuine owning adapters. Colour-only expression export without the admitted UV route, plain CorePBR interpretation, broader real-asset coverage and interactive/current-root integration remain required. Existing guards remain explicit.

The standard authored MToon1 view has the measured bound above. The generic core PBR fallback still declares `lossless:false`; no equivalent PBR bound or third-party VRM application acceptance is claimed. Exact native data remains authoritative, not a substitute for completing those adapters. No new public layout, Scene schema, export-option layout or frontend was introduced, and root Scene31/32/33/34/private branches were not consumed.
