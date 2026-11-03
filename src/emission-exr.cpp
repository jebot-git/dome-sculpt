// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/emission-interchange.hpp"
#include "emission-exr-access.hpp"
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/openexr.h>
#include <cstdlib>
#include <cstddef>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfIO.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfIntAttribute.h>
#include <algorithm>
#include <cstring>
#include <limits>
namespace dome {namespace {
namespace exr=OPENEXR_IMF_NAMESPACE;
constexpr std::array<const char*,10> channels{"R","G","B","paint.legacy.R","paint.legacy.G","paint.legacy.B","paint.physical.R","paint.physical.G","paint.physical.B","paint.coverage"};
void stop(const EmissionInterchangeLimits &l){if(l.cancelled&&l.cancelled())throw std::runtime_error("HDR emission interchange cancelled");}
void limits(const EmissionInterchangeLimits&l){if(!l.max_dimension||l.max_dimension>8192||!l.max_pixels||l.max_pixels>16*1024*1024||!l.max_encoded_bytes||l.max_encoded_bytes>256*1024*1024||!l.max_working_bytes||l.max_working_bytes>512*1024*1024)throw std::invalid_argument("Invalid HDR interchange limits");stop(l);}
size_t measure(uint32_t width,uint32_t height,const EmissionInterchangeLimits &l){limits(l);if(!width||!height||width>l.max_dimension||height>l.max_dimension||size_t(width)*height>l.max_pixels)throw std::length_error("HDR emission dimensions exceed limits");return size_t(width)*height*sizeof(std::array<float,10>);}
class Output final:public exr::OStream {
 const EmissionInterchangeLimits &limits_;size_t position_=0;
public:
 std::vector<uint8_t> bytes;
 Output(const EmissionInterchangeLimits &l,size_t forecast):exr::OStream("native-emission.exr"),limits_(l){bytes.reserve(forecast);}
 void write(const char *p,int n)override{stop(limits_);if(n<0||position_>limits_.max_encoded_bytes||size_t(n)>limits_.max_encoded_bytes-position_)throw std::length_error("HDR EXR output byte budget exceeded");if(position_+size_t(n)>bytes.size())bytes.resize(position_+size_t(n));std::memcpy(bytes.data()+position_,p,size_t(n));position_+=size_t(n);}
 uint64_t tellp()override{return position_;}
 void seekp(uint64_t p)override{stop(limits_);if(p>limits_.max_encoded_bytes)throw std::length_error("HDR EXR output seek exceeds byte budget");position_=size_t(p);}
};
class Input final:public exr::IStream {
 std::span<const uint8_t> bytes_;const EmissionInterchangeLimits &limits_;size_t position_=0;
public:
 Input(std::span<const uint8_t>b,const EmissionInterchangeLimits &l):exr::IStream("native-emission.exr"),bytes_(b),limits_(l){}
 bool read(char *p,int n)override{stop(limits_);if(n<0||position_>bytes_.size()||size_t(n)>bytes_.size()-position_)throw std::invalid_argument("Truncated HDR EXR stream");std::memcpy(p,bytes_.data()+position_,size_t(n));position_+=size_t(n);return position_<bytes_.size();}
 uint64_t tellg()override{return position_;}
 void seekg(uint64_t p)override{stop(limits_);if(p>bytes_.size())throw std::invalid_argument("HDR EXR seek outside captured bytes");position_=size_t(p);}
};
struct HeaderBudget{size_t used=0,maximum=0;};
thread_local HeaderBudget *header_budget=nullptr;
struct alignas(std::max_align_t) Allocation {HeaderBudget *budget;size_t bytes;};
void *header_alloc(size_t bytes)noexcept{auto *b=header_budget;if(!b||bytes>b->maximum-b->used||bytes>SIZE_MAX-sizeof(Allocation))return nullptr;auto *a=static_cast<Allocation*>(std::malloc(sizeof(Allocation)+bytes));if(!a)return nullptr;a->budget=b;a->bytes=bytes;b->used+=bytes;return a+1;}
void header_free(void *p)noexcept{if(!p)return;auto *a=static_cast<Allocation*>(p)-1;a->budget->used-=a->bytes;std::free(a);}
struct ReadState{std::span<const uint8_t> bytes;const EmissionInterchangeLimits *limits;std::exception_ptr failure;};
int64_t read_size(exr_const_context_t,void *p){return int64_t(static_cast<ReadState*>(p)->bytes.size());}
int64_t read_bytes(exr_const_context_t,void *p,void *out,uint64_t count,uint64_t offset,exr_stream_error_func_ptr_t){auto &s=*static_cast<ReadState*>(p);try{stop(*s.limits);}catch(...){s.failure=std::current_exception();return -1;}if(offset>=s.bytes.size())return 0;auto n=std::min<uint64_t>(count,s.bytes.size()-offset);std::memcpy(out,s.bytes.data()+size_t(offset),size_t(n));return int64_t(n);}
void quiet_error(exr_const_context_t,exr_result_t,const char*){}
size_t preflight(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l){
 HeaderBudget budget{0,std::min(l.max_working_bytes,size_t(1024*1024))};auto *old=header_budget;header_budget=&budget;exr_context_t context=nullptr;
 try{ReadState state{bytes,&l};exr_context_initializer_t init=EXR_DEFAULT_CONTEXT_INITIALIZER;init.user_data=&state;init.read_fn=read_bytes;init.size_fn=read_size;init.error_handler_fn=quiet_error;init.alloc_fn=header_alloc;init.free_fn=header_free;init.max_image_width=int(l.max_dimension);init.max_image_height=int(l.max_dimension);init.flags=EXR_CONTEXT_FLAG_STRICT_HEADER|EXR_CONTEXT_FLAG_DISABLE_CHUNK_RECONSTRUCTION;
  auto require=[&](exr_result_t result){if(state.failure)std::rethrow_exception(state.failure);if(result!=EXR_ERR_SUCCESS)throw std::invalid_argument("Invalid or oversized HDR EXR header");};require(exr_start_read(&context,"native-emission.exr",&init));int parts=0;require(exr_get_count(context,&parts));if(parts!=1)throw std::invalid_argument("HDR EXR must contain exactly one image part");exr_storage_t storage;require(exr_get_storage(context,0,&storage));exr_compression_t compression;require(exr_get_compression(context,0,&compression));if(storage!=EXR_STORAGE_SCANLINE||compression!=EXR_COMPRESSION_ZIP)throw std::invalid_argument("HDR EXR requires lossless ZIP scanlines");exr_attr_box2i_t window;require(exr_get_data_window(context,0,&window));if(window.min.x||window.min.y||window.max.x<0||window.max.y<0)throw std::invalid_argument("Invalid HDR EXR data window");const auto raw=measure(uint32_t(window.max.x)+1,uint32_t(window.max.y)+1,l);if(bytes.size()>l.max_working_bytes||raw>l.max_working_bytes-bytes.size())throw std::length_error("HDR EXR decode forecast exceeds working budget");const exr_attr_chlist_t *list=nullptr;require(exr_get_channels(context,0,&list));if(!list||list->num_channels!=10)throw std::invalid_argument("HDR EXR must contain ten named FLOAT channels");for(int i=0;i<list->num_channels;++i){const auto &channel=list->entries[i];if(channel.pixel_type!=EXR_PIXEL_FLOAT||channel.x_sampling!=1||channel.y_sampling!=1||std::find_if(channels.begin(),channels.end(),[&](const char *name){return std::strcmp(name,channel.name.str)==0;})==channels.end())throw std::invalid_argument("Invalid HDR EXR channel interpretation");}
  exr_finish(&context);header_budget=old;if(budget.used)throw std::runtime_error("HDR EXR preflight did not release header storage");return raw;
 }catch(...){if(context)exr_finish(&context);header_budget=old;throw;}
}
void check_header(const exr::Header &h){
 if(h.hasTileDescription()||h.compression()!=exr::ZIP_COMPRESSION||h.lineOrder()!=exr::INCREASING_Y||h.dataWindow().min.x||h.dataWindow().min.y||h.displayWindow()!=h.dataWindow())throw std::invalid_argument("Unsupported HDR emission EXR layout");
 const auto version=h.findTypedAttribute<exr::IntAttribute>("domeSculptEmissionVersion");const auto units=h.findTypedAttribute<exr::StringAttribute>("domeSculptEmissionUnits");const auto origin=h.findTypedAttribute<exr::StringAttribute>("domeSculptEmissionCoordinates");
 if(!version||version->value()!=1||!units||units->value()!="linear-cd/m2;legacy-relative-premult;physical-absolute-premult;coverage-linear"||!origin||origin->value()!="row0=v0")throw std::invalid_argument("Unknown HDR emission EXR interpretation");
 size_t count=0;for(auto it=h.channels().begin();it!=h.channels().end();++it){++count;if(std::find_if(channels.begin(),channels.end(),[&](const char *n){return std::strcmp(n,it.name())==0;})==channels.end()||it.channel().type!=exr::FLOAT||it.channel().xSampling!=1||it.channel().ySampling!=1)throw std::invalid_argument("HDR EXR requires its exact ten FLOAT channels");}if(count!=channels.size())throw std::invalid_argument("Missing HDR EXR contribution channels");
}
}
size_t detail::emission_exr_decoded_bytes(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l){
 limits(l);if(bytes.empty()||bytes.size()>l.max_encoded_bytes)throw std::length_error("HDR EXR capture exceeds encoded byte budget");return preflight(bytes,l);
}
void EmissionInterchangeImage::validate(const EmissionInterchangeLimits &l)const{
 const auto bytes=measure(width,height,l);if(pixels.size()!=bytes/sizeof(pixels[0])||bytes>l.max_working_bytes)throw std::length_error("HDR emission image storage exceeds limits");
 for(size_t i=0;i<pixels.size();++i){if((i&4095)==0)stop(l);const auto &p=pixels[i];for(float v:p)if(!std::isfinite(v)||v<0)throw std::invalid_argument("Invalid HDR emission sample");if(p[9]>1)throw std::invalid_argument("Invalid HDR emission coverage");for(size_t c=0;c<3;++c){if(p[c]>3*maximum_native_emission)throw std::invalid_argument("HDR resolved radiance exceeds native bound");if((p[9]==0&&(p[3+c]!=0||p[6+c]!=0))||p[3+c]>p[9]*(1+4e-6f)||p[6+c]>maximum_native_emission*p[9]*(1+4e-6f))throw std::invalid_argument("HDR emission coefficients are not premultiplied");}}
}
std::vector<uint8_t> encode_emission_exr(const EmissionInterchangeImage &image,const EmissionInterchangeLimits &l){
 image.validate(l);const auto raw=image.pixels.size()*sizeof(image.pixels[0]);const size_t forecast=raw+raw/50+1024*1024;
 if(forecast>l.max_encoded_bytes||raw>l.max_working_bytes-std::min(l.max_working_bytes,forecast))throw std::length_error("HDR EXR encode forecast exceeds working/output budgets");
 Output stream(l,forecast);exr::Header header(int(image.width),int(image.height));header.compression()=exr::ZIP_COMPRESSION;header.insert("domeSculptEmissionVersion",exr::IntAttribute(1));header.insert("domeSculptEmissionUnits",exr::StringAttribute("linear-cd/m2;legacy-relative-premult;physical-absolute-premult;coverage-linear"));header.insert("domeSculptEmissionCoordinates",exr::StringAttribute("row0=v0"));
 exr::FrameBuffer frame;for(size_t c=0;c<channels.size();++c){header.channels().insert(channels[c],exr::Channel(exr::FLOAT));frame.insert(channels[c],exr::Slice(exr::FLOAT,reinterpret_cast<char*>(const_cast<float*>(image.pixels[0].data()+c)),sizeof(image.pixels[0]),sizeof(image.pixels[0])*image.width));}
 {exr::OutputFile file(stream,header,0);file.setFrameBuffer(frame);for(uint32_t y=0;y<image.height;y+=16){stop(l);file.writePixels(int(std::min(16u,image.height-y)));}}
 stop(l);return std::move(stream.bytes);
}
EmissionInterchangeImage decode_emission_exr(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l){
 limits(l);if(bytes.empty()||bytes.size()>l.max_encoded_bytes)throw std::length_error("HDR EXR capture exceeds encoded byte budget");
 // Preflight the bounded header before constructing the high-level reader's
 // chunk-offset structures. Implemented separately from the pixel decoder.
 preflight(bytes,l);Input stream(bytes,l);exr::InputFile file(stream,0);check_header(file.header());const auto window=file.header().dataWindow();
 if(window.max.x<0||window.max.y<0)throw std::invalid_argument("Invalid HDR emission data window");EmissionInterchangeImage image;image.width=uint32_t(window.max.x)+1;image.height=uint32_t(window.max.y)+1;const auto raw=measure(image.width,image.height,l);if(bytes.size()>l.max_working_bytes||raw>l.max_working_bytes-bytes.size())throw std::length_error("HDR EXR decode forecast exceeds working budget");image.pixels.resize(raw/sizeof(image.pixels[0]));exr::FrameBuffer frame;for(size_t c=0;c<channels.size();++c)frame.insert(channels[c],exr::Slice(exr::FLOAT,reinterpret_cast<char*>(image.pixels[0].data()+c),sizeof(image.pixels[0]),sizeof(image.pixels[0])*image.width));file.setFrameBuffer(frame);for(uint32_t y=0;y<image.height;y+=16){stop(l);file.readPixels(int(y),int(std::min(image.height-1,y+15)));}image.validate(l);return image;
}
}
