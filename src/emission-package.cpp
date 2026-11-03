// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-package.hpp"
#include "humanoid-sha256.hpp"
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <cstring>
#include <zlib.h>
#include <streambuf>
#include <ostream>
namespace dome {namespace {
using Json=nlohmann::json;
constexpr const char *marker="domeSculptEmission";
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Native HDR export/extraction cancelled");}
std::string hash(std::span<const uint8_t> b){return creator_detail::sha256(b);}
size_t integer(const Json &v,size_t cap,const char *message){if(!v.is_number_unsigned()&&!v.is_number_integer())throw std::invalid_argument(message);if(!v.is_number_unsigned()&&v.get<int64_t>()<0)throw std::invalid_argument(message);const auto n=v.get<uint64_t>();if(n>cap)throw std::length_error(message);return size_t(n);}
std::string document_hash(Json j){auto &entry=j.at("extras").at(marker);entry.erase("documentSha256");const auto b=Json::to_cbor(j);return hash(b);}
bool marker_object(const Json &j){
 if(!j.is_object()||j.size()!=6||!j.at("images").is_array())throw std::invalid_argument("Unknown native HDR package interpretation");
 const auto version=integer(j.at("version"),2,"Unknown native HDR package version");
 const auto &encoding=j.at("encoding").get_ref<const std::string&>();
 if((version==1&&encoding=="scene-cbor+openexr-float32-zip")||(version==2&&encoding=="scene-cbor-deflate+openexr-float32-zip"))return version==2;
 throw std::invalid_argument("Unknown native HDR package interpretation");
}
void append_bounded(std::vector<uint8_t> &out,std::span<const uint8_t> bytes,size_t cap){
 if(out.size()>cap||bytes.size()>cap-out.size())throw std::length_error("Native HDR encoded/expanded byte limit exceeded");const size_t required=out.size()+bytes.size();
 if(required>out.capacity()){const size_t doubled=out.capacity()>cap/2?cap:out.capacity()*2;out.reserve(std::max(required,doubled));}
 out.insert(out.end(),bytes.begin(),bytes.end());
}
// Bounded CBOR output checks prospective growth, including binary leaves, before
// allocating encoded storage. The owning Scene's existing image/source limits
// remain independent from both encoded and expanded package quotas.
class BoundedCbor final:public std::streambuf {
 std::function<bool()> cancelled_;size_t next_poll_=0;
 void append(const char *bytes,size_t count){if(count>256*1024*1024-data.size())throw std::length_error("Native HDR decoded CBOR exceeds256MiB");if(data.size()>=next_poll_){stop(cancelled_);next_poll_=data.size()+65536;}append_bounded(data,std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes),count),256*1024*1024);}
 std::streamsize xsputn(const char *bytes,std::streamsize count)override{if(count<0)throw std::invalid_argument("Negative native CBOR write");append(bytes,size_t(count));return count;}
 int_type overflow(int_type value)override{if(traits_type::eq_int_type(value,traits_type::eof()))return traits_type::not_eof(value);const char byte=traits_type::to_char_type(value);append(&byte,1);return value;}
 public:std::vector<uint8_t> data;explicit BoundedCbor(const std::function<bool()> &cancelled):cancelled_(cancelled){}
};
std::vector<uint8_t> encoded_scene(const Scene &owner,const std::function<bool()> &cancelled){BoundedCbor sink(cancelled);std::ostream output(&sink);output.exceptions(std::ios::badbit|std::ios::failbit);Json::to_cbor(owner.encode(),output);stop(cancelled);return std::move(sink.data);}
std::vector<uint8_t> native_deflate(std::span<const uint8_t> input,bool encode,size_t expected,const std::function<bool()> &cancelled){
 z_stream stream{};const int init=encode?deflateInit(&stream,Z_DEFAULT_COMPRESSION):inflateInit(&stream);if(init!=Z_OK)throw std::runtime_error("Cannot initialize native HDR lossless codec");
 struct Cleanup{z_stream &s;bool encode;~Cleanup(){if(encode)deflateEnd(&s);else inflateEnd(&s);}} cleanup{stream,encode};
 const size_t cap=encode?128*1024*1024:expected;if(!cap||cap>256*1024*1024)throw std::length_error("Invalid native HDR expanded byte limit");
 std::vector<uint8_t> result;std::array<uint8_t,65536> chunk{};size_t assigned=0;int status=Z_OK;
 do{stop(cancelled);if(stream.avail_in==0&&assigned<input.size()){const auto count=std::min<size_t>(65536,input.size()-assigned);stream.next_in=const_cast<Bytef*>(input.data()+assigned);stream.avail_in=uInt(count);assigned+=count;}
 stream.next_out=chunk.data();stream.avail_out=chunk.size();const auto before=stream.total_in;
 status=encode?deflate(&stream,assigned==input.size()?Z_FINISH:Z_NO_FLUSH):inflate(&stream,Z_NO_FLUSH);
 if(status!=Z_OK&&status!=Z_STREAM_END)throw std::invalid_argument("Invalid or truncated native HDR lossless stream");const size_t produced=chunk.size()-stream.avail_out;
 if(produced>cap-result.size())throw std::length_error("Native HDR lossless expansion exceeds declared byte limit");append_bounded(result,std::span<const uint8_t>(chunk).first(produced),cap);
 if(status!=Z_STREAM_END&&!produced&&stream.total_in==before&&assigned==input.size())throw std::invalid_argument("Truncated native HDR lossless stream");
 }while(status!=Z_STREAM_END);
 if(assigned!=input.size()||stream.avail_in||(!encode&&result.size()!=expected))throw std::invalid_argument("Native HDR lossless length/trailing bytes mismatch");stop(cancelled);return result;
}
void fallback(const Json &f){
 if(!f.is_object()||f.size()!=6||f.at("evaluation").get_ref<const std::string&>()!="current-static-view")throw std::invalid_argument("Unknown native HDR fallback interpretation");
 for(const auto *key:{"strength","maximumAbsoluteError","maximumRelativeError"}){if(!f.at(key).is_number())throw std::invalid_argument("Invalid native HDR fallback metric");const auto v=f.at(key).get<double>();if(!std::isfinite(v)||v<0||v>(std::strcmp(key,"maximumRelativeError")==0?1e6:30000)||(!std::strcmp(key,"strength")&&v<1))throw std::invalid_argument("Invalid native HDR fallback metric");}
 if(!f.at("emissiveFactor").is_array()||f.at("emissiveFactor").size()!=3)throw std::invalid_argument("Invalid native HDR fallback factor");for(const auto &value:f.at("emissiveFactor")){if(!value.is_number())throw std::invalid_argument("Invalid native HDR fallback factor");const auto v=value.get<double>();if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid native HDR fallback factor");}integer(f.at("erasedNonzeroSamples"),16*1024*1024*3,"Invalid native HDR fallback erased count");
}
std::span<const uint8_t> view(const Json &j,std::span<const uint8_t> binary,const Json &record){
 if(!record.is_object()||!record.contains("bufferView")||!record.contains("sha256"))throw std::invalid_argument("Invalid native HDR resource descriptor");
 const auto &views=j.at("bufferViews");if(!views.is_array()||views.size()>65536)throw std::length_error("Native HDR bufferView table exceeds limits");const auto index=integer(record.at("bufferView"),views.size(),"Invalid native HDR bufferView");if(index>=views.size())throw std::invalid_argument("Invalid native HDR bufferView");const auto &v=views[index];
 if(!v.is_object()||v.contains("target")||v.contains("byteStride")||integer(v.at("buffer"),0,"Native HDR resource must use captured buffer zero")!=0)throw std::invalid_argument("Invalid native HDR byte view");
 const auto offset=v.contains("byteOffset")?integer(v.at("byteOffset"),binary.size(),"Invalid native HDR byte offset"):0;const auto length=integer(v.at("byteLength"),binary.size(),"Invalid native HDR byte length");if(!length||length>binary.size()-offset)throw std::invalid_argument("Native HDR byte view exceeds captured BIN");const auto result=binary.subspan(offset,length);if(record.at("sha256").get_ref<const std::string&>()!=hash(result))throw std::invalid_argument("Native HDR resource digest mismatch");return result;
}
uint32_t word(std::span<const uint8_t> bytes,size_t offset){if(offset>bytes.size()||bytes.size()-offset<4)throw std::invalid_argument("Truncated native HDR GLB");return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|(uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);}
// Our native writer uses definite CBOR. Authenticate its framing/depth/counts
// without constructing a DOM or allocating a declared byte/string/array length.
void cbor_preflight(std::span<const uint8_t> bytes,const std::function<bool()> &cancelled){
 size_t offset=0,visits=0;auto parse=[&](auto &&self,unsigned depth)->void{
  if(depth>128||offset==bytes.size()||++visits>32*1024*1024)throw std::length_error("Native HDR CBOR depth/count exceeds limits");if((visits&4095)==0)stop(cancelled);const auto code=bytes[offset++];const unsigned major=code>>5,extra=code&31;uint64_t value=extra;
  if(extra>=24){if(extra>27)throw std::invalid_argument("Native HDR CBOR requires definite framing");const size_t count=size_t(1)<<(extra-24);if(count>bytes.size()-offset)throw std::invalid_argument("Truncated native HDR CBOR head");value=0;for(size_t i=0;i<count;++i)value=(value<<8)|bytes[offset++];}
  if(major==2||major==3){if(value>bytes.size()-offset)throw std::length_error("Native HDR CBOR byte/string declaration exceeds captured payload");offset+=size_t(value);}
  else if(major==4||major==5){if(value>(bytes.size()-offset)/(major==5?2:1))throw std::length_error("Native HDR CBOR container declaration exceeds captured payload");for(uint64_t i=0;i<value;++i){self(self,depth+1);if(major==5)self(self,depth+1);}}
  else if(major==6)throw std::invalid_argument("Unsupported native HDR CBOR tag");
  else if(major==7&&extra!=20&&extra!=21&&extra!=22&&extra!=25&&extra!=26&&extra!=27)throw std::invalid_argument("Unsupported native HDR CBOR scalar");
 };parse(parse,0);if(offset!=bytes.size())throw std::invalid_argument("Trailing native HDR CBOR data");stop(cancelled);
}
}
Scene detail::emission_export_subset(const Scene &source,std::span<const uint64_t> exported){
 std::set<uint64_t> selected;for(auto id:exported){source.object(id);if(!selected.insert(id).second)throw std::invalid_argument("Duplicate native export subset object");}if(selected.empty())throw std::invalid_argument("No native export subset objects");auto owner=source.snapshot();std::vector<uint64_t> discard;for(const auto &o:owner.objects())if(!selected.contains(o.id))discard.push_back(o.id);for(auto id:discard)owner.erase(id);std::set<uint64_t> kept;for(const auto &o:owner.objects())for(const auto &g:owner.group_path(o.group))kept.insert(g.id);std::vector<uint64_t> groups;for(const auto &g:owner.groups())if(!kept.contains(g.id)&&(!g.parent||kept.contains(g.parent)))groups.push_back(g.id);for(auto id:groups)owner.erase_group(id,false);return owner;
}
void detail::attach_native_emission(GltfDocument &document,const NativeEmissionExport &options,std::span<const uint64_t> exported,std::span<const EmissionExportImage> images,size_t max_bytes,const std::function<bool()> &cancelled){
 stop(cancelled);if(!options.scene||!options.allow_measured_core_fallback)throw std::invalid_argument("Exact HDR export requires its immutable native owner and explicit measured fallback permission");if(!max_bytes||max_bytes>256*1024*1024||images.size()>256)throw std::length_error("Native HDR package exceeds output limits");
 auto &j=document.json;if(j.contains("extras")&&!j["extras"].is_object())throw std::invalid_argument("Native HDR export cannot replace nonobject extras");if(j.contains("extras")&&j["extras"].contains(marker))throw std::invalid_argument("Native HDR export refuses an existing reserved package");
 std::set<uint64_t> selected;for(auto id:exported){options.scene->object(id);if(!selected.insert(id).second)throw std::invalid_argument("Native HDR export has duplicate source objects");}if(selected.empty())throw std::invalid_argument("Native HDR export has no source objects");
 if(options.scope!=NativeEmissionExport::Scope::WholeScene&&options.scope!=NativeEmissionExport::Scope::ExportedObjects)throw std::invalid_argument("Unknown native HDR export scope");
 if(options.scope==NativeEmissionExport::Scope::WholeScene)for(const auto &o:options.scene->objects())if(o.visible&&!selected.contains(o.id)){if(o.polygon&&o.polygon->active&&o.polygon->topology.faces().empty())continue;const bool nonempty=o.mesh_active?o.painted&&!o.painted->mesh().indices.empty():!surface_mesh(*o.volume,cancelled).indices.empty();if(nonempty)throw std::invalid_argument("Whole-scene HDR export omitted a visible source; use explicit subset scope");}
 Scene owner=options.scope==NativeEmissionExport::Scope::ExportedObjects?emission_export_subset(*options.scene,exported):options.scene->snapshot();
 owner.validate();auto native=encoded_scene(owner,cancelled);cbor_preflight(native,cancelled);const size_t decoded_bytes=native.size();const bool compressed=decoded_bytes>128*1024*1024;if(compressed)native=native_deflate(native,true,0,cancelled);
 auto append=[&](std::span<const uint8_t> bytes){stop(cancelled);auto &data=document.binary;size_t padding=(4-data.size()%4)%4;if(data.size()>max_bytes||padding>max_bytes-data.size()||bytes.size()>max_bytes-data.size()-padding)throw std::length_error("Native HDR package exceeds output byte budget");data.insert(data.end(),padding,0);const auto offset=data.size();data.insert(data.end(),bytes.begin(),bytes.end());if(j["bufferViews"].size()>=65536)throw std::length_error("Native HDR bufferView count exceeds read/write limit");int index=int(j["bufferViews"].size());j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",bytes.size()}});return Json{{"bufferView",index},{"sha256",hash(bytes)}};};
 Json package={{"version",compressed?2:1},{"encoding",compressed?"scene-cbor-deflate+openexr-float32-zip":"scene-cbor+openexr-float32-zip"},{"native",append(native)},{"images",Json::array()}};if(compressed)package["native"]["decodedBytes"]=decoded_bytes;std::set<uint64_t> seen;size_t exr_bytes=0;
 for(const auto &entry:images){stop(cancelled);if(!selected.contains(entry.object)||!seen.insert(entry.object).second||entry.node<0||entry.material<0||size_t(entry.node)>=j.at("nodes").size()||size_t(entry.material)>=j.at("materials").size())throw std::invalid_argument("Native HDR image has no unique exported owner");if(exr_bytes>=128*1024*1024)throw std::length_error("Native HDR aggregate EXR budget exceeded");EmissionInterchangeLimits limits;limits.max_encoded_bytes=std::min(max_bytes,size_t(128*1024*1024)-exr_bytes);limits.cancelled=cancelled;const auto bytes=encode_emission_exr(entry.image,limits);exr_bytes+=bytes.size();auto record=append(bytes);record["object"]=entry.object;record["node"]=entry.node;record["material"]=entry.material;record["fallback"]={{"strength",entry.fallback_strength},{"emissiveFactor",entry.fallback_factor},{"maximumAbsoluteError",entry.maximum_absolute_error},{"maximumRelativeError",entry.maximum_relative_error},{"erasedNonzeroSamples",entry.erased_nonzero_samples},{"evaluation","current-static-view"}};fallback(record.at("fallback"));package["images"].push_back(std::move(record));}
 while(document.binary.size()%4){if(document.binary.size()==max_bytes)throw std::length_error("Native HDR padding exceeds output limit");document.binary.push_back(0);}j["buffers"]={{{"byteLength",document.binary.size()}}};package["binarySha256"]=hash(document.binary);package["documentSha256"]="";j["extras"][marker]=std::move(package);j["extras"][marker]["documentSha256"]=document_hash(j);if(j.dump().size()>16*1024*1024)throw std::length_error("Native HDR document JSON exceeds16MiB read/write limit");stop(cancelled);
}
std::optional<Scene> extract_native_emission(std::span<const uint8_t> bytes,const NativeEmissionReadLimits &limits){
 stop(limits.cancelled);if(!limits.max_decoded_native_bytes||limits.max_decoded_native_bytes>256*1024*1024||!limits.max_container_bytes||limits.max_container_bytes>256*1024*1024||!limits.max_json_bytes||limits.max_json_bytes>16*1024*1024||!limits.max_native_bytes||limits.max_native_bytes>128*1024*1024||!limits.max_exr_bytes||limits.max_exr_bytes>128*1024*1024||!limits.max_images||limits.max_images>256||bytes.size()>limits.max_container_bytes)throw std::length_error("Invalid native HDR container limits");
 if(bytes.size()<20||word(bytes,0)!=0x46546c67||word(bytes,4)!=2||word(bytes,8)!=bytes.size()||word(bytes,16)!=0x4e4f534a)throw std::invalid_argument("Invalid native HDR GLB header");const auto length=word(bytes,12);if(length%4||length>limits.max_json_bytes||length>bytes.size()-20)throw std::length_error("Native HDR GLB JSON exceeds bounds");
 Json j=Json::parse(bytes.begin()+20,bytes.begin()+20+length,[&](int depth,Json::parse_event_t,Json&){stop(limits.cancelled);if(depth>128)throw std::length_error("Native HDR JSON depth exceeded");return true;});if(!j.is_object())throw std::invalid_argument("Invalid native HDR GLB root");if(!j.contains("extras"))return std::nullopt;if(!j.at("extras").is_object()){return std::nullopt;}if(!j.at("extras").contains(marker))return std::nullopt;const auto &package=j.at("extras").at(marker);const bool compressed=marker_object(package);
 const auto position=20+size_t(length);if(position>bytes.size()||bytes.size()-position<8||word(bytes,position+4)!=0x004e4942)throw std::invalid_argument("Native HDR package has no captured BIN");const auto binary_length=word(bytes,position);if(binary_length%4||binary_length!=bytes.size()-position-8)throw std::invalid_argument("Native HDR BIN framing mismatch");auto binary=bytes.subspan(position+8,binary_length);if(j.at("buffers").size()!=1||integer(j.at("buffers")[0].at("byteLength"),binary.size(),"Invalid native HDR buffer length")!=binary.size())throw std::invalid_argument("Native HDR buffer declaration mismatch");
 if(package.at("binarySha256").get_ref<const std::string&>()!=hash(binary)||package.at("documentSha256").get_ref<const std::string&>()!=document_hash(j))throw std::invalid_argument("Native HDR core/resource binding digest mismatch");
 const auto &native_record=package.at("native");if(native_record.size()!=(compressed?3:2))throw std::invalid_argument("Invalid native HDR Scene descriptor");const size_t decoded_bytes=compressed?integer(native_record.at("decodedBytes"),limits.max_decoded_native_bytes,"Native HDR decoded byte limit exceeded"):0;auto native=view(j,binary,native_record);if(!compressed&&native.size()>limits.max_decoded_native_bytes)throw std::length_error("Native HDR decoded byte limit exceeded");if(native.size()>limits.max_native_bytes)throw std::length_error("Native HDR Scene payload exceeds read budget");const auto &images=package.at("images");if(images.size()>limits.max_images)throw std::length_error("Native HDR image count exceeds limits");size_t encoded=0;std::set<uint64_t> owners;
 // Validate all small descriptors and byte digests before EXR/Scene allocation.
 for(const auto &entry:images){stop(limits.cancelled);if(entry.size()!=6||integer(entry.at("object"),UINT64_MAX,"Invalid native HDR object identity")==0||!owners.insert(integer(entry.at("object"),UINT64_MAX,"Invalid native HDR object identity")).second||integer(entry.at("node"),j.at("nodes").size(),"Invalid native HDR node")>=j.at("nodes").size()||integer(entry.at("material"),j.at("materials").size(),"Invalid native HDR material")>=j.at("materials").size())throw std::invalid_argument("Invalid native HDR image owner");fallback(entry.at("fallback"));auto resource=view(j,binary,entry);if(encoded>limits.max_exr_bytes||resource.size()>limits.max_exr_bytes-encoded)throw std::length_error("Native HDR EXR bytes exceed aggregate limit");encoded+=resource.size();}
 for(const auto &entry:images){auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;decode_emission_exr(view(j,binary,entry),image_limits);}
 stop(limits.cancelled);std::vector<uint8_t> expanded;if(compressed){expanded=native_deflate(native,false,decoded_bytes,limits.cancelled);native=expanded;}cbor_preflight(native,limits.cancelled);const auto value=Json::from_cbor(native.begin(),native.end(),true,true);auto result=Scene::decode(value);for(auto id:owners)result.object(id);stop(limits.cancelled);return result;
}
}
