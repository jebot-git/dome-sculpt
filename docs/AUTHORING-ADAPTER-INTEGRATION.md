# Native authoring adapter integration

This checkpoint extends the existing native C++20 FLTK/OpenXR application. The
full original production objective remains active and incomplete. Product source
8d68dc71eb8231cfe9ab4e71b72e4e8de518a0ca combines the qualified Scene19 baseline,
post-UV prepared-key authoring, current-corner RGBA source baking, source-space
VRM0 export, desktop framing and atomic direct-caller Bake admission. Later
9e6cb1b changes tests only; runtime/header/asset/third-party bytes are identical.
Scene19/material archive5 remain current. The private Scene20/21/22 work is not
included in this checkpoint.

## Resulting authoring behavior

- Authenticated current-corner source RGBA is packed in REST order before skin
  and morph evaluation and participates in a source Bake into an ordinary target.
  The native picker offers supported sources. Nonidentity colors still require
  explicit color preparation; refusal preserves both objects and authoring
  history. Source UV1/prepared-normal sources and current-color targets still
  require their real adapters. No raw COLOR_0 substitution or silent role loss.
- Prepared-normal keys can be authored after retained UV edits. Actual source
  vertex identities unwind each owning UV recipe, author the before-model, and
  deeply replay it while preserving current paint, Clear-bake state, skin, pose
  and expression identities. Agreeing seam duplicates are supported; independently
  separating old seam vertices still needs the reserved corner-domain expansion.
- The actual CC0 female/male nineteen-part avatars retain their genuine normal
  fields through repeated one-texel UV translations, key creation/replacement,
  desktop/controller sculpting, cancellation, history, persistence and graphical
  VRM export. Fractional .95 scaling still reaches a real quality refusal and is
  not claimed as implemented. The Scene21 sampling-domain work remains required.
- Generic imported VRM0 can export its full skinned original source-node graph,
  exact authored inverse binds, edited morphs, bone names and humanoid-role
  metadata. Qualified reflected mesh nodes and positive sheared source chains
  retain graphical parity. This is not general VRM1/physics/rest-rig/static-part
  support; the separate rigid-source component and broader interoperability work
  remain required.
- Native Frame all/Home, Frame selection/Shift+F and successful-import fitting
  preserve a stable orbit pivot and existing F reset. Live GPU polygon handoffs
  and retopology drafts contribute exact bounds without geometry extraction or
  CPU readback on the UI thread.
- Direct Bake admission now stages its POD controls inside the existing queued
  Transaction before one publication. It cannot accept work and subsequently
  throw from a second caller publication. This does not claim generic
  input-thread publication-echo OOM safety.

## Qualification

The coherent optimized CPU-compute FLTK/OpenXR all-target build completed.
Fresh owned C/C++ core/support/renderer ASan/UBSan/leak checks passed 40 native
and controller suites (425.90s) and five independent graphical suites (667.50s).
The instrumented FLTK/frontend and a new physical headset run are not claimed.

Five actual Arc A770 batches passed 355 assertions: source Bake133, post-UV
keys52, real avatars87, source VRM37 and normalUV46. Actual avatar and source-VRM
images match at zero encoded levels; source Bake has maximum source-image error1
and parsed GLB error0. Post-UV synthetic MToon has maximum1 at unchanged bounds.
All87 emitted GLB/VRM files have0 Khronos errors and770 unfiltered/unlimited
warnings:701 retained non-root skins,53 legacy VRM extension-name warnings and16
runtime-generated tangent warnings. All53 emitted VRM0 files pass15 official
schemas pinned at94e82dd346fa6cf0337c4421728640e5252dd38e; each cached schema's
Git blob SHA1 is verified. The six raw source-graph input variants are excluded
from emitted-file claims and retain their original inherited diagnostics.

Actual native FLTK workflows pass9+9 color preparation/baking (normal/compact),
17+19 prepared-key controls,12 framing/orbit/paint/history and7 source-VRM export
chooser/Close/durable-Cancel/ordered-Save/retry checks. Latest private d271cf6
DomeD native Wayland/universal menus pass8. The new current-color source route
also passes11 actual-loader synthetic-controller/stereo-GL assertions; these
are not physical headset evidence.

An older polygon-color regression expected all current-color sources to remain
excluded. The new adapter deliberately offers supported sources. Its corrected
native test checks the picker plus actual unprepared-worker refusal, preserving
both objects and no history;61 assertions pass Release and fresh-owned-core
sanitizers. The initial full-run failure and all GUI setup/chooser timeout
captures remain preserved separately. The final export GUI test submits the
visible native OK control and waits for chooser closure; no product fix is
inferred from the earlier Return-only driver timeout.

Six persistent allocation-failure sanitizer suites pass in1128.87s: all858
source-field packing and4393 precommit failures preserve exact Scene/owners and
full-helper retries;13,555 reached post-UV key failures from13,556 measured
boundaries preserve the full graph and exact retry (one cache-dependent boundary
is unreached). The untruncated six-suite transcript is retained. The complete
Release317 run is STILL RUNNING. This branch is published for review, with
broad regression qualification pending; do not call it a fully qualified union
until its terminal results have been inspected and remaining failures resolved.
All component receipts remain scoped to their original frozen source builds.
