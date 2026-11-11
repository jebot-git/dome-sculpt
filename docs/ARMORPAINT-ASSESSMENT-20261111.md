# ArmorPaint painting integration assessment

Assessment uses upstream [629f8e77](https://github.com/armory3d/armorpaint/tree/629f8e77b3f19d533c60a1baa4fe9d4db9ee1163),
checked2026-11-11, and Dome Sculpt03f8edf1. Current ArmorPaint is native C with
Iron/Kong GPU infrastructure; conclusions based on its older Haxe runtime would
be misleading. Root code is zlib/libpng licensed; dependency/plugin notices must
also be retained. Exact source locations are in the
[assessment receipt](validation/ARMORPAINT-ASSESSMENT-20261111.json).

ArmorPaint is a strong candidate for reusable GPU painting, material and baking
work. Its masks, hidden layers, groups, fill materials, blend modes, UV1, skin
blobs and8/16/32-bit GPU targets already exist. They should not be described as
missing. Reusing this work could reduce the amount of advanced painting code
Dome Sculpt must develop. This is a source-based engineering assessment; no
runtime, performance or lossless interchange result is established yet.

| Integration boundary | Evidence and effect |
| --- | --- |
| Existing Dome frontend | Preserve native FLTK/OpenXR, bitmap appearance, universal menus and keybindings. Stock Linux ArmorPaint uses Vulkan/X11 and optional GTK dialogs; embedding its stock window does not meet this architecture. |
| Hosted engine | Upstream layer/render/history functions depend on global project/context/UI and its own event loop. A multi-document hosted engine requires explicit extraction and host callbacks; no ready independent library was identified. |
| GPU interoperability | Current upstream backend selection has Vulkan/D3D12/Metal/WebGPU rather than Dome's GL/EGL path. Port selected kernels to existing resource ownership, or prove same-device external-memory and synchronization before choosing a Vulkan-hosted backend. Avoid per-frame CPU texture readback. |
| Paint data | Packed base/opacity, normal/material-ID and AO/roughness/metal/height outputs differ from Dome's independent14-byte planes, prepared PBR colour and physical RGB emission/FLOAT coverage. Float32 support alone does not make those semantics interchangeable. |
| Original material roles | Preserve original paint/role coordinates, artist N/T, typed PBR/MToon interpretation, face membership and source ownership. Upstream paint mesh UV/skin support does not prove this roundtrip. |
| Document and history | Keep native layers, masks, hidden data, exact cancellation/Undo/Redo, source/generation admission, ordered saves and VRM/rig/morph metadata authoritative. Do not route an entire avatar through .arm or a flat texture export merely to paint it. |

The practical next step is a UI-free GPU-tool prototype behind explicit native
mesh/UV/brush/source/destination views. It should use a genuinely reusable
upstream tool family and independent mask/FLOAT coverage oracles, with source
provenance and no product-default change. This tests whether extraction is a
better foundation for a hosted painting backend. A successful small kernel alone
will not establish full ArmorPaint engine integration or complete the required
Substance-like workflow. Broader material graphs, layers and baking remain in
the full objective whichever backend is chosen.
