# Dome Sculpt production objective remains active

Read AGENTS.md, REQUIREMENTS.md, HANDOFF-466ebd1.md and parent DESIGN_CONVENTIONS.
Complete the full native C++20 FLTK/OpenXR sculpt, texture, UV, rig/skin/morph and
avatar suite. Animation and offline final rendering are excluded. A checkpoint
is not goal completion.

## Current source and verified milestone

Application source `20c132d` adds persistent object-level sculpt protection and
object-local XYZ symmetry to combined Retopo/authoring source2c89e8d. Native FLTK
and XR page15 expose mask Add/Remove/Smooth, strength, global Fill/Clear/Invert,
mirror origins and overlay visibility. All four geometry tools honor protection.
Mask data is independent of displacement layers, retained across explicit tool
changes, and constrained by object locks/visibility. See SCULPT-MASKS.md and INPUT.md.

Mask/control-only edits retain the immutable COW geometry owner and reuse the
worker's extracted source mesh. Protection scalars and mirrored brush feedback
are published through immutable snapshots; PBR/MToon alpha and picking remain
unchanged. Strong source/generation checks, exact cancellation/history, ordered
worker publication and allocation-free stroke rollback remain enforced.

Nondefault mask/control data uses nested Volume3; default Volumes retain2.
Flat Volume3 opens natively; Scene5–8 compatibility and aggregate quotas are
preserved. Immutable material/Retopo features still use Scene8. Skin14 and
Retopo16 remain separate from Masks15. Future original normal-frame provenance
requires Scene9/materials2; never relabel legacy derivative pixels.

The frozen optimized candidate is artifacts/verify-sculpt-ui-79fe/build.
Despite its historical directory name, every179 product source hash matches
20c132d (artifacts/sculpt-product-freeze-manifest.json). It excludes unrelated
primary frame/creator drafts. Do not mix earlier headers/static libraries with
the new Volume/Brush/Sample layouts. Fresh public clones of20c132d have the same
product source; the primary worktree may contain later unrelated WIP.

Verification is complete for this milestone:73/73 native Release173.67s;
CPU real-loader XR140/140; actual Arc ONEAPI frontend XR140/140; whole-core C/C++
ASan/UBSan/leaks17/17 in186.49s; actual Arc native12/12 in6.15s. Native menus53/53,
Skin11/11, Unicode3/3, mask controls17/17 plus compact2xBDF5/5, actual DomeD
Wayland mask9/9 and existing8/8 pass. Mask/mirror GPU38/38 and existing material
75+25 assertions pass on Arc and llvmpipe. Logs and exact scopes: VALIDATION.md.
All milestone validation handles are terminal. No physical-device claim follows
from these synthetic results.

## Toolkit, physical test and installation boundary

Canonical DomeD/FLTK282467e727df78c8e7f2452aa05a7747bac854a6 was rechecked on remote.
Use explicit FLTK_DIR=artifacts/toolkit-282467e/share/fltk and runtime lib64;
CMAKE_PREFIX_PATH alone can retain an older cached /opt toolkit. The matching
standalone compositor is artifacts/domed-282467e-target/release/dome-compositor.
Build optimized releases as regular user. No system install or active-session
restart occurred. No physical headset test was relaunched.

The user confirmed rudimentary WiVRn HMD/both-controller operation only for
8f4d67d, then requested ending that test. Recovery snapshots/report are retained
under artifacts/xr-milestone-8f4d67d; DomeD continued running. This does not prove
later sources, complete workflows, spatial stylus, reconnect or sustained latency.

## Independent work and next integration

Query collaboration ownership before editing/staging. Preserve all private
frame/creator WIP and old applied backup stashes; do not reapply those stashes.

- Local meshing foundation37e38c7 is committed privately on
  work/local-surface-foundation-2c89e8d in artifacts/local-surface-2c89e8d.
  Its75 Release/sanitizer checks and6 regression suites pass. Exact-position
  welding/collinear subdivision/directed orientation repairs an inherited tiny
  hole without moving vertices. Independent timing shows faster local extraction;
  full assembly and dirty-producer/upload costs remain distinct. It is not in
  main. The next private phase authenticates Volume dirty lineage and integrates
  Document cache updates. Incremental GPU assembly/uploads remain required.
- Original/native normal frames are private in artifacts/frame-runtime-prototype.
  Versioned Scene9/materials2 provenance, actual creator Shape/native morph/skin
  transport and frame-aware UV fixtures are under development. Export and live
  UV Apply guards remain until complete consumer/interop proof. Primary helper
  and creator drafts are not part of20c132d.

## Full remaining scope

Every incomplete REQUIREMENTS row remains required: resolution/local storage and
meshing, sustained production sculpting, projection/stencils/texture sets and
advanced baking; complete retopo/cut/procedural/plugin workflows; interactive
bone/landmark/finger fitting, robust automatic weights and editable shapekeys;
robust anime/toon authoring; original normal frames and full selected glTF/VRM1,
constraints/expressions/physics and independent graphical consumers; physical
controllers/headset/stylus/3D mouse, instance loss/reconnect and performance;
all eligible compute workloads and hardware verification for each backend.

CPU remains default. Arc ONEAPI is real hardware execution; missing GPU fails
explicitly. HIP7.1 compiles gfx1030 but AMD hardware/ROCK is absent; CUDA SDK/GPU
are unavailable. Compile-only and software rendering are not execution proof.
Earlier checkpoints and their scopes are archived in HANDOFF-2c89e8d.md,
HANDOFF-500f321.md and corresponding validation records.
