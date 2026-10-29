# Agent handoff — reconciled toolkit and ordered-input checkpoint

Repository: https://github.com/jebot-git/dome-sculpt, branch `main`.
The user requested a committed/pushed checkpoint and handoff for later resumption.
The full production objective in [REQUIREMENTS.md](REQUIREMENTS.md) is incomplete;
resume that scope when requested. Do not treat this checkpoint as completion.
The earlier full background is in [HANDOFF-466ebd1.md](HANDOFF-466ebd1.md).

## Preserved requirements and architecture

Use native C++20/FLTK/EGL/OpenXR, BDF fonts through Fontconfig/FreeType and DomeD
appearance/keybinding/universal-menu conventions. Python is optional test
orchestration only; no Qt/GTK/Pango/Erlang runtime frontend. XR controls use large
ray targets, continuous sliders/sticks and fine wheels. Super+X is shell-owned.
Blender 5.4 Alpha, Gmsh, previous OpenBrush and Wings references/provenance remain
in third_party/sources.json. Keep the full sculpt/texture/avatar scope, including
rigging, weights, shapekeys, MToon and glTF/VRM; animation/final rendering excluded.

Current implementation has sparse SDF objects, material/object editing, layered
stable-UV painting, editable UV islands/vertices/pins/cuts/relaxation/packing,
filtered paint transfer, exact undo, atomic projects and GLB export. Immutable
snapshots serve renderers; geometry, transfer and files run on workers. Read
UV.md, PAINTING.md, INPUT.md and UNIVERSAL-MENU-AUDIT.md for actual limitations.
Preserve source/generation guards, ordered saves and cancellation semantics.

## Integrated changes

This reconciles upstream `96bc62f` with the input-order repair originally saved as
`4e8254b` (local backup branch `checkpoint-input-order-4e8254b`).

- Focus loss is now an ordered cancellation boundary: queued completed strokes
  and UV Apply survive; unfinished work cancels. A deterministic burst reproduced
  loss before the fix. Shutdown closes input, drains transitions, then cancels
  drafts; late events are rejected until input reopens. Each native OpenXR READY
  transition reopens input, including STOPPING/IDLE/READY resumption.
- The runtime fixture verifies session begin/end ordering and a new sculpt stroke
  after resumption. Full recreation after instance/session loss remains pending.
- Both sets of continuous UV scale/turn/fine-wheel/cancellation regressions remain.
  Merge resolution gives their angle/before-turn locals distinct names.
- The XR harness preserves projects, separate UV/field/paint assertions and
  comparison JSONs. Reports reset at startup, update incrementally, record
  running/failed/passed status, completion and exceptions. An incomplete report
  with zero failures is not a completed success.
- Upstream's public FLTK chooser-header fix, configured build RPATH, runtime
  wrapper/CTest library selection and portable software-Vulkan test ICD remain.
  Current canonical toolkit pins and all 22 patch hashes remain in the manifest.

A reproducible queued-confirmation defect is fixed. The exact cause of the
original historical native XR failure is unproven: its comparison artifacts were
unavailable. Subsequent native runs pass with UV hash 5162942432843560021 versus
3423923137026321418, unchanged field 11207114470605020999 and paint
3979263710525981571. Do not equate the deterministic proof with proof of that
historical run's cause.

## Validation provenance and local environment

Before integration, this workspace verified:

- All ten optimized suites: artifacts/tests-input-order-full.log (223.63 s).
- Expanded native OpenXR: 38 checks, complete=true, zero failures,
  artifacts/tests-session-resume.log (155.65 s).
- Seven ASan/UBSan/leak suites: artifacts/tests-input-order-sanitize.log (352.59 s).
- Deterministic pre-fix failure: artifacts/test-focus-regression-red.log.
- Machine-readable evidence: artifacts/validation-input.json.

Those runs used the existing /opt/fltk-vr/toolkit build. Upstream `96bc62f`
separately recorded current canonical toolkit verification: ten suites/36 XR,
18 GUI, 25 menu and eight newly built nested DomeD checks. Its toolkit pins are
DomeD 742d009f446cffdeb89031664b72885ff6f16a19 and fltk-vr
4f54821e61993a9212a2d9cd03635342b58acf6a. See TOOLKIT.md and VALIDATION.md.
Its artifacts/toolkit, toolkit-deps and domed-source directories, and the parent
DESIGN_CONVENTIONS file, are absent in this workspace. Do not claim they were
rebuilt or reread here. Its local logs are not available here either.

Post-integration: all ten suites passed, including 40 OpenXR checks with
status=passed, complete=true and zero failures. The merged UV suite passed
ASan/UBSan/leak checks in 68.61 s. Logs: artifacts/tests-checkpoint-merge.log and
artifacts/tests-checkpoint-merge-sanitize.log; actual runtime linkage is in
artifacts/linkage-checkpoint-merge.log. These checks used the existing /opt
toolkit, not the absent canonical toolkit build. No new GUI/nested-compositor
checks were run. More detail is recorded in VALIDATION.md. Generated artifacts
are ignored. Physical headset/controllers/stylus, production-scale performance
and accelerator execution remain unverified. No system installation or active
session restart was performed. Inspect processes before starting further tests.

## Resume sequence

1. Read AGENTS.md, REQUIREMENTS.md, TOOLKIT.md and the universal-menu audit. Obtain
   the current workspace design conventions if available. Prepare the canonical
   pinned toolkit in an ignored prefix and use the generated runtime launcher;
   inherited LD_LIBRARY_PATH can override RUNPATH. Do not silently relabel the
   older /opt toolkit as the current canonical build.
2. Continue production UV interaction work: draft copies/validation run on the
   input worker and need profiling with real meshes; continuous samples currently
   create separate local undo records. Coalesce gestures without losing rollback.
3. Continue native import/remeshing/baking, sculpt layers/masks/symmetry/local
   meshing, stencils/projection/texture sets and conformal/stitch UV workflows.
   Reuse suitable Gmsh/VCG/MeshLab and other native upstream work without their
   GUI toolkits. Preserve paint or provide explicit reversible transfer.
4. Complete humanoid rigs/weights/skinning/morphs/MToon/glTF/VRM and requested
   plugin-equivalent workflows. Static GLB is not full avatar export.
5. Extend lost-session recovery and physical/accelerator validation. Input queue
   overflow remains bounded at 64; reference-space/document epochs intentionally
   invalidate stale poses. No unlimited retention or production latency claim.

The user authorized public commits/pushes. Keep checkpoint evidence and this
handoff accurate; do not install system-wide or restart sessions just to resume.
