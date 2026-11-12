// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/emission-interchange.hpp"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
size_t checks=0;
void check(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
template<class F>void refuses(F f,const char *s){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,s);}
}
int main()try{
  using namespace dome;
  EmissionInterchangeImage image;image.width=8;image.height=8;image.version=2;image.pixels.resize(64);
  const float largest=std::numeric_limits<float>::max(),smallest=std::numeric_limits<float>::denorm_min(),tiny=0x1p-24f;
  image.pixels[0]={largest,smallest,50000,0,0,0,largest,smallest,50000,1};
  image.pixels[1]={50000*tiny,2*tiny,smallest,0,0,0,50000*tiny,2*tiny,smallest,tiny};
  image.pixels[2]={20000,1,4,.25f,.5f,0,20000,1,4,.5f};
  auto bytes=encode_emission_exr(image);auto restored=decode_emission_exr(bytes);
  check(restored.version==2&&restored.width==8&&restored.height==8,"actual EXR header preserves resolved FLOAT interpretation and extent");
  for(size_t i=0;i<64;++i)for(size_t k=0;k<10;++k)check(std::bit_cast<uint32_t>(image.pixels[i][k])==std::bit_cast<uint32_t>(restored.pixels[i][k]),"true FLOAT32 lossless roundtrip including max/subnormal/tiny independent coverage");
  check(encode_emission_exr(restored)==bytes,"resolved EXR is byte stable through decode/encode");
  auto legacy=image;legacy.version=1;refuses([&]{encode_emission_exr(legacy);},"historical native-paint bounds are not silently reinterpreted");
  auto invalid=image;invalid.version=3;refuses([&]{encode_emission_exr(invalid);},"unsupported EXR interpretation refuses");
  invalid=image;invalid.pixels[0][9]=0;refuses([&]{encode_emission_exr(invalid);},"zero coverage cannot hide resolved base contributions");
  invalid=image;invalid.pixels[0][0]=std::numeric_limits<float>::infinity();refuses([&]{encode_emission_exr(invalid);},"infinite resolved FLOAT cannot publish");
  EmissionInterchangeLimits limits;limits.cancelled=[]{return true;};refuses([&]{encode_emission_exr(image,limits);},"resolved encode cancellation");
  refuses([&]{decode_emission_exr(bytes,limits);},"resolved decode cancellation");
  std::cout<<"Baked emission EXR "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<"Baked emission EXR FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
