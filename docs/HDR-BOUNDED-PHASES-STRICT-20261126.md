# Original HDR strict dense coverage and endpoint deadline — 2026-11-26

The unchanged 725 product with test-only 9ee fixture passes the full-size native
phase and all four independent dense phases under ASan/UBSan with leak detection.
All original 73,530,600 positions per normal role and all 73,530,600 HDR positions
pass the unchanged bounds. The original Document/history endpoint then reaches
its 900-second deadline, so complete grouped strict acceptance remains incomplete.

Invocation `5bd7f71d1f2f49baa0d6f4b36d3f269f` preserves the original 900-second,
4-GiB per-case limits and no virtual-AS cap. Native passes 43,280 controls in 608.476s,
reproducing the immutable original 122,856,849-byte native SHA exactly. The four
dense phases pass 4,109 controls each in 461.341, 501.444, 450.559 and 502.502s. Each
covers its full three triangles and 18,382,650 positions per normal role and HDR,
with all 25 original offsets and both normal roles. The independently checked
ranges cover [0,12) exactly, with no hole or overlap. These 59,716 phase controls
include repeated common checks, not additional unique functional tests.

Actual executable/maps attest the live sanitizers in each passed process. Source
and provider drift, owned survivors and DRI/Mesa use are absent. Original inputs,
sourceJSON and cross-phase native hashes remain unchanged. Every passed process
completes its leak check. These CPU-only phases never acquire the graphics lease.

The final endpoint exits -15 after 900.607s, with an exact PID/start/pgrp deadline
witness; wrapper exits 1. Peak owned RSS is 2,894,045,184 bytes, below 4 GiB. Its
prospective phase clocks record load 236.861s, encode 180.770s, native fresh
reopen/compare 390.891s and Document baseline 3.792s. It times out during the actual
Document Bake/immediately ordered Save. The final worker comparison, Undo/Redo
and source-preservation end check are not reached. No sanitizer diagnostic is
reported, but this terminated process is not a successful sanitizer/leak result.

The original status remains a failure. A separate read-only postfailure audit
verifies all four completed dense logs against their original prospective plan
and authenticates the failed endpoint's captured live provider maps. It does not
rewrite the original status or replace the missing endpoint checks. Terminal
properties/journal are captured before stopping the owned completed unit.

This old-source evidence is separate from the original monolithic 900-second
failure, the newer channel-algorithm differential proof and current 0813 application
qualification. Its measured load/validation/persistence costs justify investigating
product performance while preserving every source, sample, receipt and quota.
Physical/public display behavior and system installation are outside this scope.

Machine receipt and compressed proof:
`docs/validation/hdr-bounded-phases-strict-20261126.json` and matching `.tar.xz`.
