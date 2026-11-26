// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-artist-normal-fixture.hpp"
#include "source-artist-normal-oracle.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/normal-sampling.hpp"

namespace source_prepared_oracle {
// Independent double-precision consumer: no production basis, sampler or
// source-normal resolver is used to derive the expected directions.
using D=std::array<double,3>;
void finite_sample(D a){for(auto v:a)if(!std::isfinite(v))throw std::runtime_error("Independent consumer encountered a nonfinite sample");}
D add(D a,D b){for(size_t k=0;k<3;++k)a[k]+=b[k];return a;}
D mul(D a,double s){for(auto &v:a)v*=s;return a;}
D unit(D a){finite_sample(a);double n=0;for(auto v:a)n+=v*v;if(!std::isfinite(n)||n<1e-20)throw std::runtime_error("Independent singular normal");return mul(a,1/std::sqrt(n));}
D vc(Vec3 a){return {a.x,a.y,a.z};}
D crossd(D a,D b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
D sample(const Image &im,std::array<double,2> uv,bool srgb=false){
 if(!std::isfinite(uv[0])||!std::isfinite(uv[1])||!im.width||!im.height||im.rgba.size()!=size_t(im.width)*im.height*4)throw std::runtime_error("Independent consumer has invalid finite image/UV input");
 const double x=uv[0]*im.width-.5,y=uv[1]*im.height-.5;
 if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e9||std::abs(y)>1e9)throw std::runtime_error("Independent consumer image coordinate exceeds finite bounds");const int ix=int(std::floor(x)),iy=int(std::floor(y));D out{};
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,int(im.width)-1),py=std::clamp(iy+dy,0,int(im.height)-1);const double w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(size_t c=0;c<3;++c){double value=im.rgba[(size_t(py)*im.width+px)*4+c]/255.;if(srgb)value=value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);out[c]+=w*value;}}
 finite_sample(out);return out;
}
D direction(const SculptObject &o,size_t triangle,std::array<float,3> bary,size_t role,
    const AffineTransform &world={}){
 const auto &binding=*o.material_bindings;const auto &frames=role==0?*binding.normal_frames->pbr:*binding.normal_frames->mtoon;
 D n{},t{},b{};std::array<double,2> uv{};const auto &mesh=o.painted->mesh();
 const auto &m=world.matrix();
 const double determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);
 const double orientation=determinant<0?-1:1;
 for(size_t k=0;k<3;++k){const auto c=triangle*3+k;const auto v=mesh.indices[c];const auto nt=unit(vc(source_artist_oracle::normal(m,frames.normals[c])));const auto tt=frames.tangents[c];const auto tangent=unit(vc(source_artist_oracle::direction(m,{tt[0],tt[1],tt[2]})));n=add(n,mul(nt,bary[k]));t=add(t,mul(tangent,bary[k]));b=add(b,mul(unit(crossd(nt,tangent)),bary[k]*tt[3]*orientation));for(size_t q=0;q<2;++q)uv[q]+=mesh.uv[v][q]*bary[k];}
 const bool legacy=binding.material.version()==MToonVersion::V0;
 if(role==0){n=unit(n);t=unit(t);b=unit(b);}else if(!legacy){n=unit(n);t=unit(t);b=mul(unit(crossd(n,t)),frames.tangents[triangle*3][3]*orientation);}
 const auto maps=o.painted->preview(o.material);const auto encoded=sample(role==0?maps->normal:maps->mtoon_normal,uv);const double scale=role==0?1:binding.material.parameters.normal_scale;
 D ts{(encoded[0]*2-1)*scale,(encoded[1]*2-1)*scale,encoded[2]*2-1};
 return unit(add(add(mul(t,ts[0]),mul(b,ts[1])),mul(n,ts[2])));
}
double error(D a,Vec3 b){finite_sample(a);finite_sample(vc(b));double e=0;for(size_t k=0;k<3;++k)e=std::max(e,std::abs(a[k]-vc(b)[k]));return e;}
std::shared_ptr<const SourceMaterialBindings> blank_typed(bool legacy){
 auto b=std::make_shared<SourceMaterialBindings>();b->material=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"}}):MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});b->validate();return b;
}
uint64_t target(Scene &scene,const Mesh &mesh,bool legacy){
 const auto id=scene.add("Independent prepared destination",Volume{});auto &o=scene.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(mesh,64);o.material_bindings=blank_typed(legacy);return id;
}
} // namespace source_prepared_oracle
