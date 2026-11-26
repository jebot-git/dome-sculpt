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
void consumer_uv(const cgltf_data&data,const cgltf_material*material,const Mesh&mesh){
 bool found=false;for(size_t m=0;m<data.meshes_count;++m)for(size_t p=0;p<data.meshes[m].primitives_count;++p){const auto&primitive=data.meshes[m].primitives[p];if(primitive.material!=material)continue;
  const cgltf_accessor*uv=nullptr;for(size_t a=0;a<primitive.attributes_count;++a)if(primitive.attributes[a].type==cgltf_attribute_type_texcoord&&primitive.attributes[a].index==0)uv=primitive.attributes[a].data;
  const size_t count=primitive.indices?primitive.indices->count:(uv?uv->count:0);
  check(uv&&count==mesh.indices.size(),"Independent exported original/groom UVs keep every ordered triangle corner");
  for(size_t corner=0;corner<count;++corner){const size_t vertex=primitive.indices?cgltf_accessor_read_index(primitive.indices,corner):corner;const auto&expected=mesh.uv.at(mesh.indices.at(corner));float value[2]{};if(vertex>=uv->count||!cgltf_accessor_read_float(uv,vertex,value,2)||!std::isfinite(value[0])||!std::isfinite(value[1])||std::abs(value[0]-expected[0])>1e-7f||std::abs(value[1]-expected[1])>1e-7f)throw std::runtime_error("Export changed original or groom ordered-corner UV coordinates");}found=true;
 }check(found,"Independent consumer finds the actual material's exported geometry/UV owner");
}
void consumer_glb_colours(const cgltf_data&data,const CreatedHumanoid&model){
 size_t observed=0;for(const auto&part:model.parts){const bool original=part.name.find("HAIR")!=std::string::npos;const bool groom=part.name=="Original fitted groom";if(!original&&!groom)continue;
  const cgltf_material*material=nullptr;for(size_t i=0;i<data.materials_count;++i)if(data.materials[i].name&&part.name==data.materials[i].name)material=&data.materials[i];check(material,"Independent glTF consumer finds genuine original hair and explicitly generated groom materials");
  const auto&p=material->pbr_metallic_roughness;check(p.base_color_texture.texcoord==0,"Effective export colour uses the actual authored UV0 role");consumer_sampler(p.base_color_texture.texture,part.source_maps->color);auto image=consumer_png(p.base_color_texture.texture);
  consumer_pixels(image,*part.source_maps->color.image,{p.base_color_factor[0],p.base_color_factor[1],p.base_color_factor[2],p.base_color_factor[3]},{part.color[0],part.color[1],part.color[2]},true,part.name.c_str());
  check(material->alpha_mode==(original?cgltf_alpha_mode_blend:cgltf_alpha_mode_opaque),"Original source blend and opaque fitted-groom coverage modes survive effective colour export");consumer_uv(data,material,part.mesh);++observed;
 }check(observed==2,"Effective GLB appearance oracle covers both original source hair and fitted groom");
}
void consumer_vrm_colours(const cgltf_data&data,const HumanoidPart&part,std::array<float,3>colour){
 const auto document=nlohmann::json::parse(std::string_view(data.json,data.json_size));const auto original=nlohmann::json::parse(part.appearance->mtoon_json);const auto&source_vectors=original.at("vectorProperties");
 const auto luminance=[](const auto&v){return .2126f*consumer_linear(v[0].template get<float>())+.7152f*consumer_linear(v[1].template get<float>())+.0722f*consumer_linear(v[2].template get<float>());};const float contrast=luminance(source_vectors.at("_ShadeColor"))/luminance(source_vectors.at("_Color"));
 bool found=false;for(const auto&m:document.at("extensions").at("VRM").at("materialProperties")){if(m.at("name").get<std::string>().find("HAIR")==std::string::npos)continue;const auto&vectors=m.at("vectorProperties");
  for(const char*role:{"_MainTex","_ShadeTexture"}){const auto index=m.at("textureProperties").at(role).get<size_t>();if(index>=data.textures_count)throw std::runtime_error("VRM role texture index is outside its actual export");const auto*texture=&data.textures[index];consumer_sampler(texture,part.source_maps->color);const auto image=consumer_png(texture);const auto&value=vectors.at(std::string(role)=="_MainTex"?"_Color":"_ShadeColor");std::array<float,4>factor{consumer_linear(value[0].get<float>()),consumer_linear(value[1].get<float>()),consumer_linear(value[2].get<float>()),value[3].get<float>()};auto target=colour;if(std::string(role)=="_ShadeTexture")for(auto&c:target)c*=contrast;
   consumer_pixels(image,*part.source_maps->color.image,factor,target,std::string(role)=="_MainTex",role);check(vectors.at(role)==source_vectors.at(role),"Actual VRM base/shade UV transforms retain authentic original sampling");
  }found=true;
 }check(found,"Actual VRM effective base and shade roles retain selected RGB with original luminous contrast");
}
