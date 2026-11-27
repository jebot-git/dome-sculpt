// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/native-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "mtoon-painted-set-fixture.hpp"
#include "dome/emission-native.hpp"
#include "emission-export.hpp"
#include "emission-exr-owned.hpp"
#include "emission-paint-access.hpp"
#include "dome/source-uv-bake.hpp"
#include "texture-set-export.hpp"
#include "paint-overlay-packet.hpp"
#include "gltf-document.hpp"
#include "paint-emission-sample.hpp"
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <bit>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char*s){if(!yes)throw std::runtime_error(s);++checks;}
template<class F>void refuses(F&&f,const char*s){bool caught=false;try{f();}catch(const std::exception&e){caught=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(caught,s);}
std::vector<uint8_t>bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing actual export file");return {std::istreambuf_iterator<char>(f),{}};}
Scene fixture(bool empty=false,uint32_t resolution=64,uint32_t primary_resolution=0){
 Mesh m;m.positions={{-.8f,-.8f,0},{.8f,-.8f,0},{0,.8f,0},{-.8f,-.8f,.5f},{.8f,-.8f,.5f},{0,.8f,.5f}};m.normals.assign(6,{0,0,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}}};m.indices={0,1,2,3,4,5};
 Scene s;std::vector<TextureSetInitialField>fields(empty?3:2);for(auto&f:fields){f.resolution=resolution;f.material.double_sided=true;}for(size_t f=0;f<fields.size();++f)fields[f].material.name="Material "+std::to_string(f);
 if(primary_resolution)fields[0].resolution=primary_resolution;
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
void mtoon_authoring(){
 auto scene=fixture();
 const auto object=scene.active().id;
 const auto native=texture_set_view(scene.active(),1).material;
 SourceMaterialBindings binding;
 binding.material=MToonMaterial::from_vrm1({
  {"alphaMode","MASK"},{"alphaCutoff",.45f},{"doubleSided",true},
  {"pbrMetallicRoughness",{{"baseColorTexture",{{"index",1}}}}},
  {"extensions",{{"VRMC_materials_mtoon",{
     {"specVersion","1.0"},{"shadeColorFactor",{.3,.1,.7}},
     {"shadeMultiplyTexture",{{"index",0},{"extensions",{{"KHR_texture_transform",{{"offset",{.125,.25}}}}}}}},
     {"uvAnimationScrollXSpeedFactor",.15f}
  }}}}
 },2);
 binding.native_reference={native.color,native.emissive,int(native.alpha),native.alpha_cutoff,native.double_sided};
 auto &base=binding.roles[size_t(MToonSlot::Base)];
 base.image=std::make_shared<const Image>(Image{2,1,{203,78,152,0,116,28,110,255}});
 base.wrap_s=33071;
 base.mag_filter=9728;
 auto &shade=binding.roles[size_t(MToonSlot::Shade)];
 shade.image=std::make_shared<const Image>(Image{1,1,{29,126,193,255}});
 shade.wrap_t=33648;
 check(bind_texture_set_mtoon(scene,object,1,binding),"The editable export fixture owns an actual MToon field");
 check(edit_texture_set(scene,object,1,[](PaintedMesh &,Material &m){m.color[0]*=.5f;return true;}),
       "The typed export consumes the current native color adjustment");
 const auto exact=scene.encode();
 const auto portable=detail::make_gltf(scene.export_surfaces());
 inspect(portable,2);
 const auto &json=portable.json;
 const auto &primitives=json.at("meshes")[0].at("primitives");
 const auto &pbr=json.at("materials").at(primitives[0].at("material").get<size_t>());
 const auto &toon=json.at("materials").at(primitives[1].at("material").get<size_t>());
 check(!pbr.contains("extensions")&&toon.at("extensions").contains("VRMC_materials_mtoon"),
       "Only the authored typed primitive declares the MToon extension");
 const auto &body=toon.at("extensions").at("VRMC_materials_mtoon");
 const auto read=MToonMaterial::from_vrm1(toon,json.at("textures").size());
 check(read.parameters.alpha==MToonAlpha::Mask&&read.parameters.alpha_cutoff==.45f&&read.parameters.cull==MToonCull::Off&&
       read.parameters.base_color[0]==.5f&&read.parameters.shade_color.z==.7f&&read.parameters.scroll[0]==.15f,
       "The actual portable material retains authored alpha, shading, animation and the current native factor");
 auto role=[&](const nlohmann::json &slot,const PaintSourceTexture &original){
   const auto &tex=json.at("textures").at(slot.at("index").get<size_t>());
   const auto &view=json.at("images").at(tex.at("source").get<size_t>());
   const auto &chunk=json.at("bufferViews").at(view.at("bufferView").get<size_t>());
   const auto first=chunk.at("byteOffset").get<size_t>(),length=chunk.at("byteLength").get<size_t>();
   const auto pixels=original.image->png();
   const auto &sampler=json.at("samplers").at(tex.at("sampler").get<size_t>());
   check(first+length<=portable.binary.size()&&length==pixels.size()&&
         std::equal(pixels.begin(),pixels.end(),portable.binary.begin()+first)&&
         sampler.at("wrapS")==original.wrap_s&&sampler.at("wrapT")==original.wrap_t&&
         sampler.value("magFilter",0)==original.mag_filter,
         "The emitted role references the original image bytes and exact sampler");
 };
 role(toon.at("pbrMetallicRoughness").at("baseColorTexture"),base);
 role(body.at("shadeMultiplyTexture"),shade);
 check(read.textures[size_t(MToonSlot::Shade)].offset==std::array<float,2>{.125f,.25f},
       "The native shade role retains its independent UV transform");
 const auto &used=json.at("extensionsUsed");
 check(std::find(used.begin(),used.end(),"VRMC_materials_mtoon")!=used.end()&&
       std::find(used.begin(),used.end(),"KHR_texture_transform")!=used.end(),
       "The emitted document declares all material and sampler extensions it uses");
 auto unrigged=scene;
 unrigged.active().deformation.reset();
 const auto static_doc=detail::make_gltf(unrigged.export_surfaces());
 check(!static_doc.json.contains("skins")&&static_doc.json.at("materials").at(1)==toon,
       "The same authored MToon material exports from an ordinary unrigged mesh");
 export_glb(unrigged.export_surfaces(),"mtoon-static.glb");
 Document doc;
 doc.replace(std::move(scene));
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"MToon and PBR publish together on a rigged mesh");
 doc.export_asset("mtoon-standard.glb",AssetExportFormat::Glb).get();
 consumer("mtoon-standard.glb",2);
 const auto standard=bytes("mtoon-standard.glb");
 doc.export_asset("mtoon-editable.glb",AssetExportFormat::EditableGlb256).get();
 consumer("mtoon-editable.glb",2);
 const auto restored=extract_native_authoring(bytes("mtoon-editable.glb"));
 check(restored&&restored->encode()==exact&&doc.copy_scene().encode()==exact,
       "The editable package preserves the typed graph, image, skin, morph and independent native grids");
 auto painted=doc.copy_scene();
 check(edit_texture_set(painted,object,1,[](PaintedMesh &paint,Material &){
   Brush brush;
   brush.from=brush.to={0,0,.5f};
   brush.normal={0,0,1};
   brush.radius=.25f;
   PaintSettings settings;
   settings.front_only=false;
   settings.color={1,0,0,1};
   paint.begin_stroke();
   paint.stamp(brush,settings);
   return paint.end_stroke();
 }),"An authored MToon paint layer requires its own portable composition");
 const auto with_paint=painted.encode();
 doc.replace(std::move(painted));
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"The real MToon paint remains usable in the document");
 refuses([&]{doc.export_asset("mtoon-standard.glb",AssetExportFormat::Glb).get();},
         "Standard GLB refuses unresolved painted MToon before replacing the destination");
 check(bytes("mtoon-standard.glb")==standard,"Refused portability preserves the previous output");
 doc.export_asset("mtoon-painted-editable.glb",AssetExportFormat::EditableGlb256).get();
 const auto replay=extract_native_authoring(bytes("mtoon-painted-editable.glb"));
 check(replay&&replay->encode()==with_paint,
       "Editable GLB preserves the unresolved MToon paint and source material together");
}
void mtoon_painted_authoring(){
 auto scene=fixture();
 mtoon_paint_fixture::author(scene,1,{0,-.08f,.5f});
 const auto exact=scene.encode();
 Document doc;
 doc.replace(scene);
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Native MToon with two authored paint layers publishes");
 doc.export_asset("painted-mtoon-standard.glb",AssetExportFormat::Glb).get();
 doc.export_asset("painted-mtoon-editable.glb",AssetExportFormat::EditableGlb256).get();
 consumer("painted-mtoon-standard.glb",2);
 consumer("painted-mtoon-editable.glb",2);
 const auto standard=bytes("painted-mtoon-standard.glb");
 const auto replay=extract_native_authoring(bytes("painted-mtoon-editable.glb"));
 check(replay&&replay->encode()==exact&&doc.copy_scene().encode()==exact,
       "Painted MToon export preserves every native layer, source material and rig");
 const auto portable=detail::make_gltf(scene.export_surfaces());
 const auto &toon=portable.json.at("materials").at(1);
 const auto &bounds=toon.at("extras").at("domeSculptMToonPaint");
 const auto decoded=MToonMaterial::from_vrm1(toon,portable.json.at("textures").size());
 check(bounds.at("maximumLinearColorError").get<double>()<=.005&&
       bounds.at("maximumLinearEmissionError").get<double>()<=.005&&
       bounds.at("maximumAlphaError").get<double>()<=1./255.,
       "Portable MToon records bounded reconstruction of each paint channel");
 check(decoded.parameters.alpha==MToonAlpha::Mask&&decoded.parameters.alpha_cutoff==.45f&&
       decoded.textures[size_t(MToonSlot::Base)].index>=0&&
       decoded.textures[size_t(MToonSlot::Emission)].index>=0&&
       decoded.textures[size_t(MToonSlot::Shade)].index>=0,
       "Standard GLB retains its mask, two generated textures and original shade role");
 auto unsafe=fixture();
 mtoon_paint_fixture::author(unsafe,1,{0,-.08f,.5f},true);
 doc.replace(std::move(unsafe));
 check(doc.wait_mesh(),"The alpha-boundary control reaches the actual document");
 refuses([&]{doc.export_asset("painted-mtoon-standard.glb",AssetExportFormat::Glb).get();},
         "An unsupported quantized cutoff crossing fails before publishing");
 check(bytes("painted-mtoon-standard.glb")==standard,"The cutoff refusal preserves the complete earlier GLB");
}
void mtoon_shared_shade_authoring(){
 auto scene=fixture();
 mtoon_paint_fixture::author(scene,1,{0,-.08f,.5f},false,mtoon_paint_fixture::Background::SharedShade);
 auto field=texture_set_view(scene.active(),1);
 const auto base=size_t(MToonSlot::Base),shade=size_t(MToonSlot::Shade);
 check(field.material_bindings->roles[base].image==field.material_bindings->roles[shade].image,
       "The source base and shade are the same immutable image");
 const auto exact=scene.encode();
 Document doc;
 doc.replace(scene);
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"The shared shade scene publishes a painted material");
 doc.export_asset("shared-shade-standard.glb",AssetExportFormat::Glb).get();
 doc.export_asset("shared-shade-editable.glb",AssetExportFormat::EditableGlb256).get();
 consumer("shared-shade-standard.glb",2);
 consumer("shared-shade-editable.glb",2);
 const auto standard=bytes("shared-shade-standard.glb");
 const auto replay=extract_native_authoring(bytes("shared-shade-editable.glb"));
 check(replay&&replay->encode()==exact,"Editable GLB keeps the source alias and all native layers");
 const auto portable=detail::make_gltf(scene.export_surfaces());
 const auto &toon=portable.json.at("materials").at(1);
 const auto &bounds=toon.at("extras").at("domeSculptMToonPaint");
 const auto limits=bounds.at("maximumLinearShadeError").get<double>();
 check(limits>0&&limits<=.005&&bounds.at("maximumShadeInterpolationError").get<double>()>0,
       "The export measures a real shade clamp transition between texels");
 const auto decoded=MToonMaterial::from_vrm1(toon,portable.json.at("textures").size());
 check(decoded.textures[base].index>=0&&decoded.textures[shade].index>=0&&
       decoded.textures[base].index!=decoded.textures[shade].index&&
       decoded.textures[shade].offset==std::array<float,2>{},
       "The portable shade receives its own painted texture and current UV sampler");
 auto exceeded=*field.material_bindings;
 exceeded.material.parameters.shade_color={1,1,1};
 check(bind_texture_set_mtoon(scene,scene.active().id,1,exceeded),"The refusal control has a real authored high shade factor");
 doc.replace(std::move(scene));
 check(doc.wait_mesh(),"The high shade factor remains valid in the native viewport");
 refuses([&]{doc.export_asset("shared-shade-standard.glb",AssetExportFormat::Glb).get();},
         "The portable shade refuses a value above its normalized range");
 check(bytes("shared-shade-standard.glb")==standard,"The refused shade composition preserves the previous output");

 auto larger=fixture(false,512,64);
 mtoon_paint_fixture::author(larger,1,{0,-.08f,.5f},false,mtoon_paint_fixture::Background::SharedShade);
 const auto exact_larger=larger.encode();
 doc.replace(std::move(larger));
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"A 512-square shared shade publishes beside the 64-square PBR field");
 doc.export_asset("shared-shade-512-standard.glb",AssetExportFormat::Glb).get();
 doc.export_asset("shared-shade-512-editable.glb",AssetExportFormat::EditableGlb256).get();
 consumer("shared-shade-512-standard.glb",2);
 const auto larger_editable=extract_native_authoring(bytes("shared-shade-512-editable.glb"));
 check(larger_editable&&larger_editable->encode()==exact_larger,"The larger texture set retains exact native paint at the original resolution");
}
void editable_hdr(){
 auto scene=fixture();
 const auto object=scene.active().id;
 for(TextureSetId set:{0u,1u})check(edit_texture_set(scene,object,set,[&](PaintedMesh &paint,Material &){
   Brush brush;
   brush.from=brush.to={0,0,set?.5f:0.f};
   brush.normal={0,0,1};
   brush.radius=2;
   brush.strength=1;
   PaintSettings settings;
   settings.front_only=false;
   settings.channel=PaintChannel::Emission;
   settings.emission_units=PaintEmissionUnits::Physical;
   settings.emission=set?std::array<float,3>{.15f,9.f,1.f}:std::array<float,3>{5.f,.2f,.3f};
   paint.begin_stroke();
   paint.stamp(brush,settings);
   return paint.end_stroke();
 }),"The actual native member has a distinct physical stroke");
 const auto exact=scene.encode();
 Document doc;
 doc.replace(scene);
 check(doc.wait_mesh(std::chrono::seconds(30)),"Plain FLOAT material ranges publish their actual shared mesh");
 const std::filesystem::path path="editable-texture-set-hdr.glb";
 doc.export_asset(path,AssetExportFormat::EditableGlb512).get();
 consumer(path,2);
 const auto payload=bytes(path);
 const auto restored=extract_native_authoring(payload);
 check(restored&&restored->encode()==exact&&doc.copy_scene().encode()==exact,
       "Editable per-field HDR retains the exact native layers and shared deformation");
 const auto json_length=uint32_t(payload.at(12))|(uint32_t(payload.at(13))<<8)|
     (uint32_t(payload.at(14))<<16)|(uint32_t(payload.at(15))<<24);
 const auto json=nlohmann::json::parse(payload.begin()+20,payload.begin()+20+json_length);
 const auto &primitives=json.at("meshes").at(0).at("primitives");
 for(size_t set=0;set<2;++set){
   const auto &p=primitives.at(set);
   const auto &m=json.at("materials").at(p.at("material").get<size_t>());
   check(p.at("extras").at("dome_sculpt_texture_set").at("id")==set&&
         m.at("emissiveFactor").at(set)==1&&m.at("emissiveTexture").value("texCoord",0)==0,
         "Physical fallback keeps the actual set, dominant channel and native UV0");
   const auto strength=m.at("extensions").at("KHR_materials_emissive_strength").at("emissiveStrength").get<float>();
   check(strength>(set?7.f:3.f),"The portable material retains above-one radiance");
 }
 const std::filesystem::path standard="standard-texture-set-hdr.glb";
 doc.export_asset(standard,AssetExportFormat::Glb).get();
 consumer(standard,2);
 const auto reopened=extract_native_emission(bytes(standard));
 check(reopened&&reopened->encode()==exact&&doc.copy_scene().encode()==exact,
       "Standard per-field EXRs and native Scene reopen the exact plain texture sets");

 const auto prior_surfaces=emission_export_surfaces(scene);
 auto changed_material=scene.snapshot();
 changed_material.active().material.emissive={.7f,.4f,.1f};
 NativeEmissionExport stale;
 stale.scene=std::make_shared<const Scene>(std::move(changed_material));
 stale.allow_measured_core_fallback=true;
 refuses([&]{detail::prepare_emission_export(prior_surfaces,stale,{});},
         "An earlier plan cannot borrow a scene with different primary material factors");

 auto mixed=scene.snapshot();
 const auto single=mixed.add("Independent HDR primitive",Volume{});
 auto &ordinary=mixed.object(single);
 const auto &mesh=scene.active().painted->mesh();
 Mesh triangle;
 triangle.positions.assign(mesh.positions.begin(),mesh.positions.begin()+3);
 triangle.normals.assign(mesh.normals.begin(),mesh.normals.begin()+3);
 triangle.uv.assign(mesh.uv.begin(),mesh.uv.begin()+3);
 triangle.indices={0,1,2};
 ordinary.mesh_active=true;
 ordinary.painted=std::make_shared<PaintedMesh>(std::move(triangle),64);
 ordinary.material.emissive={4.f,.04f,1.5f};
 const auto mixed_exact=mixed.encode();
 Document both;
 both.replace(mixed);
 check(both.wait_mesh(std::chrono::seconds(30)),"Standalone HDR and texture-set HDR publish together");
 const std::filesystem::path mixed_file="mixed-standard-hdr.glb";
 both.export_asset(mixed_file,AssetExportFormat::Glb).get();
 const auto mixed_package=extract_native_emission(bytes(mixed_file));
 check(mixed_package&&mixed_package->encode()==mixed_exact,
       "Standard GLB independently replays standalone HDR alongside multiple material fields");

}

void authority(){
 auto scene=fixture();const auto exact=scene.encode();auto surfaces=scene.export_surfaces();const auto original=surfaces[0].texture_set_surface;
 surfaces[0].texture_set_surface=std::shared_ptr<const detail::TextureSetSurfacePlan>(original.get(),[](auto*){});refuses([&]{detail::make_gltf(surfaces);},"Borrowed plan address with a foreign control block cannot export");surfaces=scene.export_surfaces();surfaces[0].id++;refuses([&]{detail::make_gltf(surfaces);},"Another object identity cannot borrow a genuine material plan");surfaces=scene.export_surfaces();surfaces[0].deformation=std::make_shared<const DeformationState>(*scene.active().deformation);refuses([&]{detail::make_gltf(surfaces);},"Numerically equal pose with a foreign control block cannot replace plan authority");
 surfaces=scene.export_surfaces();TextureSetLimits limits;limits.maximum_bytes=1024;refuses([&]{detail::compile_texture_set_export(surfaces[0],limits);},"Explicit per-material layout budget refuses before geometry expansion");limits={};limits.cancelled=[]{return true;};refuses([&]{detail::compile_texture_set_export(surfaces[0],limits);},"Material export cancellation keeps the source endpoint exact");
 check(scene.encode()==exact,"Every plan/pose/budget/cancellation refusal keeps all authoring values exact");
}

void compare_production_exrs(const std::vector<uint8_t> &file,const Scene &scene){
 const auto json_length=uint32_t(file.at(12))|(uint32_t(file.at(13))<<8)|
     (uint32_t(file.at(14))<<16)|(uint32_t(file.at(15))<<24);
 const auto json=nlohmann::json::parse(file.begin()+20,file.begin()+20+json_length);
 const auto &resources=json.at("extras").at("domeSculptEmission").at("images");
 check(resources.size()==2,"Both high-resolution HDR members have independent exact EXR resources");
 const auto binary=std::span(file).subspan(28+json_length);
 for(const auto &resource:resources){
  const auto set=resource.at("textureSet").at("id").get<TextureSetId>();
  const auto field=texture_set_view(scene.active(),set);
  const detail::PaintEmissionSampler dense_reference(*field.painted);
  const auto &view=json.at("bufferViews").at(resource.at("bufferView").get<size_t>());
  const auto exr=binary.subspan(view.at("byteOffset").get<size_t>(),view.at("byteLength").get<size_t>());
  const auto resolution=field.painted->resolution();
  size_t verified=0;
  const auto description=detail::read_emission_exr_rows(exr,
      [&](uint32_t y,std::span<const detail::EmissionExrPixel> pixels){
       for(size_t i=0;i<pixels.size();++i){
        const auto q=dense_reference.sample_texel(uint32_t(i%resolution),y+uint32_t(i/resolution));
        const auto rgb=q.resolve(field.material.emissive,1.f);
        const detail::EmissionExrPixel expected{rgb[0],rgb[1],rgb[2],q.legacy[0],q.legacy[1],q.legacy[2],
                                              q.absolute[0],q.absolute[1],q.absolute[2],q.coverage};
        if(std::bit_cast<std::array<uint32_t,10>>(pixels[i])!=
           std::bit_cast<std::array<uint32_t,10>>(expected))
         throw std::runtime_error("Sparse EXR disagrees with dense reference");
        ++verified;
       }
      },{});
  check(description.width==resolution&&description.height==resolution&&description.version==2&&
        verified==size_t(resolution)*resolution,
        "Every encoded EXR channel bit matches the original dense per-texel sampler");
 }
}

void production_hdr(){
 auto scene=fixture(false,1024);
 const auto object=scene.active().id;
 for(TextureSetId set:{0u,1u})check(edit_texture_set(scene,object,set,[set](PaintedMesh &paint,Material &material){
  material.emissive=set?std::array<float,3>{.15f,.07f,.03f}:std::array<float,3>{.08f,.2f,.17f};
  PaintSettings ink;
  ink.front_only=false;
  ink.color=set?std::array<float,4>{.1f,.3f,.7f,.5f}:std::array<float,4>{.6f,.2f,.1f,.8f};
  ink.emission_units=PaintEmissionUnits::Physical;
  ink.emission=set?std::array<float,3>{.2f,9.f,1.f}:std::array<float,3>{5.f,.15f,.3f};
  ink.metallic=.18f;
  ink.roughness=.83f;
  Brush brush;
  brush.from=brush.to={0,-.1f,set?.5f:0.f};
  brush.normal={0,0,1};
  brush.radius=.046f;
  brush.strength=.82f;
  const auto stamp=[&](PaintChannel channel){
   ink.channel=channel;
   paint.begin_stroke();
   paint.stamp(brush,ink);
   check(paint.end_stroke(),"Production channel stroke commits actual sparse values");
  };
  for(auto channel:{PaintChannel::Emission,PaintChannel::Color,PaintChannel::Metallic,PaintChannel::Roughness})stamp(channel);
  paint.add_layer("Independent ordered detail");
  brush.from.x=brush.to.x=.026f;
  ink.color={.7f,.4f,.2f,.3f};
  stamp(PaintChannel::Color);
  paint.layer_properties("Independent ordered detail",true,.41f);
  paint.add_layer("Hidden physical detail");
  brush.from.x=brush.to.x=.19f;
  ink.emission={110.f,10.f,90.f};
  stamp(PaintChannel::Emission);
  paint.layer_properties("Hidden physical detail",false,1.f);
  return true;
 }),"Each production field owns independent visible and hidden layers");

 const auto exact=scene.encode();
 const auto assembly=scene.export_surfaces();
 {
  const auto compiled=detail::compile_texture_set_export(assembly.at(0),{},true);
  check(compiled.fields.size()==2,"Default export budget retains both 1024-square fields");
  for(const auto &output:compiled.fields){
   const auto field=texture_set_view(scene.active(),output.set);
   auto safe=field.material;
   safe.emissive={};
   const auto reference=detail::EmissionExportAccess::other_channels(*field.painted,safe,{});
   check(output.textures->color.rgba==reference->color.rgba&&
         output.textures->metal_rough.rgba==reference->metal_rough.rgba&&
         output.textures->transparent==reference->transparent,
         "All production albedo, alpha and ORM samples match the original compositor exactly");
   check(output.emission&&output.emission->image.width==1024&&
         output.emission->strength>1&&output.emission->strength<10,
         "Portable emission keeps full grid and visible radiance while excluding hidden detail");
  }
 }
 TextureSetLimits small;
 small.maximum_work=2'000'000;
 refuses([&]{detail::compile_texture_set_export(assembly.at(0),small,true);},
         "Cumulative sparse sampling still refuses a tight work budget");
 small={};
 small.maximum_bytes=20*1024*1024;
 refuses([&]{detail::compile_texture_set_export(assembly.at(0),small,true);},
         "Full-resolution portable images still count toward the export byte limit");
 Document doc;
 doc.replace(scene);
 check(doc.wait_mesh(std::chrono::seconds(45))&&doc.frame().state->error.empty(),
       "Document publishes the production authored material ranges");
 for(const auto &[name,format]:std::array{
     std::pair{"production-standard.glb",AssetExportFormat::Glb},
     std::pair{"production-editable.glb",AssetExportFormat::EditableGlb512}}){
  doc.export_asset(name,format).get();
  consumer(name,2);
  const auto file=bytes(name);
  if(format==AssetExportFormat::Glb)compare_production_exrs(file,scene);
  const auto restored=format==AssetExportFormat::Glb?extract_native_emission(file):extract_native_authoring(file);
  check(restored&&restored->encode()==exact&&doc.copy_scene().encode()==exact,
        "Production GLB export restores the exact native layers, membership and rig");
 }
}

void production_baked_hdr(){
 auto scene=fixture(false,1024);
 const auto target=scene.active().id;
 scene.active().deformation.reset();
 const auto mesh=scene.active().painted->mesh();
 for(TextureSetId set:{0u,1u})check(edit_texture_set(scene,target,set,[set](PaintedMesh &paint,Material &material){
  material.emissive={.1f,.2f,.3f};
  PaintSettings ink;
  ink.front_only=false;
  ink.channel=PaintChannel::Color;
  ink.color=set?std::array<float,4>{.3f,.12f,.8f,.5f}:std::array<float,4>{.7f,.4f,.1f,.75f};
  Brush brush;
  brush.from=brush.to={0,-.1f,set?.5f:0.f};
  brush.normal={0,0,1};
  brush.radius=.049f;
  brush.strength=.72f;
  paint.begin_stroke();
  paint.stamp(brush,ink);
  return paint.end_stroke();
 }),"Both native baked recipients start with distinct writable color detail");
 const auto donor=scene.add("Independent hidden HDR donor",Volume{});
 auto &source=scene.object(donor);
 source.visible=false;
 source.locked=true;
 source.mesh_active=true;
 source.material.emissive={4,.8f,.25f};
 source.material.color={.4f,.7f,.22f,1};
 auto donor_mesh=mesh;
 for(size_t vertex=0;vertex<donor_mesh.positions.size();++vertex){
  donor_mesh.positions[vertex].z+=.01f;
  donor_mesh.uv[vertex][0]=donor_mesh.uv[vertex][0]*.45f+(vertex>=3?.5f:0.f);
  donor_mesh.normals[vertex]=normalized(Vec3{vertex>=3?-.14f:.21f,.25f,1.f});
 }
 source.painted=std::make_shared<PaintedMesh>(std::move(donor_mesh),64);
 scene.select(target);
 Document doc;
 ObjectBakeSettings settings;
 settings.ao_samples=2;
 settings.allow_backfaces=true;
 settings.corresponding_topology=true;
 for(TextureSetId set:{0u,1u}){
  select_texture_set(scene,target,set);
  doc.replace(scene);
  check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Baked material remains ready while selecting its neighbor");
  doc.bake_from_object(donor,settings);
  check(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual per-member Bake publishes the renderable endpoint");
  scene=doc.copy_scene();
  const auto field=texture_set_view(scene.active(),set);
  const auto &bake=field.painted->baked();
  check(bake&&bake->emission_field&&bake->emission_field->resolution()==1024&&
        bake->emission_field->storage().dense.empty()&&
        !bake->maps.normal.rgba.empty()&&field.bake_recipe&&
        std::count(bake->coverage.begin(),bake->coverage.end(),255)>100'000,
        "Each 1024-square Bake retains covered normal, AO and physical-emission data and its recipe");
 }
 const auto prior=scene.encode();
 const auto neighbor=scene.active().painted->baked();
 doc.bake_from_object(donor,settings);
 check(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Selected production rebake publishes normally");
 scene=doc.copy_scene();
 check(scene.active().painted->baked()==neighbor&&
       active_texture_set(scene.active()).painted->baked()->emission_field->storage().dense.empty(),
       "Rebake keeps the untouched material owner and compacts the merged FLOAT result");
 const auto exact=scene.encode();
 check(doc.undo()&&doc.copy_scene().encode()==prior&&
       doc.redo()&&doc.copy_scene().encode()==exact&&doc.wait_mesh(),"Rebake retains exact Undo and Redo");
 const std::filesystem::path path="production-baked.dsculpt";
 doc.save_async(path).get();
 doc.open_async(path).get();
 check(doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==exact,
       "Both baked members publish and pick after exact cold Open");
 const auto snapshot=doc.frame().surface;
 check(snapshot&&snapshot->ranges.size()==2&&snapshot->picking,"Cold Open exposes both authenticated material ranges");
 const auto assembly=scene.export_surfaces();
 {
  const auto compiled=detail::compile_texture_set_export(assembly.at(0),{},true);
  for(const auto &output:compiled.fields){
   const auto field=texture_set_view(scene.active(),output.set);
   auto safe=field.material;
   safe.emissive={};
   const auto reference=detail::EmissionExportAccess::other_channels(*field.painted,safe,{});
   const auto same_maps=[&](const TextureMaps &maps){
    return maps.color.rgba==reference->color.rgba&&maps.metal_rough.rgba==reference->metal_rough.rgba&&
        maps.normal.rgba==reference->normal.rgba&&maps.transparent==reference->transparent;
   };
   check(same_maps(*output.textures),"Every portable baked albedo, ORM and resolved normal matches the original compositor");
   const auto &preview=*snapshot->ranges.at(output.set).textures;
   check(same_maps(preview)&&preview.baked_base&&preview.overlay&&preview.overlay->baked()==preview.baked_base,
         "Every preview material byte matches while the GPU retains the actual FLOAT Bake owner");
  }
 }
 for(const auto &[name,format]:std::array{
     std::pair{"production-baked-standard.glb",AssetExportFormat::Glb},
     std::pair{"production-baked-editable.glb",AssetExportFormat::EditableGlb512}}){
  doc.export_asset(name,format).get();
  const auto file=bytes(name);
  if(format==AssetExportFormat::Glb)compare_production_exrs(file,scene);
  const auto restored=format==AssetExportFormat::Glb?extract_native_emission(file):extract_native_authoring(file);
  check(restored&&restored->encode()==exact,"Production GLB restores exact independent Bake, recipe and paint data");
 }
}
}
int main(int argc,char**argv)try{if(argc>1){std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);}portable();authoring();mtoon_authoring();mtoon_painted_authoring();mtoon_shared_shade_authoring();editable_hdr();authority();production_hdr();production_baked_hdr();std::cout<<"PASS "<<checks<<" texture-set multi-primitive export controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
