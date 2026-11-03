# Plain paint GPU and affine-skin integration

Private runtime merge `9f524e3` combines the qualified plain-paint component
`3411fabe` / runtime `fc4d87f` with published main `7693d6b`. Test-only `6047a02`
adds initial immutable-mesh readiness to the existing retopo controller fixture.
The original component and its proof tree remain unchanged. This integration
is not main publication or completion of the full production goal.

The only merge conflict was CMake. Every independently declared parent target
is retained. Main's affine skin/transfer, exact joint export, retopo, scalar
brush and reverse-quality sources are byte-identical. Plain paint still uses
separate compute TUs, bounded immutable staging and complete result publication.
Existing one-argument preview/composite ABI remains; there are no class-field or
archive changes. See GPU-PAINT-COMPOSITE-WIP.md for eligibility and component
numerical, allocation, SDK and full-cost evidence.

Fresh optimized CPU and OneAPI builds compile complete native core/support,
rendering, FLTK/OpenXR applications and the selected fixtures. Fresh owned
C/C++ core/support/render/frontend sanitizer builds use O1, ASan, UBSan and leak
checks. No archived-core/header overlay is used in these integration builds.
Only the corrected retopo fixture is rebuilt afterward, in all three builds;
production source and archives remain unchanged.

The initial CPU run passed 20/21. The unchanged isolated retopo test also failed
at the four-endpoint Bridge enabled assertion. Exact-core test-only diagnostics
showed all four pressure samples had model generation/view epoch 2/1 but an
unpublished display snapshot 0/0, so the existing material-view input fence
correctly rejected them. The fixture had never waited for its first mesh.
Adding one initial wait_mesh assertion retains every original bridge/extrude
assertion. The diagnostic then records display/model 2/2 and 1/1, selections
1,2,3,4, and all 131 assertions pass. The runtime fence, bounds and topology
oracles are unchanged; initial failure logs remain in the receipt.

Final selected results:

- Corrected CPU 21/21 in 14.40s; wholly owned sanitizer 21/21 in 122.93s.
  Paint/cache, measured allocation prefixes, native gestures/live previews,
  projection, skin/affine authoring/controller/export, retopo authoring/edges/
  allocation, sculpt resampling and aggregate quality regressions are included.
- Selected OneAPI 17/17 in 20.14s on actual Intel Arc A770 PCI0000:03:00.0.
  Persistent external SDK heap denial is excluded; owned host allocation tests
  run in CPU and sanitizer builds. No CPU error fallback is added.
- Independent current/export consumer 85 checks on software GL plus Arc compute
  and another 85 on actual Arc GL plus compute. All 12 cases per run have full
  image error zero: 512/1024, lit/unlit, Opaque/Mask/Blend, exact legacy five-map
  output, source/paint retention and Save/reopen.
- Real FLTK 512 posed painting: CPU 3 accepted Save/Close + 3 EscapeCancel, and
  OneAPI the same 6 checks. The native worker is deterministically gated while
  actual client menu/status acknowledgements prove responsiveness and ordering.
  The accepted OneAPI application trace contains the production paint kernel.
- Native real-loader OpenXR 7 checks on Arc GL and compute: actual tracked paint
  controls, both GL eyes, balanced image ownership, exact nonpaint source, saved
  512 paint changes and production paint kernel/device-to-host chunk trace.
  Tracking is synthetic; no physical headset launch or session restart.
- 66 actual current paint and affine-skin GLBs pass Khronos validation with zero
  errors and 18 unfiltered NODE_SKINNED_MESH_NON_ROOT warnings. No issues are
  suppressed. All 913 captured source/application/archive/test/probe hashes
  are unchanged through device and application runs.

The component CUDA/HIP SDK compile and missing-device evidence remains scoped
to frozen `3411fabe`; this integration does not claim a fresh combined vendor
build, positive NVIDIA/AMD execution, device loss or vendor heap OOM recovery.

Source25 and private Source24/26 are not admitted here. Mandatory future union
review: non-null independent paint_sampling_domain/attachment and additional
normal metadata owners must deliberately retain their existing CPU route until
real packet adapters authenticate them. Absence of source_maps/baked, equal
geometry counts or retained origins cannot authorize their sampling basis.
The current route excludes source maps, bake, color/normal projection and
independent PBR tile owners. GPU stroke mutation, complex compositing, baking,
local meshing, remaining authoring adapters and controller trays stay required.

Exact inputs, commands, evidence and output hashes are recorded in
validation/GPU-PAINT-INTEGRATION.json. Parent owns main review/publication.
