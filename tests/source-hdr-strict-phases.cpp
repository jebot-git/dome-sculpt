// SPDX-License-Identifier: GPL-3.0-or-later
// Independent bounded qualification; ordinary complete entry point is unchanged.
#define DOME_JOINT_SOURCE_EMBEDDED
#include "source-joint-writable-normal-bake.cpp"
#undef DOME_JOINT_SOURCE_EMBEDDED
#include <chrono>
#include <exception>
namespace hdr_phases {
using joint_source::require;
struct Phase {
 const char* name;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();int exceptions=std::uncaught_exceptions();
 explicit Phase(const char*n):name(n){std::cout<<"PHASE_BEGIN "<<name<<std::endl;}
 ~Phase(){std::cout<<"PHASE_END "<<name<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" completed="<<(std::uncaught_exceptions()==exceptions)<<std::endl;}
};
// The immutable original artifact was produced by Json::dump, whose documented
// binary_t text representation is an object. Restore that representation only;
// otherwise typed exact equality would compare binary values with plain objects.
void restore_dumped_binary(nlohmann::json&j){
 if(j.is_object()&&j.size()==2&&j.contains("bytes")&&j.contains("subtype")&&j.at("bytes").is_array()){
  const auto&subtype=j.at("subtype");if(!subtype.is_null()&&!subtype.is_number_unsigned())throw std::invalid_argument("Invalid dumped binary subtype");
  std::vector<uint8_t>bytes;bytes.reserve(j.at("bytes").size());for(const auto&v:j.at("bytes")){if(!v.is_number_integer()||v<0||v>255)throw std::invalid_argument("Invalid dumped binary byte");bytes.push_back(v.get<uint8_t>());}
  if(subtype.is_null())j=nlohmann::json::binary(std::move(bytes));else j=nlohmann::json::binary(std::move(bytes),subtype.get<uint64_t>());return;
 }
 if(j.is_array()||j.is_object())for(auto&v:j)restore_dumped_binary(v);
}
nlohmann::json original_source(const std::filesystem::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("Missing actual original source JSON");auto j=nlohmann::json::parse(f);require(j.at("version")==30,"Actual captured pre-HDR source is Scene30");restore_dumped_binary(j);return j;}
void source_preserved(const Scene&s,uint64_t source,const nlohmann::json&original){auto source_only=s;source_only.erase(s.active().id);source_only.select(source);auto context=source_only.encode();context["next_id"]=original.at("next_id");require(context==original,"All actual joint original/source/normal/paint/pose/UV history remains exact");}
void input(const Scene&s,uint64_t source,uint32_t resolution){require(s.active().id!=source&&s.active().painted->resolution()==resolution&&s.object(source).painted->resolution()==64,"Exact actual selected target and genuine source64 remain independent");}
void native(const std::filesystem::path&before_path,const std::filesystem::path&original_path,const std::filesystem::path&out){
 Phase all("native-total");std::filesystem::create_directories(out);Scene s;{Phase p("load-before");s=Scene::load(before_path);}const auto original=original_source(original_path);const auto source=original.at("active").get<uint64_t>();input(s,source,2048);source_preserved(s,source,original);
 auto source_scene=s.snapshot();source_scene.select(source);const auto raw=detail::raw_bake_source(source_scene.active());const auto resolver=detail::source_bake_normals(source_scene.active(),{});require(raw&&!raw->source_only&&raw->overlay&&raw->normal_projection&&resolver.matches_prepared_source(*raw),"Joint source resolves colour factors separately while authenticating actual normal/material/paint owners");double direct=0,omitted_normal=0,wrong_normal=0;const auto triangles=source_scene.active().painted->mesh().indices.size()/3;for(size_t tri=0;tri<triangles;++tri){const std::array<float,3>bary{.25f,.35f,.4f};const auto wanted=joint_source::decode(source_scene.active(),tri,bary,0);direct=std::max(direct,error(wanted,resolver(tri,bary)[0]));const auto omitted=joint_source::decode(source_scene.active(),tri,bary,0,false,true),wrong=joint_source::decode(source_scene.active(),tri,bary,0,true,false);for(size_t axis=0;axis<3;++axis){omitted_normal=std::max(omitted_normal,std::abs(wanted[axis]-omitted[axis]));wrong_normal=std::max(wrong_normal,std::abs(wanted[axis]-wrong[axis]));}}require(direct<3e-6&&omitted_normal>.05&&wrong_normal>.05,"Independent joint live source normal equation rejects omitted and wrong-normal-domain controls");
 auto final_settings=joint_source::settings();final_settings.normal_filter_quality=1;final_settings.emission_transport=1;{Phase p("native-bake");require(bake_object(s,source,final_settings).projected>500,"Actual combined Source8 SOURCE second Bake projects");}
 constexpr bool seam=false,live=false,repeat=false;const auto destination=s.active().id;
 using joint_source::decode;using joint_source::paint_uv;using joint_source::physical;
 {Phase p("original-interior-oracles-and-native-save");
 const auto target_maps=s.active().painted->preview(s.active().material);const auto material=detail::EmissionExportAccess::other_channels(*s.object(source).painted,s.object(source).material,{});const auto&low=s.active().painted->mesh();double normal_error=0,material_error=0,emission_error=0,peak=0,wrong_material=0,omitted_pbr=0;size_t interior=0;
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){const std::array<double,2>q{(x+.5)/64.,(y+.5)/64.};for(size_t tri=0;tri<triangles;++tri){const auto a=low.uv[low.indices[tri*3]],b=low.uv[low.indices[tri*3+1]],c=low.uv[low.indices[tri*3+2]];const double den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);require(std::isfinite(den)&&std::abs(den)>1e-20,"Independent finite joint target UV determinant");const double u=((b[1]-c[1])*(q[0]-c[0])+(c[0]-b[0])*(q[1]-c[1]))/den,v=((c[1]-a[1])*(q[0]-c[0])+(a[0]-c[0])*(q[1]-c[1]))/den;if(std::min({u,v,1-u-v})<.08)continue;const std::array<float,3>bary{float(u),float(v),float(1-u-v)};const auto wanted=decode(s.object(source),tri,bary,0);for(size_t role=0;role<2;++role){const auto actual=direction(s.active(),tri,bary,role,target_maps.get());finite_sample(actual);for(size_t axis=0;axis<3;++axis)normal_error=std::max(normal_error,std::abs(wanted[axis]-actual[axis]));}const auto root=paint_uv(s.object(source),tri,bary);const auto wanted_color=sample(material->pbr_color,root,true),actual_color=sample(s.active().painted->baked()->maps.pbr_color,q,true);const auto legacy=sample(material->color,root,true),actual_legacy=sample(s.active().painted->baked()->maps.color,q,true);std::array<double,2> current_uv{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)current_uv[axis]+=double(s.object(source).painted->mesh().uv[s.object(source).painted->mesh().indices[tri*3+k]][axis])*bary[k];const auto wrong=sample(material->pbr_color,current_uv,true);for(size_t axis=0;axis<3;++axis){material_error=std::max({material_error,std::abs(wanted_color[axis]-actual_color[axis]),std::abs(legacy[axis]-actual_legacy[axis])});wrong_material=std::max(wrong_material,std::abs(wanted_color[axis]-wrong[axis]));omitted_pbr=std::max(omitted_pbr,std::abs(wanted_color[axis]-legacy[axis]));}const auto emission=physical(*s.object(source).painted,root);const auto&baked=*s.active().painted->baked();require(bool(baked.emission_field)!=bool(baked.emission_domain),"Joint second Bake owns exactly one actual independent FLOAT output");const auto value=(baked.emission_domain?baked.emission_domain->field():baked.emission_field)->sample(float(q[0]),float(q[1])).resolve({},1);for(size_t axis=0;axis<3;++axis){emission_error=std::max(emission_error,std::abs(emission[axis]-value[axis]));peak=std::max(peak,emission[axis]);}++interior;break;}}
 std::cout<<"JOINT_OBSERVATION seam="<<seam<<" live="<<live<<" repeat="<<repeat<<" direct="<<direct<<" normal="<<normal_error<<" material="<<material_error<<" wrongMaterial="<<wrong_material<<" omittedPbr="<<omitted_pbr<<" omittedNormal="<<omitted_normal<<" wrongNormal="<<wrong_normal<<" emission="<<emission_error<<" peak="<<peak<<" interior="<<interior<<std::endl;
 const auto source_world=s.world_transform(source),target_world=s.world_transform(destination);std::cout<<"JOINT_WORLD source="<<source_world.position.x<<','<<source_world.position.y<<','<<source_world.position.z<<" rotation="<<source_world.orientation.w<<','<<source_world.orientation.x<<','<<source_world.orientation.y<<','<<source_world.orientation.z<<" scale="<<source_world.scale<<" target="<<target_world.position.x<<','<<target_world.position.y<<','<<target_world.position.z<<" rotation="<<target_world.orientation.w<<','<<target_world.orientation.x<<','<<target_world.orientation.y<<','<<target_world.orientation.z<<" scale="<<target_world.scale<<std::endl;
 std::ofstream(out/"observed-source.json")<<original.dump(2);s.save(out/"accepted.dsculpt");
 require(interior>500&&normal_error<.035&&material_error<.025&&wrong_material>.05&&omitted_pbr>.05&&emission_error<2e-4&&peak>1,"Independent joint normal, current corner colour and HDR retain unchanged exact numerical bounds");
 }
 source_preserved(s,source,original);
}
void oracle(const std::filesystem::path&accepted_path,const std::filesystem::path&original_path,size_t partition,size_t partitions,const std::filesystem::path&out){
 Phase all("oracle-total");std::filesystem::create_directories(out);Scene s;{Phase p("load-accepted");s=Scene::load(accepted_path);}const auto original=original_source(original_path);const auto source=original.at("active").get<uint64_t>();const uint32_t checked_resolution=2048;input(s,source,checked_resolution);const size_t triangles=s.active().painted->mesh().indices.size()/3;require(partitions&&partition<partitions&&partitions<=triangles,"Every explicit dense partition has an actual triangle range");const size_t first=triangles*partition/partitions,end=triangles*(partition+1)/partitions;
 using joint_source::decode;using joint_source::paint_uv;using joint_source::physical;
 {Phase p("dense-original-oracles");
  const auto&baked=*s.active().painted->baked();require(baked.emission_domain&&!baked.emission_field&&baked.emission_domain->field()->resolution()==64&&baked.emission_domain->target_resolution()==checked_resolution,"Checked HDR owns the genuine unchanged source64 field independently from selected target grid");
  // Independent immutable double texels, read once for THIS accepted phase.
  // They come from original layer/pixel data, never the production field,
  // projection resolver or transfer result. No cache survives a mutation.
  const auto&source_paint=*s.object(source).painted;const int source_r=source_paint.resolution();std::vector<D>radiance(size_t(source_r)*source_r);
  for(int y=0;y<source_r;++y)for(int x=0;x<source_r;++x)radiance[size_t(y)*source_r+x]=physical(source_paint,{(x+.5)/source_r,(y+.5)/source_r});
  auto expected_hdr=[&](std::array<double,2>q){const double x=q[0]*source_r-.5,y=q[1]*source_r-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));D value{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto&pixel=radiance[size_t(std::clamp(iy+dy,0,source_r-1))*source_r+std::clamp(ix+dx,0,source_r-1)];value=add(value,mul(pixel,(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy))));}return value;};
  double hdr_maximum=0;uint64_t hdr_samples=0;std::array<double,2>hdr_worst{};
  const auto samples=owning_filtered::offcentre(s,source,[&](const SculptObject &object,size_t tri,std::array<float,3>bary,size_t role){
   if(role==0){const auto root=paint_uv(object,tri,bary);std::array<double,2>q{};const auto&mesh=s.active().painted->mesh();for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)q[axis]+=double(mesh.uv[mesh.indices[tri*3+k]][axis])*bary[k];const auto wanted=expected_hdr(root);const auto actual=baked.emission_domain->field()->sample(float(q[0]),float(q[1])).resolve({},1);for(size_t axis=0;axis<3;++axis){if(!std::isfinite(actual[axis]))throw std::runtime_error("Independent dense HDR consumer encountered nonfinite output");const double error=std::abs(wanted[axis]-actual[axis]);if(error>hdr_maximum){hdr_maximum=error;hdr_worst=q;}}++hdr_samples;}
   return decode(object,tri,bary,role);},[](const SculptObject &object,size_t tri,std::array<float,3>bary,size_t role,const TextureMaps *maps){return direction(object,tri,bary,role,maps);},require,first,end);
  std::cout<<std::setprecision(12)<<"CHECKED_HDR_EXTREMUM samples="<<hdr_samples<<" absolute="<<hdr_maximum<<" uv="<<hdr_worst[0]<<','<<hdr_worst[1]<<std::endl;
  require(hdr_samples>300&&hdr_maximum<2e-4,"Every independent current-grid offcentre HDR sample retains the unchanged2e-4 bound");
  require(hdr_samples==samples,"Every retained normal-role sample has the same independent HDR position");
  std::cout<<"PHASE_ORACLE_COVERAGE first="<<first<<" end="<<end<<" triangles="<<triangles<<" samples_per_role="<<samples<<" hdr_samples="<<hdr_samples<<" target="<<checked_resolution<<" offsets=25 roles=2"<<std::endl;
 }
 source_preserved(s,source,original);
}
void endpoint(const std::filesystem::path&before_path,const std::filesystem::path&accepted_path,const std::filesystem::path&original_path,const std::filesystem::path&out){
 Phase all("endpoint-total");std::filesystem::create_directories(out);Scene s,before_scene;{Phase p("load-actual-endpoints");before_scene=Scene::load(before_path);s=Scene::load(accepted_path);}const auto original=original_source(original_path);const auto source=original.at("active").get<uint64_t>();input(before_scene,source,2048);input(s,source,2048);nlohmann::json accepted,before;{Phase p("encode-actual-endpoints");accepted=s.encode();before=before_scene.encode();}const auto native=before_scene.snapshot();const auto final_settings=s.active().bake_recipe->settings;require(final_settings.normal_filter_quality==1&&final_settings.emission_transport==1,"Exact accepted recipe remains checked1 and retained emission1");
 {Phase p("native-reopen-compare");require(Scene::load(accepted_path).encode()==accepted,"Combined SOURCE Save/reopen full owning graph exact");}
 Document doc;{Phase p("document-baseline");doc.replace(native);require(doc.wait_mesh(),"Joint SOURCE worker baseline publishes");}
 {Phase p("document-bake-ordered-save");doc.bake_from_object(source,final_settings);doc.save_async(out/"ordered.dsculpt").get();require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==accepted,"Joint actual worker and immediately ordered Save match native endpoint");}
 {Phase p("document-undo-redo");require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,"Joint SOURCE Bake owns one exact UndoRedo boundary");}
 source_preserved(s,source,original);
}
}
int main(int argc,char**argv)try{
 const std::string_view mode=argc>1?argv[1]:"";
 if(mode=="--native"&&argc==5)hdr_phases::native(argv[2],argv[3],argv[4]);
 else if(mode=="--oracle"&&argc==7)hdr_phases::oracle(argv[2],argv[3],std::stoul(argv[4]),std::stoul(argv[5]),argv[6]);
 else if(mode=="--endpoint"&&argc==6)hdr_phases::endpoint(argv[2],argv[3],argv[4],argv[5]);
 else return 2;
 std::cout<<"PASS "<<joint_source::checks<<" original HDR bounded phase controls"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
