// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_suite
#include "mtoon-render.cpp"
#undef main
#include "current-color-target-fixture.hpp"
#include "source-color-consumer.hpp"
int main(int argc,char **argv)try{
 if(argc<2)return 2;auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
 NativeGraphics context;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||device.find("A770")!=std::string::npos,"Combined source/target consumer uses the requested actual device");
 Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;size_t number=0;
 for(bool source_seam:{false,true})for(bool posed:{false,true})for(auto alpha:{Material::AlphaMode::Automatic,Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  auto original=current_color_target_fixture::fixture(!source_seam,256);auto target=original.active().id,source=original.objects().back().id;
  auto separate=cut_color_fixture::scene(source_seam,false,256);auto copy=separate.active();copy.id=source;copy.name="Independent read-only current-corner source";copy.locked=true;copy.visible=false;original.object(source)=std::move(copy);
  for(auto id:{source,target}){original.object(id).material.color={.72f,.83f,.61f,.7f};original.object(id).material.alpha=alpha;original.object(id).material.alpha_cutoff=.35f;}
  original.validate();auto before=original.encode();auto source_before=current_color_bake_fixture::source_only(original.snapshot(),source);
  auto source_field=original.object(source).polygon->colors,target_field=original.object(target).polygon->colors;auto target_authority=original.active().polygon;
  check(source_field!=target_field&&source_field->source_owner()!=target_field->source_owner(),"Actual source and target have independently owned distinct current-corner authority");
  auto display=original.snapshot();display.object(target).visible=false;display.object(source).visible=true;display.select(source);if(posed)cut_color_fixture::pose(display);
  Document expected;expected.replace(display);check(expected.wait_mesh()&&expected.frame().state->error.empty(),"Independent current source publishes its saved field and evaluated pose");auto raw=draw(native,expected.frame().surface);
  Document baked;baked.replace(original.snapshot());ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;settings.padding=4;baked.bake_from_object(source,settings);
  check(baked.wait_asset()&&baked.wait_mesh()&&baked.frame().state->error.empty(),"One actual guarded worker prepares a current target and bakes a separate current source");
  auto accepted=baked.copy_scene();auto accepted_value=accepted.encode();const auto recipe=accepted.active().material_bindings->color_preparation;
  check(recipe->receipt.source_field==SourceColorField::PolygonCorners&&recipe->polygon_colors->field==target_field&&accepted.active().polygon->colors==target_field&&accepted.active().polygon->origin->polygon==target_authority,"Target preparation authenticates its own field and retains its own original authority, separately from the source");
  check(current_color_bake_fixture::source_only(accepted.snapshot(),source)==source_before&&accepted.object(source).polygon->colors==source_field,"Actual simultaneous Bake preserves every independent source field, paint, UV and retained capsule");
  check(baked.undo()&&baked.copy_scene().encode()==before&&baked.redo()&&baked.copy_scene().encode()==accepted_value,"Both real adapters form one exact native Undo/Redo transaction");
  if(posed)cut_color_fixture::pose(accepted);Document shown;shown.replace(accepted);check(shown.wait_mesh()&&shown.frame().state->error.empty()&&shown.frame().surface->source_colors.empty(),"Prepared target renders its resolved result without applying its original corner tint twice");
  auto pixels=draw(native,shown.frame().surface);auto error=delta(raw,pixels);std::cout<<"DUAL_BAKE case="<<number<<" seam="<<source_seam<<" pose="<<posed<<" alpha="<<int(alpha)<<" source_maxerror="<<error<<std::endl;
  check(error<=2,"Combined actual Bake preserves independently rendered source RGBA/alpha within two encoded levels");
  auto path=directory/(std::to_string(number)+".dsculpt");shown.save(path);Document reopened;reopened.open(path);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==accepted.encode(),"Both separate current-field authorities and the accepted target receipt restart exactly");
  auto glb=path;glb.replace_extension(".glb");reopened.export_asset(glb).get();color_consumer::Model model(glb);auto &primitive=model.data->meshes[0].primitives[0];check(!color_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_color),"Actual resolved target export has no duplicate raw COLOR_0 tint");
  fbo.bind();independent.render(model.data,nullptr,{},true);auto exported=delta(pixels,read());std::cout<<"DUAL_BAKE_GLB case="<<number<<" maxerror="<<exported<<std::endl;check(exported<=1,"Independent cgltf/PNG/GLSL reproduces the simultaneous source/target Bake and later pose");
  if(!posed){reopened.restore_polygon_source();check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==before,"Native Restore after reopening recovers the exact target source while preserving the independent current source");}
  ++number;
 }
 std::cout<<checks<<" simultaneous current source/target checks in "<<number<<" cases\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
