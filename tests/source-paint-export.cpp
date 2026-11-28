// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
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
       "Uniform partial paint over a varying original image still needs explicit portable baking");

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
 spatial_paint(out,initial,options,native,fbo,separate_roles);
 std::cout<<"PASS "<<checks<<" source-aware original standard export checks maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
