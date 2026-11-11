// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/native-authoring.hpp"
#include "dome/source-uv-bake.hpp"
#include "texture-set-export.hpp"
#include "gltf-document.hpp"
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char*s){if(!yes)throw std::runtime_error(s);++checks;}
template<class F>void refuses(F&&f,const char*s){bool caught=false;try{f();}catch(const std::exception&e){caught=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(caught,s);}
std::vector<uint8_t>bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing actual export file");return {std::istreambuf_iterator<char>(f),{}};}
Scene fixture(bool empty=false){
 Mesh m;m.positions={{-.8f,-.8f,0},{.8f,-.8f,0},{0,.8f,0},{-.8f,-.8f,.5f},{.8f,-.8f,.5f},{0,.8f,.5f}};m.normals.assign(6,{0,0,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}}};m.indices={0,1,2,3,4,5};
 Scene s;std::vector<TextureSetInitialField>fields(empty?3:2);for(auto&f:fields){f.resolution=64;f.material.double_sided=true;}for(size_t f=0;f<fields.size();++f)fields[f].material.name="Material "+std::to_string(f);
 fields.back().material.color={.1f,.7f,.3f,.6f};fields.back().material.alpha=Material::AlphaMode::Mask;fields.back().material.alpha_cutoff=.3f;
 initialize_texture_sets(s,s.active().id,m,fields,std::array<TextureSetId,2>{empty?1u:0u,empty?2u:1u});
 DeformationState d(m);Skeleton rig;Bone bone;bone.name="Actual joint";bone.rest.translation={.1f,.2f,.3f};rig.bones.push_back(bone);d.skeleton=rig;d.skin=SkinWeights{};d.skin->vertices.assign(6,VertexWeights{{0,1}});d.pose={bone.rest};d.pose[0].translation.x+=.2f;
 d.morphs=MorphSet(m);MorphTarget morph;morph.name="Actual shape";morph.delta.assign(6,Vec3{.01f,.02f,.03f});d.morphs->targets.push_back(morph);d.morph_weights={.4f};s.set_deformation(s.active().id,std::move(d));return s;
}
void inspect(const detail::GltfDocument&doc,size_t fields){
 const auto&j=doc.json;check(j.at("meshes").size()==1,"One native mesh remains one glTF mesh");const auto&primitives=j.at("meshes")[0].at("primitives");check(primitives.size()==fields,"Every nonempty set emits one real primitive");
 check(j.at("skins").size()==1&&doc.objects.size()==1,"Material sets never duplicate the native rig or emitted object mapping");
 size_t mesh_nodes=0;for(const auto&node:j.at("nodes"))if(node.contains("mesh"))++mesh_nodes;check(mesh_nodes==1,"Every material range remains beneath the same actual surface node");
 const auto&base=primitives[0];size_t total=0;const auto index_view=j.at("accessors").at(base.at("indices").get<size_t>()).at("bufferView");
 for(const auto&p:primitives){const auto&a=j.at("accessors").at(p.at("indices").get<size_t>());total+=a.at("count").get<size_t>();check(p.at("attributes")==base.at("attributes"),"Material primitives share actual POSITION/NORMAL/UV/JOINTS/WEIGHTS accessors");check(p.at("targets")==base.at("targets"),"Every primitive retains the same actual morph accessor identities");check(a.at("bufferView")==index_view,"All material ranges use slices of one shared index buffer view");}
 check(total==6,"Material membership covers every original triangle exactly once");check(j.at("meshes")[0].at("weights")==nlohmann::json::array({.4f}),"Actual authored morph weights are retained");
}
void consumer(const std::filesystem::path&path,size_t fields){
 cgltf_options options{};cgltf_data*data=nullptr;const auto file=bytes(path);check(cgltf_parse(&options,file.data(),file.size(),&data)==cgltf_result_success,"Independent cgltf parser opens the actual emitted GLB");
 struct Owner{cgltf_data*p;~Owner(){cgltf_free(p);}}owner{data};check(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success&&cgltf_validate(data)==cgltf_result_success,"Independent cgltf validates real buffers/accessors/node/skin/morph associations");
 check(data->meshes_count==1&&data->meshes[0].primitives_count==fields&&data->skins_count==1,"Independent consumer sees the same single mesh/rig with distinct material ranges");
 for(size_t p=0;p<fields;++p){const auto&primitive=data->meshes[0].primitives[p];check(primitive.indices->count==3&&primitive.targets_count==1&&primitive.material,"Independent primitives retain nonempty membership, actual morph and material");}
}
void portable(){
 for(bool empty:{false,true}){auto scene=fixture(empty);const auto exact=scene.encode();auto surfaces=scene.export_surfaces();check(surfaces.size()==1&&surfaces[0].texture_set_surface&&surfaces[0].mesh.positions.empty(),"Actual Scene export preserves its immutable owning geometry bundle");const auto doc=detail::make_gltf(surfaces);inspect(doc,2);
  const auto path=empty?"empty-primary.glb":"portable.glb";export_glb(surfaces,path);consumer(path,2);const auto&j=doc.json;check(j.at("materials").size()==2,"An empty primary material does not emit a fake empty primitive or unused material");check(j.at("materials")[1].at("alphaMode")=="MASK"&&j.at("materials")[1].at("alphaCutoff")==.3f,"Each set retains its own material alpha policy");check(scene.encode()==exact,"Portable extraction and compilation cannot mutate native fields, memberships, pose or source owners");
 }
}
void authoring(){
 auto scene=fixture();const auto exact=scene.encode();NativeAuthoringExport owner;owner.scene=std::make_shared<const Scene>(scene.snapshot());owner.allow_portable_fallback=true;
 export_glb_authoring(owner,"editable.glb");consumer("editable.glb",2);const auto restored=extract_native_authoring(bytes("editable.glb"));check(restored&&restored->encode()==exact,"Editable GLB restores the complete actual Scene40 fields/skin/morph/material/selection endpoint");
 Document doc;doc.replace(scene);check(doc.wait_mesh(std::chrono::seconds(30)),"Actual native mesh worker publishes the initial set-aware endpoint");auto task=doc.export_asset("native-editable.glb",AssetExportFormat::EditableGlb256);task.get();consumer("native-editable.glb",2);const auto native=extract_native_authoring(bytes("native-editable.glb"));check(native&&native->encode()==exact&&doc.copy_scene().encode()==exact,"Real native export worker preserves exact authoring without changing the current document");
 auto portable=doc.export_asset("native-portable.glb",AssetExportFormat::Glb);portable.get();consumer("native-portable.glb",2);
 SourceUvBakeOptions policy;policy.max_working_bytes=1024;const auto old=bytes("editable.glb");refuses([&]{export_glb_authoring(owner,"editable.glb",{},policy);},"A small actual native working budget refuses before staging the material fields");check(bytes("editable.glb")==old,"Rejected native working budget preserves the previous durable output");
}
void authority(){
 auto scene=fixture();const auto exact=scene.encode();auto surfaces=scene.export_surfaces();const auto original=surfaces[0].texture_set_surface;
 surfaces[0].texture_set_surface=std::shared_ptr<const detail::TextureSetSurfacePlan>(original.get(),[](auto*){});refuses([&]{detail::make_gltf(surfaces);},"Borrowed plan address with a foreign control block cannot export");surfaces=scene.export_surfaces();surfaces[0].id++;refuses([&]{detail::make_gltf(surfaces);},"Another object identity cannot borrow a genuine material plan");surfaces=scene.export_surfaces();surfaces[0].deformation=std::make_shared<const DeformationState>(*scene.active().deformation);refuses([&]{detail::make_gltf(surfaces);},"Numerically equal pose with a foreign control block cannot replace plan authority");
 surfaces=scene.export_surfaces();TextureSetLimits limits;limits.maximum_bytes=1024;refuses([&]{detail::compile_texture_set_export(surfaces[0],limits);},"Explicit per-material layout budget refuses before geometry expansion");limits={};limits.cancelled=[]{return true;};refuses([&]{detail::compile_texture_set_export(surfaces[0],limits);},"Material export cancellation keeps the source endpoint exact");
 check(scene.encode()==exact,"Every plan/pose/budget/cancellation refusal keeps all authoring values exact");
}
}
int main(int argc,char**argv)try{if(argc>1){std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);}portable();authoring();authority();std::cout<<"PASS "<<checks<<" texture-set multi-primitive export controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
