# Stable mesh and layered painting

Press P to prepare a painting mesh from the selected sculpt, or return to its
existing mesh. Preparation runs SDF extraction and xatlas off the input/render
threads, then publishes a stable mesh with persistent UVs. The desktop default
is 512×512; `--xr --paint` prepares 256×256 before entering XR. Core resolution
support is 64–2048 square powers of two. Alt+U opens the [manual UV editor](UV.md), which transfers paint when UVs change.

Shift+P returns to the retained sculpt source. Source sculpting does not deform
or reproject the previous painted mesh. P returns to that mesh; Paint → Prepare
painting mesh explicitly replaces its UVs/layers using the edited source. This
replacement participates in undo, which restores the previous painted asset.

Shift+left drag paints on desktop; the XR pressure trigger paints at the tracked
tip. Keys 5–9 select color, metalness, roughness, emission or mask, and 0 toggles
erasing. Alt+P opens native FLTK brush settings, Ctrl+L adds a layer. The Paint
menu selects layers, visibility, opacity and channels; the XR controls also
provide layer ordering. Color values
are linear in the controls; stored color/emission texels are sRGB, alpha and
metal/rough/masks are linear. Each channel has separate coverage. Layer masks
attenuate composition; erasing removes channel coverage. Hidden layers do not
contribute. Base material values fill unpainted areas.

XR Materials → Paint tools → Controls provides large sliders for brush RGBA,
metalness, roughness, emission, mask value, flow, radius, layer opacity and layer
order. The RGBA controls have a swatch and separate selectable rows. Preset
colors remain available within Controls → Presets. A ray/trigger drag or held
thumbstick adjusts the same value; scalar vertical motion gives fine adjustment.
The menu owner confirms with a joystick press, or either ray can click Done.
A ray-clickable Cancel button permits dismissal entirely in headset.
Focus/tracking loss or desktop Escape cancels the preview. Confirmed persistent changes form one undo step.
The controller can open the panel while a spatial stylus operates its sliders.

Painting uses pressure-sensitive swept capsules in object-local space, texel
surface positions/normals, an optional front-face filter and tile bounds. Surface
painting crosses UV seams in 3D; four-pixel chart gutters extend texture edges.
Immutable 32×32 tiles are copied on change, with exact per-stroke undo, redo and
cancellation. Object duplication shares immutable data while keeping subsequent
edits independent. Save jobs snapshot layers/tiles and retain the ordered file
worker used by sculpt edits.

The GPU preview samples color, metallic/roughness and emission textures. GLB
exports embed composited PNGs and keep the same UVs. The project retains editable
layers rather than only flattened images. `--export-project input.dsculpt
output.glb` exports a saved project without opening an editor.

This is an initial layered painting implementation. Stencils, image projection,
texture-set/UDIM workflows, normal/AO baking, paint transfer after remeshing,
advanced seam re-unwrapping/stitching, production-scale acceleration, material import and full
MToon are pending. Surface picking is a linear triangle scan; immutable surface
maps and CPU compositing have explicit memory/resolution budgets. Physical
headset/controller/stylus use has not been verified on this machine.

Baked base maps are now persistent native assets beneath editable paint layers;
see BAKING.txt. Color/alpha, emission, ORM/AO, normal and filtered coverage survive
snapshot/save/undo and UV reparameterization. The guarded object bake API is
available, while its FLTK/XR authoring controls remain unfinished. Scenes with
this data use version 5; earlier projects remain readable.
