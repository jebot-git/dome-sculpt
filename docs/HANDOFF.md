# Paged sculpt production milestone — 2026-10-30

Product merge `ee6f539` combines paged storage `a135f48` with `803479d`,
preserving published `27b14d9`, native resolution/Volume4, plane cuts and the
original-bind VRM expression fix. Checkpoint `2135d8d` contains the verified
integration; this publication changes documentation only. The full production
objective in REQUIREMENTS.md and HANDOFF-466ebd1.md remains active and incomplete.
Exact prior handoff, agent notes and validation are archived with `-27b14d9`.

## Delivered and verified

Sculpt SDF, mask, displacement and gesture journals use persistent 16³ adaptive
pages under 64 shared directory shards. Writes clone touched pages/shards rather
than complete sample maps. Owning immutable views sample and iterate without
flattening; explicit `materialize()` remains the legacy adapter. Exact float and
double values, numeric persistence, physical bands, quotas, authenticated weak
ancestry, cancellation and Undo/Redo are retained. The default tracked storage
budget is 512 MiB, including retained roots/control blocks; configure it while
producers are idle. This allocator excludes mesh/JSON/brush support buffers.

Combined CPU native/UI/render 91/91, CPU and Arc synthetic XR 150+17 each,
actual Arc compute 27 suites and GL Document22/buffers24/MToon81/masks38/
visibility25 pass. Native GUI and actual DomeD Wayland flows pass151 assertions.
Full coherent C/C++ core/render ASan/UBSan/leaks passes91/91; the later test-only
private Fontconfig reload is verified separately. See VALIDATION.md and
PAGED-PRODUCTION-INTEGRATION.md for exact scopes and retained initial failures.

Three paired CPU trials retain all25 sorted numeric-state hashes. The192-stamp
mutation workload changes from4.74–5.13s to0.091–0.130s; actual24-edit Document
publication changes from2.15–2.35s to1.09–1.18s. These are fixture measurements,
not headset pacing claims. Separate page/Volume tests exhaust12,920 allocation
failure boundaries. See PAGED-SCULPT-STORAGE.md for method and resource limits.

## Continue toward the original goal

- Integrate the private retained normal-frame Scene9/material2 work with its
  restored existing-bake preview and ordinary renderer capacity fixes. Retain
  independent GLB/VRM consumers and exact original frame/source authority.
- Integrate the private ordered seventh desktop stroke worker and generic
  Document allocation transaction hardening. Preserve six-worker behavior until
  the complete coherent merge is verified; do not replace Document wholesale.
- Complete fine-resolution empty-space picking acceleration, incremental CPU
  assembly/GPU tile uploads and sustained production performance. Full-map
  flattening is removed from storage access; full mesh assembly remains.
- Complete colored PBR, live source-UV editing/deformation conversion, remaining
  VRM/glTF features and every outstanding native authoring/device/backend row.
  Guarded partial interoperability is not full completion.

Canonical DomeD282467e/FLTK1.4.5+22 patches remains current for this validation.
Use explicit toolkit share/fltk and lib64 runtime; details in TOOLKIT.md.
Primary frame/creator WIP and unrelated local work are preserved. No system
installation or session/display-manager restart occurred. The WiVRn8f4d67d test
is stopped; user confirmed rudimentary functionality only. No newer physical
HMD or device-performance evidence is claimed, and do not relaunch unrequested.
