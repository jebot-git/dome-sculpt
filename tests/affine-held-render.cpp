// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#include "dome/source-placement.hpp"
#include "dome/material-bindings.hpp"
#include <unistd.h>
namespace {bool capture=false;size_t uploads=0;}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr n,const void*p,GLenum usage){if(capture&&(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER))uploads+=n;__real_glBufferData(target,n,p,usage);}
int main(int argc,char**argv)try {
 auto home=std::filesystem::temp_directory_path()/("dome-affine-held-gpu-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<2||std::string(argv[1])!="--hardware"||device.find("llvmpipe")==std::string::npos,"held affine consumer runs on requested device");Fbo fbo;fbo.bind();
 auto matrix=AffineTransform{}.matrix();matrix[0]=-.7;matrix[2]=.25;matrix[4]=.2;matrix[5]=.8;matrix[8]=.2;matrix[12]=.05;matrix[13]=-.03;matrix[14]=.1;
 const float angle=.3f,c=std::cos(angle),s=std::sin(angle);
 auto independent=[&](Vec3 p){Vec3 q{c*p.x+s*p.z,p.y,-s*p.x+c*p.z};return Vec3{-.7f*q.x+.2f*q.y+.2f*q.z+.05f,.8f*q.y-.03f,.25f*q.x+q.z+.1f};};
 const auto origin=independent({}),X=independent({1,0,0})-origin,Y=independent({0,1,0})-origin,Z=independent({0,0,1})-origin;const float det=dot(X,cross(Y,Z));
 auto normal=[&](Vec3 n){return normalized((cross(Y,Z)*n.x+cross(Z,X)*n.y+cross(X,Y)*n.z)/det);};auto tangent=[&](Vec3 t){return normalized(X*t.x+Y*t.y+Z*t.z);};
 for(bool retained:{false,true}) {
   auto mesh=quad();Scene scene;scene.active().material=white();scene.active().material.unlit=false;
   const Vec3 N=normalized(Vec3{.2f,.3f,1}),T=normalized(Vec3{1,.1f,.2f});
   if(retained){auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=image({220,130,240,255});scene.active().painted=PaintedMesh::from_maps(mesh,maps,64);auto first=std::make_shared<SourceUvOrigin>();first->paint=scene.active().painted->snapshot();auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=4;frame->origin_vertices=mesh.indices;frame->normals.assign(6,N);frame->tangents.assign(6,{T.x,T.y,T.z,-1});auto set=std::make_shared<NormalFrameSet>();set->pbr=frame;auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=first;binding->normal_frames=set;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;scene.active().material_bindings=binding;}
   else scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);
   scene.active().mesh_active=true;scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
   Document doc;doc.replace(scene);doc.configure_object_grab(true,true);check(doc.wait_mesh(),"held source worker publishes immutable base");const auto base=doc.frame().surface;const auto before=doc.copy_scene().encode();SurfaceRenderer renderer,reference_renderer;
   auto draw_actual=[&](std::shared_ptr<const MeshSnapshot> snapshot){renderer.draw(std::move(snapshot),{},identity,{0,0,2},96,96,true,false,false,doc.frame().state->objects);return read();};draw_actual(base);
   check(doc.begin_object_grab(scene.active().id),"actual Document begins full affine native hold");CanvasTransform local;local.orientation={0,std::sin(angle/2),0,std::cos(angle/2)};doc.move_held_object(local);
   auto reference=std::make_shared<MeshSnapshot>(*base);reference->geometry_identity.reset();reference->ranges[0].affine_world=doc.frame().state->objects[0].affine_world;
   for(size_t i=0;i<4;++i){reference->mesh.positions[i]=independent(mesh.positions[i]);reference->mesh.normals[i]=normal({0,0,1});}reference->mesh.indices={0,2,1,0,3,2};
   if(retained){reference->source_normals.assign(4,normal(N));auto t=tangent(T);reference->source_tangents.assign(4,{t.x,t.y,t.z,1});}
   auto wanted=draw(reference_renderer,reference);uploads=0;capture=true;auto preview=draw_actual(base);capture=false;
   check(delta(preview,wanted)<=1,retained?"held retained PBR normal/tangent pixels match independent sheared-delta consumer":"held geometric normal pixels use inverse transpose under sheared preview delta");check(uploads==0,"native affine preview uses uniforms without geometry uploads");
   uploads=0;capture=true;check(draw_actual(base)==preview,"second held eye retains exact pixels");capture=false;check(uploads==0,"second held eye makes zero geometry uploads");check(doc.copy_scene().encode()==before,"GPU preview never mutates authoritative source");
   doc.end_object_grab();check(doc.wait_mesh(),"native affine held release publishes complete source");uploads=0;capture=true;auto released=draw_actual(doc.frame().surface);capture=false;
   check(delta(preview,released)<=1,"held preview and committed mesh have no release jump");check(uploads>0,"committed affine geometry changes its authenticated upload key");
   const auto accepted=doc.copy_scene().encode();auto accepted_mesh=doc.frame().surface;check(doc.begin_object_grab(scene.active().id),"second affine hold starts");CanvasTransform move;move.position.x=.1;doc.move_held_object_world(move);check(draw_actual(accepted_mesh)!=released,"world gesture moves the real affine GPU preview");doc.end_object_grab(false);uploads=0;capture=true;auto cancelled=draw_actual(accepted_mesh);capture=false;check(cancelled==released&&uploads==0&&doc.copy_scene().encode()==accepted,"cancel restores exact GPU/source endpoint without geometry uploads");
 }
 std::cout<<checks<<" actual held affine GPU assertions passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
