# Live weight feedback and material-aware picking

This stage extends e65db2a on work/live-weight-feedback. The full production
objective remains incomplete in REQUIREMENTS.md. Read AGENTS.md and
WEIGHT-AUTHORING.md. The preceding integration's exact records are archived as
HANDOFF-e65db2a.md / VALIDATION-e65db2a.md; keep all earlier histories and original
creator archives. No agents were spawned here.

## Implemented

- Desktop and XR pressure gestures produce worker-coalesced immutable heatmap
  previews before release. Candidate influences are real kernel results from the
  original weights, not a cursor approximation. More samples do not starve an
  in-flight preview. Posed geometry stays fixed during the gesture; release
  computes/publishes the complete skin edit and updates actual deformation.
- Escape/focus loss/Undo restore the original surface without a transaction.
  Gesture epochs and generation/revision guards reject late previews. Accepted
  operation failure/cancellation/no-op restores its captured baseline; successful
  release remains exactly one global Undo. Saves/source-changing operations do
  not silently capture provisional weights. Document replacement cancels an
  unconfirmed gesture; held mouse motion after cancellation does not stamp into
  a released input lease.
- Posed BVH picking filters material alpha/culling, including original/derived
  PBR, V0/V1 MToon, V1 UV1, transforms, masks/animation and native overrides.
  Shared metadata helpers keep GPU and picking semantics together. The weight
  shader retains each material's alpha/blend/cull state and source bindings,
  while suppressing outlines. Packed UV1 intervals validate on the worker.
- The desktop uses the last drawn material-frame timestamp and rejects stale
  displayed geometry. Native status distinguishes pending/live feedback. BDF
  hint measurement and a fixed-footer compact weight-dialog regression are added.

## Verification

The bf4fb86 implementation passes all49 registered Release suites in333.39s,
including the complete real-loader synthetic XR report:73 checks, passed/complete,
failed0. Four whole-core C/C++ ASan/UBSan/leak suites pass in28.72s. Final frame-
time/UV1/layout refinements pass three focused Release suites in3.91s and two
whole-core sanitizer suites in14.09s. The final native held-preview/rig suite
passes14/14; compact640x360 at2x Unifont BDF passes4/4. The latest GPU suite has75
checks and material visibility has22. Exact scopes, captures and diagnostic
attempts are in VALIDATION.md. All30 upstream source hashes and the canonical
qimgvr BDF helper still match. The pinned toolkit is unchanged.

A final ownership audit moved desktop/input cleanup before returning the pending
weight-operation guard. Its new regression accepts the desktop gesture through
a rejected source-selection request, then successfully paints with XR pressure.
It passes in Release2.94s and whole-core sanitizers12.96s. The product was rebuilt;
this focused follow-up is distinct from the earlier broad/native GUI evidence.

No system install, active-session restart, fresh external Khronos validation or
physical headset/controller/stylus/3D-mouse/accelerator completion is claimed.
All validation handles were checked to terminal; inspect actual process state
before starting work. Never restart a job because an observation timed out.

## Continue the full objective

Keep the fixed-pose gesture contract explicit. Preview recomputes bounded
original-weight gestures and currently rebuilds a surface snapshot: incremental
weight envelopes, efficient GPU attribute updates and production-size latency/
memory remain important. Base-level alpha picking does not prove exact distant
mip/anisotropic coverage. Swept volume painting is not geodesic/occlusion-clipped
painting. Full automatic bind/transfer, interactive rig placement/fitting and
morph creation remain unfinished.

Continue source-map UV bake/adopt and production atlas quality, complete
retopology/cut/procedural authoring, production sculpt masks/symmetry/local
meshing, full glTF/VRM/MToon fidelity and all real device/backend rows. Retain
existing remesh/bake, conformal/stitching, pose/morph, source archive/export and
universal-menu workflows. Cold Fontconfig discovery remains a canonical qimgvr
follow-up; GUI startup allows90s and interactions retain30s deadlines. Python
is test orchestration only; authoring remains C++20/FLTK/EGL/OpenXR and BDF.

Always fetch before overlapping work; preserve any newer remote histories.
