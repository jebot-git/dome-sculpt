# Direct immutable geometry uploads — private Release component

The renderer uploads existing position, normal, UV, tangent and source-field
arrays directly into one GPU buffer. A fixed descriptor table replaces the
mesh-sized interleaved Vertex/FrameVertex temporaries. Missing source directions
alias the native normal stream; missing UV/tangent values use per-draw constants.
This keeps immutable geometry reuse and scalar-only updates intact while reducing
temporary CPU memory and unused default-attribute storage. Plain sculpt storage
is 24 bytes per vertex; the previous interleaved layout was 56 bytes. This is not
a frame-time benchmark or completed local-tile upload implementation.

On the actual Arc A770 at PCI03/renderD129, the old whole renderer passes the
reference workflow and fails the new direct-array contract. The new whole
renderer passes 37 checks, including actual GPU block/source-byte equality,
19,200-vertex geometry, stereo reuse, partial frame aliases, default restoration
and scalar updates. All six framebuffer outputs are byte-identical to the old
renderer. Existing buffer24, material81, normal-frame47, ghost36 and complete
render suites pass on the same actual hardware with unchanged predicates.

The unchanged strict device observer checks current EGL device, PCI IDs and open
DRM descriptors. Software and UHD controls fail that strict witness after actual
renderer observation; this does not change the product's supported device policy.
All 68 actual mapped DSOs retain their recorded inodes and pre-baseline change
times. Actual source/support hashes remain unchanged during these finite runs.

This is a whole-render-provider Release component with compatible c179 support,
not a new full application/ABI build. Public layouts, scene formats and runtime
frontends are unchanged. The CMake test registration is source only at this
checkpoint. Sanitizer, matching current-main/caller composition and ordinary
application checks remain pending. No installation, headset launch or session
restart occurred. The full production objective remains active.

See [the exact receipt](validation/RENDER-DIRECT-UPLOAD-R.json).
