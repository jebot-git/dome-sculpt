// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/deformation.hpp"
#include <algorithm>
namespace source_artist_oracle {
using namespace dome;
// Independent small linear solve; never calls the production frame-preview,
// normal sampler, inverse-transpose helper or normal-bake basis/encoder.
inline Vec3 normal(const auto&m,Vec3 v){double a[3][4]{};for(size_t r=0;r<3;++r){for(size_t c=0;c<3;++c)a[r][c]=m[r*4+c];a[r][3]=r==0?v.x:r==1?v.y:v.z;}for(size_t c=0;c<3;++c){size_t pivot=c;for(size_t r=c+1;r<3;++r)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;if(std::abs(a[pivot][c])<1e-20)throw std::runtime_error("Independent normal solve singular");for(size_t k=c;k<4;++k)std::swap(a[c][k],a[pivot][k]);const double s=a[c][c];for(size_t k=c;k<4;++k)a[c][k]/=s;for(size_t r=0;r<3;++r)if(r!=c){const double w=a[r][c];for(size_t k=c;k<4;++k)a[r][k]-=w*a[c][k];}}return {float(a[0][3]),float(a[1][3]),float(a[2][3])};}
inline Vec3 direction(const auto&m,Vec3 v){return {float(m[0]*v.x+m[4]*v.y+m[8]*v.z),float(m[1]*v.x+m[5]*v.y+m[9]*v.z),float(m[2]*v.x+m[6]*v.y+m[10]*v.z)};}
inline int address(int q,int n,int wrap){if(wrap==33071)return std::clamp(q,0,n-1);if(wrap==33648){q=(q%(2*n)+2*n)%(2*n);return q<n?q:2*n-1-q;}return (q%n+n)%n;}
inline double wrap(double q,int mode){if(mode==33071)return std::clamp(q,0.,1.);if(mode==33648){q=std::fmod(q,2.);if(q<0)q+=2;return q<=1?q:2-q;}return q-std::floor(q);}
inline Vec3 lookup(const PaintSourceTexture&view,std::array<float,2>uv){const auto &m=view.uv_transform;const double u=double(m[0])*uv[0]+double(m[1])*uv[1]+m[4],v=double(m[2])*uv[0]+double(m[3])*uv[1]+m[5];const auto&im=*view.image;const float x=float(wrap(u,view.wrap_s))*im.width-.5f,y=float(wrap(v,view.wrap_t))*im.height-.5f;const bool nearest=view.mag_filter==9728;const int ix=nearest?int(std::floor(x+.5f)):int(std::floor(x)),iy=nearest?int(std::floor(y+.5f)):int(std::floor(y));const float fx=nearest?0:x-ix,fy=nearest?0:y-iy;Vec3 result;for(int dy=0;dy<(nearest?1:2);++dy)for(int dx=0;dx<(nearest?1:2);++dx){const auto at=(size_t(address(iy+dy,int(im.height),view.wrap_t))*im.width+size_t(address(ix+dx,int(im.width),view.wrap_s)))*4;const float w=nearest?1:(dx?fx:1-fx)*(dy?fy:1-fy);result+=Vec3{im.rgba[at]/255.f,im.rgba[at+1]/255.f,im.rgba[at+2]/255.f}*w;}return result;}
struct Point {Vec3 mapped{},unmapped{};};
class Oracle {
 const SculptObject &o;std::vector<RigMatrix> joints;
public:
 explicit Oracle(const SculptObject&source):o(source){if(o.deformation&&o.deformation->skeleton&&o.deformation->skin&&(!o.deformation->pose.empty()||o.deformation->skeleton->inverse_bind_override))joints=o.deformation->skeleton->skin_matrices(o.deformation->pose);}
 std::array<float,2> coordinate(uint32_t triangle,std::array<float,3>bary,int set,bool original)const{
  const auto &mesh=o.painted->mesh();const auto maps=o.painted->source_maps();const auto before=o.painted->source_sampling_original();const bool prepared=bool(before);const std::vector<std::array<float,2>>*values=nullptr;
  if(original&&prepared){if(set==0)values=&before->mesh().uv;else if(set==1){if(auto uv=o.painted->source_sampling_authored_uv1())values=uv.get();else if(before->source_maps())values=before->source_maps()->uv1.get();}else if(before->source_maps()&&before->source_maps()->extra_uv_sets.contains(uint32_t(set)))values=before->source_maps()->extra_uv_sets.at(uint32_t(set)).get();}
  else if(set==0)values=&mesh.uv;else if(maps){if(set==1)values=maps->uv1.get();else if(maps->extra_uv_sets.contains(uint32_t(set)))values=maps->extra_uv_sets.at(uint32_t(set)).get();}
  if(!values)throw std::runtime_error("Independent oracle missing actual role coordinate owner");std::array<double,2>sum{};for(size_t k=0;k<3;++k){auto v=mesh.indices.at(size_t(triangle)*3+k);if(original&&prepared)v=o.painted->source_sampling_vertices()[v];for(size_t c=0;c<2;++c)sum[c]+=double(values->at(v)[c])*bary[k];}return {float(sum[0]),float(sum[1])};
 }
 Point point(uint32_t triangle,std::array<float,3>bary,size_t role,const AffineTransform&world={})const{
  const auto&binding=*o.material_bindings;const auto&set=*binding.normal_frames;const auto frame=role?set.mtoon:set.pbr;if(!frame)throw std::runtime_error("Independent oracle absent declared frame");const auto&mesh=o.painted->mesh();Vec3 N,T,B;const bool tangent=!frame->tangents.empty();float sign=1;
  const auto&w=world.matrix();const double det=w[0]*(w[5]*w[10]-w[9]*w[6])-w[4]*(w[1]*w[10]-w[9]*w[2])+w[8]*(w[1]*w[6]-w[5]*w[2]);
  for(size_t k=0;k<3;++k){const auto c=size_t(triangle)*3+k;const auto vertex=mesh.indices[c];auto n=frame->normals.at(c);Vec3 t;float hand=1;if(tangent){const auto q=frame->tangents[c];t={q[0],q[1],q[2]};hand=q[3];}
   for(const auto&target:frame->targets){const auto found=std::find(set.current_targets.begin(),set.current_targets.end(),target.id);if(found==set.current_targets.end())continue;const auto slot=size_t(found-set.current_targets.begin());const float weight=o.deformation&&!o.deformation->morph_weights.empty()?o.deformation->morph_weights[slot]:0;if(!target.normals.empty())n+=target.normals[c]*weight;if(tangent&&!target.tangents.empty())t+=target.tangents[c]*weight;}
   if(!joints.empty()){Vec3 posed_n,posed_t;for(auto influence:o.deformation->skin->vertices.at(vertex)){posed_n+=normal(joints.at(influence.joint),n)*influence.weight;if(tangent)posed_t+=direction(joints.at(influence.joint),t)*influence.weight;}n=posed_n;t=posed_t;}
   n=normalized(normal(w,n));N+=n*bary[k];if(tangent){t=normalized(direction(w,t));hand*=det<0?-1:1;T+=t*bary[k];B+=cross(n,t)*(hand*bary[k]);if(k&&hand!=sign)throw std::runtime_error("Independent oracle mixed triangle handedness");sign=hand;}
  }
  Point out;out.unmapped=normalized(N);out.mapped=out.unmapped;const bool typed=role!=0;const auto&material=binding.material;PaintSourceTexture view;float scale=1;bool enabled=true;if(!typed){const auto maps=o.painted->source_maps();if(maps){view=maps->normal;scale=maps->normal_scale;}}else{const auto&slot=material.textures[size_t(MToonSlot::Normal)];enabled=material.parameters.normal_enabled&&slot.index>=0;view=binding.roles[size_t(MToonSlot::Normal)];scale=material.parameters.normal_scale;}
  if(!enabled||!view.image)return out;if(!tangent)throw std::runtime_error("Independent oracle textured N-only role");const bool legacy=typed&&material.version()==MToonVersion::V0;if(!typed){N=normalized(N);T=normalized(T);B=normalized(B);}else if(!legacy){N=normalized(N);T=normalized(T);B=normalized(cross(N,T))*sign;}
  const auto&slot=material.textures[size_t(MToonSlot::Normal)];const int coordinate_set=typed?(slot.transform_texcoord>=0?slot.transform_texcoord:int(slot.texcoord)):view.texcoord;auto uv=coordinate(triangle,bary,coordinate_set,typed);
  if(typed){const auto&st=material.textures[legacy?size_t(MToonSlot::Base):size_t(MToonSlot::Normal)];const float a=std::cos(st.rotation),b=std::sin(st.rotation),x=uv[0]*st.scale[0],y=uv[1]*st.scale[1];uv={st.offset[0]+a*x-b*y,st.offset[1]+b*x+a*y};}
  const auto encoded=lookup(view,uv);auto q=Vec3{(encoded.x*2-1)*scale,(encoded.y*2-1)*scale,encoded.z*2-1};q=length(q)<1e-12f?Vec3{0,0,1}:normalized(q);out.mapped=normalized(T*q.x+B*q.y+N*q.z);return out;
 }
};
}
