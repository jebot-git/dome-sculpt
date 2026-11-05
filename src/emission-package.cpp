// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-package.hpp"
#include "emission-export.hpp"
#include "native-paint-sampling.hpp"
#include "native-paint-frame-export-policy.hpp"
#include "canonical-paint-export-policy.hpp"
#include "normal-frame-export.hpp"
#include "emission-paint-access.hpp"
#include <bit>
#include "emission-exr-access.hpp"
#include "native-scene-payload.hpp"
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
 const auto version=integer(j.at("version"),4,"Unknown native HDR package version");
 const auto &encoding=j.at("encoding").get_ref<const std::string&>();
 if((version==1&&encoding=="scene-cbor+openexr-float32-zip")||(version==2&&encoding=="scene-cbor-deflate+openexr-float32-zip"))return version==2;
 if(version==3){if(encoding=="scene-cbor+openexr-float32-zip+paint-domain-v1")return false;if(encoding=="scene-cbor-deflate+openexr-float32-zip+paint-domain-v1")return true;}
 if(version==4){if(encoding=="scene-cbor+openexr-float32-zip+paint-domain-v2")return false;if(encoding=="scene-cbor-deflate+openexr-float32-zip+paint-domain-v2")return true;}
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
// Package3 names the actual emitted paint coordinates. Authenticate every
// ordered emitted corner against the decoded owning native32 domain, independently
// from EXR bytes and document digests. This accepts our bounded packed writer
// representation, never arbitrary attribute/source identities.
struct DomainAccessor {
 std::span<const uint8_t> bytes;size_t count=0,components=0;
 float scalar(size_t vertex,size_t component)const{return std::bit_cast<float>(word(bytes,(vertex*components+component)*4));}
 uint32_t index(size_t at)const{return word(bytes,at*4);}
};
DomainAccessor domain_accessor(const Json &j,std::span<const uint8_t> binary,const Json &id,
    const char *type,size_t components,size_t cap,unsigned component){
 const auto &table=j.at("accessors");if(!table.is_array()||table.size()>65536)throw std::length_error("Native HDR accessor table exceeds policy");
 const auto n=integer(id,table.size(),"Native HDR coordinate accessor is invalid");if(n>=table.size())throw std::invalid_argument("Native HDR coordinate accessor is invalid");const auto &a=table[n];
 if(a.contains("sparse")||a.value("normalized",false)||a.at("type")!=type||integer(a.at("componentType"),5126,"Native HDR coordinate component is invalid")!=component)throw std::invalid_argument("Native HDR coordinate accessor interpretation differs");
 const auto count=integer(a.at("count"),cap,"Native HDR coordinate count exceeds policy");const auto &views=j.at("bufferViews");const auto v=integer(a.at("bufferView"),views.size(),"Native HDR coordinate view is invalid");if(v>=views.size()||views.size()>65536)throw std::invalid_argument("Native HDR coordinate view is invalid");const auto &view=views[v];
 if(integer(view.at("buffer"),0,"Native HDR coordinate buffer differs")||view.contains("byteStride"))throw std::invalid_argument("Native HDR coordinate view is not the packed owning representation");
 const auto begin=integer(view.value("byteOffset",Json(0)),binary.size(),"Native HDR coordinate view offset invalid"),length=integer(view.at("byteLength"),binary.size(),"Native HDR coordinate view length invalid");
 if(length>binary.size()-begin)throw std::invalid_argument("Native HDR coordinate view exceeds BIN");const auto offset=integer(a.value("byteOffset",Json(0)),length,"Native HDR coordinate accessor offset invalid");const auto needed=count*components*4;
 if((begin+offset)%4||needed>length-offset)throw std::invalid_argument("Native HDR coordinate accessor exceeds view");return {binary.subspan(begin+offset,needed),count,components};
}
size_t domain_descriptor(const Json &domain,bool canonical_allowed=false){
 if(!domain.is_object()||domain.size()!=3||integer(domain.at("version"),1,"Unknown native HDR domain version")!=1||
    (domain.at("kind")!="native-paint32"&&(!canonical_allowed||domain.at("kind")!="canonical-paint37")))
  throw std::invalid_argument("Unknown native HDR coordinate domain");
 return integer(domain.at("texCoord"),7,"Native HDR coordinate index exceeds policy");
}
void native_domain(const Json &j,std::span<const uint8_t> binary,const Json &entry,const SculptObject &object,
    const std::function<bool()> &cancelled,bool canonical_allowed=false){
 const auto coordinate=domain_descriptor(entry.at("domain"),canonical_allowed);
 const bool canonical=entry.at("domain").at("kind")=="canonical-paint37";
 const auto binding=object.material_bindings;
 const auto owner=binding?binding->native_paint_sampling:nullptr;
 const auto retained=binding?binding->canonical_paint_sampling:nullptr;
 if(!object.mesh_active||!object.painted||!binding||
    (canonical?(!retained||owner||!retained->valid_owner(retained)||!retained->matches_current(*object.painted)):
               (!owner||retained||!owner->valid_owner(owner))))
  throw std::invalid_argument("Native HDR coordinate record lacks its actual independent writable owner");
 PaintSamplingLimits limits;limits.cancelled=cancelled;
 if(canonical){retained->validate_binding(*binding,true);retained->validate_current(*object.painted,limits);}
 else owner->validate_current(*object.painted,limits);
 const auto attachment=canonical?retained->attachment():owner->attachment();
 const auto &mesh=object.painted->mesh();auto corners=attachment->domain()->corner_coordinates(mesh,limits);
 const auto node=integer(entry.at("node"),j.at("nodes").size(),"Native HDR coordinate node invalid");const auto material=integer(entry.at("material"),j.at("materials").size(),"Native HDR coordinate material invalid");if(node>=j.at("nodes").size()||material>=j.at("materials").size())throw std::invalid_argument("Native HDR coordinate binding invalid");
 const auto m=integer(j.at("nodes")[node].at("mesh"),j.at("meshes").size(),"Native HDR coordinate mesh invalid");if(m>=j.at("meshes").size())throw std::invalid_argument("Native HDR coordinate mesh invalid");const auto &primitives=j.at("meshes")[m].at("primitives");if(primitives.size()!=1)throw std::invalid_argument("Native HDR coordinate primitive differs");const auto &primitive=primitives[0];
 std::optional<detail::NormalFrameExport> frame;
 if(canonical||object.material_bindings->native_paint_frames){
  // Package3 still describes the genuine writable paint domain. A current
  // key adds its own independently sealed shading/REST map; never compare its
  // emitted normals to the older paint mesh or admit arbitrary target arrays.
  if(canonical)detail::canonical_paint_export_working_bytes(object,512*1024*1024,cancelled);
  else{object.material_bindings->native_paint_frames->validate_binding(*object.material_bindings,true,cancelled);
       detail::native_paint_frame_export_working_bytes(object,512*1024*1024,cancelled);}
  ObjectSurface source;source.id=object.id;source.name=object.name;source.transform=object.transform;
  source.mesh=mesh;source.material=object.material;source.material.emissive={};
  source.textures=detail::EmissionExportAccess::other_channels(*object.painted,source.material,cancelled);
  source.deformation=object.deformation;source.material_bindings=object.material_bindings;
  source.rig_source=object.rig_source;source.affine_edit=object.affine_edit;
  source.paint_sampling_attachment=attachment;source.paint_corner_coordinates=corners;
  frame=detail::normal_frame_export(source,detail::SourceMorphSpace::Native,cancelled,true);
 }
 if(primitive.at("material")!=material||primitive.value("mode",4)!=4||(!frame&&primitive.contains("targets")))throw std::invalid_argument("Native HDR coordinate primitive ownership differs");const auto &attributes=primitive.at("attributes");
 const auto &info=j.at("materials")[material].at("emissiveTexture");if(info.value("texCoord",0u)!=coordinate||info.contains("extensions"))throw std::invalid_argument("Native HDR emission binds a different coordinate domain");
 const auto indices=domain_accessor(j,binary,primitive.at("indices"),"SCALAR",1,6'000'000,5125);
 const auto positions=domain_accessor(j,binary,attributes.at("POSITION"),"VEC3",3,2'000'000,5126);
 const auto normals=domain_accessor(j,binary,attributes.at("NORMAL"),"VEC3",3,2'000'000,5126);
 const auto authored=domain_accessor(j,binary,attributes.at("TEXCOORD_0"),"VEC2",2,2'000'000,5126);
 const auto paint=domain_accessor(j,binary,attributes.at("TEXCOORD_"+std::to_string(coordinate)),"VEC2",2,2'000'000,5126);
 const auto &expected=frame?frame->surface.mesh:mesh;
 if(indices.count!=expected.indices.size()||positions.count!=normals.count||positions.count!=authored.count||positions.count!=paint.count||(frame&&positions.count!=expected.positions.size()))throw std::invalid_argument("Native HDR emitted corner dimensions differ");
 for(size_t c=0;c<indices.count;++c){if((c&1023)==0)stop(cancelled);const auto v=indices.index(c),source=expected.indices[c];if(v>=positions.count)throw std::invalid_argument("Native HDR emitted index exceeds coordinates");const auto p=expected.positions[source],n=expected.normals[source];
  if(frame&&v!=source)throw std::invalid_argument("Native HDR key index layout differs from its exact target correspondence");
  if(positions.scalar(v,0)!=p.x||positions.scalar(v,1)!=p.y||positions.scalar(v,2)!=p.z||normals.scalar(v,0)!=n.x||normals.scalar(v,1)!=n.y||normals.scalar(v,2)!=n.z||authored.scalar(v,0)!=expected.uv[source][0]||authored.scalar(v,1)!=expected.uv[source][1]||paint.scalar(v,0)!=corners[c][0]||paint.scalar(v,1)!=corners[c][1])throw std::invalid_argument("Native HDR emitted corners differ from their exact owning domain");
 }
 if(canonical){
  if(!frame||frame->normal_coordinates.size()!=positions.count||!frame->prepared_normal.image)
   throw std::invalid_argument("Canonical HDR lost its independent resolved normal domain");
  const auto &normal_info=j.at("materials")[material].at("normalTexture");
  if(normal_info.contains("extensions")||normal_info.value("scale",1.f)!=1.f)
   throw std::invalid_argument("Canonical HDR changed its actual normal sampling interpretation");
  const auto normal_coordinate=integer(normal_info.value("texCoord",Json(0)),7,"Canonical HDR normal coordinate exceeds policy");
  const auto normal_uv=domain_accessor(j,binary,attributes.at("TEXCOORD_"+std::to_string(normal_coordinate)),"VEC2",2,2'000'000,5126);
  if(normal_uv.count!=positions.count)throw std::invalid_argument("Canonical HDR normal coordinate dimensions differ");
  for(size_t v=0;v<normal_uv.count;++v){if((v&1023)==0)stop(cancelled);if(normal_uv.scalar(v,0)!=frame->normal_coordinates[v][0]||normal_uv.scalar(v,1)!=frame->normal_coordinates[v][1])throw std::invalid_argument("Canonical HDR borrowed another normal coordinate domain");}
  const auto &textures=j.at("textures"),&images=j.at("images"),&samplers=j.at("samplers");
  const auto ti=integer(normal_info.at("index"),textures.size(),"Canonical HDR normal texture invalid");if(ti>=textures.size())throw std::invalid_argument("Canonical HDR normal texture invalid");
  const auto &texture=textures[ti];const auto si=integer(texture.at("source"),images.size(),"Canonical HDR normal image invalid"),ss=integer(texture.at("sampler"),samplers.size(),"Canonical HDR normal sampler invalid");
  if(si>=images.size()||ss>=samplers.size())throw std::invalid_argument("Canonical HDR normal resource invalid");
  const auto &sampler=samplers[ss];if(sampler.value("wrapS",10497)!=33071||sampler.value("wrapT",10497)!=33071||sampler.value("minFilter",9729)!=9729||sampler.value("magFilter",9729)!=9729)throw std::invalid_argument("Canonical HDR normal sampler differs");
  const auto &image=images[si];if(image.contains("uri")||image.at("mimeType")!="image/png")throw std::invalid_argument("Canonical HDR normal image interpretation differs");
  const auto &views=j.at("bufferViews");const auto vi=integer(image.at("bufferView"),views.size(),"Canonical HDR normal image view invalid");if(vi>=views.size())throw std::invalid_argument("Canonical HDR normal image view invalid");
  const auto &iv=views[vi];if(integer(iv.at("buffer"),0,"Canonical HDR normal image buffer differs")||iv.contains("byteStride")||iv.contains("target"))throw std::invalid_argument("Canonical HDR normal image view differs");
  const auto begin=integer(iv.value("byteOffset",Json(0)),binary.size(),"Canonical HDR normal image offset invalid"),length=integer(iv.at("byteLength"),binary.size(),"Canonical HDR normal image length invalid");
  if(length>binary.size()-begin)throw std::invalid_argument("Canonical HDR normal image exceeds BIN");
  const auto expected_png=frame->prepared_normal.image->png();const auto actual_png=binary.subspan(begin,length);
  if(expected_png.size()!=actual_png.size()||!std::equal(expected_png.begin(),expected_png.end(),actual_png.begin()))throw std::invalid_argument("Canonical HDR normal image differs from its independent owning field");
 }
 if(frame){
  auto directions=[&](const Json &channels,const char *name,const std::vector<Vec3> &values){
   if(values.empty()){if(channels.contains(name))throw std::invalid_argument("Native HDR key invented a direction channel");return;}
   const auto view=domain_accessor(j,binary,channels.at(name),"VEC3",3,2'000'000,5126);
   if(view.count!=values.size())throw std::invalid_argument("Native HDR key direction dimensions differ");
   for(size_t v=0;v<values.size();++v){if((v&1023)==0)stop(cancelled);const auto &p=values[v];if(view.scalar(v,0)!=p.x||view.scalar(v,1)!=p.y||view.scalar(v,2)!=p.z)throw std::invalid_argument("Native HDR key attributes differ from the actual current frame");}
  };
  if(frame->tangents.empty()){if(attributes.contains("TANGENT"))throw std::invalid_argument("Native HDR key invented its base tangent frame");}
  else{const auto view=domain_accessor(j,binary,attributes.at("TANGENT"),"VEC4",4,2'000'000,5126);if(view.count!=frame->tangents.size())throw std::invalid_argument("Native HDR key tangent dimensions differ");for(size_t v=0;v<view.count;++v){if((v&1023)==0)stop(cancelled);for(size_t k=0;k<4;++k)if(view.scalar(v,k)!=frame->tangents[v][k])throw std::invalid_argument("Native HDR key base tangents differ");}}
  const auto deformation=frame->surface.deformation;const size_t count=deformation&&deformation->morphs?deformation->morphs->targets.size():0;
  if(primitive.value("targets",Json::array()).size()!=count||frame->normal_targets.size()!=count||frame->tangent_targets.size()!=count||j.at("nodes")[node].contains("weights"))throw std::invalid_argument("Native HDR key target identities differ");
  if(count){const auto &weights=j.at("meshes")[m].at("weights"),&names=j.at("meshes")[m].at("extras").at("targetNames");if(!weights.is_array()||!names.is_array()||weights.size()!=count||names.size()!=count)throw std::invalid_argument("Native HDR key names/weights dimensions differ");
   for(size_t t=0;t<count;++t){stop(cancelled);const auto &target=deformation->morphs->targets[t];const auto &channels=primitive.at("targets").at(t);if(!channels.is_object()||channels.size()!=1+size_t(!frame->normal_targets[t].empty())+size_t(!frame->tangent_targets[t].empty())||names[t]!=target.name||weights[t].get<float>()!=(deformation->morph_weights.empty()?0:deformation->morph_weights.at(t)))throw std::invalid_argument("Native HDR key target order or current weights differ");directions(channels,"POSITION",target.delta);directions(channels,"NORMAL",frame->normal_targets[t]);directions(channels,"TANGENT",frame->tangent_targets[t]);}
  }
 }
}
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
detail::NativeScenePayload detail::capture_native_scene_payload(const Scene &scene,const std::function<bool()> &cancelled){
 scene.validate();NativeScenePayload result;result.stored=encoded_scene(scene,cancelled);cbor_preflight(result.stored,cancelled);result.decoded_bytes=result.stored.size();result.compressed=result.decoded_bytes>128*1024*1024;if(result.compressed)result.stored=native_deflate(result.stored,true,0,cancelled);return result;
}
Scene detail::restore_native_scene_payload(std::span<const uint8_t> bytes,bool compressed,size_t decoded_bytes,size_t max_decoded_bytes,const std::function<bool()> &cancelled){
 if(bytes.empty()||bytes.size()>128*1024*1024||!max_decoded_bytes||max_decoded_bytes>256*1024*1024||!decoded_bytes||decoded_bytes>max_decoded_bytes||(!compressed&&bytes.size()!=decoded_bytes))throw std::length_error("Native authoring stored/expanded byte limit exceeded");
 std::vector<uint8_t> expanded;if(compressed){expanded=native_deflate(bytes,false,decoded_bytes,cancelled);bytes=expanded;}cbor_preflight(bytes,cancelled);const auto value=Json::from_cbor(bytes.begin(),bytes.end(),true,true);auto result=Scene::decode(value);stop(cancelled);return result;
}
Scene detail::emission_export_subset(const Scene &source,std::span<const uint64_t> exported){
 std::set<uint64_t> selected;for(auto id:exported){source.object(id);if(!selected.insert(id).second)throw std::invalid_argument("Duplicate native export subset object");}if(selected.empty())throw std::invalid_argument("No native export subset objects");auto owner=source.snapshot();std::vector<uint64_t> discard;for(const auto &o:owner.objects())if(!selected.contains(o.id))discard.push_back(o.id);for(auto id:discard)owner.erase(id);std::set<uint64_t> kept;for(const auto &o:owner.objects())for(const auto &g:owner.group_path(o.group))kept.insert(g.id);std::vector<uint64_t> groups;for(const auto &g:owner.groups())if(!kept.contains(g.id)&&(!g.parent||kept.contains(g.parent)))groups.push_back(g.id);for(auto id:groups)owner.erase_group(id,false);return owner;
}
void detail::attach_native_emission(GltfDocument &document,const NativeEmissionExport &options,std::span<const uint64_t> exported,std::span<const EmissionExportImage> images,size_t max_bytes,const std::function<bool()> &cancelled){
 stop(cancelled);if(!options.scene||!options.allow_measured_core_fallback)throw std::invalid_argument("Exact HDR export requires its immutable native owner and explicit measured fallback permission");if(!max_bytes||max_bytes>256*1024*1024||images.size()>256)throw std::length_error("Native HDR package exceeds output limits");
 size_t decoded_fields=0;for(const auto &entry:images){stop(cancelled);if(entry.image.pixels.size()>(128*1024*1024-decoded_fields)/sizeof(std::array<float,10>))throw std::length_error("Native HDR aggregate decoded EXR fields exceed128MiB");decoded_fields+=entry.image.pixels.size()*sizeof(std::array<float,10>);}
 auto &j=document.json;if(j.contains("extras")&&!j["extras"].is_object())throw std::invalid_argument("Native HDR export cannot replace nonobject extras");if(j.contains("extras")&&(j["extras"].contains(marker)||j["extras"].contains("domeSculptAuthoring")))throw std::invalid_argument("Native HDR export refuses an existing reserved package");
 std::set<uint64_t> selected;for(auto id:exported){options.scene->object(id);if(!selected.insert(id).second)throw std::invalid_argument("Native HDR export has duplicate source objects");}if(selected.empty())throw std::invalid_argument("Native HDR export has no source objects");
 if(options.scope!=NativeEmissionExport::Scope::WholeScene&&options.scope!=NativeEmissionExport::Scope::ExportedObjects)throw std::invalid_argument("Unknown native HDR export scope");
 if(options.scope==NativeEmissionExport::Scope::WholeScene)for(const auto &o:options.scene->objects())if(o.visible&&!selected.contains(o.id)){if(o.polygon&&o.polygon->active&&o.polygon->topology.faces().empty())continue;const bool nonempty=o.mesh_active?o.painted&&!o.painted->mesh().indices.empty():!surface_mesh(*o.volume,cancelled).indices.empty();if(nonempty)throw std::invalid_argument("Whole-scene HDR export omitted a visible source; use explicit subset scope");}
 Scene owner=options.scope==NativeEmissionExport::Scope::ExportedObjects?emission_export_subset(*options.scene,exported):options.scene->snapshot();
 owner.validate();auto native=encoded_scene(owner,cancelled);cbor_preflight(native,cancelled);const size_t decoded_bytes=native.size();const bool compressed=decoded_bytes>128*1024*1024;if(compressed)native=native_deflate(native,true,0,cancelled);
 auto append=[&](std::span<const uint8_t> bytes){stop(cancelled);auto &data=document.binary;size_t padding=(4-data.size()%4)%4;if(data.size()>max_bytes||padding>max_bytes-data.size()||bytes.size()>max_bytes-data.size()-padding)throw std::length_error("Native HDR package exceeds output byte budget");data.insert(data.end(),padding,0);const auto offset=data.size();data.insert(data.end(),bytes.begin(),bytes.end());if(j["bufferViews"].size()>=65536)throw std::length_error("Native HDR bufferView count exceeds read/write limit");int index=int(j["bufferViews"].size());j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",bytes.size()}});return Json{{"bufferView",index},{"sha256",hash(bytes)}};};
 const bool canonical_domains=std::any_of(images.begin(),images.end(),[](const auto &entry){return bool(entry.canonical_paint_domain);});
 const bool domains=canonical_domains||std::any_of(images.begin(),images.end(),[](const auto &entry){return entry.native_paint_domain;});
 Json package={{"version",canonical_domains?4:domains?3:compressed?2:1},{"encoding",compressed?"scene-cbor-deflate+openexr-float32-zip":"scene-cbor+openexr-float32-zip"},{"native",append(native)},{"images",Json::array()}};if(domains)package["encoding"]=canonical_domains?(compressed?"scene-cbor-deflate+openexr-float32-zip+paint-domain-v2":"scene-cbor+openexr-float32-zip+paint-domain-v2"):(compressed?"scene-cbor-deflate+openexr-float32-zip+paint-domain-v1":"scene-cbor+openexr-float32-zip+paint-domain-v1");if(compressed)package["native"]["decodedBytes"]=decoded_bytes;std::set<uint64_t> seen;size_t exr_bytes=0;
 for(const auto &entry:images){stop(cancelled);if(!selected.contains(entry.object)||!seen.insert(entry.object).second||entry.node<0||entry.material<0||size_t(entry.node)>=j.at("nodes").size()||size_t(entry.material)>=j.at("materials").size())throw std::invalid_argument("Native HDR image has no unique exported owner");if(exr_bytes>=128*1024*1024)throw std::length_error("Native HDR aggregate EXR budget exceeded");EmissionInterchangeLimits limits;limits.max_encoded_bytes=std::min(max_bytes,size_t(128*1024*1024)-exr_bytes);limits.cancelled=cancelled;const auto &current_object=owner.object(entry.object);const auto current_canonical=current_object.material_bindings?current_object.material_bindings->canonical_paint_sampling:nullptr;if(current_canonical&&(!entry.canonical_paint_domain||current_canonical!=entry.canonical_paint_domain||current_canonical.owner_before(entry.canonical_paint_domain)||entry.canonical_paint_domain.owner_before(current_canonical)))throw std::invalid_argument("Canonical HDR image omitted or borrowed its actual writable owner");if(entry.native_paint_domain&&entry.canonical_paint_domain)throw std::invalid_argument("Native HDR image claims competing writable owners");if(entry.native_paint_domain||entry.canonical_paint_domain){const auto &actual=owner.object(entry.object);if(entry.canonical_paint_domain){const auto expected=actual.material_bindings?actual.material_bindings->canonical_paint_sampling:nullptr;if(expected!=entry.canonical_paint_domain||expected.owner_before(entry.canonical_paint_domain)||entry.canonical_paint_domain.owner_before(expected))throw std::invalid_argument("Canonical HDR borrowed a foreign writable owner");}auto expected=entry.canonical_paint_domain?detail::canonical_paint_emission_field(actual,cancelled):detail::native_paint_emission_field(actual,cancelled);if(expected.width!=entry.image.width||expected.height!=entry.image.height||expected.pixels!=entry.image.pixels)throw std::invalid_argument("Native HDR field changed after its owning preparation");}
 const auto bytes=encode_emission_exr(entry.image,limits);exr_bytes+=bytes.size();auto record=append(bytes);record["object"]=entry.object;record["node"]=entry.node;record["material"]=entry.material;record["fallback"]={{"strength",entry.fallback_strength},{"emissiveFactor",entry.fallback_factor},{"maximumAbsoluteError",entry.maximum_absolute_error},{"maximumRelativeError",entry.maximum_relative_error},{"erasedNonzeroSamples",entry.erased_nonzero_samples},{"evaluation","current-static-view"}};fallback(record.at("fallback"));if(entry.native_paint_domain||entry.canonical_paint_domain){record["domain"]={{"version",1},{"kind",entry.canonical_paint_domain?"canonical-paint37":"native-paint32"},{"texCoord",entry.texcoord}};native_domain(j,document.binary,record,owner.object(entry.object),cancelled,canonical_domains);}package["images"].push_back(std::move(record));}
 while(document.binary.size()%4){if(document.binary.size()==max_bytes)throw std::length_error("Native HDR padding exceeds output limit");document.binary.push_back(0);}j["buffers"]={{{"byteLength",document.binary.size()}}};package["binarySha256"]=hash(document.binary);package["documentSha256"]="";j["extras"][marker]=std::move(package);j["extras"][marker]["documentSha256"]=document_hash(j);if(j.dump().size()>16*1024*1024)throw std::length_error("Native HDR document JSON exceeds16MiB read/write limit");stop(cancelled);
}
std::optional<Scene> extract_native_emission(std::span<const uint8_t> bytes,const NativeEmissionReadLimits &limits){
 stop(limits.cancelled);if(!limits.max_decoded_native_bytes||limits.max_decoded_native_bytes>256*1024*1024||!limits.max_container_bytes||limits.max_container_bytes>256*1024*1024||!limits.max_json_bytes||limits.max_json_bytes>16*1024*1024||!limits.max_native_bytes||limits.max_native_bytes>128*1024*1024||!limits.max_exr_bytes||limits.max_exr_bytes>128*1024*1024||!limits.max_images||limits.max_images>256||bytes.size()>limits.max_container_bytes)throw std::length_error("Invalid native HDR container limits");
 if(bytes.size()<20||word(bytes,0)!=0x46546c67||word(bytes,4)!=2||word(bytes,8)!=bytes.size()||word(bytes,16)!=0x4e4f534a)throw std::invalid_argument("Invalid native HDR GLB header");const auto length=word(bytes,12);if(length%4||length>limits.max_json_bytes||length>bytes.size()-20)throw std::length_error("Native HDR GLB JSON exceeds bounds");
 Json j=Json::parse(bytes.begin()+20,bytes.begin()+20+length,[&](int depth,Json::parse_event_t,Json&){stop(limits.cancelled);if(depth>128)throw std::length_error("Native HDR JSON depth exceeded");return true;});if(!j.is_object())throw std::invalid_argument("Invalid native HDR GLB root");if(!j.contains("extras"))return std::nullopt;if(!j.at("extras").is_object()){return std::nullopt;}if(!j.at("extras").contains(marker))return std::nullopt;if(j.at("extras").contains("domeSculptAuthoring"))throw std::invalid_argument("Competing native package authority");const auto &package=j.at("extras").at(marker);const bool compressed=marker_object(package);const bool canonical_domains=package.at("version")==4;const bool domains=canonical_domains||package.at("version")==3;
 const auto position=20+size_t(length);if(position>bytes.size()||bytes.size()-position<8||word(bytes,position+4)!=0x004e4942)throw std::invalid_argument("Native HDR package has no captured BIN");const auto binary_length=word(bytes,position);if(binary_length%4||binary_length!=bytes.size()-position-8)throw std::invalid_argument("Native HDR BIN framing mismatch");auto binary=bytes.subspan(position+8,binary_length);if(j.at("buffers").size()!=1||integer(j.at("buffers")[0].at("byteLength"),binary.size(),"Invalid native HDR buffer length")!=binary.size())throw std::invalid_argument("Native HDR buffer declaration mismatch");
 if(package.at("binarySha256").get_ref<const std::string&>()!=hash(binary)||package.at("documentSha256").get_ref<const std::string&>()!=document_hash(j))throw std::invalid_argument("Native HDR core/resource binding digest mismatch");
 const auto &native_record=package.at("native");if(native_record.size()!=(compressed?3:2))throw std::invalid_argument("Invalid native HDR Scene descriptor");const size_t decoded_bytes=compressed?integer(native_record.at("decodedBytes"),limits.max_decoded_native_bytes,"Native HDR decoded byte limit exceeded"):0;auto native=view(j,binary,native_record);if(!compressed&&native.size()>limits.max_decoded_native_bytes)throw std::length_error("Native HDR decoded byte limit exceeded");if(native.size()>limits.max_native_bytes)throw std::length_error("Native HDR Scene payload exceeds read budget");const auto &images=package.at("images");if(images.size()>limits.max_images)throw std::length_error("Native HDR image count exceeds limits");size_t encoded=0,decoded_fields=0,domain_count=0,canonical_count=0;std::set<uint64_t> owners;
 // Validate all small descriptors and byte digests before EXR/Scene allocation.
 for(const auto &entry:images){stop(limits.cancelled);if(entry.size()!=(domains&&entry.contains("domain")?7:6)||(!domains&&entry.contains("domain"))||integer(entry.at("object"),UINT64_MAX,"Invalid native HDR object identity")==0||!owners.insert(integer(entry.at("object"),UINT64_MAX,"Invalid native HDR object identity")).second||integer(entry.at("node"),j.at("nodes").size(),"Invalid native HDR node")>=j.at("nodes").size()||integer(entry.at("material"),j.at("materials").size(),"Invalid native HDR material")>=j.at("materials").size())throw std::invalid_argument("Invalid native HDR image owner");fallback(entry.at("fallback"));if(entry.contains("domain")){domain_descriptor(entry.at("domain"),canonical_domains);++domain_count;if(entry.at("domain").at("kind")=="canonical-paint37")++canonical_count;}auto resource=view(j,binary,entry);if(encoded>limits.max_exr_bytes||resource.size()>limits.max_exr_bytes-encoded)throw std::length_error("Native HDR EXR bytes exceed aggregate limit");encoded+=resource.size();auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;const size_t raw=detail::emission_exr_decoded_bytes(resource,image_limits);if(raw>128*1024*1024-decoded_fields)throw std::length_error("Native HDR aggregate decoded EXR fields exceed128MiB");decoded_fields+=raw;}
 if(canonical_domains&&!canonical_count)throw std::invalid_argument("Native HDR package4 has no genuine canonical writable-domain witness");
 if(domains&&!domain_count)throw std::invalid_argument("Native HDR package3 has no genuine coordinate-domain witness");
 for(const auto &entry:images){auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;decode_emission_exr(view(j,binary,entry),image_limits);}
 stop(limits.cancelled);std::vector<uint8_t> expanded;if(compressed){expanded=native_deflate(native,false,decoded_bytes,limits.cancelled);native=expanded;}cbor_preflight(native,limits.cancelled);const auto value=Json::from_cbor(native.begin(),native.end(),true,true);auto result=Scene::decode(value);for(auto id:owners)result.object(id);
 for(const auto &entry:images){const auto &object=result.object(entry.at("object").get<uint64_t>());if(object.material_bindings&&object.material_bindings->canonical_paint_sampling&&(!canonical_domains||!entry.contains("domain")||entry.at("domain").at("kind")!="canonical-paint37"))throw std::invalid_argument("Canonical HDR package omitted its actual writable-domain witness");}

 if(domains){size_t actual=0;for(const auto &entry:images)if(entry.contains("domain")){++actual;const auto &object=result.object(entry.at("object").get<uint64_t>());native_domain(j,binary,entry,object,limits.cancelled,canonical_domains);auto expected=entry.at("domain").at("kind")=="canonical-paint37"?detail::canonical_paint_emission_field(object,limits.cancelled):detail::native_paint_emission_field(object,limits.cancelled);auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;auto image=decode_emission_exr(view(j,binary,entry),image_limits);if(image.width!=expected.width||image.height!=expected.height||image.pixels.size()!=expected.pixels.size())throw std::invalid_argument("Native HDR field differs from its retained paint owner");for(size_t p=0;p<image.pixels.size();++p){if((p&1023)==0)stop(limits.cancelled);if(image.pixels[p]!=expected.pixels[p])throw std::invalid_argument("Native HDR float planes differ from their exact native paint replay");}}
 if(!actual)throw std::invalid_argument("Native HDR package3 has no genuine coordinate-domain witness");}
 stop(limits.cancelled);return result;
}
}
