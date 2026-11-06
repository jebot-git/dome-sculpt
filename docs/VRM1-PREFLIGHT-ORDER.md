# Current VRM1 preflight contract regression

Exact frozen d1f40159 Release built fourteen whole test units and fourteen new
executables against its own providers. All 35 planned commands executed: the
background producer, all 32 Normal/Shift/Rim MASK roles and uniform-role authoring
passed. Only vrm1-targets-faults failed its precise-refusal assertion. Original
source, ELF, actual generated input and failing log remain intact.

A separate logging-only whole test TU used exact current-d1 headers, flags and
providers. The original work-cap control observed the expected invariant-role
error at 133/max 133 callbacks with the exact scene unchanged. The next control,
max_image_pixels=1, observed exactly `VRM1 expression-layer pixel limit exceeded`
at 1/max 3 callbacks with the exact scene unchanged. The stale diagnostic alone
failed. Commit ff734178 explicitly moved this existing size guard before
callback-bearing source-image, Normal and additive scans; other role fault suites
already expect the earlier pixel diagnostic. Production correctly refuses early.

This correction preserves the production code and every authority/size guard.
The test requires the exact current pixel diagnostic and tightens max 3 callbacks
to max 1. Every original exhaustive allocation/cancellation prefix, work budget,
unrelated-material/current-owner and retry control remains in the default test.
Named diagnostics now expose each actual refusal. Added controls retain the same
real source, immutable images and native 64 paint packet:

- Output-only cap: native 64 fits; requested 256 exceeds a 65535-pixel policy.
- Independent source-image cap: native 64 and requested 64 fit, while the real
  original 128 shade image exceeds a 16383-pixel policy. The exact invariant-role
  error occurs after3/max 3 callbacks.
- Exact 65536-pixel endpoint reproduces original image, coordinates and work.
- Invalid pixel cap plus zero work budget rejects before source-role scans with
  the exact diagnostic and1/max 1 callback.
- Immediate cancellation retains its exact diagnostic, one callback and exact
  unchanged scene, including with the invalid pixel cap.

Native-only isolation is unavailable: the genuine native grid is 64 and 64 is the
compiler's minimum requested output size. The test records this; no source,
image or packet is resized/relabelled to manufacture a distinct proof.

The corrected whole TU was separately compiled/linked using exact current-d1
flags, headers and providers, then passed its COMPLETE default invocation:
940 allocation prefixes, 909 cancellation prefixes and 1,862 total checks. The
original failed ELF and logging-only diagnostic remain raw and unchanged. All
3,757 captured source/SDK/config/output pins remained unchanged, including the
original 282 Release and 282 sanitizer outputs. Both isolated diagnostic/corrected
outputs use 11,239,424 allocated bytes, under 96MiB including 64MiB prospective.

An additional `vrm1-targets-preflight-order` CTest registration reuses the actual
uniform-role producer fixture. Its explicit --preflight-only mode omits expensive
prefix loops; the default full-fault invocation still runs every prefix. This
focused entry does not replace complete fault qualification. The corrected
focused mode is registered but was not executed separately by this proof.

The source child is based on 57e42265 and remains separate from the active frozen
57e cohort. Current-d1 runtime qualification of this test-only correction does not
qualify new 57e layouts, full ALL, sanitizer, hardware, physical XR, main admission
or the full goal. Read validation/VRM1-PREFLIGHT-ORDER.json for exact receipts.
