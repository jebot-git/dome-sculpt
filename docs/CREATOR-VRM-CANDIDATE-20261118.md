# Creator VRM candidate — paused, not admitted

Production `b75f5d443a9c6ea2987e030635b29698e5df60a2` implements native
creator-owned nonlinear VRM0 metadata refitting and generated-groom transport.
Test-only `66d63c5b8802f6ab36f0d0e0d597f9b1020e676b` corrects the independent
consumer's assumption about source meshes sharing a material. That correction
has compiled and linked, but has **not run**. The user requested a pause; no
successor native, desktop, synthetic-XR or sanitizer runtime was started.

The implementation reuses the actual native shaping/rest factory. It verifies
embedded CC0 source hashes, immutable genesis geometry/UV/skin/morph records,
saved creator settings, current rest joints and source joint identities before
transporting metadata. First-person points cross original bone-local/rest
coordinates, the actual shape warp and new rest coordinates. Authored collider
volumes use outward interval bounds; spring leaf radii additionally account for
the consumer's fixed 7cm continuation. This is an explicit conservative refit,
not dynamic-physics acceptance. Arbitrary rest edits, stale recipes and arbitrary
affine placement cannot borrow this authority.

Generated groom retains a distinct `vroid-groom` native genesis record. It has
no fabricated imported appearance or original MToon/material index. Export adds
its current rig, generated provenance and a new MToon emission policy matching
its current unlit PBR colour field. Original source materials/images and metadata
remain separate. The female source actually has six colliders and eight spring
roots; the male source has none. The fixture preserves that genuine difference.

The coordinate and spring rules follow the pinned
[UniVRM collider consumer](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/Runtime/SpringBone/Logic/SphereCollider.cs)
and [recursive spring consumer](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/Runtime/SpringBone/Logic/SpringBoneSystem.cs).
These references do not establish runtime acceptance of this candidate.

Actual results:

- Fresh whole App79 build: exit0, 245 genuine compiler dependency owners,
  196 core owners, 95,169 dependency incidences, no unmatched inputs/drift.
- Current coherent App child: exit0, SHA256
  `0315154ce7d84e7fe28ec8d41d6ac5776ddd76823f78b0dc83ba3e70292ae3e0`.
  It binds 241 unchanged fresh ancestor owners and four complete changed core
  owners; it is not described as another clean whole-branch build.
- Current Release consumer and controller/control fixture links: exit0.
- Fresh full sanitizer build: exit0, 212 genuine dependency owners including
  all196 core owners. Current creation/consumer and controller fixture children
  compile/link0. Strict sanitizer runtime remains unrun.
- Nativev6 emitted one shaped female VRM with twenty meshes. Independent cgltf,
  pinned official VRM0 schemas and Khronos validation report zero errors.
  Khronos retains21 warnings:20 skinned non-root nodes and one VRM extension
  naming warning. This is one partial exported-file result, not suite acceptance.

Both original native failures remain preserved. Nativev5 caught the generated
holder inheriting `has_mtoon=true`; productionb75 explicitly sets false only on
that holder. Nativev6 then exported successfully but the reused UV helper
incorrectly assumed one mesh per material. Test66 now compares each actual
primitive's ordered UV corners, retaining the original tolerance. Nativev7 is
prepared and explicitly unrun at the pause boundary.

Current b75 controller44/control352 runtime, both-baseline complete VRM consumer
coverage, save/reopen/posed/worker export checks, strict sanitizer runtime and
public FLTK/synthetic-XR creator cases remain pending. Earlier source75 native
44/352 results remain separately scoped; they do not qualify these new changes.
Physical HMD, full creator completion and main admission are not claimed.

The source branch is `work/vroid-creator-export-20261118`, its worktree is
`artifacts/vroid-creator-export-20261118`, and outputs are under
`artifacts/vroid-creator-export-build-20261118`. All owned units were captured and
stopped; no creator product or lease remains held. The exact53 evidence hashes
and source roles are in
[the candidate receipt](../validation/CREATOR-VRM-CANDIDATE-20261118.json).
