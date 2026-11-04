// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <limits>
#include "paint-overlay-packet.hpp"
// Independent finite-field oracle: declared nearest source sampler, public
// sparse paint values, composition before bilinear filtering, and PNG bytes.
// It deliberately does not call either production sampling implementation.
namespace physical_oracle {
struct Result {std::array<double,3> original{},ideal{},encoded{},quantization_bound{};};
inline double decode(double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}
inline std::array<float,3> bary(std::array<float,2>a,std::array<float,2>b,std::array<float,2>c,float x,float y){
 const double den=(double(b[1])-c[1])*(a[0]-c[0])+(double(c[0])-b[0])*(a[1]-c[1]);
 const float p=float(((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den),q=float(((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den);
 return {p,q,1-p-q};
}
inline Result boundary(const dome::Scene &scene,const dome::MeshSnapshot &snapshot,
                       const cgltf_material &material){
 const auto &mesh=snapshot.mesh;const auto &range=snapshot.ranges.front();
 const auto &paint=*scene.object(range.object).painted;
 const auto &maps=*range.textures->source_maps;const auto &view=maps.emission;
 check(view.image&&view.mag_filter==9728,"Independent counterexample owns a real nearest-filtered source image");
 const auto &coords=view.texcoord==0?mesh.uv:view.texcoord==1?*maps.uv1:*maps.extra_uv_sets.at(uint32_t(view.texcoord));
 auto nearest=[&](size_t face,std::array<float,3>w){
  std::array<double,2> uv{};for(int k=0;k<3;++k)for(int c=0;c<2;++c)uv[c]+=coords[mesh.indices[face*3+k]][c]*w[k];
  const auto &m=view.uv_transform;std::array<double,2> transformed{m[0]*uv[0]+m[1]*uv[1]+m[4],m[2]*uv[0]+m[3]*uv[1]+m[5]};
  auto wrap=[](double x,int mode){if(mode==33071)return std::clamp(x,0.,1.);if(mode==33648){x-=2*std::floor(x/2);return x<=1?x:2-x;}return x-std::floor(x);};
  const auto x=std::clamp(int(std::floor(wrap(transformed[0],view.wrap_s)*view.image->width)),0,int(view.image->width)-1),y=std::clamp(int(std::floor(wrap(transformed[1],view.wrap_t)*view.image->height)),0,int(view.image->height)-1);
  std::array<double,3> value{};for(int c=0;c<3;++c)value[c]=decode(view.image->rgba[(size_t(y)*view.image->width+x)*4+c]/255.);return value;
 };
 auto coefficients=[&](float u,float v){
  using Terms=std::array<double,7>;const auto r=paint.resolution();const double px=double(u)*r-.5,py=double(v)*r-.5;const int ix=int(std::floor(px)),iy=int(std::floor(py));const double fx=px-ix,fy=py-iy;Terms value{};
  check(!paint.baked(),"Independent float-composition fixture has no unmodeled baked field");
  for(int k=0;k<4;++k){const auto x=std::clamp(ix+(k&1),0,int(r)-1),y=std::clamp(iy+(k>>1),0,int(r)-1);const double weight=((k&1)?fx:1-fx)*((k>>1)?fy:1-fy);Terms local{};
   const auto owner=range.textures->overlay->padding()->at(size_t(y)*r+x);if(owner<0)continue;const auto sx=uint32_t(owner)%r,sy=uint32_t(owner)/r;
   for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity>0){const auto common=paint.pixel(layer.id,sx,sy);const auto emission=paint.emission_pixel(layer.id,sx,sy);const double a=double(layer.opacity)*common.data[13]/255.*emission.coverage;
    for(int c=0;c<3;++c){local[c]=local[c]*(1-a)+emission.legacy[c]*a;local[3+c]=local[3+c]*(1-a)+emission.physical[c]*a;}local[6]=local[6]*(1-a)+a;
   }
   for(int c=0;c<7;++c)value[c]+=local[c]*weight;
  }return value;
 };
 auto field=[&](size_t face,std::array<float,3>w,float u,float v){const auto source=nearest(face,w);const auto terms=coefficients(u,v);std::array<double,3> out{};auto base=range.material.emissive;if(range.material_bindings&&range.material_bindings->physical_emission)base=*range.material_bindings->physical_emission;for(int c=0;c<3;++c)out[c]=base[c]*source[c]*(1-terms[6])+terms[c]+terms[3+c];return out;};
 const float wx=2*(47.5f/Fbo::size)-1,wy=2*(62.5f/Fbo::size)-1;std::array<float,2> current{};size_t face=SIZE_MAX;std::array<float,3>w{};
 for(size_t at=range.first;at<range.first+range.count;at+=3){const auto a=mesh.positions[mesh.indices[at]],b=mesh.positions[mesh.indices[at+1]],c=mesh.positions[mesh.indices[at+2]];auto q=bary({a.x,a.y},{b.x,b.y},{c.x,c.y},wx,wy);if(*std::min_element(q.begin(),q.end())>=0){face=at/3;w=q;for(int k=0;k<3;++k)for(int c=0;c<2;++c)current[c]+=mesh.uv[mesh.indices[at+k]][c]*q[k];break;}}
 check(face!=SIZE_MAX,"Independent boundary pixel has an actual posed source triangle");Result result;result.original=field(face,w,current[0],current[1]);
 auto image=color_consumer::image(material.emissive_texture.texture);const double tx=current[0]*image.width-.5,ty=current[1]*image.height-.5;const int ix=int(std::floor(tx)),iy=int(std::floor(ty));const double fx=tx-ix,fy=ty-iy;
 const double strength=material.has_emissive_strength?material.emissive_strength.emissive_strength:1;
 double unit_bound=0;for(int q=0;q<256;++q)for(double delta:{-.5,.5})unit_bound=std::max(unit_bound,std::abs(decode(q/255.)-decode(std::clamp((q+delta)/255.,0.,1.))));
 for(int c=0;c<3;++c)result.quantization_bound[c]=strength*material.emissive_factor[c]*(unit_bound+8*std::numeric_limits<float>::epsilon());
 for(int k=0;k<4;++k){const auto x=std::clamp(ix+(k&1),0,int(image.width)-1),y=std::clamp(iy+(k>>1),0,int(image.height)-1);const double weight=((k&1)?fx:1-fx)*((k>>1)?fy:1-fy);const float u=(x+.5f)/image.width,v=(y+.5f)/image.height;bool found=false;
  for(size_t at=range.first;at<range.first+range.count;at+=3){auto q=bary(mesh.uv[mesh.indices[at]],mesh.uv[mesh.indices[at+1]],mesh.uv[mesh.indices[at+2]],u,v);if(*std::min_element(q.begin(),q.end())>=0){auto value=field(at/3,q,u,v);for(int c=0;c<3;++c)result.ideal[c]+=value[c]*weight;found=true;break;}}
  check(found,"All four finite texel centers are inside genuine triangles, not guessed padding");
  for(int c=0;c<3;++c)result.encoded[c]+=decode(image.rgba[(size_t(y)*image.width+x)*4+c]/255.)*material.emissive_factor[c]*strength*weight;
 }
 for(int c=0;c<3;++c)check(std::abs(result.encoded[c]-result.ideal[c])<=result.quantization_bound[c]+1e-5,"Independent PNG factor reconstruction obeys the conservative eight-bit linear-radiance bound");
 check(result.ideal[0]-result.original[0]>4*result.quantization_bound[0],"Independent source/paint composition proves the finite-filter boundary error dominates quantization");
 std::cout<<"INDEPENDENT_PHYSICAL_FIELD original="<<result.original[0]<<" ideal="<<result.ideal[0]<<" png="<<result.encoded[0]<<" quantization_bound="<<result.quantization_bound[0]<<std::endl;
 return result;
}
}
