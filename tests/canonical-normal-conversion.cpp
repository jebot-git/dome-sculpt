// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_BAKED_NORMAL_EXPORT_HELPERS_ONLY
#include "baked-normal-export.cpp"
#include "canonical-normal-conversion.hpp"
#include "dome/tangents.hpp"
namespace {
Vec3 independent_unit(Vec3 v){double s=std::hypot(double(v.x),double(v.y),double(v.z));if(s<1e-12)throw std::runtime_error("Singular independent conversion oracle");return {float(v.x/s),float(v.y/s),float(v.z/s)};}
double angle(Vec3 a,Vec3 b){a=independent_unit(a);b=independent_unit(b);return std::atan2(double(length(cross(a,b))),std::clamp(double(dot(a,b)),-1.,1.));}
bool failure(const auto &call,std::string_view wanted){try{call();return false;}catch(const std::exception&e){return std::string_view(e.what()).find(wanted)!=std::string_view::npos;}}
void audit(const Scene &scene,const detail::CanonicalNormalConversionCandidate &candidate){
 const auto &object=scene.active();const auto &mesh=object.painted->mesh();const auto original=object.painted->composite(object.material);
 const auto frames=tangent_corners(mesh);const auto target=candidate.target_frames->pbr;const auto &image=*candidate.projection->pbr;
 size_t observed=0,changed=0;double maximum=0;
 for(size_t face=0;face<mesh.indices.size()/3;++face){
  std::array<std::array<double,2>,3> uv;for(size_t k=0;k<3;++k){const auto p=mesh.uv[mesh.indices[face*3+k]];uv[k]={p[0],p[1]};}
  const double determinant=(uv[1][1]-uv[2][1])*(uv[0][0]-uv[2][0])+(uv[2][0]-uv[1][0])*(uv[0][1]-uv[2][1]);
  for(uint32_t y=0;y<image.height;++y)for(uint32_t x=0;x<image.width;++x){
   const double u=(x+.5)/image.width,v=(y+.5)/image.height;
   std::array<double,3>w{((uv[1][1]-uv[2][1])*(u-uv[2][0])+(uv[2][0]-uv[1][0])*(v-uv[2][1]))/determinant,((uv[2][1]-uv[0][1])*(u-uv[2][0])+(uv[0][0]-uv[2][0])*(v-uv[2][1]))/determinant,0};w[2]=1-w[0]-w[1];
   // Strict interior excludes raster ownership edges; it is independent of
   // the baker's top-left coverage implementation and padding decisions.
   if(*std::min_element(w.begin(),w.end())<=1e-7)continue;
   const size_t at=(size_t(y)*image.width+x)*4;Vec3 old_n{},old_t{},old_b{},new_n{},new_t{},new_b{};
   for(size_t k=0;k<3;++k){const auto c=face*3+k;const auto n=independent_unit(mesh.normals[mesh.indices[c]]);const auto t=frames[c];const Vec3 tangent{t[0],t[1],t[2]};
    old_n+=n*float(w[k]);old_t+=tangent*float(w[k]);old_b+=cross(n,tangent)*(t[3]*float(w[k]));
    const auto nt=target->tangents[c];const auto nn=independent_unit(target->normals[c]);const auto tn=independent_unit({nt[0],nt[1],nt[2]});new_n+=nn*float(w[k]);new_t+=tn*float(w[k]);new_b+=independent_unit(cross(nn,tn))*(nt[3]*float(w[k]));
   }
   const auto &old=original->normal.rgba;const auto &converted=image.rgba;
   auto decode=[&](const auto &bytes){return Vec3{float(double(bytes[at])/127.5-1),float(double(bytes[at+1])/127.5-1),float(double(bytes[at+2])/127.5-1)};};
   const auto a=decode(old),b=decode(converted);const auto expected=old_t*a.x+old_b*a.y+old_n*a.z;
   const auto actual=independent_unit(new_t)*b.x+independent_unit(new_b)*b.y+independent_unit(new_n)*b.z;
   maximum=std::max(maximum,angle(expected,actual));changed+=!std::equal(old.begin()+at,old.begin()+at+4,converted.begin()+at);++observed;
  }
 }
 require_cage(observed>100&&maximum<=.02001,"Independent raw native TBN and normalized-column target decoder preserve actual interior sample directions under declared center bound");
 std::cout<<"CONVERSION_CENTERS "<<observed<<" changed="<<changed<<" max="<<maximum<<'\n';
 if(scene.active().painted->mesh().positions[1].z!=0)require_cage(changed>20,"Curved conversion changes actual stored pixels instead of relabeling historical normal data");
}
}
int main()try{
 for(int variant=0;variant<8;++variant){auto scene=baked_fan(variant&1,variant&2,variant&4);const auto exact=scene.encode();const auto paint=scene.active().painted;const auto deformation=scene.active().deformation;
  const ColorPreparationInput input{paint,scene.active().appearance,scene.active().material_bindings,deformation,scene.active().material};
  const auto result=detail::convert_native_canonical_normals(input);
  require_cage(result.original_paint==paint&&result.original_deformation==deformation&&result.sampled_centers>100&&result.raster_samples>=result.sampled_centers,"Detached real-baker conversion retains exact genuine original paint/deformation owners and actual sampling counts");
  require_cage(result.projection->pbr->rgba==result.projection->mtoon->rgba&&result.target_frames->current_targets.size()==size_t(bool(variant&2)),"Native PBR candidate owns real paired planes and complete current morph identities");audit(scene,result);
  require_cage(scene.encode()==exact&&scene.active().painted==paint&&scene.active().deformation==deformation,"Successful detached conversion preserves all native history, bake recipe, source, layer and shape bytes");
  NormalPreparationOptions too_small;too_small.max_working_bytes=1;
  require_cage(failure([&]{detail::convert_native_canonical_normals(input,too_small);},"memory budget")&&scene.encode()==exact,"Working memory denial preserves exact original source");
  too_small={};too_small.max_samples=1;require_cage(failure([&]{detail::convert_native_canonical_normals(input,too_small);},"sample budget"),"Sample resource bound is enforced before detached image work");
  size_t polls=0;NormalPreparationOptions options;options.cancelled=[&]{++polls;return false;};detail::convert_native_canonical_normals(input,options);
  for(auto cut:{size_t(1),std::max(size_t(2),polls/2),polls}){size_t at=0;options.cancelled=[&]{return ++at>=cut;};require_cage(failure([&]{detail::convert_native_canonical_normals(input,options);},"cancel")&&scene.encode()==exact,"Actual early/middle/late conversion cancellation preserves original source and supports retry");}
  require_cage(detail::convert_native_canonical_normals(input).projection->pbr->rgba==result.projection->pbr->rgba,"Cancelled conversion retries to identical actual converted normal pixels");
  if(variant&4){NormalPreparationOptions strict;strict.max_angular_error=0;require_cage(failure([&]{detail::convert_native_canonical_normals(input,strict);},"quantization"),"Zero-tolerance control refuses genuinely quantized curved conversion rather than silently relaxing quality");}
 }
 auto plain=baked_fan(false,false);auto clear=plain.active().painted->snapshot();clear->clear_bake();
 require_cage(failure([&]{detail::convert_native_canonical_normals({clear,{},{},{},plain.active().material});},"historical normal payload"),"No-bake input cannot claim conversion provenance");
 std::cout<<"PASS "<<cage_checks<<" native canonical conversion checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
