// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/native-authoring.hpp"
#include "native-authoring-capture.hpp"
#include "native-authoring-compiler.hpp"
#include "native-scene-payload.hpp"
#include "emission-package.hpp"
#include "emission-authority.hpp"
#include "gltf-document.hpp"
#include "humanoid-sha256.hpp"
#include <charconv>
#include <algorithm>
#include <set>
namespace dome {
namespace {
using Json=nlohmann::json;
constexpr const char *marker="domeSculptAuthoring";
constexpr const char *hdr_marker="domeSculptEmission";
constexpr size_t container_cap=256*1024*1024,stored_cap=128*1024*1024;
constexpr size_t decoded_cap=256*1024*1024,json_cap=16*1024*1024;
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Native authoring export/read cancelled");}
size_t integer(const Json &v,size_t cap,const char *message){
 if(!v.is_number_unsigned()&&!v.is_number_integer())throw std::invalid_argument(message);
 if(!v.is_number_unsigned()&&v.get<int64_t>()<0)throw std::invalid_argument(message);
 const auto n=v.get<uint64_t>();if(n>cap)throw std::length_error(message);return size_t(n);
}
uint64_t object_id(const Json &v){
 if(!v.is_string())throw std::invalid_argument("Native authoring object identity requires a decimal string");
 const auto &text=v.get_ref<const std::string&>();uint64_t value=0;
 if(text.empty()||text.size()>20||text.front()=='0')throw std::invalid_argument("Invalid native authoring object identity");
 const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
 if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!value)throw std::invalid_argument("Invalid native authoring object identity");return value;
}
std::string hash(std::span<const uint8_t> bytes){return creator_detail::sha256(bytes);}
std::string document_hash(Json value){value.at("extras").at(marker).erase("documentSha256");return hash(Json::to_cbor(value));}
uint32_t word(std::span<const uint8_t> bytes,size_t at){if(at>bytes.size()||bytes.size()-at<4)throw std::invalid_argument("Truncated native authoring GLB");return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;}
void tables(const Json &j){
 for(const auto *name:{"nodes","meshes","materials","bufferViews"})if(!j.contains(name)||!j.at(name).is_array()||j.at(name).size()>65536)throw std::length_error("Native authoring resource table exceeds limits");
}
std::span<const uint8_t> resource(const Json &j,std::span<const uint8_t> binary,const Json &record){
 if(!record.is_object()||record.size()!=3)throw std::invalid_argument("Invalid native authoring payload descriptor");
 const auto index=integer(record.at("bufferView"),j.at("bufferViews").size(),"Invalid native authoring bufferView");if(index>=j.at("bufferViews").size())throw std::invalid_argument("Invalid native authoring bufferView");const auto &view=j.at("bufferViews").at(index);
 if(!view.is_object()||view.contains("target")||view.contains("byteStride")||integer(view.at("buffer"),0,"Native authoring payload requires captured buffer zero"))throw std::invalid_argument("Invalid native authoring byte view");
 const size_t offset=view.contains("byteOffset")?integer(view.at("byteOffset"),binary.size(),"Invalid native authoring byte offset"):0;
 const size_t count=integer(view.at("byteLength"),stored_cap,"Native authoring stored payload exceeds limits");
 if(!count||count>binary.size()-offset)throw std::invalid_argument("Native authoring payload exceeds captured BIN");const auto bytes=binary.subspan(offset,count);
 if(record.at("sha256").get_ref<const std::string&>()!=hash(bytes))throw std::invalid_argument("Native authoring resource digest mismatch");return bytes;
}
void attach(detail::GltfDocument &document,const detail::NativeAuthoringCapture &capture,size_t maximum,const std::function<bool()> &cancelled){
 stop(cancelled);auto &j=document.json;if(j.contains("extras")&&!j.at("extras").is_object())throw std::invalid_argument("Native authoring cannot replace nonobject extras");
 if(j.contains("extras")&&(j.at("extras").contains(marker)||j.at("extras").contains(hdr_marker)))throw std::invalid_argument("Competing native package authority");tables(j);
 if(document.objects.size()!=capture.ids().size())throw std::invalid_argument("Native authoring compiler omitted an authenticated original object");
 Json remaps=Json::array();std::set<int> nodes;
 for(size_t i=0;i<capture.ids().size();++i){stop(cancelled);const auto &identity=document.objects.at(i);if(identity.node<0||identity.mesh<0||identity.material<0||size_t(identity.node)>=j.at("nodes").size()||size_t(identity.mesh)>=j.at("meshes").size()||size_t(identity.material)>=j.at("materials").size()||!nodes.insert(identity.node).second||j.at("nodes").at(identity.node).at("mesh").get<int>()!=identity.mesh)throw std::invalid_argument("Native authoring compiler produced an invalid object remap");
  remaps.push_back({{"object",std::to_string(capture.ids()[i])},{"node",identity.node},{"mesh",identity.mesh},{"material",identity.material}});
 }
 auto native=detail::capture_native_scene_payload(capture.scene(),cancelled);
 auto &binary=document.binary;const size_t padding=(4-binary.size()%4)%4;
 if(binary.size()>maximum||padding>maximum-binary.size()||native.stored.size()>maximum-binary.size()-padding)throw std::length_error("Native authoring package exceeds output byte budget");
 binary.insert(binary.end(),padding,0);const auto offset=binary.size();binary.insert(binary.end(),native.stored.begin(),native.stored.end());if(j.at("bufferViews").size()==65536)throw std::length_error("Native authoring bufferView table exceeds limits");const size_t view=j["bufferViews"].size();j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",native.stored.size()}});
 Json package={{"version",1},{"encoding",native.compressed?"scene-cbor-deflate":"scene-cbor"},{"scope",capture.scope()==NativeAuthoringExport::Scope::WholeScene?"wholeScene":"exportedObjects"},{"native",{{"bufferView",view},{"sha256",hash(native.stored)},{"decodedBytes",native.decoded_bytes}}},{"objects",std::move(remaps)},{"fallback",{{"view","existing-gltf-core"},{"lossless",false}}}};
 while(binary.size()%4){if(binary.size()==maximum)throw std::length_error("Native authoring padding exceeds output limit");binary.push_back(0);}j["buffers"]={{{"byteLength",binary.size()}}};package["binarySha256"]=hash(binary);package["documentSha256"]="";j["extras"][marker]=std::move(package);j["extras"][marker]["documentSha256"]=document_hash(j);
 if(j.dump().size()>json_cap)throw std::length_error("Native authoring JSON exceeds16MiB");stop(cancelled);
}
}
namespace detail {
NativeAuthoringCapture::NativeAuthoringCapture(std::shared_ptr<const Scene> scene,std::vector<ObjectSurface> originals,std::vector<uint64_t> ids,NativeAuthoringExport::Scope scope):scene_(std::move(scene)),originals_(std::move(originals)),ids_(std::move(ids)),scope_(scope){}
std::shared_ptr<const NativeAuthoringCapture> NativeAuthoringCapture::create(const NativeAuthoringExport &options,const std::function<bool()> &cancelled){
 stop(cancelled);if(!options.scene||!options.allow_portable_fallback)throw std::invalid_argument("Native authoring requires an immutable Scene and explicit portable-view permission");
 if(options.scope!=NativeAuthoringExport::Scope::WholeScene&&options.scope!=NativeAuthoringExport::Scope::ExportedObjects)throw std::invalid_argument("Unknown native authoring export scope");
 if((options.scope==NativeAuthoringExport::Scope::WholeScene&&!options.objects.empty())||(options.scope==NativeAuthoringExport::Scope::ExportedObjects&&options.objects.empty()))throw std::invalid_argument("Native authoring scope and object selection disagree");
 auto scene=options.scene->snapshot();if(options.scope==NativeAuthoringExport::Scope::ExportedObjects)scene=emission_export_subset(scene,options.objects);scene.validate();scene.require_exportable_sources();stop(cancelled);
 // No caller-provided surface or changed-surface equivalence flag enters here.
 const bool physical=std::any_of(scene.objects().begin(),scene.objects().end(),[](const auto &o){return detail::hdr_emission(o.material.emissive)||(o.painted&&o.painted->has_physical_emission())||(o.material_bindings&&detail::physical_emission(*o.material_bindings));});
 auto originals=physical?emission_export_surfaces(scene,cancelled):scene.preview_surfaces(cancelled);if(originals.empty()||originals.size()>256)throw std::length_error("Native authoring requires1..256 emitted objects");std::vector<uint64_t> ids;std::set<uint64_t> unique;
 for(const auto &object:originals){stop(cancelled);scene.object(object.id);if(!unique.insert(object.id).second)throw std::invalid_argument("Duplicate native authoring original identity");ids.push_back(object.id);}
 if(options.scope==NativeAuthoringExport::Scope::ExportedObjects&&unique!=std::set<uint64_t>(options.objects.begin(),options.objects.end()))throw std::invalid_argument("Native authoring subset contains hidden or empty nonexported objects");
 return std::shared_ptr<const NativeAuthoringCapture>(new NativeAuthoringCapture(std::make_shared<const Scene>(std::move(scene)),std::move(originals),std::move(ids),options.scope));
}
}
GltfExportReport export_glb_authoring(const NativeAuthoringExport &owner,const std::filesystem::path &path,const GltfExportOptions &options){
 return export_glb_authoring(owner,path,options,SourceUvBakeOptions{});
}
GltfExportReport export_glb_authoring(const NativeAuthoringExport &owner,const std::filesystem::path &path,const GltfExportOptions &requested,const SourceUvBakeOptions &paint_policy){
 auto options=requested;options.cancelled=[first=requested.cancelled,second=paint_policy.cancelled]{return (first&&first())||(second&&second());};
 stop(options.cancelled);if(options.emission)throw std::invalid_argument("Competing native HDR and authoring export owners");if(path.extension()!=".glb")throw std::invalid_argument("Native authoring export requires.glb");if(!options.max_output_bytes||options.max_output_bytes>container_cap)throw std::invalid_argument("Invalid native authoring output limit");
 const auto capture=detail::NativeAuthoringCapture::create(owner,options.cancelled);
 auto compiler_policy=paint_policy;compiler_policy.cancelled={};
 auto compiled=detail::compile_native_authoring(*capture,compiler_policy,options.max_output_bytes,options.cancelled);
 auto document=detail::make_gltf(compiled.surfaces,detail::SourceMorphSpace::Native,options.recompute_changed_normals,options.cancelled);
 detail::finish_native_authoring_emission(document,compiled,options.max_output_bytes,options.cancelled);
 attach(document,*capture,options.max_output_bytes,options.cancelled);const uint64_t size=28+((uint64_t(document.json.dump().size())+3)&~uint64_t(3))+((uint64_t(document.binary.size())+3)&~uint64_t(3));if(size>options.max_output_bytes)throw std::length_error("Complete native authoring GLB exceeds output budget");
 GltfExportReport report{capture->ids().size(),document.normal_targets_recomputed,{"Exact native authoring Scene retained; portable core view is not declared lossless"}};report.diagnostics.insert(report.diagnostics.end(),compiled.diagnostics.begin(),compiled.diagnostics.end());stop(options.cancelled);detail::write_glb(std::move(document),path,options.cancelled,options.before_commit);return report;
}
std::optional<Scene> extract_native_authoring(std::span<const uint8_t> bytes,const NativeAuthoringReadLimits &limits){
 stop(limits.cancelled);if(!limits.max_container_bytes||limits.max_container_bytes>container_cap||!limits.max_json_bytes||limits.max_json_bytes>json_cap||!limits.max_native_bytes||limits.max_native_bytes>stored_cap||!limits.max_decoded_native_bytes||limits.max_decoded_native_bytes>decoded_cap||!limits.max_objects||limits.max_objects>256||bytes.size()>limits.max_container_bytes)throw std::length_error("Invalid native authoring read limits");
 if(bytes.size()<20||word(bytes,0)!=0x46546c67||word(bytes,4)!=2||word(bytes,8)!=bytes.size()||word(bytes,16)!=0x4e4f534a)throw std::invalid_argument("Invalid native authoring GLB framing");const size_t length=word(bytes,12);if(length%4||length>limits.max_json_bytes||length>bytes.size()-20)throw std::length_error("Native authoring JSON exceeds read limit");size_t visits=0;
 const auto j=Json::parse(bytes.begin()+20,bytes.begin()+20+length,[&](int depth,Json::parse_event_t,const Json&){if(depth>128||++visits>1024*1024)throw std::length_error("Native authoring JSON depth/count exceeded");if((visits&4095)==0)stop(limits.cancelled);return true;});
 if(!j.is_object())throw std::invalid_argument("Invalid native authoring JSON root");if(!j.contains("extras")||!j.at("extras").is_object()||!j.at("extras").contains(marker))return std::nullopt;if(j.at("extras").contains(hdr_marker))throw std::invalid_argument("Competing native package authority");const auto &package=j.at("extras").at(marker);
 if(!package.is_object()||package.size()!=8||integer(package.at("version"),1,"Unknown native authoring package version")!=1)throw std::invalid_argument("Unknown native authoring package interpretation");const auto &encoding=package.at("encoding").get_ref<const std::string&>();if(encoding!="scene-cbor"&&encoding!="scene-cbor-deflate")throw std::invalid_argument("Unknown native authoring payload encoding");const auto &scope=package.at("scope").get_ref<const std::string&>();if(scope!="wholeScene"&&scope!="exportedObjects")throw std::invalid_argument("Unknown native authoring object scope");const auto &fallback=package.at("fallback");if(!fallback.is_object()||fallback.size()!=2||fallback.at("view").get_ref<const std::string&>()!="existing-gltf-core"||!fallback.at("lossless").is_boolean()||fallback.at("lossless").get<bool>())throw std::invalid_argument("Unknown native authoring portable-view interpretation");tables(j);
 const size_t at=20+length;if(at>bytes.size()||bytes.size()-at<8||word(bytes,at+4)!=0x004e4942)throw std::invalid_argument("Native authoring has no captured BIN");const size_t count=word(bytes,at);if(count%4||count!=bytes.size()-at-8)throw std::invalid_argument("Native authoring BIN framing mismatch");const auto binary=bytes.subspan(at+8,count);if(!j.at("buffers").is_array()||j.at("buffers").size()!=1||integer(j.at("buffers")[0].at("byteLength"),binary.size(),"Invalid native authoring buffer length")!=binary.size())throw std::invalid_argument("Native authoring buffer declaration mismatch");
 if(package.at("binarySha256").get_ref<const std::string&>()!=hash(binary)||package.at("documentSha256").get_ref<const std::string&>()!=document_hash(j))throw std::invalid_argument("Native authoring core/resource binding mismatch");const auto &maps=package.at("objects");if(!maps.is_array()||maps.empty()||maps.size()>limits.max_objects)throw std::length_error("Native authoring object remaps exceed limits");std::set<uint64_t> identities;std::set<size_t> nodes;
 for(const auto &map:maps){stop(limits.cancelled);if(!map.is_object()||map.size()!=4||!identities.insert(object_id(map.at("object"))).second)throw std::invalid_argument("Invalid native authoring object remap");const auto node=integer(map.at("node"),j.at("nodes").size(),"Invalid native authoring node"),mesh=integer(map.at("mesh"),j.at("meshes").size(),"Invalid native authoring mesh"),material=integer(map.at("material"),j.at("materials").size(),"Invalid native authoring material");if(node>=j.at("nodes").size()||mesh>=j.at("meshes").size()||material>=j.at("materials").size()||!nodes.insert(node).second||integer(j.at("nodes").at(node).at("mesh"),j.at("meshes").size(),"Invalid native authoring node mesh")!=mesh)throw std::invalid_argument("Native authoring object remap references invalid resources");const auto &primitives=j.at("meshes").at(mesh).at("primitives");if(!primitives.is_array()||primitives.empty()||integer(primitives.at(0).at("material"),j.at("materials").size(),"Invalid native authoring primitive material")!=material)throw std::invalid_argument("Native authoring material remap does not match emitted primitive");}
 const auto &native=package.at("native");const size_t expanded=integer(native.at("decodedBytes"),limits.max_decoded_native_bytes,"Native authoring expanded limit exceeded");const auto stored=resource(j,binary,native);if(stored.size()>limits.max_native_bytes)throw std::length_error("Native authoring stored limit exceeded");auto scene=detail::restore_native_scene_payload(stored,encoding=="scene-cbor-deflate",expanded,limits.max_decoded_native_bytes,limits.cancelled);for(auto id:identities)scene.object(id);if(scope=="exportedObjects"&&scene.objects().size()!=identities.size())throw std::invalid_argument("Native authoring subset contains unexported outside objects");stop(limits.cancelled);return scene;
}
}
