# Checkpoint handoff — baked surfaces merged with creator baseline

This is a resumable stage, not completion of the full production objective.
Read AGENTS.md, REQUIREMENTS.md, TOOLKIT.md and VALIDATION.md first. Runtime
remains native C++20/FLTK/EGL/OpenXR with BDF fonts; Python is optional test
orchestration. Preserve DomeD appearance, universal-menu ownership, modal
cancellation and controller-oriented sliders/fine wheels. No system install or
active-session restart was performed for this checkpoint.

## History and publication

This checkpoint joins baked-surface commit 72d9127 with creator/source baseline
811eb69, preserving both histories. The standalone verified bake commit is also
public as checkpoint-baked-before-811eb69. Separate original records are archived
in HANDOFF-72d9127.md / VALIDATION-72d9127.md and HANDOFF-811eb69.md /
VALIDATION-811eb69.md; their results describe those baselines, not this merge.

Remote main advanced during integration; the latest observed tip was c95208b.
This combined stage is published on checkpoint/baked-creator-handoff-20261029,
without replacing newer remote main. On resume fetch and integrate newer main
before further overlapping work. Its post-811eb69 changes include original GPU
source bindings, shared exported rigs, source provenance, joint/morph controls,
fitted groom and capsule weights. Those changes are NOT in this checkpoint.
Do not overwrite or claim their implementation/validation as part of this tree.

## Current combined implementation

- Immutable baked PBR/alpha/AO/emission/+Y normal maps and coverage live beneath
  editable paint. Masked rebakes blend in linear light; snapshots, exact local/
  global Undo and ordered native saves retain the base and existing layers.
- UV changes transfer baked channels and reorient normal vectors through
  per-corner MikkTSpace frames. Worker-only render expansion preserves editing
  vertex IDs, skin/morph topology and parent-group world transforms.
- Document::bake_from_object queues a guarded detached worker transaction.
  Hidden posed high sources can be sampled into prepared rest targets. Cages,
  AO distances and ray bias account for composed group scale. Failures/misses/
  cancellation leave source and target intact; no nearest-surface fallback.
- Linear normal/AO preview and GLB normal/occlusion bindings coexist with source
  alpha/unlit behavior. Explicit TANGENT export and full deformed-consumer
  shading parity remain unfinished. See BAKING.txt for kernel limits.
- Creator baseline retains actual CC0 historical VRoid female/male models as
  primary sources and MakeHuman as an explicit alternative. Group authoring,
  original image/model archives, linked source-UV painting, projection/stencil
  infrastructure, bounded immutable composite caching and standalone MToon
  modules from 811eb69 are preserved. Read their component docs.
- Scene v6 stores baked data alongside v5 grouped creator/source archives.
  Unbaked scenes emit v5. Earlier supported formats and the standalone baked-v5
  checkpoint remain readable; the latter is explicitly distinguished from
  grouped v5. Do not reuse v5 for a different incompatible schema.
- Composite cache content tokens change on bake installation/clear, and cache
  retention counts derived normal bytes. Baked overlays preserve original raw
  source images beneath uncovered texels. Mixed archive+bake persistence and
  grouped world-cage behavior have dedicated integration regressions.

## Important limits

The FLTK/XR bake source picker, sliders/wheels, progress/cancel, clear/rebake,
preview and cage editing are still unconnected. The native API/data pipeline
is not a complete headset bake authoring workflow. Raw original source normal
views fail object baking explicitly until sampler/UV/tangent integration; their
original archive remains intact. Dense baked normals are supported. Changing
UVs on raw source-map objects still requires an explicit bake. Source-authored
linked/degenerate charts must retain their existing explicit policies.

MikkTSpace is pinned at 3e895b49d05ea07e4c2133156cfa94369e19e409. Preserve the
marked two-line seed-rotate UB fix, reproducible patch, licenses and hashes.
Instrument both C and C++ for sanitizer checks. Its bounded sort/group call has
no internal cancellation hook. SurfaceQuery uses the pinned Blender 5.4 Alpha
watertight triangle adaptation; parity-based voxel filling is unchanged.

Maps plus paint tiles share 128 MiB/object; scene paint storage is bounded at
256 MiB plus existing texel limits and the 256 MiB native-file cap. These are
not a complete transient/history memory model. Global history remains limited
by entry count, and large-asset VR latency/memory evidence is still required.
Physical headset/controllers/stylus/3D mouse and accelerator completion are not
claimed. Preserve every remaining row of REQUIREMENTS.md.

## Resume order

1. Fetch and merge newer origin/main carefully, especially paint/cache/raw GPU
   source textures, Scene schema, grouped rig export and new authoring controls.
   Preserve baked v6 and legacy checkpoint compatibility. Inspect actual live
   processes/logs before restarting any validation; old handoff agent names are
   historical and are not live handles in this workspace.
2. Integrate raw source normal/AO/material sampling with the newer GPU/source
   path; retain original model/image archives and avoid silent flattening.
3. Connect the guarded bake operation to FLTK and XR with actual universal-menu
   commands, explicit source selection, world-mm sliders/fine wheels, progress,
   cancel and one exact Undo. Preserve modal isolation and Cancel-default Enter.
4. Continue production atlas/texture masks and multi-source quality, rig/weight/
   morph/avatar/MToon/glTF/VRM and all sculpt/plugin/device/backend requirements.
   Do not reduce the objective to the newly implemented bake infrastructure.

All 35 Release suites pass (297.96 seconds), universal menus pass 53/53 and
the real FLTK baked-surface regression passes 5/5. Synthetic real-loader XR
reports 53 checks, complete/passed with zero failures. Validation and
outstanding verification are recorded in VALIDATION.md. The
fresh merged sanitizer rebuild was deliberately stopped before tests at this
checkpoint; its exact resume commands and historical evidence are recorded there. Ignored
logs/fixtures are local evidence; the native test sources and invocation paths
are tracked so another workspace can reproduce them. No agent was spawned for
this checkpoint request; this document is the handoff for the resumed work.
