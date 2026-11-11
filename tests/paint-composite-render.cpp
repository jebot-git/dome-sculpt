// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "source-color-consumer.hpp"
#include "current-paint-preview.hpp"
#include <nlohmann/json.hpp>
namespace {thread_local bool reference_mode=false;}
std::string_view actual_backend() asm("__real__ZN4dome15compute_backendEv");
std::string_view test_backend() asm("__wrap__ZN4dome15compute_backendEv");
std::string_view test_backend(){return reference_mode?std::string_view{"CPU"}:actual_backend();}
namespace {
Scene fixture(uint32_t resolution){
 auto m=quad();m.uv={{{.08f,.08f}},{{.92f,.08f}},{{.92f,.92f}},{{.08f,.92f}}};auto p=PaintedMesh::from_mesh(m,resolution,{});
 for(int layer=0;layer<3;++layer){if(layer)p->add_layer("Independent "+std::to_string(layer));p->layer_properties("Independent "+std::to_string(layer),true,layer==0?.73f:.37f);
  for(int c=0;c<5;++c){PaintSettings s;s.channel=PaintChannel(c);s.color={.017f,.47f,.81f,.63f};s.emission={.91f,.031f,.27f};s.metallic=.83f;s.roughness=.17f;s.mask=.41f;s.flow=.72f;Brush b;b.radius=.29f;b.pressure=.79f;b.strength=.63f;b.normal={0,0,1};b.from={-.69f,-.6f+c*.27f+layer*.04f,0};b.to={.68f,-.4f+c*.24f,0};p->begin_stroke();p->stamp(b,s);p->end_stroke();}
  if(layer==2)p->layer_properties("Independent "+std::to_string(layer),false,.37f);
 }
 Scene scene;scene.active().painted=p;scene.active().mesh_active=true;return scene;
}
bool equal(const TextureMaps&a,const TextureMaps&b){return a.color.rgba==b.color.rgba&&a.metal_rough.rgba==b.metal_rough.rgba&&a.emission.rgba==b.emission.rgba&&a.color_coverage.rgba==b.color_coverage.rgba&&a.emission_coverage.rgba==b.emission_coverage.rgba&&a.transparent==b.transparent;}
}
int main(int argc,char**argv)try{
 if(argc<2)return 2;auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);set_paint_composite_cache_budget(0);NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<renderer<<"\nCOMPUTE "<<actual_backend()<<'\n';
 check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&renderer.find("Arc")!=std::string::npos&&renderer.find("A770")!=std::string::npos),"Requested independent consumer executes on actual Intel Arc A770");Fbo fbo;SurfaceRenderer native;color_consumer::Renderer independent;
 for(uint32_t r:{512u,1024u}){const auto original=fixture(r);for(bool unlit:{false,true})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  auto scene=original;auto &o=scene.active();o.material.color={.23f,.61f,.037f,.67f};o.material.emissive={.013f,.037f,.071f};o.material.metallic=.31f;o.material.roughness=.68f;o.material.unlit=unlit;o.material.alpha=alpha;o.material.alpha_cutoff=.6f;
  const auto raw=o.painted->encode();reference_mode=true;auto expected=PaintedMesh::decode(raw)->composite(o.material);reference_mode=false;auto selected=PaintedMesh::decode(raw)->composite(o.material);check(equal(*selected,*expected),"Selected compositor produces exact legacy CPU maps for all14 channels/layer mask/opacity/padding");
  Document document;document.replace(scene);check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual mesh worker publishes selected-backend plain writable paint");auto snapshot=document.frame().surface;check(bool(snapshot->ranges.front().textures->current_grid_preview)&&snapshot->ranges.front().textures->color.rgba.empty(),"Immutable actual worker frame retains current-grid COW inputs without eager derived image output");snapshot->ranges.front().textures->current_grid_preview->authenticate_range(snapshot->mesh,snapshot->ranges.front(),true);fbo.bind();auto current=draw(native,snapshot);auto legacy=std::make_shared<MeshSnapshot>(*snapshot);legacy->ranges.front().textures=expected;check(draw(native,legacy)==current,"Actual current selected and legacy CPU maps render identically");
  auto path=directory/(std::to_string(r)+"-"+std::to_string(unlit)+"-"+std::to_string(int(alpha))+".glb");document.export_asset(path).get();color_consumer::Model parsed(path);fbo.bind();independent.render(parsed.data,nullptr,{},true);auto error=delta(current,read());std::cout<<"PAINT_COMPOSITE_CONSUMER resolution="<<r<<" unlit="<<unlit<<" alpha="<<int(alpha)<<" maxerror="<<error<<'\n';check(error<=2,"Independent cgltf/PNG/GLSL current GLB consumer matches native14-channel paint");
  check(o.painted->encode()==raw,"Compositing/render/export leave complete source paint exact");auto saved=directory/(path.stem().string()+".dsculpt");document.save_async(saved).get();check(Scene::load(saved).encode()==scene.encode(),"Current native paint save/restart preserves layers/material/source exactly");
 }}std::cout<<"PASS "<<checks<<" paint composite renderer/export checks\n";return 0;
}catch(const std::exception&e){reference_mode=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
