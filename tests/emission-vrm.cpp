// SPDX-License-Identifier: GPL-3.0-or-later
#define main inherited_matcap_fixture
#include "vrm-matcap-import.cpp"
#undef main
#include "dome/emission-native.hpp"
#include "dome/gltf-export.hpp"
#include "dome/vrm.hpp"
#include "dome/vrm-authoring.hpp"
#include "dome/creation.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/texture-sets.hpp"
#include "dome/source-placement.hpp"
#include "emission-export.hpp"
#include "editable-avatar-eligibility.hpp"
#include "cgltf.h"
#include <fstream>
#include <zlib.h>
#include <png.h>
#include "humanoid-sha256.hpp"
#include "emission-core-consumer.hpp"
namespace inflate_probe {size_t calls=0;}
namespace deflate_probe {size_t calls=0;}
extern "C" int __real_deflate(z_streamp,int);
extern "C" int __wrap_deflate(z_streamp value,int flush){++deflate_probe::calls;return __real_deflate(value,flush);}
extern "C" int __real_inflate(z_streamp,int);
extern "C" int __wrap_inflate(z_streamp value,int flush){++inflate_probe::calls;return __real_inflate(value,flush);}
namespace {
using J=nlohmann::json;
std::vector<uint8_t> file_bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
J glb_json(const std::filesystem::path&p){auto b=file_bytes(p);uint32_t n=0;for(int k=0;k<4;++k)n|=uint32_t(b.at(12+k))<<(8*k);return J::parse(b.begin()+20,b.begin()+20+n);}
// Resource indices may change when per-part HDR images are inserted. Resolve the
// actual thumbnail payload and sampler rather than accepting a stale numeric ID.
J thumbnail(const std::filesystem::path &path,const J &document,int index,bool texture) {
 if(index<0)return index;
 J descriptor=texture?document.at("textures").at(size_t(index)):J{{"source",index}};
 const auto image_index=descriptor.at("source").get<size_t>();
 auto image=document.at("images").at(image_index);
 const auto view=document.at("bufferViews").at(image.at("bufferView").get<size_t>());
 auto bytes=file_bytes(path);uint32_t json_bytes=0;
 for(int k=0;k<4;++k)json_bytes|=uint32_t(bytes.at(12+k))<<(8*k);
 const size_t offset=28+json_bytes+view.value("byteOffset",size_t(0));
 const size_t length=view.at("byteLength").get<size_t>();
 if(offset>bytes.size()||length>bytes.size()-offset)throw std::runtime_error("Thumbnail resource range is invalid");
 image.erase("bufferView");descriptor.erase("source");
 if(descriptor.contains("sampler"))descriptor["sampler"]=document.at("samplers").at(descriptor.at("sampler").get<size_t>());
 return J{{"texture",descriptor},{"image",image},{"bytes",J::binary(std::vector<uint8_t>(bytes.begin()+offset,bytes.begin()+offset+length))}};
}
std::span<const uint8_t> resource(const std::vector<uint8_t> &bytes,const J &j,const J &record){uint32_t size=0;for(int k=0;k<4;++k)size|=uint32_t(bytes.at(12+k))<<(8*k);const auto &view=j.at("bufferViews").at(record.at("bufferView").get<size_t>());return std::span<const uint8_t>(bytes).subspan(28+size+view.value("byteOffset",size_t(0)),view.at("byteLength").get<size_t>());}
std::vector<uint8_t> rebound(const std::vector<uint8_t> &source,J j){uint32_t size=0;for(int k=0;k<4;++k)size|=uint32_t(source.at(12+k))<<(8*k);std::vector<uint8_t> binary(source.begin()+28+size,source.end());auto &package=j.at("extras").at("domeSculptEmission");package["native"]["sha256"]=creator_detail::sha256(resource(source,j,package.at("native")));package["binarySha256"]=creator_detail::sha256(binary);package.erase("documentSha256");package["documentSha256"]=creator_detail::sha256(J::to_cbor(j));auto text=j.dump();while(text.size()%4)text+=' ';std::vector<uint8_t> result;auto word=[&](uint32_t value){for(int k=0;k<4;++k)result.push_back(uint8_t(value>>(8*k)));};word(0x46546c67);word(2);word(28+text.size()+binary.size());word(text.size());word(0x4e4f534a);result.insert(result.end(),text.begin(),text.end());word(binary.size());word(0x004e4942);result.insert(result.end(),binary.begin(),binary.end());return result;}
template<class F>bool refuses_hdr(F f){try{f();return false;}catch(const std::exception&){return true;}}
void compressed_controls(const std::filesystem::path &path,const Scene &native){
 const auto bytes=file_bytes(path);const auto j=glb_json(path);const auto &package=j.at("extras").at("domeSculptEmission");if(package.at("version")!=2)return;
 const auto &record=package.at("native");auto encoded=resource(bytes,j,record);std::vector<uint8_t> decoded(record.at("decodedBytes").get<size_t>());uLongf size=decoded.size();
 check(uncompress(decoded.data(),&size,encoded.data(),encoded.size())==Z_OK&&size==decoded.size()&&decoded==J::to_cbor(native.encode()),"Independent zlib decoder reconstructs exact native CBOR with original images, aliases and history beyond128MiB stored-only limit");
 NativeEmissionReadLimits limits;limits.max_decoded_native_bytes=128*1024*1024;check(refuses_hdr([&]{extract_native_emission(bytes,limits);}),"Explicit expanded native limit refuses valid larger decoded owner before decompression");
 auto changed=j;changed["extras"]["domeSculptEmission"]["native"]["decodedBytes"]=1;auto bomb=rebound(bytes,changed);check(refuses_hdr([&]{extract_native_emission(bomb);}),"Correctly rebound lossless expansion cannot exceed a false small decoded declaration");
 changed=j;changed["bufferViews"][record.at("bufferView").get<size_t>()]["byteLength"]=encoded.size()-1;auto truncated=rebound(bytes,changed);check(refuses_hdr([&]{extract_native_emission(truncated);}),"Correctly rebound truncated deflate stream refuses before Scene publication");
 changed=j;changed["extras"]["domeSculptEmission"]["version"]=1;auto downgrade=rebound(bytes,changed);check(refuses_hdr([&]{extract_native_emission(downgrade);}),"Compressed payload cannot masquerade as legacy package1");
 inflate_probe::calls=0;limits={};limits.cancelled=[]{return inflate_probe::calls>=2;};check(refuses_hdr([&]{extract_native_emission(bytes,limits);})&&inflate_probe::calls==2,"Actual streamed inflater polls cancellation after real decompression chunks before native allocation/publication");
 deflate_probe::calls=0;auto options=std::make_shared<NativeEmissionExport>();options->scene=std::make_shared<const Scene>(native.snapshot());options->allow_measured_core_fallback=true;VrmExportOptions cancel;cancel.emission=options;cancel.cancelled=[]{return deflate_probe::calls>=2;};auto unchanged=native.encode();check(refuses_hdr([&]{export_vrm(emission_export_surfaces(native),path,cancel);})&&deflate_probe::calls==2&&file_bytes(path)==bytes&&native.encode()==unchanged,"Actual streamed encoder cancellation preserves prior VRM bytes and exact immutable source/history");
}
float decoded_srgb(uint8_t value){const float v=value/255.f;return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
void fallback_consumer(const std::filesystem::path &path){
 const auto bytes=file_bytes(path);const auto j=glb_json(path);float maximum=0;
 for(const auto &entry:j.at("extras").at("domeSculptEmission").at("images")){
  const auto raw_field=resource(bytes,j,entry);const auto field=decode_emission_exr(raw_field);const auto external_path=path.parent_path()/(path.stem().string()+"-object"+std::to_string(entry.at("object").get<uint64_t>())+".exr");{std::ofstream external(external_path,std::ios::binary);external.write(reinterpret_cast<const char*>(raw_field.data()),raw_field.size());}const auto independent=emission_core_consumer::read(external_path,field.width,field.height);check(independent.size()==field.pixels.size()&&std::memcmp(independent.data(),field.pixels.data(),independent.size()*sizeof(independent[0]))==0,"Separate OpenEXRCore C decoder reconstructs every actual embedded VRM resolved/relative/physical/coverage FLOAT32 bit");const size_t material=entry.at("material").get<size_t>();const auto &m=j.at("materials").at(material);const bool legacy=j.at("extensions").contains("VRM");std::array<float,3> factor{};size_t texture=0;
  if(legacy){const auto &typed=j.at("extensions").at("VRM").at("materialProperties").at(material);texture=typed.at("textureProperties").at("_EmissionMap").get<size_t>();for(size_t k=0;k<3;++k)factor[k]=typed.at("vectorProperties").at("_EmissionColor").at(k).get<float>();}
  else{texture=m.at("emissiveTexture").at("index").get<size_t>();float strength=1;if(m.contains("extensions")&&m.at("extensions").contains("KHR_materials_emissive_strength"))strength=m.at("extensions").at("KHR_materials_emissive_strength").at("emissiveStrength").get<float>();for(size_t k=0;k<3;++k)factor[k]=m.at("emissiveFactor").at(k).get<float>()*strength;}
  const auto &image=j.at("images").at(j.at("textures").at(texture).at("source").get<size_t>());auto encoded=resource(bytes,j,J{{"bufferView",image.at("bufferView")}});png_image decoded{};decoded.version=PNG_IMAGE_VERSION;if(!png_image_begin_read_from_memory(&decoded,encoded.data(),encoded.size()))throw std::runtime_error("Independent PNG consumer cannot read emitted emission image");struct Cleanup{png_image &image;~Cleanup(){png_image_free(&image);}}cleanup{decoded};decoded.format=PNG_FORMAT_RGBA;std::vector<uint8_t> pixels(PNG_IMAGE_SIZE(decoded));if(!png_image_finish_read(&decoded,nullptr,pixels.data(),0,nullptr))throw std::runtime_error("Independent PNG consumer cannot decode emitted emission image");check(decoded.width==field.width&&decoded.height==field.height,"Independent PNG decoder reads actual typed fallback dimensions");std::array<float,3> peak{};for(const auto &p:field.pixels)for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],p[k]);
  for(size_t i=0;i<field.pixels.size();++i)for(size_t k=0;k<3;++k){const float actual=decoded_srgb(pixels[i*4+k])*factor[k];const float error=std::abs(actual-field.pixels[i][k]);maximum=std::max(maximum,error);if(error>peak[k]*.0046f+.00001f)throw std::runtime_error("Independent typed standard-view radiance exceeds analytic sRGB8 quantization bound");}
 }
 std::cout<<"INDEPENDENT_TYPED_FALLBACK maximumAbsoluteError="<<maximum<<'\n';check(true,"Actual PNG+typed material factors reconstruct the documented static HDR view within independently bounded quantization");
}
// glTF omits leaf children; an authored empty array is invalid core JSON.
// Normalize only this generated fixture spelling, preserving every real node,
// parent/reference and ordered nonempty child list.
void canonical_leaf_nodes(import_fixture::Fixture &fixture){for(auto &node:fixture.j.at("nodes"))if(node.contains("children")&&node.at("children").empty())node.erase("children");}
void physical_ink(SculptObject&o,std::array<float,3> emission){o.painted=o.painted->snapshot();Brush b;b.from=b.to=o.painted->mesh().positions.front();b.normal={0,0,1};b.radius=5;b.strength=.67f;PaintSettings settings;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission=emission;settings.front_only=false;o.painted->begin_stroke();o.painted->stamp(b,settings);check(o.painted->end_stroke(),"Real typed source mesh accepts physical emission paint");}
J geometry(const std::filesystem::path &path){cgltf_options opts{};cgltf_data*raw=nullptr;check(cgltf_parse_file(&opts,path.c_str(),&raw)==cgltf_result_success,"Independent cgltf parses actual VRM HDR output");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(raw,cgltf_free);check(cgltf_load_buffers(&opts,raw,path.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,"Independent cgltf validates current mesh/skin/morph/resource references");J result=J::array();for(size_t n=0;n<raw->nodes_count;++n){const auto &node=raw->nodes[n];std::array<float,16> world;cgltf_node_transform_world(&node,world.data());J record={{"name",node.name?node.name:""},{"world",world}};if(node.weights_count)record["weights"]=std::vector<float>(node.weights,node.weights+node.weights_count);if(node.mesh&&node.mesh->weights_count)record["mesh_weights"]=std::vector<float>(node.mesh->weights,node.mesh->weights+node.mesh->weights_count);if(node.mesh){record["meshes"]=J::array();for(size_t p=0;p<node.mesh->primitives_count;++p){const auto &primitive=node.mesh->primitives[p];J mesh={{"attributes",J::array()},{"targets",J::array()}};auto attributes=[&](const cgltf_attribute*values,size_t count){J out=J::array();for(size_t a=0;a<count;++a){const auto &v=values[a];J data=J::array();for(size_t i=0;i<v.data->count;++i){std::array<float,16> value{};const auto count=cgltf_num_components(v.data->type);if(!cgltf_accessor_read_float(v.data,i,value.data(),count))throw std::runtime_error("Independent attribute decode failed");data.push_back(std::vector<float>(value.begin(),value.begin()+count));}out.push_back({{"type",int(v.type)},{"set",v.index},{"data",data}});}return out;};mesh["attributes"]=attributes(primitive.attributes,primitive.attributes_count);for(size_t t=0;t<primitive.targets_count;++t)mesh["targets"].push_back(attributes(primitive.targets[t].attributes,primitive.targets[t].attributes_count));if(primitive.indices){mesh["indices"]=J::array();for(size_t i=0;i<primitive.indices->count;++i)mesh["indices"].push_back(cgltf_accessor_read_index(primitive.indices,i));}record["meshes"].push_back(std::move(mesh));}}if(node.skin){record["joints"]=J::array();for(size_t i=0;i<node.skin->joints_count;++i)record["joints"].push_back(node.skin->joints[i]-raw->nodes);record["ibms"]=J::array();if(node.skin->inverse_bind_matrices)for(size_t i=0;i<node.skin->inverse_bind_matrices->count;++i){std::array<float,16> v;cgltf_accessor_read_float(node.skin->inverse_bind_matrices,i,v.data(),16);record["ibms"].push_back(v);}}result.push_back(std::move(record));}return result;}
void current(const std::filesystem::path &dir,Scene scene,std::string name){auto before=scene.encode();auto baseline=dir/(name+"-before.vrm");export_vrm(scene.preview_surfaces(),baseline);const auto expected=geometry(baseline);std::vector<uint64_t> ids;for(const auto&o:scene.objects())if(o.appearance)ids.push_back(o.id);check(!ids.empty(),"Real source avatar provides authenticated native part identities");const auto raw=scene.object(ids.front()).appearance->original_bytes;physical_ink(scene.object(ids.front()),{12,.005f,10000});if(ids.size()>1)physical_ink(scene.object(ids[1]),{100,.0003f,3});const auto original=scene.encode();auto native=std::make_shared<const Scene>(scene.snapshot());auto options=std::make_shared<NativeEmissionExport>();options->scene=native;options->scope=NativeEmissionExport::Scope::ExportedObjects;options->allow_measured_core_fallback=true;auto surfaces=emission_export_surfaces(*native);VrmExportOptions out;out.emission=options;const auto path=dir/(name+"-physical.vrm");auto report=export_vrm(surfaces,path,out);fallback_consumer(path);check(report.meshes==ids.size(),"Actual typed VRM exporter retains every source mesh instance");check(geometry(path)==expected,"Independent world matrices/ordered attributes/indices/skin IBMs/joints/morphs remain exact through HDR material compilation");auto restored=extract_native_emission(file_bytes(path));auto subset=detail::emission_export_subset(scene,ids);check(restored&&restored->encode()==subset.encode(),"Native VRM package restores exact selected avatar/paint/source graph without unrelated scene contents");check(*restored->object(ids.front()).appearance->original_bytes==*raw&&scene.encode()==original,"HDR VRM export preserves original raw owner and current native state");for(auto id:ids)check(restored->object(id).appearance->original_bytes==restored->object(ids.front()).appearance->original_bytes&&restored->object(id).appearance->decoded_images==restored->object(ids.front()).appearance->decoded_images,"Restored linked source parts preserve shared immutable source/image owner aliases");compressed_controls(path,subset);auto emitted=glb_json(path),old=glb_json(baseline);for(const auto &entry:emitted.at("extras").at("domeSculptEmission").at("images")){const auto &m=emitted.at("materials").at(entry.at("material").get<size_t>());if(m.contains("extensions")&&m.at("extensions").contains("KHR_materials_unlit"))check(entry.at("fallback").at("strength")==1&&entry.at("fallback").at("maximumAbsoluteError").get<float>()>1&&entry.at("fallback").at("erasedNonzeroSamples").get<size_t>()>0,"Core unlit fallback explicitly measures ignored radiance instead of reporting typed MToon reconstruction as core fidelity");}for(auto tag:{"VRM","VRMC_vrm","VRMC_springBone"})if(old["extensions"].contains(tag)){auto a=old["extensions"][tag],b=emitted["extensions"][tag];if(std::string_view(tag)=="VRM"){a.erase("materialProperties");b.erase("materialProperties");if(a.at("meta").contains("texture")){a["meta"]["texture"]=thumbnail(baseline,old,a.at("meta").at("texture").get<int>(),true);b["meta"]["texture"]=thumbnail(path,emitted,b.at("meta").at("texture").get<int>(),true);}}if(std::string_view(tag)=="VRMC_vrm"&&a.at("meta").contains("thumbnailImage")){a["meta"]["thumbnailImage"]=thumbnail(baseline,old,a.at("meta").at("thumbnailImage").get<int>(),false);b["meta"]["thumbnailImage"]=thumbnail(path,emitted,b.at("meta").at("thumbnailImage").get<int>(),false);}check(a==b,"Humanoid/meta/morph expressions/first-person/look-at/spring references remain unchanged");}auto again=std::make_shared<NativeEmissionExport>(*options);again->scene=std::make_shared<const Scene>(restored->snapshot());out.emission=again;auto cycle=dir/(name+"-second.vrm");export_vrm(emission_export_surfaces(*again->scene),cycle,out);check(file_bytes(cycle)==file_bytes(path),"Typed native VRM second cycle does not grow recursive sources or change emission resources");scene.save(dir/(name+".dsculpt"));check(Scene::load(dir/(name+".dsculpt")).encode()==original,"Actual typed physical source/skin/morph state restarts exactly");}
void strength_case(const std::filesystem::path &directory,float strength){
 auto f=source(false);import_fixture::Fixture original;for(auto uv:{"TEXCOORD_0","TEXCOORD_1"})f.j["meshes"][0]["primitives"][0]["attributes"][uv]=original.j["meshes"][0]["primitives"][0]["attributes"][uv];auto &material=f.j["materials"][0];material["extensions"].erase("KHR_materials_unlit");material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",strength}};material["emissiveFactor"]={.02,.04,.07};material["emissiveTexture"]={{"index",0}};auto &used=f.j["extensionsUsed"];for(auto i=used.begin();i!=used.end();)if(i->get_ref<const std::string&>()=="KHR_materials_unlit")i=used.erase(i);else ++i;used.push_back("KHR_materials_emissive_strength");
 if(strength>0){
  auto &transform=material["emissiveTexture"]["extensions"]["KHR_texture_transform"];
  transform={{"offset",{.5,0}}};
  if(strength==7){transform["rotation"]=.31f;transform["scale"]={.90f,.80f};}
  used.push_back("KHR_texture_transform");
 }
 const auto stem=strength==0?"strength-zero":strength==5?"strength-five":"strength-rotated";auto source_path=directory/(std::string(stem)+"-source.vrm");canonical_leaf_nodes(f);f.save(source_path);ImportAuthoringOptions settings;settings.paint_resolution=64;auto scene=prepare_authoring_import(Scene{},source_path,settings).scene;
 const auto selected=std::find_if(scene.objects().begin(),scene.objects().end(),[](const auto &v){return bool(v.appearance);});check(selected!=scene.objects().end(),"Typed strength fixture selects actual native source owner");auto &part=scene.object(selected->id);physical_ink(part,{.3f,.005f,10});Brush brush;brush.from=brush.to=part.painted->mesh().positions.front();brush.normal={0,0,1};brush.radius=5;brush.strength=.3f;PaintSettings paint;paint.channel=PaintChannel::Emission;paint.emission={.2f,.4f,.6f};paint.emission_units=PaintEmissionUnits::MaterialRelative;paint.front_only=false;part.painted->begin_stroke();part.painted->stamp(brush,paint);check(part.painted->end_stroke(),"Typed strength fixture mixes real relative stroke over physical contribution");
 const auto before=scene.encode();auto owner=std::make_shared<NativeEmissionExport>();owner->scene=std::make_shared<const Scene>(scene.snapshot());owner->allow_measured_core_fallback=true;VrmExportOptions options;options.emission=owner;const auto path=directory/(std::string(stem)+".vrm");export_vrm(emission_export_surfaces(*owner->scene),path,options);const auto bytes=file_bytes(path);const auto j=glb_json(path);const auto &record=j.at("extras").at("domeSculptEmission").at("images").at(0);const auto field=decode_emission_exr(resource(bytes,j,record));const auto &p=field.pixels.at(16*64+16);for(size_t k=0;k<3;++k){const float source=strength>0&&k==1?.04f*strength:0.f;const float expected=source*(1-p[9])+p[3+k]*strength+p[6+k];if(std::abs(p[k]-expected)>2e-6f)throw std::runtime_error("Typed authored strength/static original sampler is not applied exactly once");}check(p[3]>0&&p[6]>0&&p[9]>0&&p[9]<1,"Independent nearest original-image/half-tile KHR-transform oracle retains fractional relative/absolute mixture including authored-zero strength");fallback_consumer(path);auto restored=extract_native_emission(bytes);std::vector<uint64_t> ids;for(const auto &o:scene.objects())if(o.appearance)ids.push_back(o.id);check(restored&&restored->encode()==detail::emission_export_subset(scene,ids).encode()&&scene.encode()==before,"Typed strength exports retain raw sampler/factor/strength intent and exact current native history");
}

}

namespace {
Mesh accessory_part(float x) {
 Mesh mesh;
 mesh.positions={{x-.375f,-.375f,0},{x+.375f,-.375f,0},{x+.375f,.375f,0},{x-.375f,.375f,0}};
 mesh.normals.assign(4,{0,0,1});
 mesh.uv={{0,0},{1,0},{1,1},{0,1}};
 mesh.indices={0,1,2,0,2,3};
 return mesh;
}

void native_texture_set_accessory(const std::filesystem::path &out) {
 auto fixture=source(false);
 const import_fixture::Fixture uv_source;
 for(auto uv:{"TEXCOORD_0","TEXCOORD_1"})
  fixture.j["meshes"][0]["primitives"][0]["attributes"][uv]=uv_source.j.at("meshes").at(0).at("primitives").at(0).at("attributes").at(uv);
 fixture.j["materials"][0]["extensions"].erase("KHR_materials_unlit");
 auto &used=fixture.j["extensionsUsed"];
 used.erase(std::find(used.begin(),used.end(),"KHR_materials_unlit"));
 fixture.j["extensions"]["VRMC_vrm"]["expressions"]["custom"]["avatarTint"]={
     {"materialColorBinds",J::array({{{"material",0},{"type","color"},{"targetValue",{.75f,.5f,.25f,.6f}}}})}};
 canonical_leaf_nodes(fixture);
 const auto original=out/"accessory-source.vrm";
 fixture.save(original);
 ImportAuthoringOptions settings;
 settings.paint_resolution=64;

 Scene seed;
 const auto accessory=seed.active().id;
 seed.active().name="Native baked accessory";
 auto imported=prepare_authoring_import(seed,original,settings);
 auto scene=std::move(imported.scene);
 const auto avatar=imported.objects.front();
 const auto ancestry=scene.group_path(scene.object(avatar).group);
 const auto placement=std::find_if(ancestry.rbegin(),ancestry.rend(),[](const auto &g){return bool(g.source_placement);});
 check(placement!=ancestry.rend(),"Actual imported VRM1 supplies a source-instance root");
 const auto instance=placement->source_placement->source_instance;
 auto &object=scene.object(accessory);
 const auto workshop=scene.add_group("Accessory workshop",{{.125f,.25f,.5f},{},1},instance);
 object.group=scene.add_group("Accessory mount",{{-.25f,.125f,.25f},{},1},workshop);
 object.transform.position={.5f,-.25f,.125f};
 auto mesh=accessory_part(-.5f);
 const auto sibling=accessory_part(.5f);
 mesh.positions.insert(mesh.positions.end(),sibling.positions.begin(),sibling.positions.end());
 mesh.normals.insert(mesh.normals.end(),sibling.normals.begin(),sibling.normals.end());
 mesh.uv.insert(mesh.uv.end(),sibling.uv.begin(),sibling.uv.end());
 for(auto v:sibling.indices)mesh.indices.push_back(4+v);
 std::array<TextureSetInitialField,2> fields;
 fields[0].material.color={.42f,.55f,.77f,.75f};
 fields[0].material.alpha=Material::AlphaMode::Mask;
 fields[0].material.alpha_cutoff=.35f;
 fields[0].material.emissive={.012f,.024f,.042f};
 fields[0].resolution=512;
 fields[1].resolution=64;
 fields[1].material.emissive={.05f,1.25f,.2f};
 const std::array<TextureSetId,4> membership{0,0,1,1};
 initialize_texture_sets(scene,accessory,mesh,fields,membership);
 SourceMaterialBindings binding;
 const auto &native=fields[0].material;
 binding.native_reference={native.color,native.emissive,int(native.alpha),native.alpha_cutoff,native.double_sided};
 binding.material=MToonMaterial::from_vrm1({
     {"alphaMode","MASK"},{"alphaCutoff",native.alpha_cutoff},{"emissiveFactor",native.emissive},
     {"pbrMetallicRoughness",{{"baseColorFactor",native.color}}},
     {"normalTexture",{{"index",0}}},
     {"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"},{"shadeColorFactor",{.2f,.1f,.3f}}}},
                    {"KHR_materials_emissive_strength",{{"emissiveStrength",2.f}}}}}
 },1);
 binding.roles[size_t(MToonSlot::Normal)].image=std::make_shared<const Image>(Image{1,1,{140,165,248,255}});
 check(bind_texture_set_mtoon(scene,accessory,0,binding),"Native accessory owns its own typed material with a distinct PBR partition");
 auto donor_mesh=accessory_part(-.625f);
 for(auto &point:donor_mesh.positions)point.z+=.0125f;
 for(auto &normal:donor_mesh.normals)normal=normalized(Vec3{.2f,-.31f,1.f});
 const auto donor=scene.add("Hidden accessory Bake donor",Volume{},scene.object(accessory).transform);
 auto &source_object=scene.object(donor);
 source_object.group=scene.object(accessory).group;
 source_object.visible=false;
 source_object.mesh_active=true;
 source_object.material.color={.28f,.57f,.32f,.9f};
 source_object.material.emissive={.08f,1.5f,.18f};
 source_object.painted=std::make_shared<PaintedMesh>(std::move(donor_mesh),64);
 scene.select(accessory);

 Document doc;
 doc.replace(scene);
 check(doc.wait_mesh()&&doc.frame().state->can_bake,"Actual accessory uses the shared selected-field Bake worker");
 ObjectBakeSettings bake_settings;
 bake_settings.allow_partial=true;
 bake_settings.padding=0;
 bake_settings.ao_samples=2;
 doc.bake_from_object(donor,bake_settings);
 const auto project=out/"native-accessory.dsculpt";
 doc.save_async(project).get();
 const auto baked=doc.copy_scene();
 const auto &paint=texture_set_view(baked.object(accessory),0).painted;
 check(paint->baked()&&paint->baked()->emission_field&&
       std::any_of(paint->baked()->coverage.begin(),paint->baked()->coverage.end(),[](auto v){return v==0;})&&
       std::any_of(paint->baked()->coverage.begin(),paint->baked()->coverage.end(),[](auto v){return v>0;}),
       "Actual nonempty partial Bake retains exact FLOAT emission and uncovered normals");

 const auto source_only=detail::emission_export_subset(baked,imported.objects);
 const auto baseline=out/"accessory-avatar-baseline.vrm";
 export_vrm(source_only.export_surfaces(),baseline);
 check(doc.frame().state->can_export_avatar&&doc.copy_scene().active().id==accessory,
       "The selected nested accessory resolves the original avatar for Export Selected Avatar");
 const auto vrm_path=out/"native-accessory.vrm";
 doc.export_asset(vrm_path,AssetExportFormat::Avatar).get();
 const auto vrm_data=file_bytes(vrm_path);
 const auto j=glb_json(vrm_path);
 check(j.at("extensions").at("VRMC_vrm").at("meta").at("name")=="MatCap only",
       "Regular Document Avatar export preserves the original VRM1 model metadata");
 const auto graph=geometry(vrm_path);
 auto original_graph=graph;
 std::erase_if(original_graph.get_ref<J::array_t &>(),[](const auto &entry){
  const auto name=entry.at("name").template get<std::string>();
  return name=="Native baked accessory"||name=="Accessory workshop"||name=="Accessory mount";
 });
 check(original_graph==geometry(baseline),
       "Independent source node transforms, ordered geometry, and morph identities survive the accessory export");
 const auto native_graph=std::find_if(graph.begin(),graph.end(),[](const auto &entry){
  return entry.at("name")=="Native baked accessory";
 });
 const auto native_node=std::find_if(j.at("nodes").begin(),j.at("nodes").end(),[](const auto &entry){
  return entry.value("name",std::string{})=="Native baked accessory"&&entry.contains("mesh");
 });
 const auto mount=std::find_if(j.at("nodes").begin(),j.at("nodes").end(),[](const auto &entry){
  return entry.value("name",std::string{})=="Accessory mount";
 });
 check(native_graph!=graph.end()&&native_node!=j.at("nodes").end()&&mount!=j.at("nodes").end()&&
       native_graph->at("world")==J(baked.world_matrix(accessory).float_matrix())&&
       mount->at("children").at(0)==std::distance(j.at("nodes").begin(),native_node)&&
       !native_node->contains("skin"),
       "Independent importer finds the rigid native mesh under its exact nested avatar placement");

 const auto &primitives=j.at("meshes").at(native_node->at("mesh").get<size_t>()).at("primitives");
 check(primitives.size()==2&&primitives.at(0).at("extras").at("dome_sculpt_texture_set").at("id")==0&&
       primitives.at(1).at("extras").at("dome_sculpt_texture_set").at("id")==1&&
       primitives.at(0).at("attributes").contains("TANGENT"),
       "Both accessory face partitions and the certified tangent frame reach ordinary VRM primitives");
 const auto &material=j.at("materials").at(primitives.at(0).at("material").get<size_t>());
 const auto &pbr=j.at("materials").at(primitives.at(1).at("material").get<size_t>());
 check(material.at("extensions").contains("VRMC_materials_mtoon")&&
       material.at("extensions").at("KHR_materials_emissive_strength").at("emissiveStrength")>=1.49&&
       !pbr.at("extensions").contains("VRMC_materials_mtoon"),
       "The authored MToon accessory and its PBR sibling preserve separate physical materials");
 const auto &targets=j.at("extensions").at("VRMC_vrm").at("expressions").at("custom").at("avatarTint").at("materialColorBinds");
 check(targets.size()==imported.objects.size()&&std::none_of(targets.begin(),targets.end(),[&](const auto &target){
  return target.at("material")==primitives.at(0).at("material")||target.at("material")==primitives.at(1).at("material");
 }),"Original avatar expressions retain their source parts and never target the native accessory");

 const auto &entries=j.at("extras").at("domeSculptEmission").at("images");
 check(entries.size()==2&&entries.at(0).at("view")=="authored-mtoon"&&entries.at(1).at("view")=="pbr"&&
       entries.at(0).at("object")==accessory&&entries.at(1).at("object")==accessory,
       "Both FLOAT resources are bound to their genuine VRM node, material and partition");
 const auto exr=decode_emission_exr(resource(vrm_data,j,entries.at(0)));
 check(exr.width==512&&std::any_of(exr.pixels.begin(),exr.pixels.end(),[](const auto &q){
  return q[9]==1.f&&q[1]==1.5f&&q[7]==1.5f;
 }),"Independent OpenEXR decode preserves absolute donor brightness separately from MToon gain");
 auto ids=imported.objects;
 ids.push_back(accessory);
 const auto expected=detail::emission_export_subset(doc.copy_scene(),ids);
 const auto restored=extract_native_emission(vrm_data);
 check(restored&&restored->encode()==expected.encode()&&
       texture_set_view(restored->object(accessory),0).bake_recipe->source.object==unavailable_bake_object,
       "VRM restores the exact selected avatar and accessory with the excluded donor explicitly unavailable");
 auto owner=std::make_shared<NativeEmissionExport>();
 owner->scene=std::make_shared<const Scene>(restored->snapshot());
 owner->allow_measured_core_fallback=true;
 VrmExportOptions options;
 options.emission=owner;
 const auto again=out/"native-accessory-second.vrm";
 export_vrm(emission_export_surfaces(*owner->scene),again,options);
 check(file_bytes(again)==vrm_data,"Second VRM1 export reproduces every byte from the exact restored native project");

 check(detail::editable_avatar_view(doc.frame()),"The selected accessory enables the shared Editable VRM control");
 const auto editable_path=out/"native-accessory-editable.vrm";
 doc.export_asset(editable_path,AssetExportFormat::EditableAvatar512).get();
 const auto editable_bytes=file_bytes(editable_path);
 const auto editable=glb_json(editable_path);
 check(geometry(editable_path)==graph,"Editable VRM preserves the same ordered geometry, actual node placements and morphs as Standard VRM");
 const auto &editable_package=editable.at("extras").at("domeSculptAuthoring");
 const auto accessory_map=std::ranges::find_if(editable_package.at("objects"),[&](const auto &entry){
  return entry.at("object")==std::to_string(accessory);
 });
 check(accessory_map!=editable_package.at("objects").end()&&editable_package.at("scope")=="exportedObjects"&&
       editable.at("nodes").at(accessory_map->at("node").template get<size_t>()).at("name")=="Native baked accessory",
       "The editable package maps the accessory to its actual portable node in the exported subset");
 const auto &editable_primitives=editable.at("meshes").at(accessory_map->at("mesh").get<size_t>()).at("primitives");
 bool same_materials=editable_primitives.size()==primitives.size();
 const auto material_value=[&](const auto &portable_path,const auto &document,const auto &primitive) {
  auto view=document.at("materials").at(primitive.at("material").template get<size_t>());
  const auto bind=[&](J &value,const char *role) {
   if(value.contains(role)) {
    auto &slot=value.at(role);
    slot["index"]=thumbnail(portable_path,document,slot.at("index").get<int>(),true);
    slot.at("index").at("image").erase("name");
   }
  };
  for(auto role:{"normalTexture","occlusionTexture","emissiveTexture"})bind(view,role);
  for(auto role:{"baseColorTexture","metallicRoughnessTexture"})bind(view.at("pbrMetallicRoughness"),role);
  if(view.at("extensions").contains("VRMC_materials_mtoon"))bind(view.at("extensions").at("VRMC_materials_mtoon"),"shadeMultiplyTexture");
  return view;
 };
 for(size_t i=0;same_materials&&i<primitives.size();++i)
  same_materials=editable_primitives.at(i).at("extras")==primitives.at(i).at("extras")&&
      material_value(editable_path,editable,editable_primitives.at(i))==material_value(vrm_path,j,primitives.at(i));
 check(same_materials,"Independent portable images, samplers, FLOAT factors and texture-set membership match Standard VRM");
 check(editable.at("extensions").at("VRMC_vrm").at("expressions")==j.at("extensions").at("VRMC_vrm").at("expressions"),
       "Editable VRM does not route original expression bindings to the native accessory");
 const auto editable_scene=extract_native_authoring(editable_bytes);
 check(editable_scene&&editable_scene->encode()==expected.encode(),"Editable VRM restores the exact FLOAT bake, source graph and excluded-donor state");
 Document reopened;
 reopened.open_async(editable_path).get();
 check(reopened.wait_mesh()&&detail::editable_avatar_view(reopened.frame())&&reopened.copy_scene().encode()==expected.encode(),
       "Document Open reopens the exact editable subset and enables export from the accessory");
 const auto editable_again=out/"native-accessory-editable-second.vrm";
 reopened.export_asset(editable_again,AssetExportFormat::EditableAvatar512).get();
 check(file_bytes(editable_again)==editable_bytes,"Reopening and re-exporting Editable VRM reproduces every byte");

 auto unrelated=restored->snapshot();
 unrelated.object(accessory).group=unrelated.add_group("Outside the avatar");
 owner->scene=std::make_shared<const Scene>(std::move(unrelated));
 bool rejected=false;
 try {
  export_vrm(emission_export_surfaces(*owner->scene),again,options);
 } catch(const std::invalid_argument &e) {
  rejected=std::string_view(e.what()).find("outside the original avatar's instance")!=std::string_view::npos;
 }
 check(rejected&&file_bytes(again)==vrm_data,"An unrelated accessory cannot enter the avatar or replace the previous export");
 NativeAuthoringExport editable_owner;
 editable_owner.scene=owner->scene;
 editable_owner.allow_portable_fallback=true;
 rejected=false;
 try {
  export_vrm_authoring(editable_owner,editable_again);
 } catch(const std::invalid_argument &e) {
  rejected=std::string_view(e.what()).find("outside the original avatar's instance")!=std::string_view::npos;
 }
 check(rejected&&file_bytes(editable_again)==editable_bytes,"Editable VRM also refuses a foreign accessory before replacing the destination");
}
}
int main(int argc,char**argv)try{if(argc!=2&&argc!=3)return 2;std::filesystem::path out=argv[1];std::filesystem::create_directories(out);native_texture_set_accessory(out);if(argc==3){std::cout<<"VRM1 native texture set: "<<checks<<" checks\n";return 0;}for(auto base:{"vroid-beta-female","vroid-beta-male"}){Scene scene;append_humanoid(scene,humanoid_baseline_defaults(base),base,{},64);current(out,std::move(scene),base);}auto f=source(false);import_fixture::Fixture original;for(auto uv:{"TEXCOORD_0","TEXCOORD_1"})f.j["meshes"][0]["primitives"][0]["attributes"][uv]=original.j["meshes"][0]["primitives"][0]["attributes"][uv];f.j["materials"][0]["extensions"].erase("KHR_materials_unlit");auto &used=f.j["extensionsUsed"];for(auto i=used.begin();i!=used.end();)if(i->get_ref<const std::string&>()=="KHR_materials_unlit")i=used.erase(i);else ++i;auto path=out/"independent-v1-source.vrm";canonical_leaf_nodes(f);f.save(path);ImportAuthoringOptions options;options.paint_resolution=64;auto scene=prepare_authoring_import(Scene{},path,options).scene;current(out,std::move(scene),"independent-v1");for(float strength:{0.f,5.f,7.f})strength_case(out,strength);std::cout<<"HDR typed VRM: "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
