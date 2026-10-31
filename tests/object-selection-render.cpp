// SPDX-License-Identifier: GPL-3.0-or-later
#define main previous_isolation_suite
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
 NativeGraphics graphics;auto gpu=std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));std::cout<<"GPU "<<gpu<<'\n';
 check(argc<2||std::string(argv[1])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Selection executes on requested graphics device");
 Fbo target;SurfaceRenderer renderer;auto model=snapshot();model->geometry_identity=std::make_shared<const int>(1);
 ObjectInfo info{};info.id=1;info.visible=true;std::vector<ObjectInfo> display{info};
 auto paint=[&]{target.bind();transfers={};watch=true;renderer.draw(model,{},identity,{0,0,2},96,96,true,false,false,display);watch=false;return read();};
 auto ordinary=paint();auto uploads=renderer.buffer_stats().geometry_uploads;
 display[0].selected=display[0].active=true;auto active=paint();
 check(active!=ordinary&&at(active)[3]==255&&!transfers.bytes&&!transfers.read&&!transfers.map,"Active selection has visible solid accent using only GPU uniforms");
 display[0].active=false;auto member=paint();check(member!=active&&member!=ordinary&&at(member)[3]==255&&!transfers.bytes,"Multiselected member is solid with distinct active-object appearance");
 display[0].selected=false;display[0].ghost=true;auto ghost=paint();check(std::abs(at(ghost)[3]-46)<=1&&!transfers.bytes,"Unselected object is visibly transparent without altering source material");
 display[0].hovered=true;auto hover=paint();check(hover!=ghost&&at(hover)[3]==at(ghost)[3]&&!transfers.bytes,"Hover cue remains visible on transparent selectable objects");
 display[0].hovered=display[0].ghost=false;check(paint()==ordinary&&!transfers.bytes&&renderer.buffer_stats().geometry_uploads==uploads,"Clearing selection restores exact source pixels and reuses uploaded geometry");
 check(glGetError()==GL_NO_ERROR,"Selection rendering preserves valid GL state");
 std::cout<<"Selection render checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){watch=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
