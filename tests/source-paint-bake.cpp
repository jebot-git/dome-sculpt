// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-paint-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include <numeric>
#include "source-paint-bake-fixtures.hpp"

int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested source paint bake proof requires actual A770");
 Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);color_consumer::Renderer independent;prepared_normal_consumer::Renderer all_roles;Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.prepare_texture_atlas=true;options.paint_resolution=64;
 for(int kind=0;kind<8;++kind){auto fixture=portable_material_fixture(kind);
  auto path=out/("case"+std::to_string(kind)+".glb");fixture.save(path);auto imported=prepare_authoring_import(initial,path,options);ink(imported.scene);const auto &object=imported.scene.active();const auto before=imported.scene.encode();
  SourceUvBakeInput input;input.paint=object.painted;input.appearance=object.appearance;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=object.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  SourceUvBakeOptions bake_options;bake_options.resolution=512;auto portable=bake_source_paint(input,object.material,bake_options);const auto &result=portable.sampling;auto adopted=adopt_source_uv_bake(result);check(imported.scene.encode()==before,"Pure original-coordinate baking preserves the complete native project");
  check(result.layers.size()==object.painted->layers().size()&&result.active_layer==object.painted->active_layer(),"Portable atlas preparation retains every visible/hidden editable layer and active identity");
  for(size_t n=0;n<result.layers.size();++n){const auto &a=result.layers[n],&b=object.painted->layers()[n];check(a.id==b.id&&a.name==b.name&&a.opacity==b.opacity&&a.visible==b.visible&&a.tiles==b.tiles&&a.pbr_tiles==b.pbr_tiles&&a.emission_tiles==b.emission_tiles,"Same-atlas original-image conversion preserves exact sparse paint owners rather than resampling them");}
  auto converted=imported.scene.snapshot();auto &current=converted.active();current.painted=adopted.paint;current.material_bindings=adopted.material_bindings;current.deformation=adopted.deformation;current.appearance=adopted.appearance;converted.validate();auto encoded=converted.encode();check(Scene::decode(encoded).encode()==encoded,"Actual converted material/paint/frame owners survive native restart");
  Document a,b;a.replace(imported.scene);b.replace(converted);settle(a);settle(b);
  for(bool pbr:{false,true}){if(pbr){a.shading(false);b.shading(false);}else{a.authored_shading();b.authored_shading();}settle(a);settle(b);auto compiled=std::make_shared<MeshSnapshot>(*b.frame().surface);for(auto &range:compiled->ranges)if(range.object==object.id){range.textures=portable.textures;range.material_bindings=portable.material_bindings;}
   auto reference=draw(native,a.frame().surface),actual=draw(native,compiled);auto quality=fidelity(reference,actual);std::cout<<"SOURCE_PAINT_BAKE kind="<<kind<<" pbr="<<pbr<<" mean="<<quality.mean<<" max="<<quality.maximum<<" interior="<<quality.interior<<" interior_max="<<quality.interior_maximum<<'\n';check(quality.visible>20&&quality.mean<=2&&quality.interior_maximum<=2,"Explicit original-image conversion retains the existing visible mean/interior quality bounds");
   if(pbr){auto file=out/("baked"+std::to_string(kind)+".glb");auto surfaces=converted.export_surfaces();for(auto &surface:surfaces)if(surface.id==object.id){surface.textures=portable.textures;surface.material_bindings=portable.material_bindings;}export_glb(surfaces,file);color_consumer::Model model(file);fbo.bind();if(kind==6)all_roles.render(model.data);else independent.render(model.data,nullptr,{},true);check(delta(actual,read())<=2,"Explicitly compiled painted surface export matches an independently parsed standard PBR consumer");}
   if(pbr&&kind==4){
    auto changed=std::make_shared<TextureMaps>(*portable.textures);std::fill(changed->emission.rgba.begin(),changed->emission.rgba.end(),0);auto control=std::make_shared<MeshSnapshot>(*compiled);for(auto &range:control->ranges)if(range.object==object.id)range.textures=changed;
    check(delta(actual,draw(native,control))>4,"Lit portable comparison independently detects removal of actual painted emission");
    changed=std::make_shared<TextureMaps>(*portable.textures);for(size_t at=0;at<changed->metal_rough.rgba.size();at+=4){changed->metal_rough.rgba[at+1]=255;changed->metal_rough.rgba[at+2]=0;}control=std::make_shared<MeshSnapshot>(*compiled);for(auto &range:control->ranges)if(range.object==object.id)range.textures=changed;
    check(delta(actual,draw(native,control))>4,"Lit portable comparison independently detects replacement of actual metal/rough fields");
   }
  }
  if(kind==4){
   for(int choice=0;choice<5;++choice){auto bounded=bake_options;if(choice==0)bounded.max_working_bytes=4;if(choice==1)bounded.max_output_bytes=4;if(choice==2)bounded.max_sample_operations=4;if(choice==3)bounded.max_raster_samples=4;if(choice==4)bounded.max_layer_bytes=4;
    check(rejects([&]{bake_source_paint(input,object.material,bounded);})&&imported.scene.encode()==before,"Portable image/working/sample/raster/layer quota refusal preserves complete original source state");
   }
  }
 }
 {
  auto fixture=source_paint_fixture::input(0);Image checker{128,128,std::vector<uint8_t>(128*128*4)};for(size_t y=0;y<128;++y)for(size_t x=0;x<128;++x){const auto at=(y*128+x)*4;for(size_t k=0;k<3;++k)checker.rgba[at+k]=((x+y)&1)?255:0;checker.rgba[at+3]=255;}
  auto png=checker.png();while(fixture.bin.size()%4)fixture.bin.push_back(0);const auto view=fixture.j["bufferViews"].size();fixture.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",fixture.bin.size()},{"byteLength",png.size()}});fixture.bin.insert(fixture.bin.end(),png.begin(),png.end());fixture.j["images"][0]["bufferView"]=view;fixture.j["nodes"][1]["scale"]={.6f,.6f,.6f};fixture.j["materials"][0]["alphaMode"]="OPAQUE";
  const auto path=out/"minification-input.glb";fixture.save(path);auto imported=prepare_authoring_import(initial,path,options);ink(imported.scene);const auto &object=imported.scene.active();const auto before=imported.scene.encode();SourceUvBakeInput input;input.paint=object.painted;input.appearance=object.appearance;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=object.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);SourceUvBakeOptions chosen;chosen.resolution=512;auto portable=bake_source_paint(input,object.material,chosen);auto adopted=adopt_source_uv_bake(portable.sampling);auto converted=imported.scene.snapshot();auto &o=converted.active();o.painted=adopted.paint;o.material_bindings=adopted.material_bindings;o.deformation=adopted.deformation;o.appearance=adopted.appearance;Document a,b;a.replace(imported.scene);b.replace(converted);a.shading(false);b.shading(false);settle(a);settle(b);auto compiled=std::make_shared<MeshSnapshot>(*b.frame().surface);for(auto &range:compiled->ranges)if(range.object==object.id){range.textures=portable.textures;range.material_bindings=portable.material_bindings;}
  const auto quality=fidelity(draw(native,a.frame().surface),draw(native,compiled));std::cout<<"MINIFICATION_BOUNDARY mean="<<quality.mean<<" max="<<quality.maximum<<" interior_max="<<quality.interior_maximum<<'\n';
  check(quality.visible>0&&(quality.mean>2||quality.interior_maximum>2),"Real checker minification rejects treating a finite portable image as lossless independent original sampling");
  check(rejects([&]{imported.scene.export_surfaces();})&&imported.scene.encode()==before,"Strict standard export keeps its refusal and exact native source while the authenticated portable-owner adapter remains unfinished");
 }
 std::cout<<"PASS "<<checks<<" source-aware material bake checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
