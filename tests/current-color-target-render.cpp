// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_suite
#include "mtoon-render.cpp"
#undef main
#include "current-color-target-fixture.hpp"
#include "dome/color-preparation.hpp"
#include "polygon-color-preparation.hpp"
#include "source-color-consumer.hpp"
namespace {
dome::Scene prepared_target(dome::Scene scene){
 auto &target=scene.active();dome::detail::strict_polygon_color_target(target,scene.world_transform(target.id),{}, {},true);
 auto source=std::make_shared<dome::PolygonColorSurface>();source->topology=std::make_shared<const dome::PolygonMesh>(target.polygon->topology);source->field=target.polygon->colors;
 dome::ColorPreparationOptions options;options.resolve_identity=true;
 auto output=dome::prepare_source_colors({target.painted,target.appearance,target.material_bindings,target.deformation,target.material,source},options);
 target.painted=std::move(output.paint);target.material_bindings=std::move(output.material_bindings);scene.validate();return scene;
}
void paint(dome::Scene &scene){
 auto &o=scene.active();o.painted=o.painted->snapshot();dome::Brush brush;brush.from=brush.to={0,0,.5f};brush.radius=.8f;
 dome::PaintSettings settings;settings.front_only=false;settings.flow=.4f;settings.color={.8f,.25f,.55f,.65f};
 o.painted->begin_stroke();o.painted->stamp(brush,settings);o.painted->end_stroke();o.painted->layer_properties("Fractional retained edit",true,.6f);
 o.painted->add_layer("Hidden retained edit");o.painted->layer_properties("Hidden retained edit",false,.25f);
}
bool independent_projection_probe(const dome::Scene &scene){
 const auto &object=scene.active();const auto &recipe=*object.material_bindings->color_preparation;
 const auto &image=*recipe.projection->vertex_rgba;const auto &mesh=object.painted->mesh();
 auto colors=object.polygon->colors->packed(object.polygon->topology);
 for(size_t face=0;face<mesh.indices.size();face+=3)for(int a=0;a<=24;++a)for(int b=0;b<=24-a;++b){
  const double w[]={double(a)/24,double(b)/24,1-double(a+b)/24};double uv[2]{};double expected[4]{};
  for(size_t k=0;k<3;++k){auto v=mesh.indices[face+k];for(size_t c=0;c<2;++c)uv[c]+=w[k]*mesh.uv[v][c];for(size_t c=0;c<4;++c)expected[c]+=w[k]*colors[v][c];}
  const double x=uv[0]*image.width-.5,y=uv[1]*image.height-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));
  for(size_t c=0;c<4;++c){double actual=0;for(int k=0;k<4;++k){int px=std::clamp(ix+(k&1),0,int(image.width)-1),py=std::clamp(iy+(k>>1),0,int(image.height)-1);const double weight=((k&1)?x-ix:1-x+ix)*((k>>1)?y-iy:1-y+iy);actual+=weight*image.rgba[(size_t(py)*image.width+px)*4+c]/255.0;}
   if(std::abs(actual-expected[c])>recipe.projection->maximum_error[c]+2e-6)return false;
  }
 }
 return true;
}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
 NativeGraphics context;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||device.find("A770")!=std::string::npos,"Current target consumer uses the requested actual device");
 Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;size_t number=0;
 {
  auto source=current_color_target_fixture::fixture();auto prepared=prepared_target(source.snapshot());
  check(prepared.active().painted->resolution()==64&&independent_projection_probe(prepared),"Independent barycentric/bilinear probes confirm actual default64 field stays inside its declared linear RGBA bound");
  auto bad=*prepared.active().material_bindings->color_preparation;auto wrong=std::make_shared<PaintColorProjection>(*bad.projection);auto pixels=std::make_shared<Image>(*wrong->vertex_rgba);pixels->rgba.assign(pixels->rgba.size(),255);wrong->vertex_rgba=pixels;bad.projection=bad.initial_projection=wrong;
  bool refused=false;try{bad.validate();}catch(const std::invalid_argument&){refused=true;}check(refused,"A wrong projected field cannot borrow retained geometry/receipt to pass actual quality validation");
  Document document;document.replace(prepared);check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual default64 projection reaches native render publication");auto expected=draw(native,document.frame().surface);
  auto glb=directory/"default64.glb";document.export_asset(glb).get();color_consumer::Model parsed(glb);fbo.bind();independent.render(parsed.data,nullptr,{},true);auto error=delta(expected,read());std::cout<<"TARGET_DEFAULT64_GLB maxerror="<<error<<std::endl;
  check(error<=1,"Independent exported default64 target matches its actual bounded native appearance");
 }
 for(bool seam:{false,true})for(bool posed:{false,true})for(bool painted:{false,true})for(auto alpha:{Material::AlphaMode::Automatic,Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  // Resolution is an explicit native import option, not a test-only raster
  // resize. Default64 is separately checked against its accepted quality and
  // strict refusal; a256 import supports the tighter image fidelity oracle.
  auto original=current_color_target_fixture::fixture(seam,256);auto target=original.active().id,source=original.objects().back().id;
  for(auto id:{source,target}){original.object(id).material.color={.72f,.83f,.61f,.7f};original.object(id).material.alpha=alpha;original.object(id).material.alpha_cutoff=.35f;}
  if(painted)paint(original);
  auto field=original.active().polygon->colors;auto preserved=current_color_bake_fixture::source_only(original.snapshot(),source);
  auto prepared=prepared_target(original.snapshot());
  auto reference=original.snapshot();if(posed){cut_color_fixture::pose(reference);cut_color_fixture::pose(prepared);}
  Document before;before.replace(reference);check(before.wait_mesh()&&before.frame().state->error.empty(),"Actual raw current target publishes its original RGBA in the selected pose");auto raw=draw(native,before.frame().surface);
  Document converted;converted.replace(prepared);check(converted.wait_mesh()&&converted.frame().state->error.empty()&&converted.frame().surface->source_colors.empty(),"Actual target projection publishes without applying original corner tint twice");auto value=draw(native,converted.frame().surface);auto preparation_error=delta(raw,value);
  std::cout<<"TARGET_PREP case="<<number<<" seam="<<seam<<" pose="<<posed<<" painted="<<painted<<" alpha="<<int(alpha)<<" maxerror="<<preparation_error<<std::endl;
  check(preparation_error<=2,"Native target preparation preserves original current RGBA/paint/alpha within two encoded levels");
  auto path=directory/(std::to_string(number)+"-prepared.dsculpt");converted.save(path);Document reopened;reopened.open(path);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==prepared.encode(),"Actual target projection restarts with its complete original field and editable layers");
  auto glb=path;glb.replace_extension(".glb");reopened.export_asset(glb).get();color_consumer::Model model(glb);auto &primitive=model.data->meshes[0].primitives[0];
  check(!color_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_color)&&primitive.material->pbr_metallic_roughness.base_color_texture.texture,"Prepared target exports its actual PBR plane with no duplicate COLOR_0 accessor");
  fbo.bind();independent.render(model.data,nullptr,{},true);auto exported=read();auto export_error=delta(value,exported);std::cout<<"TARGET_PREP_GLB case="<<number<<" maxerror="<<export_error<<std::endl;
  check(export_error<=1,"Independent cgltf/PNG/GLSL matches actual target preparation including alpha and evaluated pose");
  // The Bake target is at authoritative REST, as required by the public API.
  Document baked;baked.replace(original.snapshot());ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;settings.padding=4;baked.bake_from_object(source,settings);
  check(baked.wait_asset()&&baked.wait_mesh()&&baked.frame().state->error.empty(),"Actual guarded native target Bake creates a real separately resolved result");
  auto baked_scene=baked.copy_scene();if(posed)cut_color_fixture::pose(baked_scene);
  Document shown;shown.replace(baked_scene);check(shown.wait_mesh()&&shown.frame().state->error.empty(),"Accepted target Bake renders with its saved current field and later native pose");auto baked_pixels=draw(native,shown.frame().surface);
  path=directory/(std::to_string(number)+"-baked.dsculpt");shown.save(path);Document restarted;restarted.open(path);check(restarted.wait_asset()&&restarted.wait_mesh(),"Actual baked target Save/reopen reaches immutable render publication");glb=path;glb.replace_extension(".glb");restarted.export_asset(glb).get();color_consumer::Model baked_model(glb);
  fbo.bind();independent.render(baked_model.data,nullptr,{},true);auto baked_export=delta(baked_pixels,read());std::cout<<"TARGET_BAKE_GLB case="<<number<<" maxerror="<<baked_export<<std::endl;
  check(baked_export<=1,"Independent GLB consumer matches the actual current-target Bake and pose");
  check(current_color_bake_fixture::source_only(restarted.copy_scene(),source)==preserved&&baked.copy_scene().active().polygon->colors==field,"Target preparation/Bake/export preserves original immutable field owner and independent read-only source");
  ++number;
 }
 std::cout<<checks<<" actual current target render/export checks in "<<number<<" cases\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
