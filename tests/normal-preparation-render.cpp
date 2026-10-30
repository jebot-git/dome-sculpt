// SPDX-License-Identifier: GPL-3.0-or-later
#define main legacy_mtoon_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/normal-preparation.hpp"
#include "normal-preparation-fixtures.hpp"
#include "prepared-normal-consumer.hpp"
namespace {
bool normal_capture=false;size_t normal_texture_calls=0,normal_buffer_calls=0;
}
extern "C" void __real_glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
extern "C" void __wrap_glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei width,GLsizei height,GLint border,GLenum format,GLenum type,const void *pixels){if(normal_capture)++normal_texture_calls;__real_glTexImage2D(target,level,internal,width,height,border,format,type,pixels);}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage){if(normal_capture)++normal_buffer_calls;__real_glBufferData(target,size,data,usage);}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr size,const void *data){if(normal_capture)++normal_buffer_calls;__real_glBufferSubData(target,offset,size,data);}
int main(int argc,char **argv)try {
  if(argc<2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());
  NativeGraphics graphics;const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<'\n';
  check(argc<3||std::string(argv[2])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Requested prepared-normal consumer graphics device is active");
  auto scene=normal_preparation_fixture::model();ObjectBakeSettings settings;settings.prepare_normals=true;settings.front_mm=500;settings.back_mm=50;settings.ao_samples=1;settings.padding=2;
  auto report=bake_object(scene,2,settings);check(report.normal_prepared&&scene.active().painted->baked(),"Actual native curved target is baked into the explicit normal attribute model before pose testing");
  scene.active().transform.position={-.6f,-.6f,0};scene.active().transform.scale=1.2f;
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;prepared_normal_consumer::Renderer independent;
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
    auto current=scene;auto deformation=*current.active().deformation;deformation.morph_weights={weight};
    if(pose){deformation.pose.resize(2);deformation.pose[0].scale=pose==1?Vec3{1.1f,.85f,.55f}:Vec3{.8f,1.05f,1.6f};
      const float angle=pose==1?.2f:-.25f;deformation.pose[1].rotation={std::cos(angle/2),0,std::sin(angle/2),0};deformation.pose[1].scale=pose==1?Vec3{.9f,1.1f,1.5f}:Vec3{1.15f,.8f,.7f};}
    current.set_deformation(current.active().id,deformation);Document document;document.replace(current);check(document.wait_mesh(),"Actual worker transports prepared native morph directions through mixed anisotropic joint poses");
    auto path=prefix;path+="-pose"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".glb";document.export_asset(path).get();prepared_normal_consumer::Model file(path);
    const auto wanted=draw(native,document.frame().surface);const auto texture_stats=native.texture_stats();
    normal_texture_calls=normal_buffer_calls=0;normal_capture=true;const auto repeated=draw(native,document.frame().surface);normal_capture=false;
    check(repeated==wanted&&normal_texture_calls==0&&normal_buffer_calls==0&&native.texture_stats().uploads==texture_stats.uploads&&native.texture_stats().pack_builds==texture_stats.pack_builds,"Actual second-eye/static prepared draw performs zero texture/buffer allocation or upload and reuses immutable source packs");
    framebuffer.bind();independent.render(file.data);const auto actual=read();const auto error=delta(wanted,actual);
    size_t covered=0;for(size_t n=3;n<wanted.size();n+=4)covered+=wanted[n]>0;
    std::cout<<"PREPARED_NORMAL_CONSUMER pose="<<pose<<" weight="<<weight<<" maxerror="<<error<<" covered="<<covered<<'\n';
    check(covered>1000,"Normal-map comparison measures a visible curved surface rather than an empty frame");
    check(error<=3,"Independent cgltf/PNG/morph/weighted inverse-transpose skin/GLSL consumer matches new native normal-map pixels");
    if(pose==0&&weight==0){auto &normal=file.data->meshes[0].primitives[0].material->normal_texture;auto texture=normal.texture;normal.texture=nullptr;framebuffer.bind();independent.render(file.data);const auto flat=read();normal.texture=texture;check(flat!=actual,"Independent exported pixels visibly depend on the actual prepared normal map rather than only transported geometric normals");}
  }
  std::cout<<"Prepared-normal consumer checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
