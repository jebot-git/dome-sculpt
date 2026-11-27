// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-package.hpp"
#include "scene-bake-archive.hpp"
#include "emission-export.hpp"
#include "texture-set-export.hpp"
#include "texture-set-field-source.hpp"
#include "polygon-surface-preview.hpp"
#include "native-paint-sampling.hpp"
#include "native-paint-frame-export-policy.hpp"
#include "canonical-paint-export-policy.hpp"
#include "normal-frame-export.hpp"
#include "emission-paint-access.hpp"
#include <bit>
#include "emission-exr-access.hpp"
#include "emission-fallback-metrics.hpp"
#include "native-scene-payload.hpp"
#include "native-read-scope.hpp"
#include "humanoid-sha256.hpp"
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <cstring>
#include <limits>
#include <optional>
#include "dome/projection.hpp"
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
 const auto version=integer(j.at("version"),6,"Unknown native HDR package version");
 const auto &encoding=j.at("encoding").get_ref<const std::string&>();
 if((version==1&&encoding=="scene-cbor+openexr-float32-zip")||(version==2&&encoding=="scene-cbor-deflate+openexr-float32-zip"))return version==2;
 if(version==3){if(encoding=="scene-cbor+openexr-float32-zip+paint-domain-v1")return false;if(encoding=="scene-cbor-deflate+openexr-float32-zip+paint-domain-v1")return true;}
 if(version==4){if(encoding=="scene-cbor+openexr-float32-zip+paint-domain-v2")return false;if(encoding=="scene-cbor-deflate+openexr-float32-zip+paint-domain-v2")return true;}
 if(version==5){
  if(encoding=="scene-cbor+openexr-float32-zip+bounded-scanlines")return false;
  if(encoding=="scene-cbor-deflate+openexr-float32-zip+bounded-scanlines")return true;
 }
 if(version==6){
  if(encoding=="scene-cbor+openexr-float32-zip+texture-set-domains")return false;
  if(encoding=="scene-cbor-deflate+openexr-float32-zip+texture-set-domains")return true;
 }
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
bool fallback(const Json &f,std::optional<uint32_t> actual_exr={}){
 const bool resolved=f.is_object()&&f.contains("version");
 if(!f.is_object()||f.size()!=(resolved?9:6)||f.at("evaluation").get_ref<const std::string&>()!="current-static-view")
  throw std::invalid_argument("Unknown native HDR fallback interpretation");
 if(resolved&&(!f.at("version").is_number_integer()||f.at("version")!=2||f.at("units")!=detail::resolved_fallback_units||(f.at("view")!="pbr"&&f.at("view")!="authored-mtoon")))
  throw std::invalid_argument("Unknown resolved FLOAT fallback interpretation/units");
 if(actual_exr&&resolved!=(*actual_exr==2))throw std::invalid_argument("Fallback interpretation differs from actual FLOAT EXR authority");
 for(const auto *key:{"strength","maximumAbsoluteError","maximumRelativeError"}){
  if(!f.at(key).is_number())throw std::invalid_argument("Invalid native HDR fallback metric");const double v=f.at(key).get<double>();
  const bool relative=std::strcmp(key,"maximumRelativeError")==0;
  const double limit=resolved?(relative?detail::maximum_resolved_fallback_relative_error:double(std::numeric_limits<float>::max())):(relative?1e6:30000);
  if(!std::isfinite(v)||v<0||v>limit||(!std::strcmp(key,"strength")&&v<1))throw std::invalid_argument("Invalid native HDR fallback metric");
  if(resolved&&!std::strcmp(key,"strength")&&double(float(v))!=v)throw std::invalid_argument("Resolved fallback strength is not its actual FLOAT scale");
 }
 if(!f.at("emissiveFactor").is_array()||f.at("emissiveFactor").size()!=3)throw std::invalid_argument("Invalid native HDR fallback factor");
 for(const auto &value:f.at("emissiveFactor")){
  if(!value.is_number())throw std::invalid_argument("Invalid native HDR fallback factor");const double v=value.get<double>();
  if(!std::isfinite(v)||v<0||v>1||(resolved&&double(float(v))!=v))throw std::invalid_argument("Invalid native HDR fallback factor");
 }
 integer(f.at("erasedNonzeroSamples"),16*1024*1024*3,"Invalid native HDR fallback erased count");return resolved;
}

std::span<const uint8_t> view(const Json &j,std::span<const uint8_t> binary,const Json &record){
 if(!record.is_object()||!record.contains("bufferView")||!record.contains("sha256"))throw std::invalid_argument("Invalid native HDR resource descriptor");
 const auto &views=j.at("bufferViews");if(!views.is_array()||views.size()>65536)throw std::length_error("Native HDR bufferView table exceeds limits");const auto index=integer(record.at("bufferView"),views.size(),"Invalid native HDR bufferView");if(index>=views.size())throw std::invalid_argument("Invalid native HDR bufferView");const auto &v=views[index];
 if(!v.is_object()||v.contains("target")||v.contains("byteStride")||integer(v.at("buffer"),0,"Native HDR resource must use captured buffer zero")!=0)throw std::invalid_argument("Invalid native HDR byte view");
 const auto offset=v.contains("byteOffset")?integer(v.at("byteOffset"),binary.size(),"Invalid native HDR byte offset"):0;const auto length=integer(v.at("byteLength"),binary.size(),"Invalid native HDR byte length");if(!length||length>binary.size()-offset)throw std::invalid_argument("Native HDR byte view exceeds captured BIN");const auto result=binary.subspan(offset,length);if(record.at("sha256").get_ref<const std::string&>()!=hash(result))throw std::invalid_argument("Native HDR resource digest mismatch");return result;
}
uint32_t word(std::span<const uint8_t> bytes,size_t offset){if(offset>bytes.size()||bytes.size()-offset<4)throw std::invalid_argument("Truncated native HDR GLB");return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|(uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);}
std::span<const uint8_t> fallback2_png(const Json &j,std::span<const uint8_t> binary,
    const Json &entry,const detail::EmissionExrDescription &description,
    const EmissionInterchangeLimits &limits,size_t exr_bytes){
 const auto &f=entry.at("fallback"),&material=j.at("materials").at(entry.at("material").get<size_t>());
 if(material.at("emissiveFactor")!=f.at("emissiveFactor"))throw std::invalid_argument("Resolved fallback factor differs from actual glTF material");
 double strength=1;
 if(material.contains("extensions")&&material.at("extensions").contains("KHR_materials_emissive_strength"))strength=material.at("extensions").at("KHR_materials_emissive_strength").at("emissiveStrength").get<double>();
 if(strength!=f.at("strength").get<double>())throw std::invalid_argument("Resolved fallback strength differs from actual glTF material");
 const auto &info=material.at("emissiveTexture");
 if(info.contains("extensions"))throw std::invalid_argument("Resolved fallback2 requires its actual untransformed image view");
 const auto &textures=j.at("textures"),&images=j.at("images"),&views=j.at("bufferViews");
 const auto &texture=textures.at(integer(info.at("index"),textures.size(),"Invalid resolved fallback texture"));
 const auto &image=images.at(integer(texture.at("source"),images.size(),"Invalid resolved fallback image"));
 if(image.contains("uri")||image.at("mimeType")!="image/png")throw std::invalid_argument("Resolved fallback2 requires its captured RGB8 PNG");
 const auto &v=views.at(integer(image.at("bufferView"),views.size(),"Invalid resolved fallback view"));
 if(v.contains("target")||v.contains("byteStride")||integer(v.at("buffer"),0,"Resolved PNG must use captured buffer zero")!=0)throw std::invalid_argument("Invalid resolved PNG view");
 const size_t at=v.contains("byteOffset")?integer(v.at("byteOffset"),binary.size(),"Invalid resolved PNG offset"):0;
 const size_t length=integer(v.at("byteLength"),64*1024*1024,"Resolved PNG exceeds encoded image budget");
 if(length>binary.size()-at||length<33)throw std::invalid_argument("Resolved PNG exceeds captured BIN");
 const auto bytes=binary.subspan(at,length);
 constexpr std::array<uint8_t,8> magic{137,80,78,71,13,10,26,10};
 auto big=[&](size_t i){return uint32_t(bytes[i])<<24|uint32_t(bytes[i+1])<<16|uint32_t(bytes[i+2])<<8|bytes[i+3];};
 if(!std::equal(magic.begin(),magic.end(),bytes.begin())||big(8)!=13||big(12)!=0x49484452||big(16)!=description.width||big(20)!=description.height||bytes[24]!=8||bytes[25]!=6||bytes[26]||bytes[27]||bytes[28])
  throw std::invalid_argument("Resolved PNG differs from its declared noninterlaced RGBA8 FLOAT-field raster");
 // This version describes the writer's simple RGBA8 fallback, not arbitrary
 // imported PNG metadata. Exclude compressed profiles/text/interlace before
 // libpng allocation so the forecast needs only bounded rows/zlib tables.
 bool idat=false,end=false;unsigned ancillary=0;size_t chunks=0;
 for(size_t p=8;p<bytes.size();){
  if((++chunks&255)==0)stop(limits.cancelled);
  if(bytes.size()-p<12)throw std::invalid_argument("Truncated resolved PNG chunk");
  const size_t n=big(p);const auto type=big(p+4);
  if(n>bytes.size()-p-12)throw std::invalid_argument("Resolved PNG chunk exceeds capture");
  if(p==8){if(type!=0x49484452||n!=13)throw std::invalid_argument("Resolved PNG needs one initial IHDR");}
  else if(type==0x49444154){if(end)throw std::invalid_argument("Resolved PNG has data after end");idat=true;}
  else if(type==0x49454e44){if(n||!idat||p+12!=bytes.size())throw std::invalid_argument("Invalid resolved PNG end");end=true;}
  else{
   unsigned bit=0;size_t count=0;
   if(type==0x73524742){bit=1;count=1;}else if(type==0x67414d41){bit=2;count=4;}else if(type==0x6348524d){bit=4;count=32;}
   if(!bit||idat||(ancillary&bit)||n!=count)throw std::invalid_argument("Unsupported resolved PNG metadata/chunk");ancillary|=bit;
  }
  p+=n+12;
 }
 if(!end)throw std::invalid_argument("Resolved PNG has no complete end");
 const size_t rgba=size_t(description.width)*description.height*4;
 size_t working=0;
 for(size_t n:{description.decoded_bytes,rgba,exr_bytes,length,size_t(1024*1024)}){
  if(working>limits.max_working_bytes||n>limits.max_working_bytes-working)throw std::length_error("Resolved quality replay exceeds existing image working budget");working+=n;
 }
 return bytes;
}
void validate_fallback2_metrics(const Json &j,const Json &entry,const EmissionInterchangeImage &image,
    std::span<const uint8_t> png,const std::function<bool()> &cancelled){
 stop(cancelled);ProjectionImageLimits bounds;bounds.max_pixels=size_t(image.width)*image.height;
 const auto pixels=decode_projection_png(png,bounds);
 if(pixels.width!=image.width||pixels.height!=image.height)throw std::invalid_argument("Resolved PNG/EXR dimensions differ");
 const auto &f=entry.at("fallback"),&material=j.at("materials").at(entry.at("material").get<size_t>());
 const bool ignored=material.contains("extensions")&&material.at("extensions").contains("KHR_materials_unlit");
 const auto factors=f.at("emissiveFactor").get<std::array<float,3>>();const float strength=f.at("strength").get<float>();
 detail::ResolvedFallbackMetrics metric;
 for(size_t i=0;i<image.pixels.size();++i){if((i&1023)==0)stop(cancelled);for(size_t k=0;k<3;++k)
  metric.sample(image.pixels[i][k],detail::resolved_fallback_radiance(pixels.rgba[i*4+k],factors[k],strength,ignored));}
 if(metric.maximum_absolute_error!=f.at("maximumAbsoluteError").get<double>()||metric.maximum_relative_error!=f.at("maximumRelativeError").get<double>()||metric.erased_nonzero_samples!=f.at("erasedNonzeroSamples").get<size_t>())
  throw std::invalid_argument("Resolved fallback metrics differ from actual PNG/FLOAT reconstruction");
}
void validate_streamed_image(const Json &j,std::span<const uint8_t> binary,const Json &entry,
    const detail::EmissionExportRaster &field,const NativeEmissionReadLimits &limits){
 const auto resource=view(j,binary,entry);
 auto image_limits=limits.image;
 image_limits.cancelled=limits.cancelled;
 const auto description=detail::emission_exr_owned_description(resource,image_limits,{},true);
 if(!entry.contains("textureSet")){
  const auto &node=j.at("nodes").at(entry.at("node").get<size_t>());
  const auto &mesh=j.at("meshes").at(node.at("mesh").get<size_t>());
  if(mesh.at("primitives").size()!=1||mesh.at("primitives").at(0).at("material")!=entry.at("material"))
   throw std::invalid_argument("Streamed HDR lost its emitted material and node");
 }
 if(field.resolution!=description.width||field.resolution!=description.height||field.version!=description.version)
  throw std::invalid_argument("Streamed EXR differs from its exact native raster");
 const auto scratch=size_t(description.width)*std::min(16u,description.height)*sizeof(detail::EmissionExrPixel);
 const auto pixels=size_t(description.width)*description.height;
 auto working=description;
 working.decoded_bytes=scratch*2+field.working_bytes;
 const auto png=fallback2_png(j,binary,entry,working,image_limits,resource.size());
 ProjectionImageLimits bounds;
 bounds.max_pixels=pixels;
 const auto fallback_image=decode_projection_png(png,bounds);
 const size_t resident=working.decoded_bytes-scratch+fallback_image.rgba.capacity();
 if(resident>=image_limits.max_working_bytes)throw std::length_error("Streamed HDR replay exceeds working budget");
 image_limits.max_working_bytes-=resident;
 std::vector<detail::EmissionExrPixel> expected(scratch/sizeof(detail::EmissionExrPixel));
 const auto &f=entry.at("fallback"),&material=j.at("materials").at(entry.at("material").get<size_t>());
 const bool ignored=material.contains("extensions")&&material.at("extensions").contains("KHR_materials_unlit");
 const auto factors=f.at("emissiveFactor").get<std::array<float,3>>();
 const auto strength=f.at("strength").get<float>();
 detail::ResolvedFallbackMetrics metrics;
 detail::read_emission_exr_rows(resource,[&](uint32_t y,std::span<const detail::EmissionExrPixel> rows){
  auto samples=std::span(expected).first(rows.size());
  field.read(y,samples);
  for(size_t i=0;i<rows.size();++i){
   if(std::memcmp(rows[i].data(),samples[i].data(),sizeof(detail::EmissionExrPixel))!=0)
    throw std::invalid_argument("Streamed FLOAT coefficients differ from their exact native replay");
   const auto at=(size_t(y)*field.resolution+i)*4;
   for(size_t k=0;k<3;++k){
    const auto q=fallback_image.rgba[at+k];
    const float value=rows[i][k];
    if(description.version==2)
     metrics.sample(value,detail::resolved_fallback_radiance(q,factors[k],strength,ignored));
    else{
     const auto restored=detail::legacy_fallback_radiance(q,factors[k],strength,ignored);
     const auto error=std::abs(value-restored);
     metrics.maximum_absolute_error=std::max(metrics.maximum_absolute_error,double(error));
     if(value>0){
      metrics.maximum_relative_error=std::max(metrics.maximum_relative_error,double(error/value));
      if(restored==0)++metrics.erased_nonzero_samples;
     }
    }
   }
  }
 },image_limits);
 if(metrics.maximum_absolute_error!=f.at("maximumAbsoluteError").get<double>()||
    metrics.maximum_relative_error!=f.at("maximumRelativeError").get<double>()||
    metrics.erased_nonzero_samples!=f.at("erasedNonzeroSamples").get<size_t>())
  throw std::invalid_argument("Streamed fallback metrics differ from its actual PNG/FLOAT reconstruction");
}
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
using FieldIdentity=std::pair<uint64_t,std::optional<uint32_t>>;
std::optional<uint32_t> texture_set_descriptor(const Json &entry,bool allowed){
 if(!entry.contains("textureSet"))return {};
 const auto &member=entry.at("textureSet");
 if(!allowed||!member.is_object()||member.size()!=2||entry.contains("domain")||
    (entry.at("view")!="pbr"&&entry.at("view")!="authored-mtoon"))
  throw std::invalid_argument("Unknown native HDR texture-set domain");
 integer(member.at("texCoord"),7,"Native HDR texture-set coordinate index exceeds policy");
 return uint32_t(integer(member.at("id"),UINT32_MAX,"Invalid native HDR texture-set identity"));
}
void check_texture_set_domain(const Json &j,std::span<const uint8_t> binary,const Json &entry,
    const SculptObject &object,const TextureSetView &field,const std::function<bool()> &cancelled){
 if(!object.texture_sets||!field.domain||field.domain->triangles().empty()||
    !detail::texture_set_physical_emission(field))
  throw std::invalid_argument("HDR texture-set record lost its actual material member");
 const auto coordinate=entry.at("textureSet").at("texCoord").get<unsigned>();
 const auto &node=j.at("nodes").at(entry.at("node").get<size_t>());
 const auto &mesh=j.at("meshes").at(node.at("mesh").get<size_t>());
 const auto &primitives=mesh.at("primitives");
 const Json *primitive=nullptr;
 for(const auto &candidate:primitives){
  if(!candidate.contains("extras")||!candidate.at("extras").contains("dome_sculpt_texture_set")||
     candidate.at("extras").at("dome_sculpt_texture_set").at("id")!=field.id)continue;
  if(primitive)throw std::invalid_argument("HDR texture-set primitive identity is ambiguous");
  primitive=&candidate;
 }
 const auto &material=j.at("materials").at(entry.at("material").get<size_t>());
 const auto extensions=material.find("extensions");
 const bool typed=entry.at("view")=="authored-mtoon";
 if(typed!=(extensions!=material.end()&&extensions->contains("VRMC_materials_mtoon"))||
    (typed&&(coordinate!=0||!field.material_bindings||!field.material_bindings->has_mtoon)))
  throw std::invalid_argument("HDR texture-set view differs from its actual material model");
 const auto &info=material.at("emissiveTexture");
 if(!primitive||primitive->at("material")!=entry.at("material")||info.contains("extensions")||
    info.value("texCoord",0u)!=coordinate)
  throw std::invalid_argument("HDR texture-set record differs from its emitted primitive or coordinate");
 const auto &attrs=primitive->at("attributes");
 if(typed){
  const auto &sampler=j.at("samplers").at(j.at("textures").at(info.at("index").get<size_t>()).at("sampler").get<size_t>());
  if(!attrs.contains("TANGENT")||sampler.at("wrapS")!=33071||sampler.at("wrapT")!=33071||
     sampler.at("minFilter")!=9729||sampler.at("magFilter")!=9729)
   throw std::invalid_argument("Typed FLOAT texture-set lost its certified tangent frame or sampler");
 }
 const auto indices=domain_accessor(j,binary,primitive->at("indices"),"SCALAR",1,6'000'000,5125);
 const auto positions=domain_accessor(j,binary,attrs.at("POSITION"),"VEC3",3,2'000'000,5126);
 const auto uv=domain_accessor(j,binary,attrs.at("TEXCOORD_"+std::to_string(coordinate)),
     "VEC2",2,2'000'000,5126);
 if(indices.count!=field.domain->triangles().size()*3||positions.count!=uv.count)
  throw std::invalid_argument("HDR texture-set emitted corner count differs from its native membership");
 const auto &rest=*field.domain->topology()->geometry();
 const auto preview=field.source?field.source->source_preview():nullptr;
 size_t at=0;
 for(const auto triangle:field.domain->triangles())for(size_t corner=0;corner<3;++corner){
  if((at&1023)==0)stop(cancelled);
  const auto logical=size_t(triangle)*3+corner;
  const auto vertex=indices.index(at++);
  if(vertex>=positions.count)throw std::invalid_argument("HDR texture-set emitted vertex exceeds its accessor");
  const auto native=rest.indices.at(logical);
  const auto p=rest.positions.at(native);
  const auto sample=preview?preview->paint_coordinates().at(logical):rest.uv.at(native);
  if(positions.scalar(vertex,0)!=p.x||positions.scalar(vertex,1)!=p.y||positions.scalar(vertex,2)!=p.z||
     uv.scalar(vertex,0)!=sample[0]||uv.scalar(vertex,1)!=sample[1])
   throw std::invalid_argument("HDR texture-set emitted paint corner differs from its exact native member");
 }
}
void validate_canonical_role_png(const Json &j,std::span<const uint8_t> binary,const Json &info,const Image &expected,
    const std::function<bool()> &cancelled){
 auto resource=[](const Json &table,const Json &index,const char *message)->const Json&{
  const auto at=integer(index,table.size(),message);
  if(!table.is_array()||at>=table.size())throw std::invalid_argument(message);
  return table[at];
 };
 const auto &textures=j.at("textures"),&images=j.at("images"),&samplers=j.at("samplers");
 const auto &texture=resource(textures,info.at("index"),"Canonical HDR texture index is invalid");
 const auto &sampler=resource(samplers,texture.at("sampler"),"Canonical HDR sampler index is invalid");
 if(sampler.value("wrapS",Json(10497))!=33071||sampler.value("wrapT",Json(10497))!=33071||
    sampler.value("minFilter",Json(9729))!=9729||sampler.value("magFilter",Json(9729))!=9729)
  throw std::invalid_argument("Canonical HDR material must retain its clamp-linear sampler");
 const auto &image=resource(images,texture.at("source"),"Canonical HDR image index is invalid");
 if(image.contains("uri")||image.at("mimeType")!="image/png")
  throw std::invalid_argument("Canonical HDR material must retain its embedded PNG");
 const auto &view=resource(j.at("bufferViews"),image.at("bufferView"),"Canonical HDR image view is invalid");
 if(integer(view.at("buffer"),0,"Canonical HDR image buffer differs")||view.contains("byteStride")||view.contains("target"))
  throw std::invalid_argument("Canonical HDR material image view differs");
 const auto begin=integer(view.value("byteOffset",Json(0)),binary.size(),"Canonical HDR image offset is invalid");
 const auto length=integer(view.at("byteLength"),binary.size(),"Canonical HDR image length is invalid");
 if(length>binary.size()-begin)throw std::invalid_argument("Canonical HDR image exceeds BIN");
 stop(cancelled);
 const auto expected_png=expected.png();
 stop(cancelled);
 const auto actual=binary.subspan(begin,length);
 if(!std::ranges::equal(expected_png,actual))
  throw std::invalid_argument("Canonical HDR image differs from its independently owned material field");
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
  validate_canonical_role_png(j,binary,normal_info,*frame->prepared_normal.image,cancelled);
  if(binding->color_preparation){
   const auto &pbr=j.at("materials")[material].at("pbrMetallicRoughness");
   const auto &color=pbr.at("baseColorTexture");
   if(color.contains("extensions")||integer(color.value("texCoord",Json(0)),7,"Canonical HDR color coordinate exceeds policy")!=coordinate||
      attributes.contains("COLOR_0")||
      pbr.value("baseColorFactor",Json::array({1,1,1,1}))!=Json::array({1,1,1,1})||
      !frame->surface.textures||frame->surface.textures->pbr_color.rgba.empty())
    throw std::invalid_argument("Canonical HDR resolved color differs from its retained sampling domain");
   validate_canonical_role_png(j,binary,color,frame->surface.textures->pbr_color,cancelled);
  }
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
 std::vector<uint8_t> expanded;if(compressed){expanded=native_deflate(bytes,false,decoded_bytes,cancelled);bytes=expanded;}cbor_preflight(bytes,cancelled);const auto value=Json::from_cbor(bytes.begin(),bytes.end(),true,true);detail::NativeReadScope native_read(cancelled);auto result=Scene::decode(value);stop(cancelled);return result;
}
Scene detail::emission_export_subset(const Scene &source,std::span<const uint64_t> exported){
 std::set<uint64_t> selected;for(auto id:exported){source.object(id);if(!selected.insert(id).second)throw std::invalid_argument("Duplicate native export subset object");}if(selected.empty())throw std::invalid_argument("No native export subset objects");auto owner=source.snapshot();std::vector<uint64_t> discard;for(const auto &o:owner.objects())if(!selected.contains(o.id))discard.push_back(o.id);for(auto id:discard)owner.erase(id);std::set<uint64_t> kept;for(const auto &o:owner.objects())for(const auto &g:owner.group_path(o.group))kept.insert(g.id);std::vector<uint64_t> groups;for(const auto &g:owner.groups())if(!kept.contains(g.id)&&(!g.parent||kept.contains(g.parent)))groups.push_back(g.id);for(auto id:groups)owner.erase_group(id,false);return owner;
}
void detail::attach_native_emission(GltfDocument &document,const NativeEmissionExport &options,std::span<const uint64_t> exported,std::span<const EmissionExportImage> images,size_t max_bytes,const std::function<bool()> &cancelled,bool streaming){
 stop(cancelled);if(!options.scene||!options.allow_measured_core_fallback)throw std::invalid_argument("Exact HDR export requires its immutable native owner and explicit measured fallback permission");if(!max_bytes||max_bytes>256*1024*1024||images.size()>256)throw std::length_error("Native HDR package exceeds output limits");
 const bool texture_sets=std::any_of(images.begin(),images.end(),[](const auto &entry){return entry.texture_set.has_value();});
 if(texture_sets&&!streaming)throw std::invalid_argument("HDR texture sets require exact streamed fields");
 size_t total_samples=0;
 const size_t sample_limit=streaming?emission_stream_pixels:emission_dense_bytes/sizeof(EmissionExrPixel);
 for(const auto &entry:images){
  stop(cancelled);
  const auto samples=streaming?size_t(entry.image.width)*entry.image.height:entry.image.pixels.size();
  if(samples>sample_limit-total_samples)
   throw std::length_error("Native HDR aggregate decoded EXR fields exceed policy");
  if(streaming&&(!samples||!entry.image.pixels.empty()))
   throw std::invalid_argument("Streamed HDR image requires its actual on-demand raster");
  total_samples+=samples;
 }
 if(streaming&&!texture_sets&&total_samples<=emission_dense_bytes/sizeof(EmissionExrPixel))
  throw std::invalid_argument("Streamed HDR requires its large-field witness");
 auto &j=document.json;if(j.contains("extras")&&!j["extras"].is_object())throw std::invalid_argument("Native HDR export cannot replace nonobject extras");if(j.contains("extras")&&(j["extras"].contains(marker)||j["extras"].contains("domeSculptAuthoring")))throw std::invalid_argument("Native HDR export refuses an existing reserved package");
 std::set<uint64_t> selected;for(auto id:exported){options.scene->object(id);if(!selected.insert(id).second)throw std::invalid_argument("Native HDR export has duplicate source objects");}if(selected.empty())throw std::invalid_argument("Native HDR export has no source objects");
 if(options.scope!=NativeEmissionExport::Scope::WholeScene&&options.scope!=NativeEmissionExport::Scope::ExportedObjects)throw std::invalid_argument("Unknown native HDR export scope");
 if(options.scope==NativeEmissionExport::Scope::WholeScene)for(const auto &o:options.scene->objects())if(o.visible&&!selected.contains(o.id)){if(o.polygon&&o.polygon->active&&o.polygon->topology.faces().empty())continue;const bool nonempty=o.mesh_active?o.painted&&!o.painted->mesh().indices.empty():!surface_mesh(*o.volume,cancelled).indices.empty();if(nonempty)throw std::invalid_argument("Whole-scene HDR export omitted a visible source; use explicit subset scope");}
 Scene owner=options.scope==NativeEmissionExport::Scope::ExportedObjects?emission_export_subset(*options.scene,exported):options.scene->snapshot();
 owner.validate();auto native=encoded_scene(owner,cancelled);cbor_preflight(native,cancelled);const size_t decoded_bytes=native.size();const bool compressed=decoded_bytes>128*1024*1024;if(compressed)native=native_deflate(native,true,0,cancelled);
 auto append=[&](std::span<const uint8_t> bytes){stop(cancelled);auto &data=document.binary;size_t padding=(4-data.size()%4)%4;if(data.size()>max_bytes||padding>max_bytes-data.size()||bytes.size()>max_bytes-data.size()-padding)throw std::length_error("Native HDR package exceeds output byte budget");data.insert(data.end(),padding,0);const auto offset=data.size();data.insert(data.end(),bytes.begin(),bytes.end());if(j["bufferViews"].size()>=65536)throw std::length_error("Native HDR bufferView count exceeds read/write limit");int index=int(j["bufferViews"].size());j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",bytes.size()}});return Json{{"bufferView",index},{"sha256",hash(bytes)}};};
 const bool canonical_domains=std::any_of(images.begin(),images.end(),[](const auto &entry){return bool(entry.canonical_paint_domain);});
 const bool domains=canonical_domains||std::any_of(images.begin(),images.end(),[](const auto &entry){return entry.native_paint_domain;});
 Json package={{"version",texture_sets?6:streaming?5:canonical_domains?4:domains?3:compressed?2:1},{"encoding",compressed?"scene-cbor-deflate+openexr-float32-zip":"scene-cbor+openexr-float32-zip"},{"native",append(native)},{"images",Json::array()}};if(domains)package["encoding"]=canonical_domains?(compressed?"scene-cbor-deflate+openexr-float32-zip+paint-domain-v2":"scene-cbor+openexr-float32-zip+paint-domain-v2"):(compressed?"scene-cbor-deflate+openexr-float32-zip+paint-domain-v1":"scene-cbor+openexr-float32-zip+paint-domain-v1");if(streaming)package["encoding"]=compressed?"scene-cbor-deflate+openexr-float32-zip+bounded-scanlines":
  "scene-cbor+openexr-float32-zip+bounded-scanlines";if(texture_sets)package["encoding"]=compressed?"scene-cbor-deflate+openexr-float32-zip+texture-set-domains":
  "scene-cbor+openexr-float32-zip+texture-set-domains";if(compressed)package["native"]["decodedBytes"]=decoded_bytes;std::vector<uint8_t>().swap(native);std::set<FieldIdentity> seen;size_t exr_bytes=0;
 for(const auto &entry:images){stop(cancelled);if(!selected.contains(entry.object)||!seen.emplace(entry.object,entry.texture_set).second||entry.node<0||entry.material<0||size_t(entry.node)>=j.at("nodes").size()||size_t(entry.material)>=j.at("materials").size())throw std::invalid_argument("Native HDR image has no unique exported owner");if(exr_bytes>=128*1024*1024)throw std::length_error("Native HDR aggregate EXR budget exceeded");EmissionInterchangeLimits limits;limits.max_encoded_bytes=std::min(max_bytes,size_t(128*1024*1024)-exr_bytes);limits.cancelled=cancelled;const auto &current_object=owner.object(entry.object);const auto current_canonical=current_object.material_bindings?current_object.material_bindings->canonical_paint_sampling:nullptr;if(!entry.texture_set&&current_canonical&&(!entry.canonical_paint_domain||current_canonical!=entry.canonical_paint_domain||current_canonical.owner_before(entry.canonical_paint_domain)||entry.canonical_paint_domain.owner_before(current_canonical)))throw std::invalid_argument("Canonical HDR image omitted or borrowed its actual writable owner");if(entry.native_paint_domain&&entry.canonical_paint_domain)throw std::invalid_argument("Native HDR image claims competing writable owners");if(!streaming&&(entry.native_paint_domain||entry.canonical_paint_domain)){const auto &actual=owner.object(entry.object);if(entry.canonical_paint_domain){const auto expected=actual.material_bindings?actual.material_bindings->canonical_paint_sampling:nullptr;if(expected!=entry.canonical_paint_domain||expected.owner_before(entry.canonical_paint_domain)||entry.canonical_paint_domain.owner_before(expected))throw std::invalid_argument("Canonical HDR borrowed a foreign writable owner");}auto expected=entry.canonical_paint_domain?detail::canonical_paint_emission_field(actual,cancelled):detail::native_paint_emission_field(actual,cancelled);if(expected.width!=entry.image.width||expected.height!=entry.image.height||expected.pixels!=entry.image.pixels)throw std::invalid_argument("Native HDR field changed after its owning preparation");}
 std::vector<uint8_t> bytes;
 if(streaming){
  TextureSetLimits set_limits;
  set_limits.cancelled=cancelled;
  const auto field=entry.texture_set?
      texture_set_emission_raster(texture_set_view(current_object,*entry.texture_set),set_limits,entry.authored_mtoon):
      export_emission_raster(current_object,cancelled,entry.authored_mtoon);
  if(field.resolution!=entry.image.width||field.resolution!=entry.image.height||field.version!=entry.image.version)
   throw std::invalid_argument("Streamed HDR differs from its exact native raster");
  const auto scratch=size_t(field.resolution)*std::min(16u,field.resolution)*sizeof(EmissionExrPixel);
  const auto forecast=5*scratch+8u*1024*1024;
  if(field.working_bytes>=limits.max_working_bytes||forecast>=limits.max_working_bytes-field.working_bytes)
   throw std::length_error("Streamed HDR raster exceeds working budget");
  limits.max_working_bytes-=field.working_bytes;
  limits.max_encoded_bytes=std::min(limits.max_encoded_bytes,limits.max_working_bytes-forecast);
  bytes=encode_emission_exr_rows(field.resolution,field.resolution,field.version,field.read,limits);
 }else bytes=encode_emission_exr(entry.image,limits);
 exr_bytes+=bytes.size();auto record=append(bytes);record["object"]=entry.object;record["node"]=entry.node;record["material"]=entry.material;if(streaming)record["view"]=entry.authored_mtoon?"authored-mtoon":"pbr";record["fallback"]={{"strength",entry.fallback_strength},{"emissiveFactor",entry.fallback_factor},{"maximumAbsoluteError",entry.maximum_absolute_error},{"maximumRelativeError",entry.maximum_relative_error},{"erasedNonzeroSamples",entry.erased_nonzero_samples},{"evaluation","current-static-view"}};if(entry.image.version==2){record["fallback"]["version"]=2;record["fallback"]["units"]=detail::resolved_fallback_units;record["fallback"]["view"]=entry.authored_mtoon?"authored-mtoon":"pbr";}fallback(record.at("fallback"),entry.image.version);if(entry.native_paint_domain||entry.canonical_paint_domain){record["domain"]={{"version",1},{"kind",entry.canonical_paint_domain?"canonical-paint37":"native-paint32"},{"texCoord",entry.texcoord}};native_domain(j,document.binary,record,owner.object(entry.object),cancelled,canonical_domains);}if(entry.texture_set){
  record["textureSet"]={{"id",*entry.texture_set},{"texCoord",entry.texcoord}};
  check_texture_set_domain(j,document.binary,record,current_object,texture_set_view(current_object,*entry.texture_set),cancelled);
 }
 package["images"].push_back(std::move(record));}
 while(document.binary.size()%4){if(document.binary.size()==max_bytes)throw std::length_error("Native HDR padding exceeds output limit");document.binary.push_back(0);}j["buffers"]={{{"byteLength",document.binary.size()}}};package["binarySha256"]=hash(document.binary);package["documentSha256"]="";j["extras"][marker]=std::move(package);j["extras"][marker]["documentSha256"]=document_hash(j);if(j.dump().size()>16*1024*1024)throw std::length_error("Native HDR document JSON exceeds16MiB read/write limit");stop(cancelled);
}
std::optional<Scene> extract_native_emission(std::span<const uint8_t> bytes,const NativeEmissionReadLimits &limits){
 stop(limits.cancelled);if(!limits.max_decoded_native_bytes||limits.max_decoded_native_bytes>256*1024*1024||!limits.max_container_bytes||limits.max_container_bytes>256*1024*1024||!limits.max_json_bytes||limits.max_json_bytes>16*1024*1024||!limits.max_native_bytes||limits.max_native_bytes>128*1024*1024||!limits.max_exr_bytes||limits.max_exr_bytes>128*1024*1024||!limits.max_images||limits.max_images>256||bytes.size()>limits.max_container_bytes)throw std::length_error("Invalid native HDR container limits");
 if(bytes.size()<20||word(bytes,0)!=0x46546c67||word(bytes,4)!=2||word(bytes,8)!=bytes.size()||word(bytes,16)!=0x4e4f534a)throw std::invalid_argument("Invalid native HDR GLB header");const auto length=word(bytes,12);if(length%4||length>limits.max_json_bytes||length>bytes.size()-20)throw std::length_error("Native HDR GLB JSON exceeds bounds");
 Json j=Json::parse(bytes.begin()+20,bytes.begin()+20+length,[&](int depth,Json::parse_event_t,Json&){stop(limits.cancelled);if(depth>128)throw std::length_error("Native HDR JSON depth exceeded");return true;});if(!j.is_object())throw std::invalid_argument("Invalid native HDR GLB root");if(!j.contains("extras"))return std::nullopt;if(!j.at("extras").is_object()){return std::nullopt;}if(!j.at("extras").contains(marker))return std::nullopt;if(j.at("extras").contains("domeSculptAuthoring"))throw std::invalid_argument("Competing native package authority");const auto &package=j.at("extras").at(marker);const bool compressed=marker_object(package);const bool texture_sets=package.at("version")==6;const bool streaming=texture_sets||package.at("version")==5;
 const bool canonical_domains=package.at("version")==4;const bool domains=canonical_domains||package.at("version")==3;
 const bool allow_domains=domains||streaming, allow_canonical=canonical_domains||streaming;
 const auto position=20+size_t(length);if(position>bytes.size()||bytes.size()-position<8||word(bytes,position+4)!=0x004e4942)throw std::invalid_argument("Native HDR package has no captured BIN");const auto binary_length=word(bytes,position);if(binary_length%4||binary_length!=bytes.size()-position-8)throw std::invalid_argument("Native HDR BIN framing mismatch");auto binary=bytes.subspan(position+8,binary_length);if(j.at("buffers").size()!=1||integer(j.at("buffers")[0].at("byteLength"),binary.size(),"Invalid native HDR buffer length")!=binary.size())throw std::invalid_argument("Native HDR buffer declaration mismatch");
 if(package.at("binarySha256").get_ref<const std::string&>()!=hash(binary)||package.at("documentSha256").get_ref<const std::string&>()!=document_hash(j))throw std::invalid_argument("Native HDR core/resource binding digest mismatch");
 const auto &native_record=package.at("native");if(native_record.size()!=(compressed?3:2))throw std::invalid_argument("Invalid native HDR Scene descriptor");const size_t decoded_bytes=compressed?integer(native_record.at("decodedBytes"),limits.max_decoded_native_bytes,"Native HDR decoded byte limit exceeded"):0;auto native=view(j,binary,native_record);if(!compressed&&native.size()>limits.max_decoded_native_bytes)throw std::length_error("Native HDR decoded byte limit exceeded");if(native.size()>limits.max_native_bytes)throw std::length_error("Native HDR Scene payload exceeds read budget");const auto &images=package.at("images");if(images.size()>limits.max_images)throw std::length_error("Native HDR image count exceeds limits");size_t encoded=0,decoded_fields=0,domain_count=0,canonical_count=0;std::set<uint64_t> owners,resolved_owners;
 std::set<FieldIdentity> fields;
 size_t set_fields=0;
 // Validate all small descriptors and byte digests before EXR/Scene allocation.
 for(const auto &entry:images){
  stop(limits.cancelled);
  const auto set=texture_set_descriptor(entry,texture_sets);
  const auto object=integer(entry.at("object"),UINT64_MAX,"Invalid native HDR object identity");
  if(entry.size()!=(allow_domains&&entry.contains("domain")?7u:6u)+unsigned(streaming)+unsigned(bool(set))||
     (!allow_domains&&entry.contains("domain"))||!object||!fields.emplace(object,set).second||
     integer(entry.at("node"),j.at("nodes").size(),"Invalid native HDR node")>=j.at("nodes").size()||
     integer(entry.at("material"),j.at("materials").size(),"Invalid native HDR material")>=j.at("materials").size())
    throw std::invalid_argument("Invalid native HDR image owner");
  owners.insert(object);
  set_fields+=set.has_value();
  fallback(entry.at("fallback"));if(entry.contains("domain")){domain_descriptor(entry.at("domain"),allow_canonical);++domain_count;if(entry.at("domain").at("kind")=="canonical-paint37")++canonical_count;}auto resource=view(j,binary,entry);if(encoded>limits.max_exr_bytes||resource.size()>limits.max_exr_bytes-encoded)throw std::length_error("Native HDR EXR bytes exceed aggregate limit");encoded+=resource.size();auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;const auto description=detail::emission_exr_owned_description(resource,image_limits,{},streaming);
  fallback(entry.at("fallback"),description.version);
  if(streaming&&((entry.at("view")!="pbr"&&entry.at("view")!="authored-mtoon")||
      (description.version==2&&entry.at("fallback").at("view")!=entry.at("view"))))
   throw std::invalid_argument("Unknown streamed HDR emission view");
  if(description.version==2){
   if(!set)resolved_owners.insert(entry.at("object").get<uint64_t>());
   if(!streaming&&(resource.size()>image_limits.max_working_bytes||
       description.decoded_bytes>(image_limits.max_working_bytes-resource.size())/2))
    throw std::length_error("Resolved native replay exceeds existing image working budget");
   auto working=description;
   if(streaming)working.decoded_bytes=size_t(description.width)*std::min(16u,description.height)*sizeof(detail::EmissionExrPixel)*2;
   (void)fallback2_png(j,binary,entry,working,image_limits,resource.size());
  }
  const size_t raw=description.decoded_bytes;
  const auto aggregate=streaming?detail::emission_stream_pixels*sizeof(detail::EmissionExrPixel):detail::emission_dense_bytes;
  if(raw>aggregate-decoded_fields)throw std::length_error("Native HDR aggregate decoded EXR fields exceed policy");
  decoded_fields+=raw;
 }
 if(texture_sets&&!set_fields)throw std::invalid_argument("HDR texture-set package has no material member");
 if(streaming&&!texture_sets&&decoded_fields<=detail::emission_dense_bytes)
  throw std::invalid_argument("Streamed HDR requires its large-field witness");
 if(canonical_domains&&!canonical_count)throw std::invalid_argument("Native HDR package4 has no genuine canonical writable-domain witness");
 if(domains&&!domain_count)throw std::invalid_argument("Native HDR package3 has no genuine coordinate-domain witness");
 if(!streaming)for(const auto &entry:images){auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;const auto resource=view(j,binary,entry);const auto image=decode_emission_exr(resource,image_limits);
  if(image.version==2){const detail::EmissionExrDescription d{image.pixels.size()*sizeof(image.pixels[0]),2,image.width,image.height};
   const auto png=fallback2_png(j,binary,entry,d,image_limits,resource.size());validate_fallback2_metrics(j,entry,image,png,limits.cancelled);}
 }
 stop(limits.cancelled);std::vector<uint8_t> expanded;if(compressed){expanded=native_deflate(native,false,decoded_bytes,limits.cancelled);native=expanded;}cbor_preflight(native,limits.cancelled);const auto value=Json::from_cbor(native.begin(),native.end(),true,true);detail::NativeReadScope native_read(limits.cancelled);auto result=Scene::decode(value);for(auto id:owners)result.object(id);
 for(auto id:resolved_owners){const auto &object=result.object(id);
  if((value.at("version")!=41&&value.at("version")!=42&&value.at("version")!=detail::shared_bake_scene_version)||!object.mesh_active||!object.painted||!object.painted->baked()||!object.painted->baked()->emission_field)
   throw std::invalid_argument("Resolved fallback2 requires its actual Scene41 current FLOAT field owner");
 }
 if(!streaming)for(const auto &entry:images)if(resolved_owners.contains(entry.at("object").get<uint64_t>())&&!entry.contains("domain")){
  const auto &object=result.object(entry.at("object").get<uint64_t>());
  const bool authored=entry.at("fallback").at("view")=="authored-mtoon";
  const auto expected=detail::baked_emission_field(object,limits.cancelled,authored);
  auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;const auto actual=decode_emission_exr(view(j,binary,entry),image_limits);
  if(expected.version!=2||actual.version!=2||actual.width!=expected.width||actual.height!=expected.height||actual.pixels.size()!=expected.pixels.size())
   throw std::invalid_argument("Resolved EXR differs from its actual native baked owner");
  for(size_t i=0;i<actual.pixels.size();++i){if((i&1023)==0)stop(limits.cancelled);if(actual.pixels[i]!=expected.pixels[i])throw std::invalid_argument("Resolved FLOAT contributions differ from actual native baked replay");}
 }


 for(const auto &entry:images){const auto &object=result.object(entry.at("object").get<uint64_t>());if(!entry.contains("textureSet")&&object.material_bindings&&object.material_bindings->canonical_paint_sampling&&(!allow_canonical||!entry.contains("domain")||entry.at("domain").at("kind")!="canonical-paint37"))throw std::invalid_argument("Canonical HDR package omitted its actual writable-domain witness");}

 if(streaming)for(const auto &entry:images){
  stop(limits.cancelled);
  const auto &object=result.object(entry.at("object").get<uint64_t>());
  if(const auto set=texture_set_descriptor(entry,texture_sets)){
   if(!object.visible||!object.texture_sets)throw std::invalid_argument("HDR texture-set record needs its visible native collection");
   const auto field=texture_set_view(object,*set);
   check_texture_set_domain(j,binary,entry,object,field,limits.cancelled);
   TextureSetLimits set_limits;
   set_limits.cancelled=limits.cancelled;
   validate_streamed_image(j,binary,entry,detail::texture_set_emission_raster(field,set_limits,entry.at("view")=="authored-mtoon"),limits);
   continue;
  }
  if(texture_sets&&object.texture_sets)throw std::invalid_argument("HDR texture-set field omitted its material identity");
  const bool independent=object.material_bindings&&
      (object.material_bindings->canonical_paint_sampling||object.material_bindings->native_paint_sampling);
  if(independent!=entry.contains("domain"))
   throw std::invalid_argument("Streamed HDR lost its exact paint coordinate witness");
  if(independent)native_domain(j,binary,entry,object,limits.cancelled,true);
  validate_streamed_image(j,binary,entry,detail::export_emission_raster(object,limits.cancelled,
      entry.at("view")=="authored-mtoon"),limits);
 }

 if(texture_sets)for(const auto &object:result.objects())if(object.visible&&object.texture_sets){
  const auto require_field=[&](TextureSetId set){
   const auto field=texture_set_view(object,set);
   if(!field.domain->triangles().empty()&&detail::texture_set_physical_emission(field)&&
      !fields.contains({object.id,set}))
    throw std::invalid_argument("HDR texture-set package omitted a visible physical material");
  };
  require_field(0);
  for(const auto &member:object.texture_sets->additional())require_field(member.id);
 }

 if(domains){size_t actual=0;for(const auto &entry:images)if(entry.contains("domain")){++actual;const auto &object=result.object(entry.at("object").get<uint64_t>());native_domain(j,binary,entry,object,limits.cancelled,canonical_domains);auto expected=entry.at("domain").at("kind")=="canonical-paint37"?detail::canonical_paint_emission_field(object,limits.cancelled):detail::native_paint_emission_field(object,limits.cancelled);auto image_limits=limits.image;image_limits.cancelled=limits.cancelled;auto image=decode_emission_exr(view(j,binary,entry),image_limits);if(image.version!=expected.version||image.width!=expected.width||image.height!=expected.height||image.pixels.size()!=expected.pixels.size())throw std::invalid_argument("Native HDR field differs from its retained paint owner");for(size_t p=0;p<image.pixels.size();++p){if((p&1023)==0)stop(limits.cancelled);if(image.pixels[p]!=expected.pixels[p])throw std::invalid_argument("Native HDR float planes differ from their exact native paint replay");}}
 if(!actual)throw std::invalid_argument("Native HDR package3 has no genuine coordinate-domain witness");}
 stop(limits.cancelled);return result;
}
}
