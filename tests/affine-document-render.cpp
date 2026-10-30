// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#include "dome/source-placement.hpp"
#include "dome/material-bindings.hpp"
#include <unistd.h>
namespace {
bool capture=false;size_t upload_bytes=0;
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr bytes,const void*p,GLenum usage){if(capture&&(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER))upload_bytes+=bytes;__real_glBufferData(target,bytes,p,usage);}
int main(int argc,char**argv)try {
 auto home=std::filesystem::temp_directory_path()/("dome-affine-gpu-"+std::to_string(getpid()));std::filesystem::create_directories(home);
 setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
 check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"requested affine consumer device is hardware");
 Fbo framebuffer;framebuffer.bind();SurfaceRenderer renderer;Document document;
 auto matrix=AffineTransform{}.matrix();matrix[0]=-.7;matrix[2]=.25;matrix[4]=.2;matrix[5]=.8;matrix[12]=.05;matrix[13]=-.03;matrix[14]=.1;
 auto transformed_normal=[](Vec3 n){return normalized(Vec3{-n.x/.7f+.25f*n.z/.7f,n.x*.2f/(.7f*.8f)+n.y/.8f-.25f*.2f*n.z/(.7f*.8f),n.z});};
 auto transformed_tangent=[](Vec3 t){return normalized(Vec3{-.7f*t.x+.2f*t.y,.8f*t.y,.25f*t.x+t.z});};
 for(bool retained:{false,true}) {
   Scene scene;auto mesh=quad();auto material=white();material.unlit=false;material.roughness=.8f;scene.active().material=material;
   const Vec3 N=normalized(Vec3{.2f,.3f,1}),T=normalized(Vec3{1,.1f,.2f});
   if(retained){
     auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=image({220,130,240,255});scene.active().painted=PaintedMesh::from_maps(mesh,maps,64);
     auto origin=std::make_shared<SourceUvOrigin>();origin->paint=scene.active().painted->snapshot();
     auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=4;frame->origin_vertices=mesh.indices;
     frame->normals.assign(6,N);frame->tangents.assign(6,{T.x,T.y,T.z,-1});
     auto frames=std::make_shared<NormalFrameSet>();frames->pbr=frame;
     auto bindings=std::make_shared<SourceMaterialBindings>();bindings->has_mtoon=false;bindings->uv_origin=origin;bindings->normal_frames=frames;bindings->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;scene.active().material_bindings=bindings;
   }else scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);
   scene.active().mesh_active=true;scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
   document.replace(scene);check(document.wait_mesh(std::chrono::seconds(15))&&document.frame().state->error.empty(),"actual affine Document worker publishes complete GPU source");
   const auto actual=document.frame().surface;auto reference=std::make_shared<MeshSnapshot>(*actual);reference->geometry_identity.reset();
   check(actual->mesh.positions.size()==4,"affine fixture retains original vertex identity without unnecessary expansion");
   for(size_t i=0;i<4;++i){auto p=mesh.positions[i];reference->mesh.positions[i]={-.7f*p.x+.2f*p.y+.05f,.8f*p.y-.03f,.25f*p.x+.1f};reference->mesh.normals[i]=transformed_normal({0,0,1});}
   reference->mesh.indices={0,2,1,0,3,2};
   if(retained){reference->source_normals.assign(4,transformed_normal(N));auto t=transformed_tangent(T);reference->source_tangents.assign(4,{t.x,t.y,t.z,1});}
   auto render=[&](std::shared_ptr<const MeshSnapshot> s){renderer.draw(std::move(s),{},identity,{0,0,2},96,96,true,false,false,document.frame().state->objects);return read();};
   auto wanted=render(reference);check(at(wanted)[3]>0,"independent analytic affine baseline renders visible pixels");
   upload_bytes=0;capture=true;auto first=render(actual);capture=false;
   check(delta(first,wanted)<=1,retained?"actual retained PBR normal/tangent/reflection pixels match independent affine consumer":"actual geometric inverse-transpose pixels match independent affine consumer");
   check(upload_bytes>0,"first actual affine source makes real GPU geometry uploads");
   upload_bytes=0;capture=true;auto second=render(actual);capture=false;check(second==first&&upload_bytes==0,"second affine eye reuses actual GPU geometry with identical pixels");
 }
 std::cout<<checks<<" actual affine Document GPU checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
