// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_UV_RENDER_NO_MAIN
#include "normal-uv-render.cpp"
#undef DOME_NORMAL_UV_RENDER_NO_MAIN
#include "current-color-target-fixture.hpp"
#include "polygon-color-target.hpp"
#include "dome/uv.hpp"
#include "dome/source-uv-bake.hpp"
#include <png.h>
#include <set>
namespace joint {
void capture(const std::filesystem::path &path,const Pixels &input){
 Pixels output(input.size());for(int y=0;y<Fbo::size;++y)std::copy_n(input.begin()+size_t(y)*Fbo::size*4,Fbo::size*4,output.begin()+size_t(Fbo::size-1-y)*Fbo::size*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=Fbo::size;image.format=PNG_FORMAT_RGBA;if(!png_image_write_to_file(&image,path.c_str(),false,output.data(),0,nullptr))throw std::runtime_error("Joint capture failed");
}

Scene prepare(bool seam,Material::AlphaMode alpha){
 auto scene=current_color_target_fixture::fixture(seam,256);auto source=scene.objects().back().id;
 cut_color_fixture::pose(scene);auto d=*scene.active().deformation;d.pose.clear();d.morph_weights.assign(d.morphs->targets.size(),0);scene.set_deformation(scene.active().id,std::move(d));
 for(auto id:{source,scene.active().id}){auto &object=scene.object(id);object.material.unlit=false;object.material.alpha=alpha;object.material.color={.72f,.83f,.61f,.7f};}
 auto &high=scene.object(source);auto m=high.painted->mesh();for(size_t v=0;v<m.positions.size();++v){auto p=m.positions[v];m.normals[v]=normalized(m.normals[v]+Vec3{.19f*p.y,.23f*p.x,.11f*p.z});}for(auto &p:m.positions)p.x=(p.x-.2f)*.5f;high.painted=std::make_shared<PaintedMesh>(m,256);
 Document doc;doc.replace(scene);settled(doc);ObjectBakeSettings settings;settings.prepare_colors=settings.prepare_normals=true;settings.ao_samples=0;settings.allow_partial=true;doc.bake_from_object(source,settings);settled(doc);scene=doc.copy_scene();scene.active().transform.scale=2.5f;return scene;
}
Scene transfer(const Scene &scene){
 const auto &object=scene.active();UvWorkspace uv(object.painted->mesh(),256);auto &m=object.painted->mesh();size_t face=0;while(face<m.indices.size()/3&&m.normals[m.indices[face*3]].z<.9f)++face;
 std::array<float,2> p{};for(size_t k=0;k<3;++k){auto q=m.uv[m.indices[face*3+k]];for(size_t d=0;d<2;++d)p[d]+=q[d]/3;}uv.edit(UvAction::Pick,p[0],p[1]);uv.edit(UvAction::Scale,.7f);uv.edit(UvAction::Rotate,1.57079632679f);auto view=uv.view();if(!view.valid)throw std::runtime_error("Joint UV draft invalid");
 auto derivative=prepare_polygon_uv_derivative(object,*view.mesh,*view.source_vertices,{});SourceUvBakeInput input;input.paint=object.painted;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.polygon_authority=object.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
 SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;auto adopted=adopt_source_uv_bake(bake_source_uv(input,options));auto result=scene.snapshot();auto &out=result.active();out.painted=adopted.paint;out.material_bindings=adopted.material_bindings;out.appearance=adopted.appearance;out.deformation=adopted.deformation;out.polygon=derivative.authority;result.validate();return result;
}
}
#ifndef DOME_NORMAL_COLOR_RENDER_NO_MAIN
int main(int argc,char **argv)try{
 if(argc<2)return 2;auto prefix=std::filesystem::path(argv[1]);std::filesystem::create_directories(prefix);NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;check(argc<3||std::string_view(argv[2])!="--hardware"||(device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Joint fields use the requested actual graphics device");Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;unsigned number=0;
 for(bool seam:{false,true})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  auto original=joint::prepare(seam,alpha);auto changed=joint::transfer(original);check(changed.active().painted->normal_projection()->version==2&&changed.active().material_bindings->color_preparation->paint_sampling,"Actual fractional .7 scale and 90-degree turn own independent prepared fields");auto project=prefix/(std::to_string(number)+".dsculpt");changed.save(project);changed=Scene::load(project);check(changed.encode().at("version")==24,"Graphical consumers use an actually restarted joint Scene24");
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
   auto before=original,after=changed;posed(before,pose,weight);posed(after,pose,weight);Document b,a;b.replace(before);a.replace(after);settled(b);settled(a);auto pixels=draw(renderer,b.frame().surface);auto current=draw(renderer,a.frame().surface);size_t covered=0;std::set<std::array<uint8_t,4>> samples;for(size_t p=0;p<pixels.size();p+=4)if(pixels[p+3]){++covered;samples.insert({pixels[p],pixels[p+1],pixels[p+2],pixels[p+3]});}std::cout<<"JOINT_VISIBLE covered="<<covered<<" samples="<<samples.size()<<std::endl;check(covered>100&&samples.size()>16,"Pose/alpha quality oracle covers genuine varying visible shaded RGBA pixels");if(pose==0&&weight==0){joint::capture(prefix/(std::to_string(number)+"-before.png"),pixels);joint::capture(prefix/(std::to_string(number)+"-current.png"),current);}auto error=delta(pixels,current);std::cout<<"JOINT_NATIVE seam="<<seam<<" alpha="<<int(alpha)<<" pose="<<pose<<" weight="<<weight<<" max="<<error<<std::endl;
   if(error>2){joint::capture(prefix/"failure-before.png",pixels);joint::capture(prefix/"failure-current.png",current);before.save(prefix/"failure-before.dsculpt");after.save(prefix/"failure-current.dsculpt");auto ub=before,ua=after;ub.active().material.unlit=ua.active().material.unlit=true;Document db,da;db.replace(ub);da.replace(ua);settled(db);settled(da);auto bx=draw(renderer,db.frame().surface),ax=draw(renderer,da.frame().surface);joint::capture(prefix/"failure-before-unlit.png",bx);joint::capture(prefix/"failure-current-unlit.png",ax);std::cout<<"JOINT_UNLIT max="<<delta(bx,ax)<<std::endl;}
   check(error<=2,"Before/after lit normal+RGBA survives fractional scale/turn through mixed skin/morph and alpha");
   const auto path=prefix/(std::to_string(number)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");b.export_asset(path).get();prepared_normal_consumer::Model file(path);fbo.bind();independent.render(file.data,nullptr);auto reference=read();const auto reference_error=delta(current,reference);std::cout<<"JOINT_ORIGINAL_GLB max="<<reference_error<<std::endl;check(reference_error<=3,"Independent original-domain GLB/PNG/GLSL oracle agrees with the restarted current normal+colour fields");
   const auto emitted=prefix/(std::to_string(number)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+"-current.glb");a.export_asset(emitted).get();prepared_normal_consumer::Model output(emitted);fbo.bind();independent.render(output.data,nullptr);auto compiled=read();auto compiled_error=delta(current,compiled);std::cout<<"JOINT_CURRENT_GLB max="<<compiled_error<<std::endl;check(compiled_error<=3,"Independent newly emitted current GLB preserves native lossless normal and writable paint domains");
  }++number;
 }
 std::cout<<"PASS "<<checks<<" joint normal+colour graphical checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}

#endif
