// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <OpenEXR/openexr.h>
#include <array>
#include <vector>
#include <cstring>
#include <filesystem>
#include <stdexcept>
namespace emission_core_consumer {
inline void core(exr_result_t r){if(r!=EXR_ERR_SUCCESS)throw std::runtime_error("Independent OpenEXRCore decode failed");}
inline std::vector<std::array<float,10>> read(const std::filesystem::path &path,uint32_t width,uint32_t height){
 const char* names[]{"R","G","B","paint.legacy.R","paint.legacy.G","paint.legacy.B","paint.physical.R","paint.physical.G","paint.physical.B","paint.coverage"};
 exr_context_t context=nullptr;exr_decode_pipeline_t decoder=EXR_DECODE_PIPELINE_INITIALIZER;bool ready=false;
 try{core(exr_start_read(&context,path.c_str(),nullptr));std::vector<std::array<float,10>> pixels(size_t(width)*height);for(uint32_t y=0;y<height;){exr_chunk_info_t chunk{};core(exr_read_scanline_chunk_info(context,0,int(y),&chunk));if(!ready){core(exr_decoding_initialize(context,0,&chunk,&decoder));ready=true;}else core(exr_decoding_update(context,0,&chunk,&decoder));
   for(int n=0;n<decoder.channel_count;++n){auto &channel=decoder.channels[n];size_t c=0;while(c<10&&std::strcmp(names[c],channel.channel_name))++c;if(c==10)throw std::runtime_error("Unknown independent EXR channel");channel.user_data_type=EXR_PIXEL_FLOAT;channel.user_bytes_per_element=4;channel.user_pixel_stride=sizeof(pixels[0]);channel.user_line_stride=int(width*sizeof(pixels[0]));channel.decode_to_ptr=reinterpret_cast<uint8_t*>(pixels[size_t(chunk.start_y)*width].data()+c);}
   core(exr_decoding_choose_default_routines(context,0,&decoder));core(exr_decoding_run(context,0,&decoder));y+=uint32_t(chunk.height);
  }if(ready)core(exr_decoding_destroy(context,&decoder));core(exr_finish(&context));return pixels;
 }catch(...){if(ready)exr_decoding_destroy(context,&decoder);if(context)exr_finish(&context);throw;}
}
}
