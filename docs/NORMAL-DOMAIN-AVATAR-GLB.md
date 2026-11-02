# Private retained-normal avatar and transition consumers

This test-only checkpoint extends f822358 without changing product source,
headers, schemas or runtime ABI. It qualifies current-state core-PBR GLB pixels
for actual CC0 female/male 19-part avatars and explicit normal-detail transitions.
It does not admit Scene21 into production or complete the remaining full goal.

The actual default avatar core fallback is unlit. Its original independent
whole-avatar image matches at error zero, but disabling normals changes zero
pixels, so the strengthened sensitivity assertion correctly fails. That log is
preserved separately. The positive fixture explicitly edits native material
lighting to lit before preparation and genuine geometry-ray normal Bake.
Raw assets, typed roles and has_mtoon remain intact; the exporter performs no
material rewrite to manufacture sensitivity.

The edited-lit female/male fixture passes 89 assertions over 18 complete GLBs:
retained .95 UV Scale, native Save/reopen, three mixed-joint poses and morph
weights 0/.35/1. Each GLB keeps all 19 parts. Actual Document PBR-mode rendering
matches independent cgltf/PNG/GLSL output within one channel value in software
and sanitizers, exactly on PCI03 A770. Disabling the genuine FaceSkin normal
field changes pixels by 22/37; sampling the wrong atlas remains visibly wrong.
Original source, artist frames and the complete original scene remain exact.

The second fixture passes 51 assertions over 36 GLBs for generic and typed
sources. RGBA-only Bake can expand common colour coverage while the independent
normal event and normal coverage stay exact. Clear explicitly selects intrinsic
normal sampling. Both routes survive restart and match the actual PBR viewport
through the same poses and morph weights, exactly in software, sanitizers and
on A770. No new positive-normal rebake or metadata reinterpretation is admitted.

Each added fixture TU was compiled with the exact optimized and sanitizer flags
against the frozen f822 fresh coherent core/support/renderer archives. All
product source/header bytes match that source. This is an instrumented test
extension using those owned archives, not a new whole-suite run. Device hashes
cover source, binaries, libraries and actual raw models before/after. Two normal
CMake targets are registered; their generated compile flags equal the actual
fixture compile flags. All 54 successful GLBs pass unlimited/unfiltered Khronos
validation with zero errors and 378 skinned-parent warnings.

The pre-UV colour/alpha failure, native third-UV reimport refusal, guarded VRM
sampling compiler, Paint24 composition and broader affine/corner-normal routes
remain required. These tests add no new GUI, controller or human headset claim.
See [the exact receipt](validation/NORMAL-DOMAIN-AVATAR-GLB.json) and the preserved
local artifacts/normal-domain-avatar-f822358/artifacts logs and models.
