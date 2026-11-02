// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "normal-color-uv-render.cpp"
#undef DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "dome/color-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "normal-sampling-access.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/paint-sampling.hpp"
namespace {
Scene retain_writable(const Scene &scene) {
 const auto &object=scene.active();UvWorkspace uv(object.painted->mesh(),256);const auto &m=object.painted->mesh();size_t face=0;while(face<m.indices.size()/3&&m.normals[m.indices[face*3]].z<.9f)++face;
 std::array<float,2> p{};for(size_t k=0;k<3;++k){auto q=m.uv[m.indices[face*3+k]];for(size_t d=0;d<2;++d)p[d]+=q[d]/3;}
 uv.edit(UvAction::Pick,p[0],p[1]);uv.edit(UvAction::Scale,.7f);uv.edit(UvAction::Rotate,1.57079632679f);auto view=uv.view();if(!view.valid)throw std::runtime_error("Retained writable UV draft invalid");
 auto derivative=prepare_polygon_uv_derivative(object,*view.mesh,*view.source_vertices,{});
 auto descriptor=std::make_shared<const PolygonColorSurface>(PolygonColorSurface{std::make_shared<const PolygonMesh>(derivative.authority->topology),derivative.authority->colors});
 auto result=retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,derivative.source_vertices);
 SourceUvBakeInput input;input.paint=object.painted;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
 SourceUvBakeResult normal_result;SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;
 auto normal=detail::retain_normal_sampling(input,normal_result,options,result.paint->paint_sampling_domain());
 auto joint=adopt_retained_paint_normal(*result.paint,*normal);auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);binding->color_preparation=result.colors;
 auto preparation=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);preparation->projection=normal->projection;preparation->sampling_domain=normal;preparation->uv_reprojection.reset();binding->normal_preparation=std::move(preparation);
 auto output=scene.snapshot();auto &out=output.active();out.painted=joint;out.material_bindings=binding;out.polygon=derivative.authority;out.deformation=detail::normal_frame_remap_deformation(object.deformation,object.painted->mesh(),derivative.mesh,derivative.source_vertices,{});output.validate();return output;
}
}
int main(int argc,char **argv)try {
 if(argc<2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix);
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Writable domain fixture uses actual requested graphics device");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;size_t number=0;
 for(bool seam:{false,true})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}) {
  auto original=joint::prepare(seam,alpha);auto current=retain_writable(original);auto file=prefix/(std::to_string(number)+".dsculpt");current.save(file);current=Scene::load(file);
  check(current.encode().at("version")==24,"Actual fractional scale/turn restarts its owning Scene24 normal/colour/paint closure");
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}) {
   auto before=original,after=current;posed(before,pose,weight);posed(after,pose,weight);Document b,a;b.replace(before);a.replace(after);settled(b);settled(a);
   auto surface=a.frame().surface;check(!surface->paint_coordinates.empty()&&surface->paint_coordinates.size()==surface->mesh.positions.size(),"Worker publishes explicit retained paint coordinates beside independent normal coordinates");
   auto pixels=draw(renderer,b.frame().surface);auto result=draw(renderer,surface);size_t covered=0;std::set<std::array<uint8_t,4>> varied;for(size_t p=0;p<pixels.size();p+=4)if(pixels[p+3]){++covered;varied.insert({pixels[p],pixels[p+1],pixels[p+2],pixels[p+3]});}
   check(covered>100&&varied.size()>16,"Quality oracle covers varying partial-bake colour/alpha and independent normal detail");auto error=delta(pixels,result);std::cout<<"PAINT_DOMAIN seam="<<seam<<" alpha="<<int(alpha)<<" pose="<<pose<<" morph="<<weight<<" max="<<error<<std::endl;
   if(error>2){joint::capture(prefix/"failure-before.png",pixels);joint::capture(prefix/"failure-current.png",result);before.save(prefix/"failure-before.dsculpt");after.save(prefix/"failure-current.dsculpt");}
   check(error<=2,"Same writable grid preserves actual before/after partial Bake through fractional UV, pose, morph and alpha");
   const auto stats=renderer.buffer_stats();check(result==draw(renderer,surface)&&renderer.buffer_stats().geometry_uploads==stats.geometry_uploads,"Second eye reuses the authenticated immutable paint stream without geometry upload");
   if(pose==0&&weight==0) {
    joint::capture(prefix/(std::to_string(number)+"-before.png"),pixels);joint::capture(prefix/(std::to_string(number)+"-current.png"),result);
    auto bad=std::make_shared<MeshSnapshot>(*surface);bad->geometry_identity.reset();bad->paint_coordinates[bad->mesh.indices.front()][0]+=.01f;check(rejects([&]{draw(renderer,bad);}),"Renderer refuses changed paint corner sampling rather than borrowing normal authority");
    bad=std::make_shared<MeshSnapshot>(*surface);bad->geometry_identity.reset();bad->paint_coordinates.clear();check(rejects([&]{draw(renderer,bad);}),"Renderer refuses omitted paint stream");
   }
   auto path=prefix/(std::to_string(number)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");b.export_asset(path).get();prepared_normal_consumer::Model model(path);fbo.bind();independent.render(model.data,nullptr);auto reference=read();auto external=delta(result,reference);std::cout<<"PAINT_ORIGINAL_GLB max="<<external<<std::endl;check(external<=3,"Independent original-basis GLB consumer agrees with current writable-domain pixels");
  }
  ++number;
 }
 std::cout<<"PASS "<<checks<<" retained writable-domain graphical checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
