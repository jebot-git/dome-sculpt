// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/emission-native.hpp"
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include "paint-overlay-sample.hpp"
namespace {
import_fixture::Fixture source(int kind,bool flat,const char *alpha,float weight){
 import_fixture::Fixture f;while(f.bin.size()%4)f.bin.push_back(0);
 auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"]["TEXCOORD_0"]=f.values({-1.25f,-.5f,1.75f,-.5f,1.75f,2.5f,-1.25f,2.5f},2,"VEC2");
 f.j["nodes"][0]["translation"]={0,0,0};f.j["nodes"][0]["scale"]={3,3,3};
 f.j["nodes"][1]["translation"]={-.14f,0,0};f.j["nodes"][1]["scale"]={1,1,1};f.j["nodes"][2]["translation"]={.14f,0,0};
 f.j["nodes"][1]["weights"]={weight};f.j["nodes"][2]["weights"]={weight};
 auto &m=f.j["materials"][0];m.erase("extensions");m["alphaMode"]=alpha;m["pbrMetallicRoughness"]["baseColorFactor"][3]=.75f;
 if(kind==1){m["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=1;
  auto png=image({230,153,204,255})->png();while(f.bin.size()%4)f.bin.push_back(0);const auto view=f.j["bufferViews"].size();
  f.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",f.bin.size()},{"byteLength",png.size()}});f.bin.insert(f.bin.end(),png.begin(),png.end());
  const auto im=f.j["images"].size();f.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});const auto texture=f.j["textures"].size();f.j["textures"].push_back({{"source",im},{"sampler",0}});m["normalTexture"]={{"index",texture},{"texCoord",1},{"scale",.6f}};
 }
 if(kind>=2)m["pbrMetallicRoughness"].erase("baseColorTexture");
 if(kind==3){p["attributes"].erase("TEXCOORD_0");p["attributes"].erase("TEXCOORD_1");}
 if(flat){p["attributes"].erase("NORMAL");p["targets"][0].erase("NORMAL");}
 return f;
}
void settled(Document &d){check(d.wait_mesh()&&d.frame().state->error.empty(),"Actual imported native project publishes its complete render surface");}
std::vector<uint8_t> file_bytes(const std::filesystem::path &path){
 std::ifstream input(path,std::ios::binary);
 return {std::istreambuf_iterator<char>(input),{}};
}
void paint_uniform(PaintedMesh &paint,const PaintSettings &settings,float radius=5){
 Brush brush;
 brush.radius=radius;
 brush.normal={0,0,1};
 paint.begin_stroke();
 paint.stamp(brush,settings);
 check(paint.end_stroke(),"Actual paint tools author the test field");
}
void certified_paint(const std::filesystem::path &out,const Scene &initial,
    const ImportAuthoringOptions &options,SurfaceRenderer &native,Fbo &fbo,
    prepared_normal_consumer::Renderer &consumer) {
 auto fixture=source(1,false,"BLEND",.65f);
 auto &pbr=fixture.j["materials"][0]["pbrMetallicRoughness"];
 pbr["metallicFactor"]=.75f;
 pbr["metallicRoughnessTexture"]={{"index",0},{"texCoord",1}};
 const auto input=out/"certified-input.glb";
 fixture.save(input);
 auto scene=prepare_authoring_import(initial,input,options).scene;
 const auto id=scene.active().id;
 for(const auto &object:scene.objects())if(object.id!=id)scene.object(object.id).visible=false;
 auto &object=scene.active();
 const auto original=object.painted->source_sampling_original()->source_maps();
 const auto pristine=out/"certified-pristine.glb";
 Document untouched;
 untouched.replace(scene.snapshot());
 untouched.export_asset(pristine).get();

 object.painted=object.painted->snapshot();
 PaintSettings settings;
 settings.front_only=false;
 settings.channel=PaintChannel::Mask;
 settings.mask=.35f;
 paint_uniform(*object.painted,settings,.06f);
 const auto masked=scene.encode();
 const auto mask_path=out/"certified-mask-only.glb";
 Document neutral;
 neutral.replace(scene.snapshot());
 neutral.export_asset(mask_path).get();
 check(file_bytes(pristine)==file_bytes(mask_path)&&neutral.copy_scene().encode()==masked,
       "Mask-only Source4 work exports the original material and alternate coordinates byte for byte");

 object.painted=object.painted->snapshot();
 object.painted->add_layer("Whole-surface finish");
 settings.channel=PaintChannel::Color;
 settings.color={.18f,.71f,.32f,.55f};
 paint_uniform(*object.painted,settings);
 settings.channel=PaintChannel::Metallic;
 settings.metallic=.6f;
 paint_uniform(*object.painted,settings);
 const auto exact=scene.encode();
 const auto surfaces=scene.export_surfaces();
 check(surfaces.size()==1&&surfaces[0].textures->source_only&&
       !surfaces[0].textures->source_maps->color.image&&
       surfaces[0].textures->source_maps->normal.image==original->normal.image,
       "Whole-surface paint folds into factors and borrows the original UV1 normal texture");
 const auto &orm=surfaces[0].textures->source_maps->metal_rough;
 bool preserved=orm.texcoord==1&&orm.mag_filter==original->metal_rough.mag_filter&&
                orm.min_filter==original->metal_rough.min_filter&&orm.wrap_s==original->metal_rough.wrap_s;
 for(size_t at=0;at<orm.image->rgba.size();at+=4)
  preserved&=orm.image->rgba[at+1]==original->metal_rough.image->rgba[at+1]&&orm.image->rgba[at+2]==255;
 check(preserved&&std::abs(surfaces[0].material.metallic-.6f)<1e-6f,
       "Uniform metallic paint preserves every untreated roughness sample and the alternate ORM sampler");
 Document painted;
 painted.replace(scene.snapshot());
 painted.shading(false);
 settled(painted);
 const auto reference=draw(native,painted.frame().surface);
 const auto output=out/"certified-factors.glb",saved=out/"certified-native.dsculpt";
 auto exported=painted.export_asset(output);
 auto native_save=painted.save_async(saved);
 exported.get();
 native_save.get();
 color_consumer::Model result(output);
 fbo.bind();
 consumer.render(result.data);
 check(delta(reference,read())<=2,"Independent GLB consumer reproduces uniform paint, original normals, alpha and the active morph");
 const auto &material=result.data->materials[0];
 check(!material.pbr_metallic_roughness.base_color_texture.texture&&material.normal_texture.texcoord==1&&
       material.pbr_metallic_roughness.metallic_roughness_texture.texcoord==1&&
       !result.json.value("extras",nlohmann::json::object()).contains("domeSculptAuthoring"),
       "Standard GLB carries direct Core PBR factors and the original numbered UV attributes");
 check(painted.copy_scene().encode()==exact&&Scene::load(saved).encode()==exact,
       "Certified export and ordered Save retain the original masks, paint and source metadata");

 auto partial=scene.snapshot();
 partial.active().painted=partial.active().painted->snapshot();
 partial.active().painted->layer_properties("Whole-surface finish",true,.5f);
 const auto refused=partial.encode();
 check(rejects([&]{partial.export_surfaces();})&&partial.encode()==refused,
       "Factor-only avatar admission retains exact project ownership for a varying source coat");

 auto flat=source(1,false,"BLEND",0);
 flat.j["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"]=1;
 const auto flat_input=out/"certified-flat-input.glb";
 flat.save(flat_input);
 auto flat_scene=prepare_authoring_import(initial,flat_input,options).scene;
 auto &flat_object=flat_scene.active();
 flat_object.painted=flat_object.painted->snapshot();
 settings.channel=PaintChannel::Color;
 settings.color={0,1,0,1};
 paint_uniform(*flat_object.painted,settings);
 flat_object.painted->layer_properties("Half-opacity finish",true,.5f);
 const auto &flat_map=flat_object.painted->source_sampling_original()->source_maps()->color;
 auto sample=flat_map;
 sample.texcoord=0;
 const auto underlying=sample_source_texture(sample,0,0,true);
 const auto flat_surfaces=flat_scene.export_surfaces();
 const auto view=std::find_if(flat_surfaces.begin(),flat_surfaces.end(),
     [&](const auto &surface){return surface.id==flat_object.id;});
 check(view!=flat_surfaces.end()&&!view->textures->source_maps->color.image,
       "Uniform partial coverage folds a constant original color image into Core PBR factors");
 for(size_t channel=0;channel<4;++channel){
  const float expected=(flat_object.material.color[channel]*underlying[channel]+settings.color[channel])*.5f;
  check(std::abs(view->material.color[channel]-expected)<1e-6f,
        "Partial uniform paint preserves linear color and alpha");
 }
}

void uniform_coat(const std::filesystem::path &out,const Scene &initial,const ImportAuthoringOptions &options,
    SurfaceRenderer &native,Fbo &fbo,prepared_normal_consumer::Renderer &consumer) {
 enum class Coat { Blend, ConstantAlpha, TextureMask, VertexMask, PassMask, RejectMask };
 const auto make_scene=[&](std::string_view name,Coat coat){
  auto fixture=source(1,false,coat==Coat::Blend?"BLEND":"MASK",.65f);
  auto &material=fixture.j["materials"][0];
  auto &pbr=material["pbrMetallicRoughness"];
  auto &attrs=fixture.j["meshes"][0]["primitives"][0]["attributes"];
  if(coat==Coat::TextureMask){
   attrs.erase("COLOR_0");
   material["alphaCutoff"]=.57f;
  }
  if(coat==Coat::VertexMask){
   while(fixture.bin.size()%4)fixture.bin.push_back(0);
   attrs["COLOR_0"]=fixture.values({.5f,.2f,.8f,.72f,.2f,.7f,.3f,.72f,
                                  .8f,.1f,.2f,.72f,.3f,.4f,.5f,.72f},4,"VEC4");
   material["alphaCutoff"]=.426f;
  }
  if(coat==Coat::PassMask)material["alphaCutoff"]=.15f;
  if(coat==Coat::RejectMask)material["alphaCutoff"]=.9f;
  pbr["metallicFactor"]=.62f;
  pbr["roughnessFactor"]=.67f;
  pbr["metallicRoughnessTexture"]={{"index",0},{"texCoord",1}};
  material["occlusionTexture"]={{"index",0},{"texCoord",1},{"strength",.65f}};
  material["emissiveFactor"]={.19f,.12f,.26f};
  material["emissiveTexture"]={{"index",0},{"texCoord",0},{"extensions",
      {{"KHR_texture_transform",{{"offset",{.19f,-.11f}}}}}}};
  fixture.j["extensionsUsed"].push_back("KHR_texture_transform");
  // More than one device readback batch. The alternating fine detail also
  // exercises the original mip sampler at the rendered scale.
  Image image{193,89,std::vector<uint8_t>(193*89*4)};
  for(size_t at=0;at<image.rgba.size();at+=4){
   const auto x=at/4%image.width,y=at/4/image.width;
   image.rgba[at]=uint8_t((x+y)%2?210:44);
   image.rgba[at+1]=uint8_t(64+(x/11+y/9)%6*19);
   image.rgba[at+2]=uint8_t(36+(x/7+2*(y/6))%7*28);
   image.rgba[at+3]=coat==Coat::ConstantAlpha?255:uint8_t(96+(x/11+y/7)%8*19);
  }
  const auto png=image.png();
  while(fixture.bin.size()%4)fixture.bin.push_back(0);
  const auto view=fixture.j["bufferViews"].size();
  fixture.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",fixture.bin.size()},{"byteLength",png.size()}});
  fixture.bin.insert(fixture.bin.end(),png.begin(),png.end());
  fixture.j["images"][0]["bufferView"]=view;
  const auto path=out/(std::string(name)+"-input.glb");
  fixture.save(path);
  auto scene=prepare_authoring_import(initial,path,options).scene;
  const auto active=scene.active().id;
  for(const auto &object:scene.objects())if(object.id!=active)scene.object(object.id).visible=false;
  auto &object=scene.active();
  object.painted=object.painted->snapshot();
  PaintSettings settings;
  settings.front_only=false;
  settings.color={.19f,.69f,.24f,.72f};
  paint_uniform(*object.painted,settings);
  settings.channel=PaintChannel::Metallic;settings.metallic=.57f;
  paint_uniform(*object.painted,settings);
  settings.channel=PaintChannel::Emission;settings.emission={.08f,.12f,.18f};
  paint_uniform(*object.painted,settings);
  object.painted->layer_properties("Paint",true,.43f);
  return scene;
 };
 const std::array cases{
     std::pair{Coat::Blend,"uniform-coat"},std::pair{Coat::ConstantAlpha,"uniform-masked"},
     std::pair{Coat::TextureMask,"uniform-mask-cross"},std::pair{Coat::VertexMask,"uniform-mask-vertex"},
     std::pair{Coat::PassMask,"uniform-mask-pass"},std::pair{Coat::RejectMask,"uniform-mask-reject"}};
 for(const auto &[coat,stem]:cases){
  const std::string name=stem;
  const bool transported=coat!=Coat::Blend&&coat!=Coat::ConstantAlpha;
  auto scene=make_scene(name,coat);
  const auto exact=scene.encode();
  const auto &object=scene.active();
  const auto &raw=*object.painted->source_sampling_original()->source_maps();
  const auto packet=detail::PaintOverlayFactory::capture(*object.painted);
  const auto owner=std::ranges::find_if(*packet->padding(),[](int32_t p){return p>=0;});
  const auto ink=detail::sample_overlay_texel_owner(*packet,size_t(*owner),true).values;
  Document doc;doc.replace(scene.snapshot());doc.shading(false);settled(doc);
  const auto reference=draw(native,doc.frame().surface);
  const auto path=out/(name+".glb"),save=out/(name+".dsculpt");
  auto exported=doc.export_asset(path);auto saved=doc.save_async(save);
  exported.get();saved.get();
  color_consumer::Model file(path);
  fbo.bind();consumer.render(file.data);
  const auto error=delta(reference,read());
  std::cout<<"UNIFORM_SOURCE "<<name<<" max="<<error<<'\n';
  check(error<=2,"Independent GLB consumer preserves a uniform coat through the original mip sampler");
  check(Scene::load(save).encode()==exact&&doc.copy_scene().encode()==exact,
        "Uniform coat export and queued Save preserve the original editable project");
  const auto &material=file.data->materials[0];
  const auto &pbr=material.pbr_metallic_roughness;
  const auto &attrs=file.json["meshes"][0]["primitives"][0]["attributes"];
  check(pbr.base_color_texture.texcoord==1&&pbr.metallic_roughness_texture.texcoord==1&&
        material.emissive_texture.texcoord==0&&material.emissive_texture.has_transform&&
        material.emissive_texture.transform.offset[0]==.19f&&material.emissive_texture.transform.offset[1]==-.11f&&
        !attrs.contains("TEXCOORD_2"),"Every coat keeps its original UV set and the retained emission transform");
  const auto compare_view=[&](const cgltf_texture_view &out_view,const PaintSourceTexture &original){
   const auto &sampler=*out_view.texture->sampler;
   return int(sampler.wrap_s)==original.wrap_s&&int(sampler.wrap_t)==original.wrap_t&&
       int(sampler.min_filter)==original.min_filter&&int(sampler.mag_filter)==original.mag_filter;
  };
  check(compare_view(pbr.base_color_texture,raw.color)&&compare_view(pbr.metallic_roughness_texture,raw.metal_rough)&&
        compare_view(material.emissive_texture,raw.emission),"Source wrap, magnification and mip filters stay paired with each rewritten image");
  const auto color=prepared_normal_consumer::image(pbr.base_color_texture.texture);
  const auto orm=prepared_normal_consumer::image(pbr.metallic_roughness_texture.texture);
  const auto glow=prepared_normal_consumer::image(material.emissive_texture.texture);
  const auto ao=prepared_normal_consumer::image(material.occlusion_texture.texture);
  const auto normal=prepared_normal_consumer::image(material.normal_texture.texture);
  check(color.width==193&&color.height==89&&orm.width==193&&orm.height==89&&
        glow.width==193&&glow.height==89,"GPU batching rewrites each image at its original rectangular resolution");
  check(ao.rgba==raw.occlusion.image->rgba&&normal.rgba==raw.normal.image->rgba,
        "An occlusion view of the same source image and the independent normal stay byte-exact");
  bool formula=true,untouched=true;
  const auto linear=[](uint8_t byte){return srgb_to_linear(byte/255.f);};
  for(size_t at=0;at<color.rgba.size();at+=4){
   const auto &input=raw.color.image->rgba;
   for(unsigned c=0;c<4;++c){
    if(transported&&c==3){untouched&=color.rgba[at+c]==input[at+c];continue;}
    const auto decode=[&](uint8_t byte){return c<3?linear(byte):byte/255.f;};
    const float expected=object.material.color[c]*decode(input[at+c])*(1-ink[3][0])+ink[0][c];
    formula&=std::abs(expected-pbr.base_color_factor[c]*decode(color.rgba[at+c]))<=(c<3?.0045f:.002f);
    if(c<3){
     const float emitted=object.material.emissive[c]*linear(input[at+c])*(1-ink[3][3])+ink[2][c];
     formula&=std::abs(emitted-material.emissive_factor[c]*linear(glow.rgba[at+c]))<=.0045f;
    }
   }
   const float metal=object.material.metallic*(input[at+2]/255.f)*(1-ink[3][1])+ink[1][2];
   formula&=std::abs(metal-pbr.metallic_factor*(orm.rgba[at+2]/255.f))<=.002f;
   untouched&=orm.rgba[at]==input[at]&&orm.rgba[at+1]==input[at+1]&&orm.rgba[at+3]==input[at+3];
  }
  check(formula&&untouched,"All rewritten texels obey the independent linear coat equation and preserve untouched packed bytes");
  if(coat==Coat::ConstantAlpha){
   const float alpha=object.material.color[3]*(1-ink[3][0])+ink[0][3];
   check(material.alpha_mode==cgltf_alpha_mode_mask&&pbr.base_color_factor[3]==alpha,
         "A constant masked alpha travels in the exact factor outside the RGB rewrite");
  }else if(coat==Coat::Blend){
   scene.active().material.alpha=Material::AlphaMode::Mask;
   scene.active().material.alpha_cutoff=.48f;
   const auto state=scene.encode();
   const auto previous=file_bytes(path);
   Document strict;strict.replace(scene.snapshot());
   auto refused=strict.export_asset(path);
   check(rejects([&]{refused.get();})&&strict.copy_scene().encode()==state&&file_bytes(path)==previous,
         "Uncertified varying mask alpha preserves the earlier GLB and exact editable owner");
  }else{
   const bool crossing=coat==Coat::TextureMask||coat==Coat::VertexMask;
   const float vertex=coat==Coat::VertexMask?.72f:1.f;
   const float scale=object.material.color[3]*(1-ink[3][0]);
   check(material.alpha_mode==cgltf_alpha_mode_mask,"Transported cutouts retain the glTF MASK material mode");
   if(crossing){
    const auto cutoff=(object.material.alpha_cutoff-ink[0][3]*vertex)/scale;
    check(pbr.base_color_factor[3]==1&&std::abs(material.alpha_cutoff-cutoff)<2e-6f,
          "The cutoff moves to the untouched alpha texture and constant vertex domain");
   }else if(coat==Coat::PassMask){
    check(material.alpha_cutoff==0&&pbr.base_color_factor[3]==1,
          "Bounded varying vertex alpha preserves the fully surviving mask");
   }else{
    check(material.alpha_cutoff==object.material.alpha_cutoff&&pbr.base_color_factor[3]==0,
          "Bounded varying vertex alpha preserves the fully discarded mask");
   }
   bool agrees=true;
   unsigned pass=0,reject=0;
   const auto eval=[&](float alpha,float vertex_alpha){
    const bool before=(scale*alpha+ink[0][3])*vertex_alpha>=object.material.alpha_cutoff;
    const bool after=pbr.base_color_factor[3]*alpha*vertex_alpha>=material.alpha_cutoff;
    agrees&=before==after;
    if(before)++pass;else ++reject;
   };
   const auto &rgba=raw.color.image->rgba;
   for(size_t at=3;at<rgba.size();at+=4){
    const float alpha=rgba[at]/255.f;
    if(crossing)eval(alpha,vertex);
    else for(float v:{.4f,.6f,.8f,1.f})eval(alpha,v);
   }
   check(agrees&&(crossing?(pass>0&&reject>0):(coat==Coat::PassMask?reject==0:pass==0)),
         "Original and parsed GLB alpha predicates agree across every original texel");
   size_t visible=0;
   for(size_t at=3;at<reference.size();at+=4)visible+=reference[at]!=0;
   if(crossing){
    auto all=std::make_shared<MeshSnapshot>(*doc.frame().surface);
    for(auto &range:all->ranges)range.material.alpha_cutoff=0;
    const auto opaque=draw(native,all);
    size_t covered=0;
    for(size_t at=3;at<opaque.size();at+=4)covered+=opaque[at]!=0;
    check(visible>0&&visible<covered,"Independent rendering exercises both sides of the transported cutoff");
   }else{
    check(coat==Coat::PassMask?visible>0:visible==0,"Rendered cutoff envelope fixes complete pass or discard");
   }
  }
 }
}

void spatial_paint(const std::filesystem::path &out,const Scene &initial,const ImportAuthoringOptions &options,
    SurfaceRenderer &native,Fbo &fbo,prepared_normal_consumer::Renderer &consumer) {
 const auto load=[&](std::string_view tag,bool constant_color,const char *alpha,bool source_orm){
  auto fixture=source(1,false,alpha,.65f);
  auto &pbr=fixture.j["materials"][0]["pbrMetallicRoughness"];
  if(constant_color)pbr["baseColorTexture"]["index"]=1;
  if(std::string_view(alpha)=="MASK")pbr["baseColorFactor"][3]=1;
  pbr["roughnessFactor"]=.64f;
  pbr["metallicFactor"]=.36f;
  if(source_orm)pbr["metallicRoughnessTexture"]={{"index",0},{"texCoord",1}};
  const auto input=out/(std::string(tag)+"-input.glb");
  fixture.save(input);
  auto scene=prepare_authoring_import(initial,input,options).scene;
  const auto active=scene.active().id;
  for(const auto &object:scene.objects())if(object.id!=active)scene.object(object.id).visible=false;
  scene.active().painted=scene.active().painted->snapshot();
  return scene;
 };
 const auto publish=[&](const Scene &scene,std::string_view tag){
  const auto exact=scene.encode();
  Document doc;
  doc.replace(scene.snapshot());
  doc.shading(false);
  settled(doc);
  const auto reference=draw(native,doc.frame().surface);
  const auto path=out/(std::string(tag)+".glb"),native_path=out/(std::string(tag)+".dsculpt");
  auto export_job=doc.export_asset(path);
  auto save_job=doc.save_async(native_path);
  export_job.get();
  save_job.get();
  color_consumer::Model loaded(path);
  fbo.bind();
  consumer.render(loaded.data);
  const auto error=delta(reference,read());
  std::cout<<"SPATIAL_STANDARD "<<tag<<" max="<<error<<'\n';
  check(error<=2,"Independent GLB consumer reproduces spatial paint, original normal and morph");
  check(doc.copy_scene().encode()==exact&&Scene::load(native_path).encode()==exact,
        "Direct spatial export and queued Save preserve the full editable owner");
  return path;
 };
 const auto atlas_slot=[&](const cgltf_texture_view &view,const cgltf_primitive &primitive){
  check(view.texture&&view.texcoord==2&&!view.has_transform&&
        view.texture->sampler&&view.texture->sampler->min_filter==cgltf_filter_type_linear&&
        view.texture->sampler->mag_filter==cgltf_filter_type_linear&&
        view.texture->sampler->wrap_s==cgltf_wrap_mode_clamp_to_edge,
        "Varying paint samples its own clamped linear TEXCOORD_2 atlas");
  const auto uv=prepared_normal_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_texcoord,2);
  check(uv&&uv->count==prepared_normal_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_position)->count,
        "Independent writable UVs have their exact emitted vertex count");
 };
 PaintSettings settings;
 settings.front_only=false;
 settings.color={.19f,.74f,.24f,.52f};
 auto color=load("spatial-color",true,"BLEND",true);
 paint_uniform(*color.active().painted,settings,.28f);
 color.active().painted->add_layer("Painted light");
 settings.channel=PaintChannel::Emission;
 settings.emission={.08f,.15f,.048f};
 paint_uniform(*color.active().painted,settings,.22f);
 const auto path=publish(color,"spatial-color");
 color_consumer::Model painted(path);
 const auto &p=painted.data->meshes[0].primitives[0];
 const auto &m=*p.material;
 atlas_slot(m.pbr_metallic_roughness.base_color_texture,p);
 atlas_slot(m.emissive_texture,p);
 check(m.normal_texture.texcoord==1&&m.pbr_metallic_roughness.metallic_roughness_texture.texcoord==1&&
       m.pbr_metallic_roughness.metallic_roughness_texture.texture->sampler->wrap_s==cgltf_wrap_mode_mirrored_repeat,
       "Alternate normal and ORM keep their original UVs and sampler while color and emission share the new atlas");
 const auto parsed_atlas=prepared_normal_consumer::image(m.pbr_metallic_roughness.base_color_texture.texture);
 check(parsed_atlas.width==64&&parsed_atlas.height==64,"Standard GLB uses the exact authored paint raster size");

 auto orm=load("spatial-orm",false,"OPAQUE",false);
 settings.channel=PaintChannel::Metallic;settings.metallic=.54f;
 paint_uniform(*orm.active().painted,settings,.27f);
 settings.channel=PaintChannel::Roughness;settings.roughness=.82f;
 paint_uniform(*orm.active().painted,settings,.21f);
 color_consumer::Model orm_file(publish(orm,"spatial-orm"));
 const auto &op=orm_file.data->meshes[0].primitives[0];
 atlas_slot(op.material->pbr_metallic_roughness.metallic_roughness_texture,op);
 check(op.material->pbr_metallic_roughness.base_color_texture.texcoord==1&&op.material->normal_texture.texcoord==1,
       "Metal and roughness can share an atlas while textured color and normal stay on UV1");

 auto covered=load("spatial-cover",false,"MASK",false);
 settings.channel=PaintChannel::Color;
 settings.color={.73f,.19f,.22f,1};
 paint_uniform(*covered.active().painted,settings);
 settings.color={.21f,.44f,.82f,1};
 paint_uniform(*covered.active().painted,settings,.26f);
 color_consumer::Model mask(publish(covered,"spatial-cover"));
 const auto &mp=mask.data->meshes[0].primitives[0];
 atlas_slot(mp.material->pbr_metallic_roughness.base_color_texture,mp);
 check(mp.material->alpha_mode==cgltf_alpha_mode_mask&&mp.material->pbr_metallic_roughness.base_color_factor[3]==1,
       "Fully covered spatial paint replaces an original varying image with exact constant alpha at the MASK cutoff");

 auto unsafe=load("spatial-mask-refusal",true,"MASK",true);
 settings.color[3]=.36f;
 paint_uniform(*unsafe.active().painted,settings,.26f);
 const auto before=unsafe.encode();
 Document strict;
 strict.replace(unsafe.snapshot());
 const auto refused=out/"spatial-refused.glb";
 {std::ofstream previous(refused,std::ios::binary);previous<<"existing GLB";}
 auto rejected=strict.export_asset(refused);
 check(rejects([&]{rejected.get();})&&strict.copy_scene().encode()==before&&
       file_bytes(refused)==std::vector<uint8_t>({'e','x','i','s','t','i','n','g',' ','G','L','B'}),
       "Varying alpha that cannot preserve an exact MASK cutoff leaves the previous file and native project intact");
}
void mixed_emission(const std::filesystem::path &out,Fbo &fbo,
                    prepared_normal_consumer::Renderer &consumer) {
 auto scene=Scene::load(out/"spatial-color.dsculpt");
 const auto painted_id=scene.active().id;
 const auto light_id=scene.add("Physical light",Volume{});
 scene.select(painted_id);
 auto &light=scene.object(light_id);
 light.mesh_active=true;
 light.painted=std::make_shared<PaintedMesh>(quad(),64);
 light.material.emissive={8,.125f,.04f};
 PaintSettings ink;
 ink.channel=PaintChannel::Emission;
 ink.emission_units=PaintEmissionUnits::Physical;
 ink.emission={96,.005f,.0003f};
 ink.front_only=false;
 paint_uniform(*light.painted,ink,.32f);

 color_consumer::Model ordinary(out/"spatial-color.glb");
 fbo.bind();
 consumer.render(ordinary.data);
 const auto ordinary_pixels=read();
 for(const bool visible:{true,false}){
  light.visible=visible;
  const auto exact=scene.encode();
  const auto stem=visible?"mixed-source":"mixed-source-hidden";
  const auto path=out/(std::string(stem)+".glb"),saved=out/(std::string(stem)+".dsculpt");
  Document doc;
  doc.replace(scene.snapshot());
  auto written=doc.export_asset(path);
  auto save=doc.save_async(saved);
  written.get();
  save.get();

  const auto bytes=file_bytes(path);
  auto restored=extract_native_emission(bytes);
  check(restored&&restored->encode()==exact&&doc.copy_scene().encode()==exact&&
        Scene::load(saved).encode()==exact,"Mixed Standard GLB, queued Save and live document retain both native owners exactly");
  const size_t json_size=bytes.at(12)|(size_t(bytes.at(13))<<8)|(size_t(bytes.at(14))<<16)|(size_t(bytes.at(15))<<24);
  const auto json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_size);
  const auto &images=json.at("extras").at("domeSculptEmission").at("images");
  check(images.size()==size_t(visible),"Only emitted physical light owns an HDR field");

  color_consumer::Model mixed(path);
  if(visible){
   const auto &entry=images.at(0);
   check(entry.at("object")==light_id,"HDR field is attached only to the physical light");
   auto &node=mixed.data->nodes[entry.at("node").get<size_t>()];
   check(node.mesh&&node.mesh->primitives[0].material->has_emissive_strength&&
         node.mesh->primitives[0].material->emissive_texture.texture,
         "The separate light has its measured standard emissive fallback");
   const auto &view=json.at("bufferViews").at(entry.at("bufferView").get<size_t>());
   const auto exr=decode_emission_exr(std::span(bytes).subspan(28+json_size+view.at("byteOffset").get<size_t>(),
                                                               view.at("byteLength").get<size_t>()));
   check(exr.width==64&&exr.height==64&&std::any_of(exr.pixels.begin(),exr.pixels.end(),[](const auto &pixel){
    return pixel[0]>90&&pixel[2]>0&&pixel[2]<.002f;
   }),"Mixed Standard GLB keeps the light's bright and dim native FLOAT emission in the same samples");
   node.mesh=nullptr;
  }
  check(mixed.data->meshes_count==(visible?2u:1u),"Hidden physical light remains in the native project only");
  const auto &primitive=mixed.data->meshes[0].primitives[0];
  const auto &material=*primitive.material;
  check(material.pbr_metallic_roughness.base_color_texture.texcoord==2&&material.emissive_texture.texcoord==2&&
        material.normal_texture.texcoord==1&&material.pbr_metallic_roughness.metallic_roughness_texture.texcoord==1&&
        primitive.targets_count==ordinary.data->meshes[0].primitives[0].targets_count,
        "Mixed export preserves editable paint atlas, independent original channels and morphs in core GLB");
  fbo.bind();
  consumer.render(mixed.data);
  const auto error=delta(ordinary_pixels,read());
  std::cout<<"MIXED_STANDARD "<<stem<<" max="<<error<<'\n';
  check(error==0,"Independent GLB consumer sees identical ordinary material with HDR packaging present");

  const auto retry=out/(std::string(stem)+"-retry.glb");
  doc.replace(std::move(*restored));
  doc.export_asset(retry).get();
  check(file_bytes(retry)==bytes,"Native mixed scene extraction and re-export reproduce the exact GLB");
 }
}
void single_surface_emission(const std::filesystem::path &out,const Scene &initial,
    const ImportAuthoringOptions &options,SurfaceRenderer &native,Fbo &fbo,
    prepared_normal_consumer::Renderer &consumer) {
 auto fixture=source(1,false,"BLEND",.65f);
 auto &material=fixture.j["materials"][0];
 auto &pbr=material["pbrMetallicRoughness"];
 pbr["baseColorTexture"]["index"]=1;
 pbr["metallicRoughnessTexture"]={{"index",0},{"texCoord",1}};
 material["emissiveTexture"]={{"index",1},{"texCoord",1}};
 material["emissiveFactor"]={.08f,.16f,.25f};
 const auto input=out/"single-surface-hdr-input.glb";
 fixture.save(input);
 auto scene=prepare_authoring_import(initial,input,options).scene;
 auto &object=scene.active();
 for(const auto &other:scene.objects())if(other.id!=object.id)scene.object(other.id).visible=false;
 object.painted=object.painted->snapshot();
 PaintSettings ink;
 ink.front_only=false;
 ink.color={.19f,.74f,.24f,.52f};
 paint_uniform(*object.painted,ink,.28f);
 object.painted->add_layer("Physical light");
 ink.channel=PaintChannel::Emission;
 ink.emission_units=PaintEmissionUnits::Physical;
 ink.emission={3.5f,.02f,.43f};
 paint_uniform(*object.painted,ink,.22f);
 const auto before=scene.encode();
 Document doc;
 doc.replace(scene.snapshot());
 doc.shading(false);
 settled(doc);
 const auto reference=draw(native,doc.frame().surface);
 const auto path=out/"single-surface-hdr.glb",saved=out/"single-surface-hdr.dsculpt";
 auto written=doc.export_asset(path);
 auto save=doc.save_async(saved);
 written.get();
 save.get();

 color_consumer::Model exported(path);
 const auto &primitive=exported.data->meshes[0].primitives[0];
 const auto &compiled=*primitive.material;
 check(compiled.has_emissive_strength&&compiled.emissive_texture.texcoord==2&&
       compiled.pbr_metallic_roughness.base_color_texture.texcoord==2&&
       compiled.normal_texture.texcoord==1&&compiled.pbr_metallic_roughness.metallic_roughness_texture.texcoord==1&&
       primitive.targets_count==1,"Same-surface HDR shares the writable UV with color and retains original UV1 maps and morph");
 fbo.bind();
 consumer.render(exported.data);
 const auto error=delta(reference,read());
 std::cout<<"SINGLE_SURFACE_STANDARD max="<<error<<'\n';
 check(error<=3,"Independent GLB renderer reproduces painted color and HDR on their independent normal and ORM surface");
 const auto bytes=file_bytes(path);
 const auto glb=std::span(bytes);
 const size_t json_size=glb[12]|(size_t(glb[13])<<8)|(size_t(glb[14])<<16)|(size_t(glb[15])<<24);
 const auto json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_size);
 const auto &package=json.at("extras").at("domeSculptEmission");
 const auto &entry=package.at("images").at(0);
 check(package.at("version")==7&&entry.at("object")==object.id&&
       entry.at("domain").at("kind")=="prepared-source"&&entry.at("domain").at("texCoord")==2,
       "The native FLOAT field identifies its actual writable coordinates");
 const auto &view=json.at("bufferViews").at(entry.at("bufferView").get<size_t>());
 const auto exr=decode_emission_exr(glb.subspan(28+json_size+view.at("byteOffset").get<size_t>(),
                                               view.at("byteLength").get<size_t>()));
 check(std::any_of(exr.pixels.begin(),exr.pixels.end(),[](const auto &p){
        return p[9]>.5f&&p[6]==3.5f*p[9]&&p[7]==.02f*p[9]&&p[8]==.43f*p[9];
       })&&
       std::any_of(exr.pixels.begin(),exr.pixels.end(),[](const auto &p){
        const auto remaining=1-p[9];
        return remaining>.1f&&std::abs(p[0]-(.08f*srgb_to_linear(230.f/255)*remaining+p[6]))<5e-7f&&
               std::abs(p[1]-(.16f*srgb_to_linear(153.f/255)*remaining+p[7]))<5e-7f;
       }),"EXR carries exact physical ink coefficients and the constant original textured backdrop");
 auto restored=extract_native_emission(bytes);
 check(restored&&restored->encode()==before&&Scene::load(saved).encode()==before&&
       doc.copy_scene().encode()==before,"HDR GLB recovery and ordered Save preserve source geometry, layers, and physical paint exactly");
 doc.replace(std::move(*restored));
 const auto retry=out/"single-surface-hdr-retry.glb";
 doc.export_asset(retry).get();
 check(file_bytes(retry)==bytes,"Restored single-surface HDR project exports the same exact GLB");

 material["emissiveTexture"]["index"]=0;
 const auto varying=out/"varying-source-hdr-input.glb";
 fixture.save(varying);
 auto unsupported=prepare_authoring_import(initial,varying,options).scene;
 auto &varying_object=unsupported.active();
 for(const auto &other:unsupported.objects())if(other.id!=varying_object.id)unsupported.object(other.id).visible=false;
 varying_object.painted=varying_object.painted->snapshot();
 paint_uniform(*varying_object.painted,ink,.22f);
 const auto original=unsupported.encode();
 doc.replace(unsupported.snapshot());
 auto refused=doc.export_asset(path);
 auto queued_save=doc.save_async(out/"varying-source-hdr-preserved.dsculpt");
 check(rejects([&]{refused.get();}),"Varying original emission still requires the explicit portable compiler");
 queued_save.get();
 check(file_bytes(path)==bytes&&Scene::load(out/"varying-source-hdr-preserved.dsculpt").encode()==original,
       "Unsupported emission preserves the earlier GLB and saves all native authoring data");
}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested standard source export proof requires actual A770");
 Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;prepared_normal_consumer::Renderer separate_roles;int maximum=0;
 Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;
 for(int kind=0;kind<4;++kind)for(bool flat:{false,true})for(const char *alpha:{"OPAQUE","MASK","BLEND"})for(float weight:{0.f,.65f}){
  auto fixture=source(kind,flat,alpha,weight);const auto prefix=std::to_string(kind)+(flat?"-flat-":"-artist-")+alpha+"-"+std::to_string(weight);const auto path=out/(prefix+"-input.glb");fixture.save(path);
  auto imported=prepare_authoring_import(initial,path,options);const auto before=imported.scene.encode();
  Document document;document.replace(Scene::decode(before));document.shading(false);settled(document);const auto pixels=draw(native,document.frame().surface);
  size_t visible=0;for(size_t p=3;p<pixels.size();p+=4)visible+=pixels[p]>0;check(visible>128,"Independent export comparison contains substantial visible surface coverage");
  const auto output=out/(prefix+"-output.glb");document.export_asset(output).get();
  color_consumer::Model model(output);fbo.bind();if(kind==1)separate_roles.render(model.data);else independent.render(model.data,nullptr,{},true);const int error=delta(pixels,read());maximum=std::max(maximum,error);
  std::cout<<"SOURCE_EXPORT "<<prefix<<" max="<<error<<'\n';check(error<=2,"Independently parsed standard GLB matches actual source-aware imported PBR pixels, source sampling, frames, alpha, colour and morphs");
  check(document.copy_scene().encode()==before&&Scene::decode(before).encode()==before,"Standard export compilation leaves exact native paint, source coordinate owners and source history unchanged");
  size_t materials=0;for(size_t n=0;n<model.data->materials_count;++n){const auto &material=model.data->materials[n];
   if(kind<2){const auto &texture=material.pbr_metallic_roughness.base_color_texture;check(texture.texture&&texture.texcoord==kind&&texture.texture->sampler&&texture.texture->sampler->wrap_s==cgltf_wrap_mode_mirrored_repeat&&texture.texture->sampler->min_filter==cgltf_filter_type_nearest_mipmap_linear,"Standard export retains original image coordinate, mirrored repeat and mip filter");++materials;}
   else check(!material.pbr_metallic_roughness.base_color_texture.texture,"Untextured export invents no replacement image");
   if(kind==1)check(material.normal_texture.texture&&material.normal_texture.texcoord==1&&std::abs(material.normal_texture.scale-.6f)<1e-6f,"Original UV1 normal role and strength survive writable atlas import/export");
  }
  check(kind>=2||materials==2,"Both selected source instances retain their independently emitted material bindings");
  if(kind==1&&!flat&&std::string_view(alpha)=="OPAQUE"&&weight==0){
   for(size_t n=0;n<model.data->materials_count;++n)model.data->materials[n].pbr_metallic_roughness.base_color_texture.texcoord=0;
   fbo.bind();separate_roles.render(model.data);check(delta(pixels,read())>4,"Independent source consumer visibly rejects substituting UV0 for the actual UV1 base role");
  }
  if(kind==0&&!flat&&std::string_view(alpha)=="OPAQUE"&&weight==0){
   int polls=0;auto fresh=Scene::decode(before);auto probe=fresh.export_surfaces([&]{++polls;return false;});check(polls>1&&probe.size()==2,"Actual original-coordinate export staging exposes cancellation checkpoints");
   for(int boundary=1;boundary<=polls;++boundary){int count=0;auto candidate=Scene::decode(before);check(rejects([&]{candidate.export_surfaces([&]{return ++count>=boundary;});})&&count==boundary&&candidate.encode()==before,"Every actual export coordinate cancellation preserves the original complete native scene");}
   auto &object=imported.scene.active();auto paint=object.painted->snapshot();PaintSettings ink;ink.front_only=false;ink.color={.1f,.8f,.3f,.6f};Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=1;paint->begin_stroke();paint->stamp(brush,ink);check(paint->end_stroke(),"Real source-overlay paint contributes before the unimplemented portable bake control");object.painted=paint;
   const auto painted=imported.scene.encode();check(rejects([&]{imported.scene.export_surfaces();})&&imported.scene.encode()==painted,"Unimplemented painted standard bake refuses without silently losing paint or changing source state");
   Document rejected;rejected.replace(imported.scene);const auto protected_file=out/"protected-painted.glb",saved=out/"painted-queued.dsculpt";
   {std::ofstream file(protected_file,std::ios::binary);file<<"earlier export";}
   auto failed=rejected.export_asset(protected_file);auto save=rejected.save_async(saved);check(rejects([&]{failed.get();}),"Actual native export worker reports the required portable painted bake rather than emitting an incomplete model");save.get();
   std::ifstream file(protected_file,std::ios::binary);std::string protected_bytes((std::istreambuf_iterator<char>(file)),{});
   check(protected_bytes=="earlier export"&&rejected.copy_scene().encode()==painted&&Scene::load(saved).encode()==painted,"Failed painted standard export preserves prior output, native ownership and immediately queued Save");
  }
 }
 certified_paint(out,initial,options,native,fbo,separate_roles);
 uniform_coat(out,initial,options,native,fbo,separate_roles);
 spatial_paint(out,initial,options,native,fbo,separate_roles);
 mixed_emission(out,fbo,separate_roles);
 single_surface_emission(out,initial,options,native,fbo,separate_roles);
 std::cout<<"PASS "<<checks<<" source-aware original standard export checks maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
