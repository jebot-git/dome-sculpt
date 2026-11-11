#!/usr/bin/env python3
"""Generate a UI-free, explicitly altered GLSL adaptation of ArmorPaint blur.

Upstream: armory3d/armorpaint 629f8e77b3f19d533c60a1baa4fe9d4db9ee1163.
Preserve vendor/LICENSE.armorpaint and vendor/make_blur.c verbatim.
"""
import hashlib
import argparse
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=pathlib.Path, required=True)
args = parser.parse_args()
output = args.output_dir.resolve()
if output == root or root in output.parents:
    parser.error('Generated files must be outside the tracked experiment source directory')
output.mkdir(parents=True, exist_ok=True)
source = (root / "vendor/make_blur.c").read_bytes()
if hashlib.sha256(source).hexdigest() != "a2403c5ef2ffd23fc5a1798b47c04b29ecc9e5362faa8f16f20e4f842314fbf4":
    raise RuntimeError('Pinned ArmorPaint source hash mismatch')
text = source.decode()
block = text.split('char *str_get_blur_tool_weight = "\\\n', 1)[1].split('";', 1)[0]
weight_function = block.replace('\\\n', '\n')
if len(re.findall(r'return ', weight_function)) != 15:
    raise RuntimeError('Pinned weight function shape mismatch')
shader = '''#version 330 core
// Altered ArmorPaint blur kernel. Original weight function follows verbatim.
uniform sampler2D sourcePlane;
uniform sampler2D destinationMask;
uniform float strength;
layout(location=0) out vec4 associatedResult;
''' + weight_function + '''
void main() {
    ivec2 size = textureSize(sourcePlane, 0);
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec4 sum = vec4(0.0);
    // Adapted make_blur.c:92-121: cross-shaped 15+15 samples, including
    // duplicated center, not a separable Gaussian or a 15x15 convolution.
    for (int axis=0; axis<2; ++axis) {
        for (int i=0; i<15; ++i) {
            ivec2 q = p + (axis==0 ? ivec2(i-7,0) : ivec2(0,i-7));
            vec4 sampleValue = texelFetch(sourcePlane, clamp(q,ivec2(0),size-1),0);
            float w = get_blur_tool_weight(i);
            // make_blur.c:99-100/119-120: associated RGB and independent alpha.
            sum.rgb += sampleValue.rgb * sampleValue.a * w;
            sum.a += sampleValue.a * w;
        }
    }
    vec4 original = texelFetch(sourcePlane,p,0);
    original.rgb *= original.a;
    float gate = texelFetch(destinationMask,p,0).r * strength;
    // Dome-side adaptation: explicit mask/strength convex combination in
    // associated space; no shared material-ID/normal/emission packed target,
    // discard threshold, UNORM8 conversion or division by tiny coverage.
    associatedResult = mix(original,sum,gate);
}
'''
(output / "blur.frag").write_text(shader)
(output / "blur_shader.hpp").write_text('#pragma once\ninline constexpr char blur_shader[] = R"shader(' + shader + ')shader";\n')
