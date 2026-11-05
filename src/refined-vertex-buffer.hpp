// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace dome::detail {
// Private transport prototype. The caller supplies complete evaluated world
// position/N/T/B, not raw source attributes or an ownership certificate.
// Four RGBA32F texels per logical output vertex; no vertex attribute slots.
inline std::string refined_vertex_buffer_shader(bool canonical) {
 const std::string n=canonical?"canonical_":"source_";
 return "#version 330 core\n"
 "uniform samplerBuffer columns; uniform mat4 view_projection;\n"
 "out vec3 world; out vec3 "+n+"n; out vec3 "+n+"t; out vec3 "+n+"b;\n"
 "void main(){int i=gl_VertexID*4; world=texelFetch(columns,i).xyz;"
 +n+"n=texelFetch(columns,i+1).xyz;"+n+"t=texelFetch(columns,i+2).xyz;"
 +n+"b=texelFetch(columns,i+3).xyz; gl_Position=view_projection*vec4(world,1);}\n";
}
}
