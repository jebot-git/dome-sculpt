# Current-corner plane-cut adapter: resumable private WIP

The goal was steered to a headset test followed by a pause. This checkpoint is
private work based on central `085d353`. Verified Scene14 checkpoint `84601fe` remains
unchanged. No native cut capability is enabled here: the original public
`publish_cut`, Document begin-cut and menu-readiness refusals for current-color
polygons were restored exactly. Do not merge this as a completed cut feature.

## Implemented detached path

- Additive `transfer_polygon_colors` composes explicit target CORNER transfers
  through the existing immutable initial field basis. It shares `source_owner`,
  checks complete convex mappings and bounds contributors/storage; it does not
  make sampled raster values or fabricated raw source IDs authoritative.
- Private `detail::publish_colored_cut` authenticates ordered source positions,
  normals, UVs, RGBA and material/paint, then recomputes the clipping result and
  compares its geometry/barycentric correspondence. Shell field provenance uses
  the actual clipped triangle barycentrics.
- Common material/paint components are baked without the vertex-color stream,
  leaving current straight linear RGBA to multiply exactly once in shading.
  Target low tangents use the renderer's world frame. Shell topology retains
  native source-vertex provenance and shared-edge identity without welding
  unrelated coincident source vertices by position alone.
- Outputs retain original local transform/native affine edit under an identity
  subgroup of the original parent. Full inverse placement and reflected corner
  order are handled. Shell/cap share one complete `capture_polygon_origin`
  capsule, including prior polygon, shape_source and affine placement. Existing
  archive4 machinery is used; no new persistent field/version was introduced.
- Progress callbacks/cancellation precede the final detached Scene swap. Prepared
  color/normal, retained role frames and other unsupported source shading remain
  explicitly refused in the private helper.

## Actual completed checks

Fresh coherent regular-user Release core and targets build with the canonical
EEE bitmap runtime/FLTK. Two native suites pass in 1.39s: 72 detached current-color
checks and 68 unchanged legacy cut checks. The new fixture exports actual raw
COLOR_0 GLB, imports/converts it to the existing current field, and exercises
both gradients and zero-alpha seams with ordinary and reflected/sheared
placement. It checks exact shared source basis, complete origin/shape capsule,
independent cap material, archive16/v4 roundtrip, source content/placement
restoration, analytic clipped geometry/RGBA, wrong-source and forged-map refusal,
cancellation and late-callback rollback. The original fixture's overlapping UV
layout was corrected to independent padded tiles; its initial failure is
retained. This is not native current-color Document/controller/GUI qualification.
No sanitizer, hardware, independent image/GLB-consumer, headset or installation
proof is claimed for this adapter.

## Resume next

1. Preserve root's newer combined source and held/source/normal/frame guards when
   integrating this085-based WIP. Existing Document/worker files have no diff in
   this checkpoint; do not overwrite them.
2. Add comprehensive transfer/malformed/cancellation/allocation-boundary tests,
   including one shared explicit preparation-work budget. Audit cap connectivity,
   closed combined geometry, repeated cuts, sampled paint/normal quality and
   source pose/rig preservation. `same_preview` still needs explicit plane and
   displayed cap metadata tamper tests in addition to the checked result arrays.
3. Finish independent GLB and software/Arc image oracles for alpha modes,
   source seams, paint/PBR components and reflected placement. Retained/prepared
   shading stays refused until its actual adapter is proven.
4. Only then wire the public current-field dispatch to the private publisher;
   capture `MeshSnapshot::source_colors` in Document cut-source slices; use the
   cheap supported-source predicate for begin/menu readiness. Authenticate all
   packed range bounds and preserve root's accepted-worker/error ordering.
5. Add actual native Document/controller/FLTK/OpenXR cut Apply/Cancel/no-op,
   source leases/staleness/held rules, exactly one Undo and immediate Save/Close,
   disk reopen and explicit source restoration. Use fresh coherent builds and
   full scoped C/C++ sanitizers before removing the frontend refusal.

The current-field color/bake adapter and the remaining full production goal
remain required. No new broad/device work was started for the headset launch.

## Resumed current-main work, 2026-11-02

Private branch agent/polygon-color-cut-8d46dfb ports the detached adapter to
physically accepted selection main8d46dfb. The blanket refusal remains in the
published application; this private working checkpoint replaces it with narrow
native admission for supported current fields, but is **not qualified for
integration yet**. It preserves UV1/prepared color/prepared normal/artist-frame
refusals. Current fields share their original immutable corner basis through
clipping; the generated cap has independent material. Original polygon, shape,
paint/UV and affine placement capsules remain retained.

Added bounded work/component/raster/provenance limits, complete ordered source
and preview authentication, connected cap topology, per-boundary field-transfer
cancellation, and actual native pose evaluation. A render check caught the
initial mistaken use of PaintedMesh::evaluate (paint query preparation) in place
of DeformationState::preview; this is corrected. The original pose fixture also
used XYZW rather than native WXYZ; the corrected fixture preserves the rest bone
translation and applies a real small joint rotation plus artist morph.

Fresh current-main Release initially passed three scopes: detached colored cut,
new quota/forged-source/cancellation/pose boundaries, and legacy cut (1.94s).
Independent real Document/GLB/PNG/GLSL comparisons then passed 32 cases and257
checks on software GL and actual Intel Arc A770: gradient and zero-alpha seams,
Opaque/Mask/Blend, unlit/PBR, artist paint/common bake, reflected/sheared placement,
actual skin/morph pose, visible caps, disk restart and source restoration.
All before/after appearance and independent GLB maximum channel errors were at
most1. The independent consumer now handles negative-determinant glTF winding and
can explicitly derive missing triangle tangents; existing strict consumers keep
their prior tangent requirement by default.

Persistent owned-new cut preparation found two real xatlas lifetime defects.
453a532 repairs a lost shutdown wakeup and baf6501 unwinds partially constructed
native workers/context/storage. Both are separate dependency checkpoints; see
XATLAS-SHUTDOWN.md. The original real cut boundary7288 now preserves exact source
and retries successfully. A complete56,629-boundary preparation sweep is still
running at this checkpoint; its completion is not claimed.

New native/controller tests are incomplete: legacy cut and existing color
Document scopes pass, while immediate Save immediately after accepted cut Apply
is rejected by finish_locked. The required cut-only ordered accepted-snapshot
Save extension will be developed in a NEW isolated checkout so this checkpoint
and running sweep remain unchanged. No native GUI, real-loader XR, complete-core
sanitizer or final integrated proof is claimed for the resumed working snapshot.
No physical headset, install or active-session changes occurred.
