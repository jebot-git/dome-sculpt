// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_suite
#include "mtoon-render.cpp"
#undef main
namespace {struct Transfers{uint64_t data=0,subdata=0,read=0,map=0;size_t bytes=0;};Transfers transfers;bool watch=false;}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum a,GLsizeiptr n,const void*p,GLenum b){if(watch){++transfers.data;transfers.bytes+=n;}__real_glBufferData(a,n,p,b);}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum a,GLintptr o,GLsizeiptr n,const void*p){if(watch){++transfers.subdata;transfers.bytes+=n;}__real_glBufferSubData(a,o,n,p);}
extern "C" void __real_glReadPixels(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
extern "C" void __wrap_glReadPixels(GLint x,GLint y,GLsizei w,GLsizei h,GLenum f,GLenum t,void*p){if(watch)++transfers.read;__real_glReadPixels(x,y,w,h,f,t,p);}
extern "C" void* __real_glMapBufferRange(GLenum,GLintptr,GLsizeiptr,GLbitfield);
extern "C" void* __wrap_glMapBufferRange(GLenum a,GLintptr b,GLsizeiptr n,GLbitfield f){if(watch)++transfers.map;return __real_glMapBufferRange(a,b,n,f);}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Ghost and held-preview fixture executes on the requested graphics device");
 Fbo target;target.bind();SurfaceRenderer renderer;auto model=snapshot();model->generation=1;model->geometry_identity=std::make_shared<const int>(1);model->ranges[0].world_frame_valid=true;
 ObjectInfo item{};item.id=1;item.visible=true;item.editable=true;item.world_transform_valid=true;std::vector<ObjectInfo> display{item};
 auto render=[&](std::shared_ptr<const MeshSnapshot> mesh,const Mat4 &vp=identity){target.bind();transfers={};watch=true;try{renderer.draw(mesh,{},vp,{0,0,2},96,96,true,false,false,display);}catch(...){watch=false;throw;}watch=false;return read();};
 const auto ordinary=render(model);check(at(ordinary)==std::array<int,4>{255,255,255,255}&&transfers.data>0,"Ordinary authored material draws unchanged while its first buffers upload");auto stat=renderer.buffer_stats();auto texture=renderer.texture_stats();
 display[0].editable=false;display[0].ghost=true;auto ghost=render(model);
 check(ghost!=ordinary&&std::abs(at(ghost)[3]-46)<=1,"Ghost override produces actual transparent pixels for an opaque source material");
 check(!transfers.data&&!transfers.subdata&&!transfers.read&&!transfers.map&&renderer.buffer_stats().geometry_uploads==stat.geometry_uploads,"Ghost-only override performs no geometry/scalar transfer, host readback or mapping");
 auto eye=identity;eye[12]=.02f;render(model,eye);check(!transfers.data&&!transfers.subdata&&renderer.texture_stats().uploads==texture.uploads,"The second stereo eye reuses immutable geometry and textures while ghosting");
 display[0].ghost=false;check(render(model)==ordinary&&!transfers.bytes,"Releasing ghost presentation restores exact ordinary pixels without an upload");
 for(int i=0;i<20;++i){display[0].world_transform.position.x=.2f+.01f*i;auto moved=render(model);check(moved!=ordinary&&!transfers.data&&!transfers.subdata&&!transfers.read&&!transfers.map,"Held motion changes real pixels exclusively through GPU uniforms");}
 auto preview=render(model);auto committed=std::make_shared<MeshSnapshot>(*model);const auto final_delta=display[0].world_transform.position.x;for(auto &p:committed->mesh.positions)p.x+=final_delta;committed->ranges[0].object_world.position.x=final_delta;committed->geometry_identity=std::make_shared<const int>(2);++committed->revision;
 check(render(committed)==preview,"Committed packed-world authority avoids applying the held transform twice at release");
 display[0].visible=false;check(at(render(committed))[3]==0&&!transfers.bytes,"Individual hiding skips its draw immediately without rewriting geometry or material");display[0].visible=true;
 check(render(committed)==preview&&!transfers.bytes,"Showing an individual mesh restores its precise held/source presentation");
 for(bool legacy:{false,true}){auto m=typed(legacy);m.parameters.base_color={.8f,.6f,.4f,1};auto toon=snapshot();toon->generation=2;toon->geometry_identity=std::make_shared<const int>(legacy?4:3);toon->ranges[0].appearance=appearance(m);toon->ranges[0].world_frame_valid=true;display[0].world_transform={};auto original_metadata=toon->ranges[0].appearance->mtoon_json;auto normal=render(toon);auto uploads=renderer.texture_stats().uploads;display[0].ghost=true;auto dim=render(toon);check(dim!=normal&&std::abs(at(dim)[3]-46)<=1&&!transfers.bytes,"Both legacy and modern typed MToon support transparent GPU ghosts without material rewriting");display[0].ghost=false;check(render(toon)==normal&&renderer.texture_stats().uploads==uploads&&toon->ranges[0].appearance->mtoon_json==original_metadata,"Typed MToon restores exact pixels and original metadata after ghost release");}
 auto weighted=snapshot();weighted->generation=3;weighted->geometry_identity=std::make_shared<const int>(5);weighted->selected_weights.assign(4,.5f);weighted->ranges[0].world_frame_valid=true;auto heat=render(weighted);check(close(at(heat),{0,255,0,255}),"Ordinary selected-weight overlay retains its original priority");display[0].ghost=true;auto translucent=render(weighted);check(at(translucent)[3]>0&&at(translucent)[3]<100&&!transfers.bytes,"Ghost rendering cannot disappear behind stored linked-weight flags");
 GLint depth=0,blend=0,active=0;glGetIntegerv(GL_DEPTH_WRITEMASK,&depth);glGetIntegerv(GL_BLEND,&blend);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);check(depth==GL_TRUE&&blend==GL_FALSE&&active==GL_TEXTURE0&&glGetError()==GL_NO_ERROR,"Ghost pass restores ordinary renderer depth/blend/texture state");
 std::cout<<"Ghost GPU checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
