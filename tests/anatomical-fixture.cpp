// SPDX-License-Identifier: GPL-3.0-or-later
// Repository CC0 source -> genuinely unskinned glTF -> actual native import and
// explicit metadata-landmark fit. Artist weight values never enter the solver.
#include "dome/import-authoring.hpp"
#include "dome/rig-fit.hpp"
#include "dome/source-maps.hpp"
#include "gltf-document.hpp"
#include "native-anatomical-weights.hpp"
#include <fstream>
#include <iostream>
#include <set>
using namespace dome;
namespace {
using Json=nlohmann::json;
uint32_t word(std::istream&f){std::array<unsigned char,4>b{};f.read(reinterpret_cast<char*>(b.data()),4);if(!f)throw std::runtime_error("Truncated source word");return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;}
void require(bool b,const char*why){if(!b)throw std::runtime_error(why);}
Json static_source(const std::filesystem::path&input,const std::filesystem::path&output){
 std::ifstream file(input,std::ios::binary);require(bool(file),"Repository source asset must exist");
 const auto magic=word(file),version=word(file),length=word(file),json_length=word(file),json_type=word(file);
 require(magic==0x46546c67&&version==2&&json_type==0x4e4f534a&&json_length<=4u*1024u*1024u&&length<=64u*1024u*1024u,"Bounded repository GLB2 source required");
 std::string text(json_length,' ');file.read(text.data(),text.size());require(bool(file),"Complete source JSON required");auto j=Json::parse(text);const auto metadata=j;
 const auto bin_length=word(file),bin_type=word(file);const auto bin_offset=size_t(file.tellg());
 require(bin_type==0x004e4942&&bin_offset+bin_length==length&&j.at("buffers").size()==1&&!j.at("buffers")[0].contains("uri"),"Single complete embedded source BIN required");
 require(j.at("accessors").size()<=16384&&j.at("bufferViews").size()<=16384&&j.at("nodes").size()<=4096,"Bounded source tables required");
 std::set<size_t>accessors,removed_accessors,views,removed_views;
 auto keep=[&](Json&index){auto v=index.get<size_t>();require(v<j.at("accessors").size(),"Actual accessor index required");accessors.insert(v);};
 for(auto&m:j.at("meshes"))for(auto&p:m.at("primitives")){
  auto&attributes=p.at("attributes");std::vector<std::string>remove;
  for(auto&entry:attributes.items())if(entry.key().starts_with("JOINTS_")||entry.key().starts_with("WEIGHTS_")){removed_accessors.insert(entry.value().get<size_t>());remove.push_back(entry.key());}else keep(entry.value());
  for(auto&key:remove)attributes.erase(key);if(p.contains("indices"))keep(p["indices"]);
  if(p.contains("targets"))for(auto&t:p["targets"])for(auto&entry:t.items())keep(entry.value());
 }
 if(j.contains("skins"))for(auto&s:j["skins"])if(s.contains("inverseBindMatrices"))removed_accessors.insert(s["inverseBindMatrices"].get<size_t>());
 auto accessor_views=[&](size_t a,std::set<size_t>&out){const auto&x=j.at("accessors").at(a);if(x.contains("bufferView"))out.insert(x.at("bufferView").get<size_t>());if(x.contains("sparse")){out.insert(x.at("sparse").at("indices").at("bufferView").get<size_t>());out.insert(x.at("sparse").at("values").at("bufferView").get<size_t>());}};
 for(auto a:accessors)accessor_views(a,views);for(auto a:removed_accessors)accessor_views(a,removed_views);
 if(j.contains("images"))for(auto&image:j["images"]){require(image.contains("bufferView")&&!image.contains("uri"),"Repository images must be embedded");views.insert(image.at("bufferView").get<size_t>());}
 for(auto v:views)for(auto w:removed_views){
  const auto&a=j.at("bufferViews").at(v);const auto&b=j.at("bufferViews").at(w);
  const size_t x=a.value("byteOffset",size_t(0)),y=b.value("byteOffset",size_t(0)),n=a.at("byteLength").get<size_t>(),m=b.at("byteLength").get<size_t>();
  require(x<=bin_length&&n<=bin_length-x&&y<=bin_length&&m<=bin_length-y,"Bounded original retained/skin intervals required");
  require(!n||!m||x+n<=y||y+m<=x,"Overlapping retained/artist skin payload requires a separate fixture adapter");
 }
 detail::GltfDocument document;auto&out=document.json;out=j;out.erase("skins");out.erase("animations");
 for(auto&node:out["nodes"])node.erase("skin");
 if(out.contains("extensions")){out["extensions"].erase("VRM");if(out["extensions"].empty())out.erase("extensions");}
 for(auto key:{"extensionsUsed","extensionsRequired"})if(out.contains(key)){auto&v=out[key];v.erase(std::remove(v.begin(),v.end(),Json("VRM")),v.end());if(v.empty())out.erase(key);}
 out["accessors"]=Json::array();out["bufferViews"]=Json::array();std::map<size_t,size_t>amap,vmap;size_t copied=0;
 for(auto v:views){auto view=j.at("bufferViews").at(v);require(view.value("buffer",size_t(0))==0,"Actual single source buffer owner required");size_t offset=view.value("byteOffset",size_t(0)),bytes=view.at("byteLength").get<size_t>();require(offset<=bin_length&&bytes<=bin_length-offset,"Actual bounded source view required");while(document.binary.size()%4)document.binary.push_back(0);view["byteOffset"]=document.binary.size();vmap[v]=out["bufferViews"].size();out["bufferViews"].push_back(view);auto start=document.binary.size();require(start<=64u*1024u*1024u&&bytes<=64u*1024u*1024u-start,"Total retained static BIN budget exceeded");document.binary.resize(start+bytes);file.seekg(bin_offset+offset);file.read(reinterpret_cast<char*>(document.binary.data()+start),bytes);require(bool(file),"Complete retained view required");copied+=bytes;}
 for(auto a:accessors){auto accessor=j.at("accessors").at(a);if(accessor.contains("bufferView"))accessor["bufferView"]=vmap.at(accessor.at("bufferView").get<size_t>());if(accessor.contains("sparse"))for(auto key:{"indices","values"})accessor["sparse"][key]["bufferView"]=vmap.at(accessor["sparse"][key]["bufferView"].get<size_t>());amap[a]=out["accessors"].size();out["accessors"].push_back(std::move(accessor));}
 for(auto&m:out["meshes"])for(auto&p:m["primitives"]){for(auto&entry:p["attributes"].items())entry.value()=amap.at(entry.value().get<size_t>());if(p.contains("indices"))p["indices"]=amap.at(p["indices"].get<size_t>());if(p.contains("targets"))for(auto&t:p["targets"])for(auto&entry:t.items())entry.value()=amap.at(entry.value().get<size_t>());}
 if(out.contains("images"))for(auto&image:out["images"])image["bufferView"]=vmap.at(image["bufferView"].get<size_t>());
 out["buffers"]=Json::array({Json{{"byteLength",document.binary.size()}}});require(!removed_accessors.empty()&&copied<bin_length,"Actual original skin payload must be omitted, not merely unreferenced");
 detail::write_glb(std::move(document),output);std::cout<<"SOURCE retained_accessors="<<accessors.size()<<" removed_skin_accessors="<<removed_accessors.size()<<" retained_views="<<views.size()<<" retained_bytes="<<copied<<" original_BIN="<<bin_length<<std::endl;return metadata;
}
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::invalid_argument("Repository CC0 VRM asset, output fixture directory");const std::filesystem::path input=argv[1],out=argv[2];std::filesystem::create_directories(out);const auto metadata=static_source(input,out/"unskinned.glb");
 ImportLimits limits;limits.mode=ImportMode::PreserveGltf;const auto raw=import_mesh(out/"unskinned.glb",limits);require(raw.skins.empty(),"Genuine static source must have no imported skins");for(auto&p:raw.primitives)require(p.influences.empty(),"Genuine static source must have no imported artist influence values");
 ImportAuthoringOptions choices;choices.paint_resolution=64;auto imported=prepare_authoring_import(Scene{},out/"unskinned.glb",choices);auto&scene=imported.scene;require(imported.objects.size()==19,"Pinned repository base must retain all nineteen parts");
 uint64_t reference=0;size_t largest=0;for(auto id:imported.objects){const auto&mesh=scene.object(id).painted->mesh();if(mesh.positions.size()>largest){reference=id;largest=mesh.positions.size();}}
 require(reference!=0,"Actual imported reference geometry required");HumanLandmarks landmarks;landmarks.left={-1,0,0};landmarks.front={0,0,-1};const auto inverse=scene.world_matrix(reference).inverse();
 for(const auto&entry:metadata.at("extensions").at("VRM").at("humanoid").at("humanBones")){auto role=entry.at("bone").get<std::string>();if(role.ends_with("ThumbProximal"))role.replace(role.size()-8,8,"Metacarpal");else if(role.ends_with("ThumbIntermediate"))role.replace(role.size()-12,12,"Proximal");auto node=entry.at("node").get<size_t>();require(node<raw.nodes.size(),"Authored landmark node must remain in genuine retained source graph");const auto&m=raw.nodes[node].world;const auto p=inverse.point({m[12],m[13],m[14]});landmarks.points.emplace(role,Vec3{float(p[0]),float(p[1]),float(p[2])});}
 RigFitOptions fit;fit.bind=false;const auto fitted=author_humanoid_fit(scene,reference,landmarks,fit);require(fitted.bones>0&&!fitted.bound&&scene.object(reference).deformation->skeleton&&!scene.object(reference).deformation->skin,"Real metadata-guided native fit must remain unweighted");scene.save(out/"fitted-unskinned.dsculpt");const auto before=scene.encode();
 auto request=detail::capture_native_anatomical(scene,imported.objects,reference);auto prepared=detail::prepare_native_anatomical(request);const auto report=prepared.report();require(request.matches(scene)&&scene.encode()==before,"Actual owning preparation must preserve complete input");require(detail::adopt_native_anatomical(scene,request,std::move(prepared)),"Actual owning expected-field adoption required");scene.save(out/"bound.dsculpt");
 std::ofstream(out/"report.json")<<Json{{"repository_asset",input.string()},{"parts",imported.objects.size()},{"reference",reference},{"bones",fitted.bones},{"native_landmarks",landmarks.encode()},{"source_has_skin_or_influences",false},{"work",report.work},{"workspace",report.workspace},{"total_admission",report.total_admission},{"changed",report.changed},{"expected_field","real owning factory; public Document path tested separately"}}.dump(2);
 std::cout<<"PASS repository anatomical fixture parts="<<imported.objects.size()<<" reference="<<reference<<" work="<<report.work<<" workspace="<<report.workspace<<" total="<<report.total_admission<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
