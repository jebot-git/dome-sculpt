// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-image.hpp"
#include "dome/projection.hpp"
#include "import-jpeg.h"
#include <cstdlib>
namespace dome {
namespace {
struct Cancellation {
  const std::function<bool()> &callback;
  std::exception_ptr failure;
  static int check(void *value) noexcept {
    auto &c=*static_cast<Cancellation*>(value);
    try{return c.callback&&c.callback();}catch(...){c.failure=std::current_exception();return 1;}
  }
};
}
std::array<uint32_t,2> imported_image_dimensions(const ImportedImage &encoded,size_t remaining_pixels) {
  if(!remaining_pixels || remaining_pixels>64*1024*1024 || encoded.bytes.size()>128*1024*1024)
    throw std::length_error("Imported image byte/pixel budget exceeded");
  if(encoded.mime_type=="image/png"){
    const auto &b=encoded.bytes;constexpr uint8_t magic[]={137,80,78,71,13,10,26,10};
    if(b.size()<33 || !std::equal(std::begin(magic),std::end(magic),b.begin()) || b[12]!='I'||b[13]!='H'||b[14]!='D'||b[15]!='R')
      throw std::runtime_error("Invalid imported PNG header");
    auto big=[&](size_t at){return uint32_t(b[at])<<24|uint32_t(b[at+1])<<16|uint32_t(b[at+2])<<8|b[at+3];};
    auto width=big(16),height=big(20);
    if(!width||!height||width>8192||height>8192||height>remaining_pixels/width)
      throw std::length_error("Imported PNG dimensions exceed remaining pixel budget");
    return {width,height};
  }
  if(encoded.mime_type!="image/jpeg")throw std::runtime_error("Imported image requires glTF core PNG or JPEG");
  unsigned width=0,height=0;char message[256]{};
  if(!dome_import_jpeg(encoded.bytes.data(),encoded.bytes.size(),remaining_pixels,nullptr,&width,&height,message,sizeof(message),nullptr,nullptr))
    throw std::runtime_error(std::string("Cannot inspect imported JPEG: ")+message);
  return {width,height};
}
Image decode_import_image(const ImportedImage &encoded,size_t remaining_pixels,const std::function<bool()> &cancelled) {
  Cancellation cancel{cancelled,{}};
  if(Cancellation::check(&cancel)){if(cancel.failure)std::rethrow_exception(cancel.failure);throw std::runtime_error("Image import cancelled");}
  if(!remaining_pixels || remaining_pixels>64*1024*1024 || encoded.bytes.size()>128*1024*1024)
    throw std::length_error("Imported image byte/pixel budget exceeded");
  if(encoded.mime_type=="image/png") {
    (void)imported_image_dimensions(encoded,remaining_pixels);
    auto image=decode_projection_png(encoded.bytes);
    if(size_t(image.width)*image.height>remaining_pixels)
      throw std::length_error("Imported decoded image budget exceeded");
    return image;
  }
  if(encoded.mime_type!="image/jpeg")throw std::runtime_error("Imported image requires glTF core PNG or JPEG");
  uint8_t *pixels=nullptr;unsigned width=0,height=0;char message[256]{};
  if(!dome_import_jpeg(encoded.bytes.data(),encoded.bytes.size(),remaining_pixels,&pixels,&width,&height,message,sizeof(message),Cancellation::check,&cancel)){
    if(cancel.failure)std::rethrow_exception(cancel.failure);
    throw std::runtime_error(std::string("Cannot decode imported JPEG: ")+message);
  }
  std::unique_ptr<uint8_t,decltype(&std::free)> owner(pixels,std::free);
  Image image{width,height,std::vector<uint8_t>(pixels,pixels+size_t(width)*height*4)};
  image.validate();return image;
}
} // namespace dome
