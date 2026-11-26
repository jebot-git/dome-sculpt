// SPDX-License-Identifier: GPL-3.0-or-later
// Included after the native fixture's check() helper. cgltf owns the independent
// glTF structure; libpng decodes the emitted bytes directly, outside Dome's importer.
float consumer_linear(float byte){return byte<=.04045f?byte/12.92f:std::pow((byte+.055f)/1.055f,2.4f);}
Image consumer_png(const cgltf_texture*texture){
 if(!texture||!texture->image||!texture->image->buffer_view)throw std::runtime_error("Export colour texture has no embedded image");
 const auto*view=texture->image->buffer_view;if(!view->buffer||!view->buffer->data)throw std::runtime_error("Export image buffer is absent");
 png_image png{};png.version=PNG_IMAGE_VERSION;
 struct Guard{png_image*p;~Guard(){png_image_free(p);}}guard{&png};
 if(!png_image_begin_read_from_memory(&png,static_cast<const uint8_t*>(view->buffer->data)+view->offset,view->size))throw std::runtime_error("Independent PNG header decode failed");
 if(!png.width||!png.height||uint64_t(png.width)*png.height>64*1024*1024)throw std::runtime_error("Export image exceeds native fixture pixel budget");
 png.format=PNG_FORMAT_RGBA;Image out;out.width=png.width;out.height=png.height;out.rgba.resize(PNG_IMAGE_SIZE(png));
 if(!png_image_finish_read(&png,nullptr,out.rgba.data(),0,nullptr))throw std::runtime_error("Independent PNG pixel decode failed");return out;
}
void consumer_sampler(const cgltf_texture*texture,const PaintSourceTexture&source){
 if(!texture||!texture->sampler)throw std::runtime_error("Export lost explicit source sampler");const auto&s=*texture->sampler;
 check(int(s.wrap_s)==source.wrap_s&&int(s.wrap_t)==source.wrap_t&&int(s.min_filter)==source.min_filter&&int(s.mag_filter)==source.mag_filter,"Independent consumer retains the actual original/groom image wrapping and filtering");
}
void consumer_pixels(const Image&actual,const Image&source,std::array<float,4>factor,std::array<float,3>expected,bool alpha,const char*label){
 check(actual.width==source.width&&actual.height==source.height,"Effective colour export retains original hair or generated-groom image dimensions");
 for(float v:factor)if(!std::isfinite(v))throw std::runtime_error("Exported material colour contains a nonfinite factor");
 float maximum=0,low=1e9f,high=0,expected_low=1e9f,expected_high=0;size_t visible=0;
 for(size_t pixel=0;pixel<actual.rgba.size();pixel+=4){
  if(alpha&&std::abs(actual.rgba[pixel+3]/255.f*factor[3]-source.rgba[pixel+3]/255.f)>1.f/255+1e-6f)throw std::runtime_error("Export changed effective authored alpha");
  if(source.rgba[pixel+3]<128)continue;
  std::array<float,3> rgb{},wanted{};for(size_t k=0;k<3;++k){rgb[k]=consumer_linear(actual.rgba[pixel+k]/255.f)*factor[k];wanted[k]=consumer_linear(source.rgba[pixel+k]/255.f)*expected[k];maximum=std::max(maximum,std::abs(rgb[k]-wanted[k]));}
  const float l=.2126f*rgb[0]+.7152f*rgb[1]+.0722f*rgb[2];low=std::min(low,l);high=std::max(high,l);const float wanted_l=.2126f*wanted[0]+.7152f*wanted[1]+.0722f*wanted[2];expected_low=std::min(expected_low,wanted_l);expected_high=std::max(expected_high,wanted_l);++visible;
 }
 // Maximum half-step linear quantisation at sRGB1 is0.00446;0.006 also
 // covers ordinary float/gamma roundoff. No normal-error bounds are involved.
 std::cout<<"EXPORT_COLOUR "<<label<<" pixels="<<visible<<" max_linear_error="<<maximum<<" luminance="<<low<<".."<<high<<'\n';
 check(visible>1000&&maximum<.006f&&expected_high-expected_low>.001f&&std::abs((high-low)-(expected_high-expected_low))<.0121f,"Independent decoded emitted pixels times material factor retain actual selected colour, alpha and textured contrast");
}
void consumer_vrm_colours(const cgltf_data &data, const HumanoidPart &part,
                          const std::array<float, 4> &tint) {
  const auto document = nlohmann::json::parse(std::string_view(data.json, data.json_size));
  const auto original = nlohmann::json::parse(part.appearance->mtoon_json);
  const auto &asset = *part.appearance->asset;
  const auto &source_vectors = original.at("vectorProperties");
  const auto &properties = document.at("extensions").at("VRM").at("materialProperties");
  bool found = false;
  for (const auto &material : properties) {
    if (material.at("name").get<std::string>().find("HAIR") == std::string::npos)
      continue;
    const auto &vectors = material.at("vectorProperties");
    for (const char *role : {"_MainTex", "_ShadeTexture"}) {
      const bool base = std::string_view(role) == "_MainTex";
      const auto source_index = original.at("textureProperties").at(role).get<size_t>();
      const auto &source = asset.textures.at(source_index);
      PaintSourceTexture sampler;
      sampler.wrap_s = source.wrap_s;
      sampler.wrap_t = source.wrap_t;
      sampler.min_filter = source.min_filter;
      sampler.mag_filter = source.mag_filter;
      const auto &source_image = *part.appearance->decoded_images->at(size_t(source.image));

      const auto index = material.at("textureProperties").at(role).get<size_t>();
      if (index >= data.textures_count)
        throw std::runtime_error("VRM role texture index exceeds the exported texture table");
      const auto *texture = &data.textures[index];
      consumer_sampler(texture, sampler);
      const char *factor = base ? "_Color" : "_ShadeColor";
      const auto &old = source_vectors.at(factor);
      const auto &value = vectors.at(factor);
      std::array<float, 3> expected{};
      std::array<float, 4> actual{};
      for (size_t channel = 0; channel < 3; ++channel) {
        expected[channel] = consumer_linear(old[channel].get<float>()) *
                            (base ? tint[channel] : 1.f);
        actual[channel] = consumer_linear(value[channel].get<float>());
      }
      actual[3] = value[3].get<float>();
      check(std::abs(actual[3] - old[3].get<float>()) < 1e-6f,
            "Exported hair role preserves the source alpha factor");
      const auto image = consumer_png(texture);
      // Compare the authored image alpha directly. The material's independent
      // alpha factor was verified above.
      actual[3] = 1.f;
      consumer_pixels(image, source_image, actual, expected, base, role);
      check(vectors.at(role) == source_vectors.at(role),
            "Exported hair retains authentic base and shade UV transforms");
    }
    found = true;
  }
  check(found, "Exported VRM preserves the current linear hair tint and authored MToon shade");
}
