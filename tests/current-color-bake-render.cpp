// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_suite
#include "mtoon-render.cpp"
#undef main
#include "current-color-bake-fixture.hpp"
#include "dome/bake-authoring.hpp"
#include "source-color-consumer.hpp"
int main(int argc,char **argv)try{
 if(argc<2)return 2;auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
 NativeGraphics context;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||device.find("A770")!=std::string::npos,"Current source Bake consumer uses requested actual device");
 Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;size_t number=0;
 for(bool seam:{false,true})for(bool posed:{false,true})for(auto alpha:{Material::AlphaMode::Automatic,Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  auto original=current_color_bake_fixture::fixture(seam,posed);auto source=original.objects().front().id;auto target=original.active().id;
  for(auto id:{source,target}){original.object(id).material.color={.72f,.83f,.61f,.7f};original.object(id).material.alpha=alpha;original.object(id).material.alpha_cutoff=.35f;}
  auto source_value=current_color_bake_fixture::source_only(original.snapshot(),source);auto field=original.object(source).polygon->colors;
  auto display=original.snapshot();display.object(target).visible=false;display.object(source).visible=true;display.select(source);
  Document expected;expected.replace(display);check(expected.wait_mesh()&&expected.frame().state->error.empty(),"Actual original current-field source publishes in its displayed saved pose");
  auto raw=draw(native,expected.frame().surface);size_t visible=0;for(size_t i=3;i<raw.size();i+=4)visible+=raw[i]>0;if(alpha==Material::AlphaMode::Opaque)check(visible>128,"Source-fidelity fixture has substantial real source coverage");
  Document baked;baked.replace(original.snapshot());ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;settings.padding=4;baked.bake_from_object(source,settings);
  check(baked.wait_asset()&&baked.wait_mesh()&&baked.frame().state->error.empty(),"Actual native worker bakes current source RGBA to the separate prepared target plane");
  check(!expected.frame().surface->source_colors.empty()&&baked.frame().surface->source_colors.empty(),"Raw source field affects source shading exactly once and resolved target suppresses duplicate tint");
  auto value=draw(native,baked.frame().surface);int source_error=delta(raw,value);
  std::cout<<"SOURCE_BAKE case="<<number<<" seam="<<seam<<" pose="<<posed<<" alpha="<<int(alpha)<<" source_full_image_maxerror="<<source_error<<std::endl;
  check(source_error<=2,"Actual prepared Bake preserves independently rendered source RGBA and alpha within two encoded levels");
  auto path=directory/(std::to_string(number)+".dsculpt");baked.save(path);Document reopened;reopened.open(path);
  check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==baked.copy_scene().encode(),"Actual saved source/target color authority restarts exactly");
  auto glb=path;glb.replace_extension(".glb");reopened.export_asset(glb).get();color_consumer::Model model(glb);auto &primitive=model.data->meshes[0].primitives[0];
  check(!color_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_color)&&primitive.material->pbr_metallic_roughness.base_color_texture.texture,"Actual resolved target exports its prepared PBR texture without a duplicate COLOR_0 accessor");
  auto restart=draw(native,reopened.frame().surface);fbo.bind();independent.render(model.data,nullptr,{},true);auto external=read();auto export_error=delta(restart,external);
  std::cout<<"SOURCE_BAKE_GLB case="<<number<<" maxerror="<<export_error<<std::endl;
  check(export_error<=1,"Independent cgltf/PNG/GLSL rendering matches actual exported baked target color and alpha");
  check(current_color_bake_fixture::source_only(reopened.copy_scene(),source)==source_value&&baked.copy_scene().object(source).polygon->colors==field,"Current source basis, all original layers/UVs, rig, morph and capsules remain exact through bake/restart/export");
  ++number;
 }
 std::cout<<checks<<" actual current source Bake render/export checks in "<<number<<" cases\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
