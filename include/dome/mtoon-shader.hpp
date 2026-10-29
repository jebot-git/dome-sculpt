// SPDX-License-Identifier: MIT
// Copyright (c) 2018 Masataka SUMI for MToon
// Copyright (c) 2020 VRM Consortium
// Native GLSL adaptation of licensed UniVRM reference; full notice in
// docs/MTOON-LICENSE.txt. No Unity runtime or shader compiler is required.
/*
MIT License

Copyright (c) 2020 VRM Consortium
Copyright (c) 2018 Masataka SUMI for MToon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <string_view>
namespace dome {
inline constexpr std::string_view mtoon_shader_source = R"GLSL(
#ifndef DOME_MTOON_INCLUDED
#define DOME_MTOON_INCLUDED
struct DomeMToonSamples {
  vec4 base; vec3 shade; vec3 emission; vec3 rim; vec3 matcap;
  float shift; float receiveShadow; float grade;
};
struct DomeMToonLight {
  vec3 normal; vec3 view; vec3 direction; vec3 color;
  vec3 gi; vec3 giUp; vec3 giDown; float shadow; bool normalOriented;
};
vec3 dome_mtoon_unit(vec3 v) {
  float m=max(max(abs(v.x),abs(v.y)),abs(v.z));
  return m>1e-20?normalize(v/m):vec3(0,0,1);
}
vec3 dome_mtoon_srgb(vec3 c) {
  return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
}
vec4 dome_mtoon_decode_texel(vec4 texel,int encoding) {
  return vec4(encoding==1?dome_mtoon_srgb(texel.rgb):texel.rgb,texel.a);
}
vec3 dome_mtoon_facing_normal(int version,vec3 n,vec3 view,bool front,bool outline) {
  n=dome_mtoon_unit(n);view=dome_mtoon_unit(view);
  if(version==0){if(dot(n,view)<0.0)n=-n;if(outline)n=-n;}
  else if(!front)n=-n;
  return n;
}
vec3 dome_mtoon_normal_map(vec3 encoded,float scale) {
  return dome_mtoon_unit(vec3((encoded.xy*2.0-1.0)*scale,encoded.z*2.0-1.0));
}
vec3 dome_mtoon_shading_normal(int version,vec3 geometric,vec4 tangent,
                                vec3 encoded,float scale,vec3 view,bool front,bool outline) {
  vec3 n=dome_mtoon_unit(geometric);
  if(version==1&&!front)n=-n;
  vec3 t=dome_mtoon_unit(tangent.xyz);t=dome_mtoon_unit(t-n*dot(t,n));
  vec3 b=cross(n,t)*tangent.w,ts=dome_mtoon_normal_map(encoded,scale);
  vec3 mapped=dome_mtoon_unit(t*ts.x+b*ts.y+n*ts.z);
  return version==0?dome_mtoon_facing_normal(0,mapped,view,front,outline):mapped;
}
vec3 dome_mtoon_material_normal(vec4 p[12],vec3 geometric,vec4 tangent,vec3 encoded,vec3 view,bool front,bool outline) {
  return p[11].x>0.5?dome_mtoon_shading_normal(int(p[8].x),geometric,tangent,encoded,p[6].z,view,front,outline):dome_mtoon_facing_normal(int(p[8].x),geometric,view,front,outline);
}
vec3 dome_mtoon_desaturate(vec3 c,float t) {
  return mix(c,vec3(max(0.0009765625,max(c.r,max(c.g,c.b)))),t);
}
// p uses the exact12vec4 layout returned by MToonMaterial::gpu_parameters().
// All samples are already linear; texture alpha is always linear.
vec4 dome_mtoon_shade(vec4 p[12],DomeMToonSamples s,DomeMToonLight l,
                       bool front,bool outline,out bool killed) {
  bool legacy=p[8].x<0.5;
  vec3 n=l.normalOriented?dome_mtoon_unit(l.normal):dome_mtoon_facing_normal(legacy?0:1,l.normal,l.view,front,outline);
  vec3 view=dome_mtoon_unit(l.view),light=dome_mtoon_unit(l.direction);
  float ndl=dot(n,light),alpha=p[0].a*s.base.a;
  int mode=int(p[8].y+0.5);killed=mode==1&&alpha<p[6].w;
  if(mode!=2)alpha=1.0;
  vec3 lit=p[0].rgb*s.base.rgb,shade=p[1].rgb*s.shade;
  vec3 uniformGi=(l.giUp+l.giDown)*0.5,lighting=l.color,gi;
  float ratio;
  if(legacy){
    float receive=p[7].x*s.receiveShadow;
    float attenuation=l.shadow*mix(1.0,l.shadow,receive);
    float grade=1.0-p[7].y*(1.0-s.grade);
    float intensity=(ndl*0.5+0.5)*attenuation*grade*2.0-1.0;
    float high=mix(1.0,p[1].a,p[2].a);
    ratio=clamp((intensity-p[1].a)/max(0.0009765625,high-p[1].a),0.0,1.0);
    lighting=dome_mtoon_desaturate(lighting,p[7].z);
    gi=dome_mtoon_desaturate(mix(uniformGi,l.gi,p[7].w),p[7].z);
  }else{
    float x=ndl+p[1].a+s.shift*p[10].w,width=1.0-p[2].a;
    ratio=width<=1e-7?step(0.0,x):clamp((x+width)/(2.0*width),0.0,1.0);
    lighting*=l.shadow;gi=mix(l.gi,uniformGi,p[6].x);
  }
  vec3 color=mix(shade,lit,ratio)*lighting+gi*lit;
  if(legacy)color=min(color,lit);
  if(!legacy||!outline){
    float rim=pow(clamp(1.0-dot(n,view)+p[4].a,0.0,1.0),max(p[3].a,0.0009765625));
    vec3 rimLight=mix(vec3(1),lighting+(legacy?gi:l.gi),p[6].y);
    if(legacy)color+=p[3].rgb*s.rim*rim*rimLight+p[4].rgb*s.matcap;
    else color+=(p[4].rgb*s.matcap+p[3].rgb*rim)*s.rim*rimLight;
    color+=p[2].rgb*s.emission;
  }
  if(outline)color=legacy&&p[8].w>0.5?p[5].rgb:p[5].rgb*mix(vec3(1),color,p[5].w);
  return vec4(color,alpha);
}
// UV origin is glTF top-left. For V0 pass shared_MainTex offset/scale;
// legacy non-main per-slot ST values are retained metadata, not shader inputs.
vec2 dome_mtoon_uv(vec4 p[12],vec2 uv,vec2 offset,vec2 scale,float rotation,
                    bool matcap,bool maskSlot,bool widthSlot,float seconds,float mask) {
  bool legacy=p[8].x<0.5;vec2 q=uv;
  if(matcap){if(legacy)return q;}
  bool animated=!matcap&&!maskSlot&&!(legacy&&widthSlot);
  if(legacy){
    q=q*scale+offset;
    if(animated){q.y=1.0-q.y;q+=p[9].xy*seconds*mask;
      float a=p[9].z*6.283185307179586*seconds*mask,c=cos(a),s=sin(a);q-=0.5;
      q=vec2(c*q.x-s*q.y,s*q.x+c*q.y)+0.5;q.y=1.0-q.y;}
  }else{
    if(animated){float a=p[9].z*seconds*mask,c=cos(a),s=sin(a);q-=0.5;
      q=vec2(c*q.x+s*q.y,-s*q.x+c*q.y)+0.5+p[9].xy*seconds*mask;}
    q*=scale;float c=cos(rotation),s=sin(rotation);
    q=vec2(c*q.x-s*q.y,s*q.x+c*q.y)+offset;
  }return q;
}
vec2 dome_mtoon_matcap_uv(vec3 n,vec3 view,vec3 up) {
  n=dome_mtoon_unit(n);view=dome_mtoon_unit(view);up=dome_mtoon_unit(up);vec3 right=cross(view,up);
  if(length(right)<1e-6)right=cross(view,abs(view.x)<0.8?vec3(1,0,0):vec3(0,0,1));
  right=dome_mtoon_unit(right);up=dome_mtoon_unit(cross(right,view));
  return vec2(0.5)-vec2(dot(right,n),dot(up,n))*0.5;
}
float dome_mtoon_outline_world_width(vec4 p[12],float widthSample) {
  return int(p[10].x+0.5)==1?p[9].w*widthSample*(p[8].x<0.5?0.01:1.0):0.0;
}
// Exact legacy object-space extrusion convention, including nonuniform scale.
vec3 dome_mtoon0_outline_object_offset(vec3 normalObject,mat3 normalMatrix,
                                       float authoredWidth,float widthSample) {
  return 0.01*authoredWidth*widthSample*length(normalMatrix*normalObject)*normalObject;
}
vec2 dome_mtoon_outline_clip_offset(vec4 p[12],vec2 projected,vec3 viewNormal,
                                     float clipW,float aspect,float widthSample) {
  if(int(p[10].x+0.5)!=2||length(projected)<1e-12)return vec2(0);
  float width=p[9].w*widthSample;
  if(p[8].x<0.5)width*=0.01*min(clipW,p[10].y)*clamp(1.0-abs(dome_mtoon_unit(viewNormal).z),0.0,1.0);
  else width*=2.0*clipW;
  return normalize(projected)*vec2(1.0/aspect,1.0)*width;
}
#endif
)GLSL";
} // namespace dome
