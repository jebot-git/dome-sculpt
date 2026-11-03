// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/emission-interchange.hpp"
#include <OpenEXR/openexr.h>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool v,const char *message){if(!v)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
bool refuses(auto f){try{f();}catch(const std::exception&){return true;}return false;}
void core(exr_result_t r){if(r!=EXR_ERR_SUCCESS)throw std::runtime_error("Independent OpenEXRCore decode failed");}
std::vector<std::array<float,10>> independent(const std::filesystem::path &path,uint32_t width,uint32_t height){
 const char* names[]{"R","G","B","paint.legacy.R","paint.legacy.G","paint.legacy.B","paint.physical.R","paint.physical.G","paint.physical.B","paint.coverage"};
 exr_context_t context=nullptr;exr_decode_pipeline_t decoder=EXR_DECODE_PIPELINE_INITIALIZER;bool ready=false;
 try{core(exr_start_read(&context,path.c_str(),nullptr));std::vector<std::array<float,10>> pixels(size_t(width)*height);for(uint32_t y=0;y<height;){exr_chunk_info_t chunk{};core(exr_read_scanline_chunk_info(context,0,int(y),&chunk));if(!ready){core(exr_decoding_initialize(context,0,&chunk,&decoder));ready=true;}else core(exr_decoding_update(context,0,&chunk,&decoder));
   for(int n=0;n<decoder.channel_count;++n){auto &channel=decoder.channels[n];size_t c=0;while(c<10&&std::strcmp(names[c],channel.channel_name))++c;if(c==10)throw std::runtime_error("Unknown independent EXR channel");channel.user_data_type=EXR_PIXEL_FLOAT;channel.user_bytes_per_element=4;channel.user_pixel_stride=sizeof(pixels[0]);channel.user_line_stride=int(width*sizeof(pixels[0]));channel.decode_to_ptr=reinterpret_cast<uint8_t*>(pixels[size_t(chunk.start_y)*width].data()+c);}
   core(exr_decoding_choose_default_routines(context,0,&decoder));core(exr_decoding_run(context,0,&decoder));y+=uint32_t(chunk.height);
  }if(ready)core(exr_decoding_destroy(context,&decoder));core(exr_finish(&context));return pixels;
 }catch(...){if(ready)exr_decoding_destroy(context,&decoder);if(context)exr_finish(&context);throw;}
}
bool exact(const auto&a,const auto&b){return a.size()==b.size()&&std::memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))==0;}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);EmissionInterchangeImage image;image.width=65;image.height=33;image.pixels.resize(size_t(image.width)*image.height);
 for(size_t i=0;i<image.pixels.size();++i){auto &p=image.pixels[i];p[9]=float(i%17)/16;for(size_t c=0;c<3;++c){p[3+c]=float(c+1)/3*p[9];p[6+c]=(c==0?10000.f:c==1?.005f:.0003f)*p[9];p[c]=.25f*(1-p[9])+p[3+c]*1.3f+p[6+c];}}
 image.pixels[0][3]=-0.f;image.pixels[0][6]=-0.f;const auto encoded=encode_emission_exr(image);auto restored=decode_emission_exr(encoded);check(image.width==restored.width&&image.height==restored.height&&exact(image.pixels,restored.pixels),"ZIP FLOAT32 native EXR preserves every coefficient/resolved/signed-zero bit");auto path=std::filesystem::path(argv[1])/"actual-emission.exr";std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());out.close();auto external=independent(path,image.width,image.height);check(exact(image.pixels,external),"Separate OpenEXRCore channel/chunk decoder independently reconstructs every FLOAT32 sample");
 check(external[1][6]>0&&external[1][7]>0&&external[1][8]>0&&external[1][6]/external[1][7]>1'000'000,"Independent decoder retains tiny and bright physical values in the same texture");
 auto invalid=image;invalid.pixels[0][9]=0;invalid.pixels[0][6]=.005f;check(refuses([&]{encode_emission_exr(invalid);}),"Uncovered physical contribution cannot masquerade as premultiplied data");invalid=image;invalid.pixels[1][0]=INFINITY;check(refuses([&]{encode_emission_exr(invalid);}),"Nonfinite radiance refuses before codec allocation");
 auto limited=EmissionInterchangeLimits{};limited.max_working_bytes=1;check(refuses([&]{encode_emission_exr(image,limited);})&&refuses([&]{decode_emission_exr(encoded,limited);}),"Independent encode/read working forecasts refuse bounded memory before bulk image allocation");limited={};limited.max_dimension=32;check(refuses([&]{decode_emission_exr(encoded,limited);}),"Reader caps image dimensions during low-level header parsing");limited={};limited.max_encoded_bytes=encoded.size()-1;check(refuses([&]{decode_emission_exr(encoded,limited);}),"Immutable encoded-byte budget is enforced before parsing");
 auto corrupt=encoded;corrupt.resize(corrupt.size()/2);check(refuses([&]{decode_emission_exr(corrupt);}),"Truncated compressed data refuses instead of returning partial HDR pixels");
 auto cancellation=EmissionInterchangeLimits{};int polls=0;cancellation.cancelled=[&]{++polls;return false;};encode_emission_exr(image,cancellation);check(polls>4,"Encoder polls cancellation through validation and compressed stream writes");for(int point:{1,polls/2,polls}){int n=0;auto cancel=EmissionInterchangeLimits{};cancel.cancelled=[&]{return ++n>=point;};check(refuses([&]{encode_emission_exr(image,cancel);}),"Cancellation at an actually measured encoder boundary refuses atomically");}
 auto again=decode_emission_exr(encode_emission_exr(restored));check(exact(image.pixels,again.pixels),"Repeated exact EXR generation has no float precision decay");std::cout<<"Emission EXR: "<<checks<<" checks bytes="<<encoded.size()<<" polls="<<polls<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
