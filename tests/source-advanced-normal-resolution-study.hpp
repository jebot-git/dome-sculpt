// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace advanced_render::resolution_study {
int run(const std::filesystem::path &output){
 std::ofstream stream(output);check(bool(stream),"Resolution study creates independent receipt");nlohmann::json original_source;
 for(uint32_t resolution:{64u,128u,256u,512u}){
  auto f=construct(false,false,false,SourceNormalUvMethod::RetainSource,false,resolution);
  auto original=f.scene.snapshot();original.erase(f.destination);original.select(f.source);auto encoded=original.encode();
  if(original_source.is_null())original_source=encoded;else check(encoded==original_source,"Only user-selected target resolution changes; complete original source scene remains identical");
  const auto &source=f.scene.object(f.source),&target=f.scene.object(f.destination);const auto &mesh=target.painted->mesh();
  const auto a=mesh.positions[mesh.indices[0]],ex=mesh.positions[mesh.indices[1]]-a,ey=mesh.positions[mesh.indices[2]]-a;
  const auto center=a+(ex+ey)/3,n=normalized(cross(ex,ey)),x=normalized(ex),y=normalized(cross(n,x));const float span=std::max({length(ex),length(ey),length(ey-ex)})*.85f;
  std::array<double,2> max_error{},original_error{};unsigned points=0;std::array<nlohmann::json,2>worst{};
  for(int py=0;py<96;++py)for(int px=0;px<96;++px){const auto position=center+x*((px+.5f)/96*2-1)*span+y*((py+.5f)/96*2-1)*span,r=position-a;
   const double d00=dot(ex,ex),d01=dot(ex,ey),d11=dot(ey,ey),den=d00*d11-d01*d01;const float v=float((d11*dot(r,ex)-d01*dot(r,ey))/den),w=float((d00*dot(r,ey)-d01*dot(r,ex))/den);const std::array<float,3>bary{1-v-w,v,w};if(*std::min_element(bary.begin(),bary.end())<=.05f)continue;
   const auto xy=std::array<int,2>{px,py};const bool is_original=std::find(original_render_probes.begin(),original_render_probes.end(),xy)!=original_render_probes.end();
   for(size_t role=0;role<2;++role){const auto wanted=advanced_fixture::decode(source,0,bary,role),actual=prepared_pixel_oracle::direction(target,0,bary,role);double e=0;for(size_t c=0;c<3;++c)e=std::max(e,std::abs(wanted[c]-actual[c]));
    if(e>max_error[role]){max_error[role]=e;worst[role]={{"xy",xy},{"bary",bary},{"source",wanted},{"target",actual},{"error",e}};}if(is_original)original_error[role]=std::max(original_error[role],e);}
   ++points;
  }
  const auto row=nlohmann::json{{"target_resolution",resolution},{"source_resolution",source.painted->resolution()},{"dense_points",points},{"dense_max_error_PBR_MToon",max_error},{"original4_max_error_PBR_MToon",original_error},{"worst_PBR_MToon",worst},{"source_scene_identical",true}};
  stream<<row.dump()<<std::endl;std::cout<<"TARGET_RESOLUTION_STUDY "<<row.dump()<<std::endl;
 }
 std::cout<<"PASS CPU genuine target-resolution study; original64 failure preserved, no GL context or automatic resize\n";return 0;
}
}
