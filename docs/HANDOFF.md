# Authenticated local sculpt meshing, GPU buffers and plane cuts — 2026-10-30

Combined application source `cf89a81c45d36c1ed538a3f24bbab56869d25e2f`
preserves both `42b2ff3` local-meshing/GPU history and remote `a8af3f8` native
plane-cut history. Test-only `91ceb04` moves a rendering fixture clear of the
cut kernel's explicitly unsupported vertex-touching planes. Test-only `8b8dc60`
adds actual native UTF-8 clipboard acknowledgement before restoring the test X11
keymap. Product source remains identical to cf89a81. The full production goal in
[REQUIREMENTS.md](REQUIREMENTS.md) and HANDOFF-466ebd1.md remains active/incomplete.

Read ../AGENTS.md, AGENT-HANDOFF.md, VALIDATION.md, TOOLKIT.md,
AUTHENTICATED-LOCAL-MESHING.md, GPU-BUFFER-REUSE.md, LOCAL-SURFACE-MESHING.md,
SCULPT-MASKS.md, PLANE-CUTS.md and the existing worker/material/frame notes.
Exact incoming handoffs/evidence are archived with -a8af3f8; earlier -9a0cfc4,
-55dafa0 and other histories remain retained. Their environments and scopes
remain separate from this workstation's new results.

## Changes in the combined application

- Actual changed SDF samples create bounded immutable ancestry events using weak
  map owners and exact shared ownership. The worker authenticates local tile
  updates, safely rebuilding for missing/expired/branched history or limits.
  Its cache retains only typed SDF sample ownership plus voxel/physical band,
  excluding displacement/mask/history maps. Cancel restores exact old ownership.
- The Document mesh worker caches per object/generation; mask/control-only edits
  retain extraction and assembled geometry. Ordered visible source/deformation/
  transform/material-frame dependencies produce a strong geometry identity.
  Empty objects, source bindings and canonical corner expansion are accounted for.
- Static position/normal/UV0/UV1/tangent and index buffers are separate from
  weight4/protection6 scalar streams. Mask-only changes transfer4 bytes/vertex;
  material/texture-only changes need no geometry transfer. A second eye/navigation
  reuses buffers. Scalars still upload on new snapshots; geometric edits still
  assemble/copy/upload the complete packed mesh. No per-tile GPU claim is made.
- Cut caps receive their own immutable geometry identity; changed cap geometry
  cannot inherit source buffers. Cap-free clipping changes uniforms and retains
  source geometry. Original protection and cap -1 sentinels remain exact. Cancel
  restores exact source pixels/key; mapped paint never includes display overlays.
- Exact representable marching intersections and bounded adjacency repair retain
  legitimate tiny faces; mesh inspection uses double-precision exact collinearity.
  Unsupported repair/contour cases fail explicitly, preserving original objects.
- Incoming Cut17, Masks15, Skin14, Retopo16, six-worker shutdown, cut cancellation/
  retry/ordered saves, uniform mask interpolation and mask-only-empty geometry
  exclusion remain intact. Scene8 and Volume2/3 compatibility and current sourceUV
  guards are retained; no private Scene9 or Volume4 authoring was silently adopted.

## Verification

Fresh optimized CPU FLTK/OpenXR and complete OneAPI FLTK/OpenXR builds use current
canonical DomeD282467e (22 patches, FLTK1.4.5, lib64). This workstation is an
Intel i7-12700/GCC16.2; DPC++7.2 targets actual Arc A770. Details and final results
are in VALIDATION.md; no ADL-N/toolkit-authoring result is relabelled as local proof.

- CPU80/80 native suites pass484.25s; whole-core C/C++ plus renderer ASan/UBSan/
  leak-detection24/24 pass345.57s. Actual Document-to-GL22 assertions include masks,
  textures, material, geometry, transforms, caps, clipping and exact cancellation.
- Arc17/17 selected native suites pass7.80s; real-loader syntheticXR150/150 pass,
  plus hardware Document22/buffer24/MToon81/mask38/visibility25 fixtures. The compute
  oracle retains184 independent endpoints and224 batches/445088 samples, maximum
  error8.9407e-08, explicit no-GPU rejection and no CPU fallback.
- CPU real-loader syntheticXR150/150 passes with complete=true/failed=0; the
  matching Arc report also completes150/150. NativeGUI/compositor125 assertions
  pass: general53, cut12+compact6, masks17+compact5, skin11, Unicode4, actual
  DomeD Wayland masks9+general8. Normal/compact cut screenshots were inspected.
- An initial Unicode GUI run lost a character while its temporary keymap was
  restored without native-client acknowledgement. An isolated unchanged run
  passed; the strengthened4-check fixture now waits for exact native clipboard
  content and passes while retaining all original assertions. No product Unicode
  behavior or expected bytes were relaxed. Earlier failed logs remain retained.
- The actual WiVRn headset test was stopped at the user's request. Rudimentary
  operation is confirmed only for8f4d67d; recovery/report remain under
  artifacts/xr-milestone-8f4d67d. Do not relaunch a physical HMD without request.

## Continuing work

1. Integrate the separately verified resolution/Volume4 authoring branch with
   Cut17/six workers after fresh combined GUI/compositor/synthetic/sanitizer proof.
   The agent is reconciling on cf89; keep its frozen earlier proof scoped.
2. Validate and integrate adaptive paged field/mask/history storage. The prototype
   removes whole-map COW and growing stroke-history copies; first exact-state
   sustained replay is promising, but repeats/allocation faults/SAN remain pending.
3. Finish raw source/canonical normal-frame runtime, sourceUV and glTF/VRM exports.
   Independent consumers exposed a genuine multi-primitive VRM expression remap
   bug now being fixed privately. Keep source identities and explicit refusal for
   representations not yet proven correct; guard removal is not full completion.
4. Move expensive desktop mutation off the UI thread, finish bounded assembly/
   tile GPU uploads, then production-size sustained strokes and hardware pacing.
5. Preserve the full remaining matrix: projection/stencil/texture sets, advanced
   bake/retopo/cuts/procedural tools, fitting/weights/shapes, full import/export,
   controller/stylus/3D-mouse workflows and all required backend/device evidence.

Primary checkout still contains unrelated private normal-frame/creator files.
Preserve them, all backup stashes and all local commits. Build regular-user
Release; no installation or active-session/display-manager restart was performed.
