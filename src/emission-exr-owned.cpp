// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-exr-owned.hpp"
#include <OpenEXR/openexr.h>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <string_view>
namespace dome::detail {namespace {
constexpr const char *channels[]{"R","G","B","paint.legacy.R","paint.legacy.G","paint.legacy.B","paint.physical.R","paint.physical.G","paint.physical.B","paint.coverage"};
void stop(const EmissionInterchangeLimits &l){if(l.cancelled&&l.cancelled())throw std::runtime_error("HDR emission interchange cancelled");}
void limits(const EmissionInterchangeLimits &l){if(!l.max_dimension||l.max_dimension>8192||!l.max_pixels||l.max_pixels>16*1024*1024||!l.max_encoded_bytes||l.max_encoded_bytes>256*1024*1024||!l.max_working_bytes||l.max_working_bytes>512*1024*1024)throw std::invalid_argument("Invalid HDR interchange limits");stop(l);}
size_t measure(uint32_t w,uint32_t h,const EmissionInterchangeLimits &l){limits(l);if(!w||!h||w>l.max_dimension||h>l.max_dimension||size_t(w)*h>l.max_pixels)throw std::length_error("HDR emission dimensions exceed limits");return size_t(w)*h*sizeof(std::array<float,10>);}
struct State;
thread_local State *active=nullptr;
struct alignas(std::max_align_t) Block {Block *next,*previous;size_t bytes;};
struct State {
 const EmissionInterchangeLimits &limits;EmissionExrMemory provider;
 Block *head=nullptr;size_t live=0,budget=0;bool out_of_memory=false,working=false,encoded=false,bad_free=false;
 std::exception_ptr exception;std::span<const uint8_t> input;std::vector<uint8_t> output;
 State *previous=nullptr;char message[256]{};
 State(const EmissionInterchangeLimits &l,EmissionExrMemory p):limits(l),provider(p),budget(l.max_working_bytes){if(bool(p.allocate)!=bool(p.release))throw std::invalid_argument("Incomplete owning EXR allocator");}
 ~State(){while(head)release(head);}
 void release(Block *b)noexcept{if(b->previous)b->previous->next=b->next;else head=b->next;if(b->next)b->next->previous=b->previous;live-=b->bytes;if(provider.release)provider.release(provider.state,b);else std::free(b);}
 void reduce(size_t cap){if(live>cap)throw std::length_error("Owned HDR codec exceeds aggregate working budget");budget=cap;}
 void check(exr_result_t result=EXR_ERR_SUCCESS){if(exception)std::rethrow_exception(exception);if(working)throw std::length_error("Owned HDR codec exceeds working budget");if(encoded)throw std::length_error("HDR EXR output byte budget exceeded");if(out_of_memory)throw std::bad_alloc();if(bad_free)throw std::logic_error("Owned HDR codec released foreign allocation");if(result!=EXR_ERR_SUCCESS)throw std::invalid_argument(message[0]?message:"Invalid owned HDR codec operation");stop(limits);}
};
struct Scope {State &state;explicit Scope(State &s):state(s){s.previous=active;active=&s;}~Scope(){active=state.previous;}};
void *allocate(size_t n)noexcept{
 auto *s=active;if(!s)return nullptr;if(n>SIZE_MAX-sizeof(Block)||s->live>s->budget||n+sizeof(Block)>s->budget-s->live){s->working=true;return nullptr;}
 const auto total=n+sizeof(Block);void *p=s->provider.allocate?s->provider.allocate(s->provider.state,total):std::malloc(total);if(!p){s->out_of_memory=true;return nullptr;}
 auto *b=static_cast<Block*>(p);b->next=s->head;b->previous=nullptr;b->bytes=total;if(b->next)b->next->previous=b;s->head=b;s->live+=total;return b+1;
}
void release(void *p)noexcept{if(!p)return;for(auto *s=active;s;s=s->previous)for(auto *b=s->head;b;b=b->next)if(b+1==p){s->release(b);return;}if(active)active->bad_free=true;}
void error(exr_const_context_t,exr_result_t,const char *text){if(active&&!active->message[0]&&text){const auto n=std::min(std::strlen(text),sizeof(active->message)-1);std::memcpy(active->message,text,n);active->message[n]=0;}}
int64_t read(exr_const_context_t,void *data,void *buffer,uint64_t n,uint64_t offset,exr_stream_error_func_ptr_t){auto &s=*static_cast<State*>(data);try{stop(s.limits);if(offset>=s.input.size())return 0;const auto count=std::min<uint64_t>(n,s.input.size()-size_t(offset));std::memcpy(buffer,s.input.data()+size_t(offset),size_t(count));return int64_t(count);}catch(...){if(!s.exception)s.exception=std::current_exception();return -1;}}
int64_t size(exr_const_context_t,void *data){return int64_t(static_cast<State*>(data)->input.size());}
int64_t write(exr_const_context_t,void *data,const void *buffer,uint64_t n,uint64_t offset,exr_stream_error_func_ptr_t){auto &s=*static_cast<State*>(data);try{stop(s.limits);if(offset>s.limits.max_encoded_bytes||n>s.limits.max_encoded_bytes-size_t(offset)){s.encoded=true;return -1;}const auto required=size_t(offset+n);if(required>s.output.capacity()){s.working=true;return -1;}if(required>s.output.size())s.output.resize(required);std::memcpy(s.output.data()+size_t(offset),buffer,size_t(n));return int64_t(n);}catch(...){if(!s.exception)s.exception=std::current_exception();return -1;}}
struct Context {
 State &state;exr_context_t value=nullptr;
 explicit Context(State &s):state(s){}
 ~Context(){if(value)exr_finish(&value);}
 exr_context_initializer_t initializer(bool writing){exr_context_initializer_t i=EXR_DEFAULT_CONTEXT_INITIALIZER;i.alloc_fn=allocate;i.free_fn=release;i.error_handler_fn=error;i.user_data=&state;i.max_image_width=int(state.limits.max_dimension);i.max_image_height=int(state.limits.max_dimension);if(writing){i.flags=EXR_CONTEXT_FLAG_WRITE_LEGACY_HEADER;i.write_fn=write;}else{i.flags=EXR_CONTEXT_FLAG_STRICT_HEADER|EXR_CONTEXT_FLAG_DISABLE_CHUNK_RECONSTRUCTION;i.read_fn=read;i.size_fn=size;}return i;}
 void open(bool writing){auto i=initializer(writing);state.check(writing?exr_start_write(&value,"native-emission.exr",EXR_WRITE_FILE_DIRECTLY,&i):exr_start_read(&value,"native-emission.exr",&i));}
 void close(){const auto result=exr_finish(&value);state.check(result);}
};
struct Encoding {exr_context_t context;exr_encode_pipeline_t value=EXR_ENCODE_PIPELINE_INITIALIZER;explicit Encoding(exr_context_t c):context(c){}~Encoding(){exr_encoding_destroy(context,&value);}};
struct Decoding {exr_context_t context;exr_decode_pipeline_t value=EXR_DECODE_PIPELINE_INITIALIZER;explicit Decoding(exr_context_t c):context(c){}~Decoding(){exr_decoding_destroy(context,&value);}};
size_t channel(const char *name){if(!name)throw std::invalid_argument("Missing HDR FLOAT channel");for(size_t i=0;i<10;++i)if(!std::strcmp(name,channels[i]))return i;throw std::invalid_argument("Unknown HDR FLOAT channel");}
std::pair<uint32_t,uint32_t> header(Context &c){
 auto &s=c.state;int count=0;s.check(exr_get_count(c.value,&count));if(count!=1)throw std::invalid_argument("HDR EXR must contain exactly one image part");exr_storage_t storage;s.check(exr_get_storage(c.value,0,&storage));exr_compression_t compression;s.check(exr_get_compression(c.value,0,&compression));if(storage!=EXR_STORAGE_SCANLINE||compression!=EXR_COMPRESSION_ZIP)throw std::invalid_argument("HDR EXR requires lossless ZIP scanlines");
 exr_attr_box2i_t box,display;s.check(exr_get_data_window(c.value,0,&box));s.check(exr_get_display_window(c.value,0,&display));exr_lineorder_t order;s.check(exr_get_lineorder(c.value,0,&order));if(box.min.x||box.min.y||box.max.x<0||box.max.y<0||display.min.x!=box.min.x||display.min.y!=box.min.y||display.max.x!=box.max.x||display.max.y!=box.max.y||order!=EXR_LINEORDER_INCREASING_Y)throw std::invalid_argument("Unsupported HDR emission EXR layout");
 int32_t version=0;s.check(exr_attr_get_int(c.value,0,"domeSculptEmissionVersion",&version));if(version!=1)throw std::invalid_argument("Unknown HDR emission EXR interpretation");
 auto text=[&](const char *name,std::string_view expected){int32_t length=0;const char *value=nullptr;s.check(exr_attr_get_string(c.value,0,name,&length,&value));if(!value||length<0||std::string_view(value,size_t(length))!=expected)throw std::invalid_argument("Unknown HDR emission EXR interpretation");};text("domeSculptEmissionUnits","linear-cd/m2;legacy-relative-premult;physical-absolute-premult;coverage-linear");text("domeSculptEmissionCoordinates","row0=v0");const auto w=uint32_t(box.max.x)+1,h=uint32_t(box.max.y)+1;measure(w,h,s.limits);const exr_attr_chlist_t *list=nullptr;s.check(exr_get_channels(c.value,0,&list));if(!list||list->num_channels!=10)throw std::invalid_argument("HDR EXR must contain ten named FLOAT channels");unsigned seen=0;for(int i=0;i<10;++i){const auto &entry=list->entries[i];if(entry.pixel_type!=EXR_PIXEL_FLOAT||entry.x_sampling!=1||entry.y_sampling!=1)throw std::invalid_argument("Invalid HDR EXR channel interpretation");const auto n=channel(entry.name.str);if(seen&(1u<<n))throw std::invalid_argument("Repeated HDR FLOAT channel");seen|=1u<<n;}return {w,h};
}
void chunk(const exr_chunk_info_t &c,uint32_t w,uint32_t h,int y){if(c.start_x!=0||c.start_y!=y||c.width!=int(w)||c.height<=0||uint32_t(c.height)>h-uint32_t(y))throw std::invalid_argument("Invalid HDR scanline chunk extent");}
}
std::vector<uint8_t> encode_emission_exr_owned(const EmissionInterchangeImage &image,const EmissionInterchangeLimits &l,EmissionExrMemory provider){
 image.validate(l);const auto raw=measure(image.width,image.height,l),forecast=raw+raw/50+1024*1024;if(raw>l.max_working_bytes-std::min(l.max_working_bytes,forecast))throw std::length_error("HDR EXR encode forecast exceeds working/output budgets");State s(l,provider);s.output.reserve(std::min(forecast,l.max_encoded_bytes));s.reduce(l.max_working_bytes-raw-s.output.capacity());Scope scope(s);Context c(s);c.open(true);int part=-1;s.check(exr_add_part(c.value,nullptr,EXR_STORAGE_SCANLINE,&part));s.check(exr_initialize_required_attr_simple(c.value,part,int(image.width),int(image.height),EXR_COMPRESSION_ZIP));for(auto name:channels)s.check(exr_result_t(exr_add_channel(c.value,part,name,EXR_PIXEL_FLOAT,EXR_PERCEPTUALLY_LOGARITHMIC,1,1)));
 s.check(exr_attr_set_int(c.value,part,"domeSculptEmissionVersion",1));s.check(exr_attr_set_string(c.value,part,"domeSculptEmissionUnits","linear-cd/m2;legacy-relative-premult;physical-absolute-premult;coverage-linear"));s.check(exr_attr_set_string(c.value,part,"domeSculptEmissionCoordinates","row0=v0"));s.check(exr_write_header(c.value));
 {Encoding pipe(c.value);bool first=true;for(int y=0;y<int(image.height);){exr_chunk_info_t info{};s.check(exr_write_scanline_chunk_info(c.value,part,y,&info));chunk(info,image.width,image.height,y);s.check(first?exr_encoding_initialize(c.value,part,&info,&pipe.value):exr_encoding_update(c.value,part,&info,&pipe.value));for(int n=0;n<pipe.value.channel_count;++n){auto &entry=pipe.value.channels[n];const auto k=channel(entry.channel_name);entry.user_bytes_per_element=4;entry.user_data_type=EXR_PIXEL_FLOAT;entry.user_pixel_stride=40;entry.user_line_stride=int(image.width*40);entry.encode_from_ptr=reinterpret_cast<const uint8_t*>(&image.pixels[size_t(y)*image.width][k]);}if(first)s.check(exr_encoding_choose_default_routines(c.value,part,&pipe.value));s.check(exr_encoding_run(c.value,part,&pipe.value));first=false;y+=info.height;}}
 c.close();s.check();return std::move(s.output);
}
size_t emission_exr_owned_decoded_bytes(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l,EmissionExrMemory provider){limits(l);if(bytes.empty()||bytes.size()>l.max_encoded_bytes||bytes.size()>l.max_working_bytes)throw std::length_error("HDR EXR capture exceeds encoded byte budget");State s(l,provider);s.input=bytes;s.reduce(std::min(l.max_working_bytes-bytes.size(),size_t(1024*1024)));Scope scope(s);Context c(s);c.open(false);const auto [w,h]=header(c);const auto raw=measure(w,h,l);if(raw>l.max_working_bytes-bytes.size())throw std::length_error("HDR EXR decode forecast exceeds working budget");c.close();return raw;}
EmissionInterchangeImage decode_emission_exr_owned(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l,EmissionExrMemory provider){
 limits(l);if(bytes.empty()||bytes.size()>l.max_encoded_bytes||bytes.size()>l.max_working_bytes)throw std::length_error("HDR EXR capture exceeds encoded byte budget");State s(l,provider);s.input=bytes;s.reduce(std::min(l.max_working_bytes-bytes.size(),size_t(1024*1024)));Scope scope(s);Context c(s);c.open(false);const auto [w,h]=header(c);const auto raw=measure(w,h,l);if(raw>l.max_working_bytes-bytes.size())throw std::length_error("HDR EXR decode forecast exceeds working budget");s.reduce(l.max_working_bytes-bytes.size()-raw);EmissionInterchangeImage image;image.width=w;image.height=h;image.pixels.resize(size_t(w)*h);
 {Decoding pipe(c.value);bool first=true;for(int y=0;y<int(h);){exr_chunk_info_t info{};s.check(exr_read_scanline_chunk_info(c.value,0,y,&info));chunk(info,w,h,y);s.check(first?exr_decoding_initialize(c.value,0,&info,&pipe.value):exr_decoding_update(c.value,0,&info,&pipe.value));for(int n=0;n<pipe.value.channel_count;++n){auto &entry=pipe.value.channels[n];const auto k=channel(entry.channel_name);entry.user_bytes_per_element=4;entry.user_data_type=EXR_PIXEL_FLOAT;entry.user_pixel_stride=40;entry.user_line_stride=int(w*40);entry.decode_to_ptr=reinterpret_cast<uint8_t*>(&image.pixels[size_t(y)*w][k]);}if(first)s.check(exr_decoding_choose_default_routines(c.value,0,&pipe.value));s.check(exr_decoding_run(c.value,0,&pipe.value));first=false;y+=info.height;}}
 c.close();image.validate(l);return image;
}
}
