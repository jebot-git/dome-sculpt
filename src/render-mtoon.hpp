// SPDX-License-Identifier: GPL-3.0-or-later
// Host GLSL integration around the separately licensed native MToon module.
#pragma once
#include "dome/mtoon.hpp"
namespace dome {
inline std::string mtoon_renderer_common(){
  return std::string(mtoon_glsl())+R"GLSL(
uniform int mt_mode,mt_outline,mt_present[16],mt_coord[16],mt_derived[16],mt_channel[16];
uniform vec4 mt[12];uniform vec2 mt_offset[16],mt_scale[16];uniform float mt_rotation[16],mt_time;
uniform vec3 mt_camera_up,mt_camera_forward;uniform float mt_aspect;
uniform vec3 mt_pbr_x[2],mt_pbr_y[2];
uniform sampler2D tex0,tex1,tex2,tex3,tex4,tex5,tex6,tex7,tex8,tex9,tex10,tex11,tex12,tex13,tex14,tex15;
vec4 mt_read(int r,vec2 q){
if(mt_present[r]==0){if(r==2)return vec4(.5,.5,1,1);if(r==5||r==6)return vec4(0);if(r==11)return vec4(0,0,0,1);return vec4(1);}
if(r==0)return texture(tex0,q);if(r==1)return texture(tex1,q);if(r==2)return texture(tex2,q);if(r==3)return texture(tex3,q);
if(r==4)return texture(tex4,q);if(r==5)return texture(tex5,q);if(r==6)return texture(tex6,q);if(r==7)return texture(tex7,q);
if(r==8)return texture(tex8,q);if(r==9)return texture(tex9,q);if(r==10)return texture(tex10,q);if(r==11)return texture(tex11,q);
if(r==12)return texture(tex12,q);if(r==13)return texture(tex13,q);if(r==14)return texture(tex14,q);return texture(tex15,q);}
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
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec2 uv;layout(location=3)in vec2 uv1;layout(location=4)in float selected_weight;
uniform mat4 model,mvp,view_projection;uniform vec3 camera;
out vec3 world,n;out vec2 texcoord,texcoord1;out float weight_value;
void main(){vec4 p=model*vec4(position,1);vec3 N=dome_mtoon_unit(mat3(model)*normal),V=dome_mtoon_unit(camera-p.xyz);
world=p.xyz;n=N;texcoord=uv;texcoord1=uv1;weight_value=selected_weight;gl_Position=mvp*vec4(position,1);
if(mt_mode==1&&mt_outline==1&&selected_weight<0){float mask=mt_scalar(8,mt_read(8,mt_uv(8,uv,uv1,N,V,1)));
float width=mt_scalar(7,mt_read(7,mt_uv(7,uv,uv1,N,V,mask)));
if(int(mt[10].x+.5)==1){world+=N*dome_mtoon_outline_world_width(mt,width);gl_Position=view_projection*vec4(world,1);}
else if(int(mt[10].x+.5)==2){vec2 projected=(view_projection*vec4(N,0)).xy;vec3 right=dome_mtoon_unit(cross(mt_camera_forward,mt_camera_up));
vec3 viewNormal=vec3(dot(N,right),dot(N,mt_camera_up),-dot(N,mt_camera_forward));gl_Position.xy+=dome_mtoon_outline_clip_offset(mt,projected,viewNormal,gl_Position.w,mt_aspect,width);}
}})GLSL";}
inline std::string mtoon_renderer_fragment(){return R"GLSL(
uniform vec4 mt_effective_color,mt_native_color;uniform int mt_shade_alias;
vec4 mt_surface(vec3 geometric,vec3 V,out bool killed){
vec3 N=dome_mtoon_unit(geometric);float mask=mt_scalar(8,mt_read(8,mt_uv(8,texcoord,texcoord1,N,V,1)));
vec2 normalUV=mt_uv(2,texcoord,texcoord1,N,V,mask),a=dFdx(normalUV),b=dFdy(normalUV);vec3 dx=dFdx(world),dy=dFdy(world);
float determinant=a.x*b.y-a.y*b.x;vec4 tangent=vec4(1,0,0,1);bool valid=abs(determinant)>1e-8*max(length(a)*length(b),1e-20);
if(valid){vec3 T=(dx*b.y-dy*a.y)/determinant,B=(dy*a.x-dx*b.x)/determinant;T=dome_mtoon_unit(T-N*dot(N,T));tangent=vec4(T,dot(cross(N,T),B)<0?-1:1);}
if(valid)N=dome_mtoon_material_normal(mt,N,tangent,mt_read(2,normalUV).rgb,V,gl_FrontFacing,mt_outline==1);
else N=dome_mtoon_facing_normal(int(mt[8].x),N,V,gl_FrontFacing,mt_outline==1);
DomeMToonSamples s;s.base=mt_read(0,mt_uv(0,texcoord,texcoord1,N,V,mask));s.shade=mt_read(1,mt_uv(1,texcoord,texcoord1,N,V,mask)).rgb;
if(mt_derived[0]!=0){float coverage=mt_read(11,texcoord).r;vec4 original=mt_read(12,mt_uv(12,texcoord,texcoord1,N,V,mask));vec4 pbr=mt_read(14,mt_uv(14,texcoord,texcoord1,N,V,mask));
vec3 paint=coverage==0?vec3(0):max(vec3(0),s.base.rgb-mt_native_color.rgb*pbr.rgb*(1-coverage));
if(mt_shade_alias==1){vec3 contribution;for(int k=0;k<3;++k)contribution[k]=mt_native_color[k]!=0?paint[k]/mt_native_color[k]:paint[k];s.shade=max(vec3(0),s.shade*(1-coverage)+contribution);}
s.base=coverage==0?mt_effective_color*original:max(vec4(0),s.base+(mt_effective_color*original-mt_native_color*pbr)*(1-coverage));s.base.a=clamp(s.base.a,0,1);}
s.emission=mt_read(3,mt_uv(3,texcoord,texcoord1,N,V,mask)).rgb;s.rim=mt_read(4,mt_uv(4,texcoord,texcoord1,N,V,mask)).rgb;s.matcap=mt_read(5,mt_uv(5,texcoord,texcoord1,N,V,mask)).rgb;
s.shift=mt_scalar(6,mt_read(6,mt_uv(6,texcoord,texcoord1,N,V,mask)));s.receiveShadow=mt_scalar(9,mt_read(9,mt_uv(9,texcoord,texcoord1,N,V,mask)));s.grade=mt_scalar(10,mt_read(10,mt_uv(10,texcoord,texcoord1,N,V,mask)));
DomeMToonLight q;q.normal=N;q.view=V;q.direction=normalize(vec3(-.4,.8,.7));q.color=vec3(1);q.giUp=vec3(.25);q.giDown=vec3(.10);q.gi=mix(q.giDown,q.giUp,clamp(.5+.5*N.y,0,1));q.shadow=1;q.normalOriented=true;
return dome_mtoon_shade(mt,s,q,gl_FrontFacing,mt_outline==1,killed);
}
)GLSL";}
} // namespace dome
