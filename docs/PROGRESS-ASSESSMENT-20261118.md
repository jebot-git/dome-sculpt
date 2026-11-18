# Dome Sculpt progress and pause assessment — 2026-11-18

The suite has a working native C++20 FLTK/OpenXR authoring baseline and a newer
combined Release candidate. It remains in integration and production hardening;
the full original objective is incomplete. This round closes for the user's
requested pause. Preserve all scope in [REQUIREMENTS.md](REQUIREMENTS.md).

## Qualified results and their limits

- Main's admitted application remains product `1bc13125`, with its scoped
  Release/sanitizer, public desktop, synthetic XR and Wayland evidence. Newer
  production components are on explicit branches; documentation merges do not
  admit their code. See [current admission](NORMAL-RUNTIME-ADMISSION-20261117.md).
- Combined candidate `05f5d1cc` builds an optimized application and selected
  fixtures. All 15 selected Release cases pass, totaling 265,913 checks, with
  loader/version checks passing. This is not the complete all-target regression
  or public-application qualification. See
  [coherent Release result](CURRENT-COMBINED-SAMPLER-RUNTIME-20261118.md).
- Its strict ASan/UBSan build passes. Runtime v2 passes the 55-check sampler
  regression with live sanitizer witnesses, then the full-resolution advanced
  normal workflow exceeds its unchanged 900-second deadline (900.127 seconds,
  SIGTERM/-15). Peak sampled owned RSS is 2,625,769,472 bytes, below 3 GiB.
  The subsequent cases do not run. Full strict acceptance is pending; a timeout
  is neither a positive sanitizer result nor proof of a numerical defect.
  Invocation `695aa961493d49299c1c84da387f9870` has terminal/journal capture,
  no owned survivors, and an independently available device lease. Raw evidence
  is retained under `artifacts/authoring-current-sampler-sanitize-20261118/native-selected-v2/`.
- Filtered normal fidelity passes its scoped native, independent software and
  Intel Arc matrices. Independent HDR preview passes 4,422 checks on software
  and Arc; source coefficients remain exact. HDR/source export has separate
  Release and strict component evidence plus independent file validation. See
  [Arc preview](HDR-INDEPENDENT-GPU-ARC-20261118.md) and
  [source export](RETAINED-HDR-SOURCE-EXPORT-20261118.md).
- The VRoid creator passes native workflow/export checks and a separate older
  component's 14-check desktop workflow. The current combined build's public
  desktop and XR accept/cancel/focus workflows remain unrun. A test-runtime
  timestamp overflow is repaired separately; its regression and current XR
  workflows are prepared, not qualified by compilation.
- Sculpt fix `3aa7aea8` lets four independently edited full-size objects fit the
  original 512 MiB retained-storage quota: 327,634,597 bytes and 1,039,800
  triangles. The original second-edit failure is preserved. Follow-up
  `6d3ca25e` builds the native cold-open/recovery and sustained independent-edit
  fixtures; their runtime and strict qualification remain pending.
- HDR context fix `51488b3c` narrows retained provenance to real source
  dependencies without raising the 32 MiB quota. Its Release build/audit passes;
  the prepared closure and genuine 2048-target runtime probes remain unrun.
  The already-running strict compile-only job now also finishes successfully:
  21 complete translation units, refreshed owning archives and 16 links, with
  zero input drift or unknown dependencies. Neither build is a runtime pass.

## Main remaining goals, in order of dependency

1. Close current integration: diagnose the strict full-resolution deadline,
   run remaining strict cases and current public desktop/XR workflows, then
   merge qualified HDR/sculpt fixes into a new frozen candidate and complete
   coupled regression. Preserve prior source/build evidence and original bounds.
2. Finish production-size multi-object sculpting, local meshing, layers/masks,
   recovery, topology/paint preservation, polygon editing, remesh/retopology and
   complete UV workflows, including sustained independent authoring.
3. Complete layered painting, projection/stencils, texture sets and normal/AO/
   material baking across source, UV and deformation cases. ArmorPaint remains
   assessed/prototyped; no hosted ArmorPaint engine is integrated.
4. Complete CC0 VRoid-only humanoid creation, fitting, reliable automatic
   weights, rig/weight/morph editing and required import/export coverage:
   FBX/OBJ plus glTF/VRM expressions, metadata, physics and constraints.
5. Validate complete authoring with physical XR/controllers and other required
   input devices, session/device loss and sustained performance. Extend viable
   GPU workloads. CUDA/ROCm builds and missing-device refusals do not establish
   positive NVIDIA/AMD execution; current Arc evidence has its own scoped limits.
6. After basic production targets, implement controller tool/function/selection
   trays and spatial node-based editing/history. Preserve the user-accepted
   direct selection, hierarchy, multiselection and translucent overview.

All future builds must keep canonical DomeD FLTK, universal-menu, appearance,
bitmap-font, keybinding and Wayland conventions. No new installation, physical
headset test or session restart occurred in this round. No completion percentage
is assigned: feature implementation and production qualification are different
gates, and substantial requirements remain partial.
