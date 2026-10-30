// SPDX-License-Identifier: GPL-3.0-or-later
// Host GLSL integration around the separately licensed native MToon module.
#pragma once
#include "dome/mtoon.hpp"
namespace dome {
inline std::string mtoon_renderer_common(){
  return std::string(mtoon_glsl())+R"GLSL(
uniform int source_frame,canonical_frame,prepared_pbr_frame;
uniform int mt_mode,mt_outline,mt_present[16],mt_coord[16],mt_derived[16],mt_channel[16];
uniform vec4 mt[12];uniform vec2 mt_offset[16],mt_scale[16];uniform float mt_rotation[16],mt_time;
uniform vec3 mt_camera_up,mt_camera_forward;uniform float mt_aspect;
uniform vec3 mt_pbr_x[2],mt_pbr_y[2];
uniform sampler2D tex0,tex1,tex2,tex3,tex4,tex5,tex6,tex7,tex8,tex9,tex10,tex11,tex12,tex13,tex15;
vec4 mt_read(int r,vec2 q){
if(mt_present[r]==0){if(r==2)return vec4(.5,.5,1,1);if(r==5||r==6)return vec4(0);if(r==11)return vec4(0,0,0,1);return vec4(1);}
if(r==0)return texture(tex0,q);if(r==1)return texture(tex1,q);if(r==2)return texture(tex2,q);if(r==3)return texture(tex3,q);
if(r==4)return texture(tex4,q);if(r==5)return texture(tex5,q);if(r==6)return texture(tex6,q);if(r==7)return texture(tex7,q);
if(r==8)return texture(tex8,q);if(r==9)return texture(tex9,q);if(r==10)return texture(tex10,q);if(r==11)return texture(tex11,q);
if(r==12)return texture(tex12,q);if(r==13)return texture(tex13,q);if(r==14)return vec4(1);return texture(tex15,q);}
float mt_scalar(int r,vec4 x){return mt_channel[r]==1?x.g:mt_channel[r]==2?x.b:mt_channel[r]==3?x.a:x.r;}
vec2 mt_uv(int r,vec2 a,vec2 b,vec3 N,vec3 V,float mask){
if(r==11||mt_derived[r]!=0)return a;
if(r>=14){int k=r-14;return vec2(dot(mt_pbr_x[k],vec3(a,1)),dot(mt_pbr_y[k],vec3(a,1)));}
int s=r==12?0:r==13?3:r;vec2 q=s==5?dome_mtoon_matcap_uv(N,V,mt_camera_up):(mt_coord[s]==1?b:a);
int transform=mt[8].x<.5?0:s;
return dome_mtoon_uv(mt,q,mt_offset[transform],mt_scale[transform],mt_rotation[transform],s==5,s==8,s==7,mt_time,mask);}
)GLSL";
}
inline std::string mtoon_renderer_vertex(){return std::string("#version 330 core\n")+mtoon_renderer_common()+R"GLSL(
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec2 uv;layout(location=3)in vec2 uv1;layout(location=4)in float selected_weight;layout(location=5)in vec4 tangent;layout(location=6)in float sculpt_mask;layout(location=7)in vec3 retained_normal;layout(location=8)in vec4 retained_tangent;layout(location=9)in vec3 retained_canonical_normal;
layout(location=10)in vec4 retained_color;out vec4 source_color;
uniform mat4 model,mvp,view_projection;uniform vec3 camera;
out vec3 world,n,canonical_t,canonical_b,cut_position,canonical_n,source_n,source_t,source_b;out float source_w;out vec2 texcoord,texcoord1;out float weight_value,mask_value;
void main(){vec4 p=model*vec4(position,1);vec3 geometric=dome_mtoon_unit(mat3(model)*normal),V=dome_mtoon_unit(camera-p.xyz);
source_n=dome_mtoon_unit(mat3(model)*dome_mtoon_unit(retained_normal));source_t=dome_mtoon_unit(mat3(model)*dome_mtoon_unit(retained_tangent.xyz));source_w=retained_tangent.w;source_b=cross(source_n,source_t)*source_w;
if(prepared_pbr_frame==1)source_b=dome_mtoon_unit(source_b);
vec3 N=source_frame==1?source_n:geometric;world=p.xyz;cut_position=position;n=geometric;source_color=retained_color;
canonical_n=canonical_frame==1?dome_mtoon_unit(mat3(model)*dome_mtoon_unit(retained_canonical_normal)):geometric;
canonical_t=dome_mtoon_unit(mat3(model)*tangent.xyz);canonical_b=cross(canonical_n,canonical_t)*tangent.w;texcoord=uv;texcoord1=uv1;weight_value=selected_weight;mask_value=sculpt_mask;gl_Position=mvp*vec4(position,1);
if(mt_mode==1&&mt_outline==1&&selected_weight<0){float mask=mt_scalar(8,mt_read(8,mt_uv(8,uv,uv1,N,V,1)));
float width=mt_scalar(7,mt_read(7,mt_uv(7,uv,uv1,N,V,mask)));
if(int(mt[10].x+.5)==1){world+=N*dome_mtoon_outline_world_width(mt,width);gl_Position=view_projection*vec4(world,1);}
else if(int(mt[10].x+.5)==2){vec2 projected=(view_projection*vec4(N,0)).xy;vec3 right=dome_mtoon_unit(cross(mt_camera_forward,mt_camera_up));
vec3 viewNormal=vec3(dot(N,right),dot(N,mt_camera_up),-dot(N,mt_camera_forward));gl_Position.xy+=dome_mtoon_outline_clip_offset(mt,projected,viewNormal,gl_Position.w,mt_aspect,width);}
}})GLSL";}
inline std::string mtoon_renderer_fragment(){return R"GLSL(
uniform vec4 mt_effective_color,mt_native_color;uniform vec3 mt_effective_emission,mt_native_emission;
uniform int mt_shade_alias,mt_baked_normal;
uniform samplerBuffer packedSources;
uniform ivec4 mt_pack_levels[56],mt_pack_sampler[4];uniform ivec2 mt_pack_info[4];
vec2 mt_packed_wrap(vec2 q,ivec2 wrap){for(int axis=0;axis<2;++axis){
if(wrap[axis]==10497)q[axis]=fract(q[axis]);
else if(wrap[axis]==33648){float cell=floor(q[axis]);q[axis]=fract(q[axis]);if(mod(cell,2.)>=1.)q[axis]=1.-q[axis];}
else q[axis]=clamp(q[axis],0.,1.);}return q;}
int mt_packed_coordinate(int q,int n,int wrap){if(wrap==10497)return (q%n+n)%n;
if(wrap==33648){q=(q%(n*2)+n*2)%(n*2);return q<n?q:n*2-1-q;}return clamp(q,0,n-1);}
vec4 mt_packed_texel(int view,int level,ivec2 q){ivec4 data=mt_pack_levels[view*14+level];
ivec2 wrap=mt_pack_sampler[view].xy;q=ivec2(mt_packed_coordinate(q.x,data.y,wrap.x),mt_packed_coordinate(q.y,data.z,wrap.y));
vec4 v=texelFetch(packedSources,data.x+q.y*data.y+q.x);
if(mt_pack_info[view].y==1)v.rgb=mix(v.rgb/12.92,pow((v.rgb+.055)/1.055,vec3(2.4)),step(vec3(.04045),v.rgb));return v;}
vec4 mt_packed_level(int view,int level,vec2 q,bool linear){vec2 size=vec2(mt_pack_levels[view*14+level].yz);
q=mt_packed_wrap(q,mt_pack_sampler[view].xy);
if(!linear)return mt_packed_texel(view,level,ivec2(floor(q*size)));
vec2 p=q*size-.5,f=fract(p);ivec2 a=ivec2(floor(p));
return mix(mix(mt_packed_texel(view,level,a),mt_packed_texel(view,level,a+ivec2(1,0)),f.x),
 mix(mt_packed_texel(view,level,a+ivec2(0,1)),mt_packed_texel(view,level,a+ivec2(1,1)),f.x),f.y);}
vec4 mt_packed_sample(int view,vec2 q,vec2 dx,vec2 dy){int count=mt_pack_info[view].x;
if(count==0)return view==3?vec4(0,0,0,1):vec4(1);
vec2 size=vec2(mt_pack_levels[view*14].yz);float lod=log2(max(max(length(dx*size),length(dy*size)),1e-20));
ivec2 filtering=mt_pack_sampler[view].zw;
// The c=0 magnification boundary is explicitly permitted by coreOpenGL.
if(lod<=0.)return mt_packed_level(view,0,q,filtering.y==9729);
if(filtering.x==9728||filtering.x==9729)return mt_packed_level(view,0,q,filtering.x==9729);
float level=clamp(lod,0.,float(count-1));bool linear=filtering.x==9985||filtering.x==9987;
if(filtering.x==9984||filtering.x==9985)return mt_packed_level(view,max(0,int(ceil(level+.5))-1),q,linear);
int a=int(floor(level)),b=min(a+1,count-1);return mix(mt_packed_level(view,a,q,linear),mt_packed_level(view,b,q,linear),fract(level));}
vec4 mt_packed_read(int view,vec2 q){return mt_packed_sample(view,q,dFdx(q),dFdy(q));}
// Reconstruct canonical detail in coverage-premultiplied space. Protected
// texels may contain arbitrary unused normal bytes and must not bleed in.
vec3 mt_baked_detail(out float coverage){
ivec2 size=textureSize(tex13,0);vec2 p=texcoord*vec2(size)-.5;ivec2 base=ivec2(floor(p));vec2 f=fract(p);vec3 sum=vec3(0);coverage=0;
for(int y=0;y<2;++y)for(int x=0;x<2;++x){ivec2 q=clamp(base+ivec2(x,y),ivec2(0),size-1);float w=(x==1?f.x:1-f.x)*(y==1?f.y:1-f.y)*texelFetch(tex15,q,0).r;coverage+=w;sum+=(texelFetch(tex13,q,0).rgb*2-1)*w;}
return coverage>1e-8?sum/coverage:vec3(0,0,1);
}
vec2 mt_original_emission_uv(vec2 a,vec2 b,vec3 N,vec3 V,float mask){
int transform=mt[8].x<.5?0:3;vec2 q=mt_coord[3]==1?b:a;
return dome_mtoon_uv(mt,q,mt_offset[transform],mt_scale[transform],mt_rotation[transform],false,false,false,mt_time,mask);}
vec3 mt_source_normal(vec3 encoded){
vec3 N=dome_mtoon_unit(source_n);bool legacy=mt[8].x<.5;
if(!legacy&&!gl_FrontFacing)N=-N;
if(mt[11].x<.5)return N;
vec3 ts=dome_mtoon_normal_map(encoded,mt[6].z);
if(legacy)return dome_mtoon_unit(source_t*ts.x+source_b*ts.y+source_n*ts.z);
vec3 T=dome_mtoon_unit(source_t),B=dome_mtoon_unit(cross(N,T))*(source_w>0?1:-1);
return dome_mtoon_unit(T*ts.x+B*ts.y+N*ts.z);
}
vec4 mt_surface(vec3 geometric,vec3 V,out bool killed){
vec3 N=dome_mtoon_unit(source_frame==1?source_n:geometric);float mask=mt_scalar(8,mt_read(8,mt_uv(8,texcoord,texcoord1,N,V,1)));
vec2 normalUV=mt_uv(2,texcoord,texcoord1,N,V,mask),a=dFdx(normalUV),b=dFdy(normalUV);vec3 dx=dFdx(world),dy=dFdy(world);
float determinant=a.x*b.y-a.y*b.x;vec4 tangent=vec4(1,0,0,1);bool valid=abs(determinant)>1e-8*max(length(a)*length(b),1e-20);
if(valid){vec3 T=(dx*b.y-dy*a.y)/determinant,B=(dy*a.x-dx*b.x)/determinant;T=dome_mtoon_unit(T-N*dot(N,T));tangent=vec4(T,dot(cross(N,T),B)<0?-1:1);}
if(mt_baked_normal==0){
if(source_frame==1){N=mt_source_normal(mt_read(2,normalUV).rgb);if(mt[8].x<.5)N=dome_mtoon_facing_normal(0,N,V,gl_FrontFacing,mt_outline==1);}
else if(valid)N=dome_mtoon_material_normal(mt,N,tangent,mt_read(2,normalUV).rgb,V,gl_FrontFacing,mt_outline==1);
else N=dome_mtoon_facing_normal(int(mt[8].x),N,V,gl_FrontFacing,mt_outline==1);
}else{
bool back=mt[8].x>.5&&!gl_FrontFacing;vec3 original=back?-N:N;
if(source_frame==1)original=mt_source_normal(mt_read(2,normalUV).rgb);
else if(valid&&mt[11].x>.5){vec3 T=tangent.xyz,B=cross(original,T)*tangent.w,ts=dome_mtoon_normal_map(mt_read(2,normalUV).rgb,mt[6].z);original=dome_mtoon_unit(T*ts.x+B*ts.y+original*ts.z);}
float coverage;vec3 detail=mt_baked_detail(coverage);vec3 Tc=canonical_t,Bc=canonical_b,Nc=canonical_n;
vec3 canonical=dome_mtoon_unit(Tc*detail.x+(back?-Bc:Bc)*detail.y+(back?-Nc:Nc)*detail.z);
N=dome_mtoon_unit(mix(original,canonical,coverage));if(mt[8].x<.5)N=dome_mtoon_facing_normal(0,N,V,gl_FrontFacing,mt_outline==1);
}
DomeMToonSamples s;s.base=mt_read(0,mt_uv(0,texcoord,texcoord1,N,V,mask));s.shade=mt_read(1,mt_uv(1,texcoord,texcoord1,N,V,mask)).rgb;
if(mt_derived[0]!=0){float coverage=mt_read(11,texcoord).r;vec4 original=mt_read(12,mt_uv(12,texcoord,texcoord1,N,V,mask));vec4 pbr=mt_packed_read(0,mt_uv(14,texcoord,texcoord1,N,V,mask));
vec3 paint=coverage==0?vec3(0):max(vec3(0),s.base.rgb-mt_native_color.rgb*pbr.rgb*(1-coverage));
if(mt_shade_alias==1){vec3 contribution;for(int k=0;k<3;++k)contribution[k]=mt_native_color[k]!=0?paint[k]/mt_native_color[k]:paint[k];s.shade=max(vec3(0),s.shade*(1-coverage)+contribution);}
s.base=coverage==0?mt_effective_color*original:max(vec4(0),s.base+(mt_effective_color*original-mt_native_color*pbr)*(1-coverage));s.base.a=clamp(s.base.a,0,1);}
s.emission=mt_read(3,mt_uv(3,texcoord,texcoord1,N,V,mask)).rgb;
if(mt_derived[3]!=0){float coverage=mt_packed_read(3,texcoord).r;
vec3 original=mt_packed_read(1,mt_original_emission_uv(texcoord,texcoord1,N,V,mask)).rgb;
vec2 genericUV=vec2(dot(mt_pbr_x[1],vec3(texcoord,1)),dot(mt_pbr_y[1],vec3(texcoord,1)));
vec3 generic=mt_packed_read(2,genericUV).rgb;
s.emission=coverage==0?mt_effective_emission*original:max(vec3(0),s.emission+(mt_effective_emission*original-mt_native_emission*generic)*(1-coverage));}
s.rim=mt_read(4,mt_uv(4,texcoord,texcoord1,N,V,mask)).rgb;s.matcap=mt_read(5,mt_uv(5,texcoord,texcoord1,N,V,mask)).rgb;
s.shift=mt_scalar(6,mt_read(6,mt_uv(6,texcoord,texcoord1,N,V,mask)));s.receiveShadow=mt_scalar(9,mt_read(9,mt_uv(9,texcoord,texcoord1,N,V,mask)));s.grade=mt_scalar(10,mt_read(10,mt_uv(10,texcoord,texcoord1,N,V,mask)));
DomeMToonLight q;q.normal=N;q.view=V;q.direction=normalize(vec3(-.4,.8,.7));q.color=vec3(1);q.giUp=vec3(.25);q.giDown=vec3(.10);q.gi=mix(q.giDown,q.giUp,clamp(.5+.5*N.y,0,1));q.shadow=1;q.normalOriented=true;
return dome_mtoon_shade(mt,s,q,gl_FrontFacing,mt_outline==1,killed);
}
)GLSL";}
} // namespace dome
