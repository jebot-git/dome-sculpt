// SPDX-License-Identifier: GPL-3.0-or-later
#include "refined-vertex-stage.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
namespace {
Vec3 unit(Vec3 v){const float m=std::max({std::abs(v.x),std::abs(v.y),std::abs(v.z)});if(!std::isfinite(m))throw std::invalid_argument("Nonfinite before vertex stage");if(m<=1e-20f)return {0,0,1};v=v/m;return v/std::sqrt(dot(v,v));}
Vec3 linear(const std::array<float,16>&m,Vec3 v){return {m[0]*v.x+m[4]*v.y+m[8]*v.z,m[1]*v.x+m[5]*v.y+m[9]*v.z,m[2]*v.x+m[6]*v.y+m[10]*v.z};}
Vec3 normal(const std::array<float,9>&m,Vec3 v){return {m[0]*v.x+m[3]*v.y+m[6]*v.z,m[1]*v.x+m[4]*v.y+m[7]*v.z,m[2]*v.x+m[5]*v.y+m[8]*v.z};}
}
VertexStageMatrices vertex_stage_affine(const AffineTransform &a){VertexStageMatrices out;out.model=a.float_matrix();const auto inverse=a.inverse().float_matrix();for(size_t c=0;c<3;++c)for(size_t r=0;r<3;++r)out.normal[c*3+r]=inverse[r*4+c];const auto sign=a.orientation();if(sign==AffineOrientation::Singular)throw std::invalid_argument("Singular before vertex transform");out.hand=sign==AffineOrientation::Reversing?-1.f:1.f;return out;}
PulledBackVertexStage pullback_vertex_stage(const KeyMirrorRefinement &r,
 BeforeVertexColumns before,const VertexStageMatrices &m,VertexBitangentPolicy policy,
 const KeyMirrorLimits &limits){
 if(!r.source||!limits.max_bytes||limits.max_bytes>256*1024*1024||!limits.max_work||limits.max_work>256*1024*1024||size_t(policy)>1||(m.hand!=1&&m.hand!=-1))throw std::invalid_argument("Invalid before vertex pullback policy");
 auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Before vertex pullback cancelled");};stop();
 const size_t count=r.source->indices.size(),n=r.corners.size();
 if(!count||count%3||count>6'000'000||!n||n%3||n>6'000'000||r.opposite.size()!=n||before.positions.size()!=count||before.normals.size()!=count||(!before.tangents.empty()&&before.tangents.size()!=count))throw std::invalid_argument("Before vertex pullback needs explicit logical corner columns");
 if(!std::all_of(m.model.begin(),m.model.end(),[](float x){return std::isfinite(x);})||!std::all_of(m.normal.begin(),m.normal.end(),[](float x){return std::isfinite(x);}))throw std::invalid_argument("Before vertex uniforms are nonfinite");
 size_t bytes=sizeof(PulledBackVertexStage);auto charge=[&](size_t amount,size_t unit){if(amount>(limits.max_bytes-bytes)/unit)throw std::length_error("Before vertex pullback working quota exceeded before allocation");bytes+=amount*unit;};if(bytes>limits.max_bytes)throw std::length_error("Before vertex pullback working quota exceeded before allocation");
 charge(before.positions.size()+before.normals.size(),sizeof(Vec3));charge(before.tangents.size(),sizeof(std::array<float,4>));charge(r.corners.capacity(),sizeof(MirrorRefinementCorner));charge(r.opposite.capacity(),sizeof(uint32_t));charge(n,before.tangents.empty()?2*sizeof(Vec3):4*sizeof(Vec3));
 if(n>limits.max_work/3)throw std::length_error("Before vertex pullback work quota exceeded before allocation");
 PulledBackVertexStage out;out.working_bytes=bytes;out.source_vertex_evaluations=n*3;out.positions.resize(n);out.normals.resize(n);if(!before.tangents.empty()){out.tangents.resize(n);out.bitangents.resize(n);}
 for(size_t c=0;c<n;++c){stop();const auto &p=r.corners[c].current;if(p.triangle>=count/3||r.opposite[c]>=n||r.opposite[r.opposite[c]]!=c)throw std::invalid_argument("Before vertex pullback lost actual corner provenance");double sum=0;
  for(size_t k=0;k<3;++k){const auto w=p.barycentric[k];if(!std::isfinite(w)||w< -1e-12||w>1+1e-12)throw std::invalid_argument("Before vertex pullback barycentrics are invalid");sum+=w;const size_t at=p.triangle*3+k;const auto point=before.positions[at],N=before.normals[at];if(!finite(point)||!finite(N))throw std::invalid_argument("Before vertex columns are nonfinite");auto world=linear(m.model,point)+Vec3{m.model[12],m.model[13],m.model[14]};auto nvec=unit(normal(m.normal,unit(N)));if(!finite(world))throw std::invalid_argument("Before vertex position is unrepresentable");out.positions[c]+=world*float(w);out.normals[c]+=nvec*float(w);
   if(!before.tangents.empty()){const auto t=before.tangents[at];if(!finite(Vec3{t[0],t[1],t[2]})||(t[3]!=1&&t[3]!=-1))throw std::invalid_argument("Before vertex tangent is invalid");auto tvec=unit(linear(m.model,unit({t[0],t[1],t[2]})));auto bvec=cross(nvec,tvec)*(t[3]*m.hand);if(policy==VertexBitangentPolicy::UnitCross)bvec=unit(bvec);out.tangents[c]+=tvec*float(w);out.bitangents[c]+=bvec*float(w);}
  }if(std::abs(sum-1)>1e-12)throw std::invalid_argument("Before vertex pullback is not affine");
 }stop();return out;
}
}
