# Resumption entry point — verified cut and sculpt protection integration

Read HANDOFF.md, ../AGENTS.md, VALIDATION.md, SCULPT-MASKS.md, PLANE-CUTS.md and
REQUIREMENTS.md. The full production goal is active and incomplete. Current
branch: work/cut-sculpt-integration, repository /home/blux/de-xr/dome-sculpt.
The parent is not a repository. No agents were spawned for this stage.

Application merge 4755e62 preserves55dafa0 / source 2ec8a5a and9a0cfc4 /
source 20c132d. Harness-only3e602ae adds 90s cold BDF startup; interaction waits
remain 30s. All 182 product hashes match 4755e62. Exact old handoffs/validation are
archived under -55dafa0 and -9a0cfc4. Fetch/status/collaboration inspection must
precede overlapping work, and remote histories/unrelated local edits must survive.

Both mask and cut workflows are integrated: Masks page 15, Skin page 14, Retopo page 16, Cut page 17.
Keep the shared bounds, parameter ranges, row counts, menu state and controller
ownership consistent. Cut cap vertices append sculpt_masks=-1; original source
feedback stays intact. Mask controls/bulk work/pressure cannot bypass a cut draft.
Original Volume v3 mask/control data survives Cancel/Apply/Undo; display protection
must not enter shell paint or remain on generated mesh outputs. Geometry and
bake-source eligibility exclude mask storage without actual sculpt samples.

Uniform global fields and constant sparse cells now sample exactly. Keep nested
difference interpolation and position validation before the uniform shortcut.
The original exact .75 cut assertion exposed real drift and was not weakened;
independent fractional and affine-field cases cover the correction. Cut suite 68
includes protected/mirrored source, ordinary cap attributes, white paint transfer,
source isolation and exact Undo. All four geometry tools and backend arithmetic
remain upstream's implementation; no selected GPU backend is silently replaced.

Preserve immutable render snapshots, source/session/generation guards, current
preview_request matching, coalesced worker updates and cache disposal outside
model/input locks. FileJob::cut_session owns accepted publication. Cancel during
that lease requests worker cancellation and retains the draft until ACK, so retry
and one global Undo remain exact. wait_asset drains accepted cut work; standalone
final save discards only remaining unconfirmed retopo/cut drafts. Keep the sixth
worker predicate-mutex handshake and deterministic shutdown/fsync regressions.

Public Volume/Brush/Sample layouts are coherent with this source. Do not link
older archives. Combined Scene v8/legacy codecs and Volume v3 quotas are preserved.
Keep immutable material origins and SourceUV normal-frame/export guards; private
local-mesh/frame/creator prototypes and old backup stashes are not merged here.
Do not relabel legacy UvDerivative normal pixels as original-frame data.

Verified:75 non-XR Release suites625.60s +registered XR372.17s /150 checks;
23 whole-core sanitizer suites320.94s;125 GUI checks; actual Intel ADL-N MToon 81,
mask/mirror 38 and spatial renderer. Hardware graphics is separate from physical
XR/input and compute. No system install/session restart/new compute execution.
All stage handles are terminal; no compiler/test/app jobs remained. The failed
concurrent102-check XR trace and corrected GUI-launch/startup diagnostics remain
recorded. Final XR150 passes with the unchanged 40-second client timeout.

Use build-cut-sculpt, toolkit-authoring/lib, and build-sanitize. Older build/ and
build-cut-integration binaries are not current. Exact copied GUI scripts and the
correct build symlink are in artifacts/cut-sculpt-ui. The product freeze manifest
and logs are ignored local artifacts, unavailable in fresh clones; reproduce with
HANDOFF.md's explicit toolkit flags. Canonical BDF/helper hashes match.

Next: group/multi-material repeated cuts and the full remaining requirements.
The active-object shell/cap workflow already exists; do not reimplement it as a
kernel-only plan. General Booleans/contours, compaction/caching, durable recipes,
quality, new material/rig/morph transfer and recovery are still open. Keep native
C++20/FLTK/EGL/OpenXR, bitmap Fontconfig/FreeType and DomeD universal menus,
modal history/Cancel/Close and controller slider/wheel conventions.
