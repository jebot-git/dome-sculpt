# Current-corner plane-cut adapter: resumable private WIP

The goal was steered to a headset test followed by a pause. This checkpoint is
private work based on central085d353. Verified Scene14 checkpoint84601fe remains
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
EEE bitmap runtime/FLTK. Two native suites pass in1.22s:72 detached current-color
checks and68 unchanged legacy cut checks. The new fixture exports actual raw
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
