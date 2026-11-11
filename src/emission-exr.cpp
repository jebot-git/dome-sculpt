// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/emission-interchange.hpp"
#include "dome/baked-emission.hpp"
#include "emission-exr-access.hpp"
#include "emission-exr-owned.hpp"
namespace dome {namespace {
void stop(const EmissionInterchangeLimits &l){if(l.cancelled&&l.cancelled())throw std::runtime_error("HDR emission interchange cancelled");}
size_t measure(uint32_t width,uint32_t height,const EmissionInterchangeLimits &l){if(!l.max_dimension||l.max_dimension>8192||!l.max_pixels||l.max_pixels>16*1024*1024||!l.max_encoded_bytes||l.max_encoded_bytes>256*1024*1024||!l.max_working_bytes||l.max_working_bytes>512*1024*1024)throw std::invalid_argument("Invalid HDR interchange limits");stop(l);if(!width||!height||width>l.max_dimension||height>l.max_dimension||size_t(width)*height>l.max_pixels)throw std::length_error("HDR emission dimensions exceed limits");return size_t(width)*height*sizeof(std::array<float,10>);}
}
void EmissionInterchangeImage::validate(const EmissionInterchangeLimits &l)const{
 if(version!=1&&version!=2)throw std::invalid_argument("Unsupported HDR emission field interpretation");
 const auto bytes=measure(width,height,l);if(pixels.size()!=bytes/sizeof(pixels[0])||bytes>l.max_working_bytes)throw std::length_error("HDR emission image storage exceeds limits");
 for(size_t i=0;i<pixels.size();++i){if((i&4095)==0)stop(l);const auto &p=pixels[i];for(float v:p)if(!std::isfinite(v)||v<0)throw std::invalid_argument("Invalid HDR emission sample");if(p[9]>1)throw std::invalid_argument("Invalid HDR emission coverage");if(version==2){BakedEmissionSample{{p[3],p[4],p[5]},{p[6],p[7],p[8]},p[9]}.validate();continue;}for(size_t c=0;c<3;++c){if(p[c]>3*maximum_native_emission)throw std::invalid_argument("HDR resolved radiance exceeds native bound");if((p[9]==0&&(p[3+c]!=0||p[6+c]!=0))||p[3+c]>p[9]*(1+4e-6f)||p[6+c]>maximum_native_emission*p[9]*(1+4e-6f))throw std::invalid_argument("HDR emission coefficients are not premultiplied");}}
}
size_t detail::emission_exr_decoded_bytes(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l){return emission_exr_owned_decoded_bytes(bytes,l);}
std::vector<uint8_t> encode_emission_exr(const EmissionInterchangeImage &image,const EmissionInterchangeLimits &l){return detail::encode_emission_exr_owned(image,l);}
EmissionInterchangeImage decode_emission_exr(std::span<const uint8_t> bytes,const EmissionInterchangeLimits &l){return detail::decode_emission_exr_owned(bytes,l);}
}
