# Native paint and current key frame ownership — private Scene36

Runtime source is af8def66eaebbc4c7241015adc55e524bb3973cd. This milestone adds
a genuine independently replayed current normal-frame capture to native32 paint,
then uses the real derived-normal key factory for Create/Replace. The current
normal origin and historical writable paint basis are separate immutable owners.
Neither grants the other colour, normal-projection or sampling authority.

The append-only holder is `SourceMaterialBindings::native_paint_frames`, pointing
to private immutable `detail::NativePaintFrameSource`. Its accessors are
`before_paint`, `before_material`, `origin`, `base_frames`, `frames`, `keys`;
`validate_binding(false)` checks bounded sealed scalar and strong-owner identity,
while `validate_binding(true)` independently replays the captured frame source.
The node itself is factory-private/noncopyable and has a genuine owner lease.
The actual current field still requires the independent frame/key geometry proof.
Unrelated material style edits remain legal.

The first-key `uv_origin` belongs to actual current normal capture geometry.
`native_paint_sampling` retains the older writable paint geometry and exact
triangle-corner correspondence. Export must join the full frame and paint
corner split, preserving ordered triangle ownership. Generated normal UV0 is
the capture-domain UV0; it cannot silently mean current UV0 or paint UV.
Root owns the separate normal-frame/export policy adapter.

Conditional Scene36/material18 stores a bounded native-frame-source pool with
record1 `{version,paint,before_binding,origin,base_frames,frames,key_derivation}`.
Before-material references are earlier, graph reachability/orphans/cycles/budgets
remain mandatory, and origin/frame/key owners must match the current holder.
Before-only feature reachability requires36. Existing <=35 semantics and material
versions remain unchanged when the node is absent. Other actors' baked/canonical
contexts derive from actual linked flags/pools, not numerical outer version.

Fresh own Release and whole-owned sanitizer app/core/render/support plus seven
consumers qualify this appended class ABI. Final Release7/7 and SAN7/7 pass;38
native Scene outputs match byte for byte. Exact sources, configurations, binaries,
logs, preserved failed phases and scope are in
[the receipt](validation/NATIVE-PAINT-FRAME36.json). The sanitizer build support
is immutable at `build-native36-sanitize`; it is safe for explicitly scoped
matched-source component consumers, not a claim of a fresh whole-project run.

No native36 morph-export image, GUI/controller, complete new36 fault-sweep,
post-key UV, inverse-row UV-source Restore or main-admission claim is made here.
Those remain actual required work. Palette's independent canonical/F32 paint37
owner must be composed semantically with this node in a future fresh ABI union.
