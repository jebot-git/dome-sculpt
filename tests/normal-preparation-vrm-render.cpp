// SPDX-License-Identifier: GPL-3.0-or-later
#define main legacy_mtoon_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "colored-vrm-bake-fixture.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/vrm.hpp"
int main(int argc,char **argv)try {
  if(argc<2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());
  NativeGraphics graphics;const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<'\n';
  check(argc<3||std::string(argv[2])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Prepared VRM independent consumers use the requested graphics device");
  auto scene=colored_vrm_fixture::model(true,true,true);Document document;document.replace(scene);
  ObjectBakeSettings settings;settings.prepare_colors=settings.prepare_normals=true;settings.front_mm=settings.back_mm=500;settings.ao_samples=0;settings.padding=4;settings.allow_partial=true;
  document.bake_from_object(scene.objects().back().id,settings);check(document.wait_asset()&&document.frame().state->error.empty()&&document.wait_mesh(),"Actual curved artist-N/T VRM target adopts color and new normal preparation through the native bake worker");
  scene=document.copy_scene();check(scene.active().material_bindings->normal_preparation&&scene.active().material_bindings->color_preparation&&!scene.active().material_bindings->normal_frames->canonical,"Typed preparation retains both original receipts and artist frames without fabricating canonical authority");
  const auto project=prefix.string()+".dsculpt";document.save(project);Document restart;restart.open(project);check(restart.wait_asset()&&restart.wait_mesh()&&restart.copy_scene().encode()==scene.encode(),"Actual VRM normal/color before-state survives a new native Document restart");
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;prepared_normal_consumer::Renderer consumer;
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
    auto current=scene;auto &o=current.active();auto d=*o.deformation;d.morph_weights={weight};
    if(pose){d.pose[0].scale=pose==1?Vec3{1.1f,.9f,.6f}:Vec3{.8f,1.05f,1.4f};const float a=pose==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};d.pose[1].scale=pose==1?Vec3{.9f,1.05f,1.4f}:Vec3{1.1f,.9f,.8f};}
    current.set_deformation(o.id,d);Document posed;posed.replace(current);check(posed.wait_mesh(),"Native typed worker transports full artist normal/tangent morphs through mixed anisotropic joints");
    auto path=prefix;path+="-pose"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".vrm";posed.export_asset(path,AssetExportFormat::Avatar).get();prepared_normal_consumer::Model file(path);
    const auto wanted=draw(native,posed.frame().surface);framebuffer.bind();consumer.render(file.data,&file.json.at("extensions").at("VRM"));const auto actual=read();const auto error=delta(wanted,actual);
    size_t covered=0;for(size_t n=3;n<wanted.size();n+=4)covered+=wanted[n]>0;std::cout<<"PREPARED_NORMAL_VRM pose="<<pose<<" weight="<<weight<<" maxerror="<<error<<" covered="<<covered<<'\n';
    check(covered>1000&&error<=4,"Independent parsed VRM0 raw-interpolated TBN consumer matches the actual prepared artist-frame normal-map output");
    auto fallback=current;auto binding=std::make_shared<SourceMaterialBindings>(*fallback.active().material_bindings);binding->has_mtoon=false;binding->roles={};fallback.active().material_bindings=binding;fallback.active().material.toon=false;
    Document pbr;pbr.replace(fallback);check(pbr.wait_mesh(),"Separate actual PBR range selects the same compatible transported target attributes");
    const auto pbr_wanted=draw(native,pbr.frame().surface);framebuffer.bind();consumer.render(file.data);const auto pbr_actual=read();const auto pbr_error=delta(pbr_wanted,pbr_actual);
    std::cout<<"PREPARED_NORMAL_VRM_PBR pose="<<pose<<" weight="<<weight<<" maxerror="<<pbr_error<<'\n';
    check(pbr_wanted!=wanted&&pbr_error<=4,"Independent core PBR fallback of the SAME exported VRM preserves its separate normal plane and distinct visible lighting");
  }
  for(float strength:{0.f,.65f,1.6f}){
    auto current=scene;auto binding=std::make_shared<SourceMaterialBindings>(*current.active().material_bindings);binding->material.parameters.normal_scale=strength;current.active().material_bindings=binding;
    auto d=*current.active().deformation;d.morph_weights={.35f};d.pose[0].scale={1.1f,.9f,.6f};d.pose[1].scale={.9f,1.05f,1.4f};current.set_deformation(current.active().id,d);
    Document document;document.replace(current);check(document.wait_mesh(),"Edited prepared MToon normal strength reaches native authoring without losing its immutable recipe");
    auto path=prefix;path+="-strength"+std::to_string(int(strength*100))+".vrm";document.export_asset(path,AssetExportFormat::Avatar).get();prepared_normal_consumer::Model file(path);
    const auto wanted=draw(native,document.frame().surface);framebuffer.bind();consumer.render(file.data,&file.json.at("extensions").at("VRM"));const auto error=delta(wanted,read());
    check(std::abs(file.json.at("extensions").at("VRM").at("materialProperties")[0].at("floatProperties").at("_BumpScale").get<float>()-strength)<1e-6f&&error<=4,"Independent VRM normal pixels preserve current zero/fractional/amplified strength instead of resetting prepared maps to unit scale");
  }
  {
    auto current=scene;auto binding=std::make_shared<SourceMaterialBindings>(*current.active().material_bindings);binding->material.parameters.normal_enabled=false;current.active().material_bindings=binding;
    Document document;document.replace(current);check(document.wait_mesh(),"Prepared typed normal map can be disabled without dropping retained normal images or target attributes");
    auto path=prefix;path+="-disabled.vrm";document.export_asset(path,AssetExportFormat::Avatar).get();prepared_normal_consumer::Model file(path);
    const auto wanted=draw(native,document.frame().surface);framebuffer.bind();consumer.render(file.data,&file.json.at("extensions").at("VRM"));
    const auto &exported=file.json.at("extensions").at("VRM").at("materialProperties")[0];
    check((!exported.contains("keywordMap")||!exported.at("keywordMap").value("_NORMALMAP",false))&&delta(wanted,read())<=4,"Independent VRM consumer honors disabled prepared normal mapping with original planes preserved");
  }
  std::cout<<"Prepared-normal VRM checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
