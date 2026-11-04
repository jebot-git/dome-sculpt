# VRM1 colour-only expression paint layers

The private `fa4f1fd` runtime extends the bounded dynamic paint adapter from
`c2f1705` to genuine base-colour expression bindings without any texture-transform
bindings. The original lower material retains its factors, textures, samplers,
source IDs and expressions. An independent standard blended primitive carries
actual nonuniform colour paint with identity writable coordinates. Its base
factor is not expression-tinted, and no dummy UV expression is emitted.

This is an additive typed adapter, not static expression baking. Existing UV
bindings still require their actual common finite nonzero target; all previous
source/paint authentication, opaque/role/precision/resource guards remain.
No public header, C++ layout, persistent schema, export option or frontend changes.

## Counterfactual and qualification

Commit `c76ddf1` preserves a genuine authored colour-only INPUT, formed by removing
only UV expression bindings from the qualified background fixture while keeping
the original BIN and other metadata. It passes six pinned official schema
instances and Khronos validation with zero errors. After two actual nonuniform
native paint strokes the old worker refuses colour-expression export; all 14
checks preserve the previous output and complete native source/paint state.

Qualification rebuilds both complete changed production TUs and complete consumer
TUs over copied immutable, matching `c2`/`8f` owned Release and sanitizer support.
It does not reuse root Scene35/anatomical headers or claim a fresh application,
full build, UI proof or complete regression. The two profiles passed:

- Actual colour-only and existing UV workers: 44 checks per case, including
  256/512/1024 output, one paint Undo/Redo, ordered Save, Open/second cycle,
  running and queued Cancel, quota rollback and atomic publication cancellation.
- Colour-only factory: all 886 persistent C++ allocation-denial prefixes,
  all 647 cancellation points, four target/authority refusals and retry: 1,538.
- Existing UV factory: all 908 prefixes, all 647 cancellation points and the
  same four negative controls/retry: 1,560.
- Independently parsed rendering: 112 checks and 36 pairs per case, across three
  resolutions, two poses, three weights and both original camera views.
  Every pair has maximum difference 1 at the unchanged bound 4; wrong paint
  coordinates and tint remain sensitive controls.
- Existing static expressions 81, painted authoring 762 and painted images 22.

Strict PCI03 A770 execution passed both cases: 226 checks / 72 independent pairs,
maximum 1. Actual llvmpipe with the same hardware flag refuses at the device gate.
Process maps and mapped-library hashes are captured. Hardware handle 2057 is
terminal and the lease is released. These are graphics checks, not GPU compute,
physical HMD or third-party VRM application evidence.

All eight actual writer containers pass 72 pinned official schema instances and
unfiltered Khronos validation: zero errors, 32 warnings, 144 infos, zero hints.
All 144 PNG and 19 native/container file pairs are byte-identical between Release
and sanitizer profiles; the latter cohort explicitly includes the two unchanged
queued-file sentinels and authored colour-only INPUT. Configuration registers
634 tests with a real three-test colour fixture graph; no full CTest run is claimed.

See [the exact receipt](validation/VRM1-COLOUR-EXPRESSION-LAYERS.json) and
[the original counterfactual](validation/VRM1-COLOUR-ONLY-COUNTER.json) for source,
archive, binary, input/output hashes, commands, timings and preserved setup
negatives. The validator report routing correction changed no source or result.

## Remaining requirements

The adapter still guards MASK/Blend, shared base/shade interpretation, normal,
MatCap, rim, shift and UV-mask roles, independent prepared/key/native/physical
owners and unsupported UV targets. Plain CorePBR equality and an actual external
VRM application consumer remain separate work. Posed rendering does not claim
physics simulation or posed constraint export. Current root semantic integration,
application/UI qualification and the full original objective remain unfinished.

The original unoptimized 66,187-prefix sanitizer job remains the SAME live frozen
handle 64560. Its receipt records an observation, not completion; these complete
factory proofs do not replace it or relabel it as sampled success.
