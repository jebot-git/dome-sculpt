// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-decoded-images.hpp"
#include <map>
namespace dome::detail {
std::shared_ptr<const DecodedSourceImages> decode_source_images(
    std::span<const ImportedImage> source,size_t maximum_pixels,
    const std::function<bool()> &cancelled){
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Imported image decoding cancelled");};
  stop();
  if(!maximum_pixels||maximum_pixels>64*1024*1024||source.size()>256)
    throw std::length_error("Imported image count/pixel budget exceeded");
  size_t encoded_bytes=0;
  for(const auto &encoded:source){stop();
    if(encoded.bytes.size()>128*1024*1024 || encoded.bytes.size()>256*1024*1024-encoded_bytes)
      throw std::length_error("Imported encoded image budget exceeded");
    encoded_bytes+=encoded.bytes.size();
  }
  auto images=std::make_shared<DecodedSourceImages>();
  images->reserve(source.size());
  std::map<std::pair<uint64_t,size_t>,std::vector<size_t>> buckets;
  size_t owned_pixels=0;
  for(size_t index=0;index<source.size();++index){
    stop();const auto &encoded=source[index];
    // Validate every record's MIME/header/dimensions, including aliases. The
    // actual decoder validates complete data before a unique leaf is stored.
    const auto size=imported_image_dimensions(encoded,maximum_pixels);
    const size_t pixels=size_t(size[0])*size[1];
    uint64_t hash=14695981039346656037ull;
    for(size_t byte=0;byte<encoded.bytes.size();++byte){
      if((byte&65535)==0)stop();
      hash=(hash^encoded.bytes[byte])*1099511628211ull;
    }
    auto &candidates=buckets[{hash,encoded.bytes.size()}];
    std::shared_ptr<const Image> image;
    for(auto candidate:candidates){
      stop();const auto &other=source[candidate];
      if(other.mime_type==encoded.mime_type && other.bytes==encoded.bytes){
        image=images->at(candidate);break;
      }
    }
    if(!image){
      if(pixels>maximum_pixels-owned_pixels)
        throw std::length_error("Imported unique decoded image budget exceeded");
      // Check/charge the complete distinct resource before its allocation.
      owned_pixels+=pixels;
      image=std::make_shared<const Image>(decode_import_image(
          encoded,maximum_pixels-(owned_pixels-pixels),cancelled));
      candidates.push_back(index);
    }
    images->push_back(std::move(image));
  }
  stop();return images;
}
} // namespace dome::detail
