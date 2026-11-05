# Refined complete vertex-column buffer prototype

Private8073499 passes375 optimized and375 matching owned sanitizer checks;2206 source/archive/input hashes remain exact in each profile. This adds a private GLSL samplerBuffer/gl_VertexID prototype with four RGBA32F texels per logical vertex and zero extra vertex attributes. Public headers and production shaders remain unchanged.

The independent fragment oracle uses the complete N/T/B field, world position and three eye positions. Across81 actual Source/Canonical before-VS scenarios and243 rendered views, original production vertex output versus derived36 buffer transport differs at most one UNORM8 code under an unchanged one-code bound. Every view covers at least1970 pixels. Incorrectly recomputing B from interpolated N/T differs17 codes; changing eye position differs89. These controls make the lighting proof sensitive to both field preservation and camera-dependent shading.

The underlying actual before-VS world/N/T/B error remains3.00342e-7<=1e-6. The buffer prototype checks actual GL texel capacity; it is not a qualified production allocation/upload policy. Readback is test-only; the private CPU pullback has no OpenGL calls.

Production CorePBR/MToon/physical field composition, renderer/query ownership and lifecycle, exact skin refinement, new-key delta semantics, owning FramePosition3/Paint5/domain2/Scene38 codec and real native Mirror publication remain required. No hardware, standard GLB exactness or main-admission claim is made. The original ignored driver replacement TypeError and first no-coverage-assertion cohort remain preserved separately.
