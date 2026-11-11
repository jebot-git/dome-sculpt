// SPDX-License-Identifier: GPL-3.0-or-later
#include "render-store-ledger.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <new>
#include <cstdlib>
#include <cstring>
using Ledger=dome::detail::ContextStoreLedger;
using Kind=Ledger::Kind;using Domain=Ledger::Domain;
namespace {
size_t checks=0,names=0,fences=0,waits=0;bool pending=false;int fence_fault=0,query_fault=0,delete_fault=0;
thread_local bool fail_alloc=false;thread_local size_t alloc_point=0,alloc_seen=0;
void allocation(){if(fail_alloc&&++alloc_seen>=alloc_point)throw std::bad_alloc();}
void check(bool p,const char*why){++checks;if(!p)throw std::runtime_error(why);}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}
struct Graphics {
 EGLDisplay d=EGL_NO_DISPLAY;EGLContext c=EGL_NO_CONTEXT;EGLSurface s=EGL_NO_SURFACE;
 Graphics(){auto platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));check(platform,"Real surfaceless EGL platform available");d=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);EGLint major,minor;check(d!=EGL_NO_DISPLAY&&eglInitialize(d,&major,&minor),"Real EGL initialized");check(eglBindAPI(EGL_OPENGL_API),"Actual OpenGL API selected");const EGLint config[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};EGLConfig cfg;EGLint count;check(eglChooseConfig(d,config,&cfg,1,&count)&&count==1,"Real EGL config selected");const EGLint attrs[]{EGL_CONTEXT_MAJOR_VERSION,3,EGL_CONTEXT_MINOR_VERSION,3,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,attrs);const EGLint size[]{EGL_WIDTH,4,EGL_HEIGHT,4,EGL_NONE};s=eglCreatePbufferSurface(d,cfg,size);check(c!=EGL_NO_CONTEXT&&s!=EGL_NO_SURFACE&&eglMakeCurrent(d,s,s,c),"Real OpenGL3.3 context current");std::cout<<"GL_RENDERER "<<glGetString(GL_RENDERER)<<'\n';}
 ~Graphics(){eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(c!=EGL_NO_CONTEXT)eglDestroyContext(d,c);if(s!=EGL_NO_SURFACE)eglDestroySurface(d,s);if(d!=EGL_NO_DISPLAY)eglTerminate(d);}
};
void hardware(){auto q=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));auto str=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));EGLAttrib dev=0;check(q&&str&&q(eglGetCurrentDisplay(),EGL_DEVICE_EXT,&dev),"Actual EGLDevice queried");const char*node=str(reinterpret_cast<EGLDeviceEXT>(dev),EGL_DRM_RENDER_NODE_FILE_EXT);check(node,"Actual EGL DRM node available");const auto n=std::filesystem::path(node).filename();const auto sys=std::filesystem::path("/sys/class/drm")/n/"device";std::string vendor,id;std::ifstream(sys/"vendor")>>vendor;std::ifstream(sys/"device")>>id;const std::string renderer(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));check(std::filesystem::canonical(sys).filename()=="0000:03:00.0"&&vendor=="0x8086"&&id=="0x56a0"&&renderer.find("Arc")!=std::string::npos&&renderer.find("A770")!=std::string::npos,"Real context authenticates strictPCI03 A770");std::cout<<"ACTUAL_DEVICE "<<node<<" PCI0000:03:00.0 "<<vendor<<' '<<id<<'\n';}
Ledger::Store buffer(const std::shared_ptr<Ledger>&l,Ledger::Group g,size_t n,Domain d=Domain::Material){auto p=l->create(Kind::Buffer,n,d,g);glBindBuffer(GL_COPY_WRITE_BUFFER,p.name());std::vector<unsigned char>bytes(n,37);glBufferData(GL_COPY_WRITE_BUFFER,GLsizeiptr(n),bytes.data(),GL_DYNAMIC_DRAW);check(glGetError()==GL_NO_ERROR,"Actual requested buffer initialized");return p;}
void drain(const std::shared_ptr<Ledger>&l){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!l->collect()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();check(l->usage(Domain::Material).retired_stores+l->usage(Domain::Geometry).retired_stores==0,"Actual zero-time fences drain within finite10s validation bound");}
void exact(GLuint id,size_t n,unsigned char value=37){std::vector<unsigned char>v(n);glBindBuffer(GL_COPY_READ_BUFFER,id);glGetBufferSubData(GL_COPY_READ_BUFFER,0,GLsizeiptr(n),v.data());check(glGetError()==GL_NO_ERROR&&std::all_of(v.begin(),v.end(),[&](auto x){return x==value;}),"Validation-only readback proves original actual GPU bytes exact");}
}
void*operator new(size_t n){allocation();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void*operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}void*operator new[](size_t n,const std::nothrow_t&t)noexcept{return ::operator new(n,t);}void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
void*operator new(size_t n,std::align_val_t a){allocation();void*p=nullptr;if(posix_memalign(&p,std::max(size_t(a),sizeof(void*)),n?n:1)==0)return p;throw std::bad_alloc();}void*operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
void*operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}void*operator new[](size_t n,std::align_val_t a,const std::nothrow_t&t)noexcept{return ::operator new(n,a,t);}void operator delete(void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
extern "C" {
void __real_glGenBuffers(GLsizei,GLuint*);void __wrap_glGenBuffers(GLsizei n,GLuint*p){names+=size_t(n);__real_glGenBuffers(n,p);}
void __real_glDeleteBuffers(GLsizei,const GLuint*);void __wrap_glDeleteBuffers(GLsizei n,const GLuint*p){if(delete_fault){--delete_fault;__real_glDeleteBuffers(-1,p);}else __real_glDeleteBuffers(n,p);}
GLsync __real_glFenceSync(GLenum,GLbitfield);GLsync __wrap_glFenceSync(GLenum a,GLbitfield b){++fences;if(fence_fault){--fence_fault;return __real_glFenceSync(a,1);}return __real_glFenceSync(a,b);}
GLenum __real_glClientWaitSync(GLsync,GLbitfield,GLuint64);GLenum __wrap_glClientWaitSync(GLsync a,GLbitfield b,GLuint64 c){++waits;if(c)std::abort();if(query_fault){--query_fault;return __real_glClientWaitSync(a,2,0);}if(pending)return GL_TIMEOUT_EXPIRED;return __real_glClientWaitSync(a,b,c);}
}
int main(int argc,char**argv)try{
 Graphics graphics;if(argc>1&&std::string_view(argv[1])=="--hardware")hardware();else check(std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))).find("llvmpipe")!=std::string::npos,"Software control uses actual llvmpipe");
 Ledger::Limits limits;limits.material_bytes=512;limits.material_images=4;limits.material_samplers=2;limits.geometry_bytes=512;
 auto ledger=std::make_shared<Ledger>(limits);const auto a=ledger->create_group(),b=ledger->create_group();
 {
  auto p=buffer(ledger,a,128);auto q=buffer(ledger,b,256);const auto id=p.name(),stable=q.name();auto copy=p;p.reset();check(glIsBuffer(id)&&ledger->usage(a).retired_stores==0,"Copyable lease keeps actual accepted resource alive");copy.reset();check(glIsBuffer(id)&&ledger->usage(a).bytes==128&&ledger->usage(a).retired_bytes==128,"Last lease drop neither deletes actual name nor credits bytes");
  const auto before=names;check(refuses([&]{ledger->create(Kind::Buffer,192,Domain::Material,b);})&&names==before,"Shared groups' old/new/debt quota refuses before actual GL generation");
  pending=true;check(!ledger->collect()&&glIsBuffer(id)&&ledger->usage(Domain::Material).bytes==384,"Controlled timeout retains real GL stores and shared charged debt");pending=false;drain(ledger);check(!glIsBuffer(id)&&ledger->usage(Domain::Material).bytes==256,"Real completed fence deletes prior name and releases exact shared bytes");exact(stable,256);
  auto next=buffer(ledger,a,256);check(ledger->usage(Domain::Material).bytes==512,"Same actual budget admits new store only after prior completion");next.reset();q.reset();drain(ledger);
 }
 {
  auto p=buffer(ledger,a,128);const auto id=p.name();ledger->seal(true);const auto before=fences;p.reset();drain(ledger);check(fences==before&&!glIsBuffer(id),"CPU-only retirement after forced frame seal reuses actual presealed coverage");
  auto source=buffer(ledger,a,128),destination=buffer(ledger,b,128);ledger->seal(true);const auto old=fences;ledger->mark_used();std::array<unsigned char,128>v;v.fill(73);glBindBuffer(GL_COPY_READ_BUFFER,source.name());glBufferSubData(GL_COPY_READ_BUFFER,0,128,v.data());glBindBuffer(GL_COPY_WRITE_BUFFER,destination.name());glCopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,0,0,128);check(glGetError()==GL_NO_ERROR,"Real use after seal queues actual GPU copy");source.reset();drain(ledger);check(fences==old+1,"Use after earlier seal requires a newer actual fence before retirement credit");exact(destination.name(),128,73);destination.reset();drain(ledger);
 }
 {
  auto p=buffer(ledger,a,128);const auto id=p.name();std::array<Ledger::Store,1>parents{p};auto view=ledger->create(Kind::Texture,0,Domain::Material,b,parents);glBindTexture(GL_TEXTURE_BUFFER,view.name());glTexBuffer(GL_TEXTURE_BUFFER,GL_R32F,p.name());check(glGetError()==GL_NO_ERROR,"Actual zero-byte TBO view owns genuine buffer dependency");
  p.reset();parents[0].reset();check(ledger->usage(Domain::Material).images==1&&ledger->usage(Domain::Material).bytes==128,"TBO name and underlying requested bytes are independently charged");
  check(ledger->collect()&&glIsBuffer(id),"A genuine TBO alias alone keeps its actual buffer lease charged");
  view.reset();drain(ledger);check(!glIsBuffer(id),"Completed TBO retirement releases both real view and backing store");
 }
 drain(ledger);
 {
  auto p=buffer(ledger,a,128,Domain::Geometry);const auto id=p.name();auto vao=ledger->create(Kind::VertexArray,0,Domain::Geometry,b,std::span(&p,1));glBindVertexArray(vao.name());glBindBuffer(GL_ARRAY_BUFFER,p.name());glVertexAttribPointer(0,1,GL_FLOAT,GL_FALSE,4,nullptr);glEnableVertexAttribArray(0);check(glGetError()==GL_NO_ERROR,"Actual VAO retains typed geometry dependency");p.reset();check(ledger->usage(Domain::Geometry).bytes==128,"Geometry lease stays charged while actual VAO retains its buffer");vao.reset();drain(ledger);check(!glIsBuffer(id)&&ledger->usage(Domain::Geometry).bytes==0,"Completed alias retirement safely releases both real VAO and buffer");
 }
 {
  auto stable=buffer(ledger,a,64);for(int fault=0;fault<3;++fault){auto p=buffer(ledger,b,64);const auto id=p.name();p.reset();auto before=ledger->usage(Domain::Material).bytes;
   if(fault==0){fence_fault=1;check(refuses([&]{ledger->seal();}),"Real invalid GL fence creation refuses");}
   if(fault==1){ledger->seal();query_fault=1;check(refuses([&]{ledger->collect();}),"Real invalid GL wait query refuses");}
   if(fault==2){fence_fault=1;ledger->seal_noexcept();check(refuses([&]{ledger->collect();}),"Rollback seal failure is latched and reported before later collection");}
   check(glIsBuffer(id)&&ledger->usage(Domain::Material).bytes==before,"Actual fence failure preserves owned GL names and charged retired stores");exact(stable.name(),64);drain(ledger);check(!glIsBuffer(id),"Real subsequent fence retry reclaims only completed rejected stores");}
  stable.reset();drain(ledger);
 }
 {
  std::array<Ledger::Store,4> textures;std::array<GLuint,4> ids{};
  for(size_t i=0;i<4;++i){textures[i]=ledger->create(Kind::Texture,64,Domain::Material,a);ids[i]=textures[i].name();glBindTexture(GL_TEXTURE_2D,ids[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);check(glIsTexture(ids[i])&&glGetError()==GL_NO_ERROR,"Real image store initialized with requested RGBA8 bytes");}
  check(ledger->usage(a).bytes==256&&ledger->usage(a).images==4,"All actual texture stores share image/name and byte quotas");textures[0].reset();check(refuses([&]{ledger->create(Kind::Texture,64,Domain::Material,b);})&&glIsTexture(ids[0]),"Retired image names remain charged before completion");drain(ledger);check(!glIsTexture(ids[0]),"Actual image is deleted only after its completed fence");auto replacement=ledger->create(Kind::Texture,64,Domain::Material,b);for(auto&t:textures)t.reset();replacement.reset();drain(ledger);
  std::array<Ledger::Store,2> samplers{ledger->create(Kind::Sampler,0,Domain::Material,a),ledger->create(Kind::Sampler,0,Domain::Material,b)};check(glIsSampler(samplers[0].name())&&glIsSampler(samplers[1].name())&&ledger->usage(Domain::Material).samplers==2,"Actual sampler names are bounded independently from texture bytes");const auto sampler=samplers[0].name();samplers[0].reset();check(refuses([&]{ledger->create(Kind::Sampler,0,Domain::Material,a);})&&glIsSampler(sampler),"Sampler retirement cannot grant premature name credit");drain(ledger);check(!glIsSampler(sampler),"Real completed sampler retirement releases its name");samplers[1].reset();drain(ledger);
  auto rejected=buffer(ledger,a,64);const auto id=rejected.name();rejected.reset();ledger->seal(true);glFinish();delete_fault=1;check(refuses([&]{ledger->collect();})&&glIsBuffer(id)&&ledger->usage(a).bytes==64,"Real GL deletion failure preserves actual name and charged completed debt");drain(ledger);check(!glIsBuffer(id),"Subsequent actual deletion retry releases completed store exactly once");
 }
 {
  auto parent=buffer(ledger,a,64);std::array<Ledger::Store,1>deps{parent};size_t count=0;{alloc_seen=0;alloc_point=SIZE_MAX;fail_alloc=true;auto measured=ledger->create(Kind::Buffer,32,Domain::Material,b,deps);fail_alloc=false;count=alloc_seen;measured.reset();drain(ledger);}check(count==2,"Actual helper ownership path has exactly dependency-vector and stable-node allocations before GL");
  for(size_t point=1;point<=count;++point){const auto before=ledger->usage(Domain::Material);const auto generated=names;bool failed=false;alloc_seen=0;alloc_point=point;fail_alloc=true;try{auto p=ledger->create(Kind::Buffer,32,Domain::Material,b,deps);}catch(const std::bad_alloc&){failed=true;}fail_alloc=false;check(failed&&names==generated&&ledger->usage(Domain::Material).bytes==before.bytes&&ledger->usage(Domain::Material).stores==before.stores,"Every persistent helper allocation prefix rolls back before GL and preserves actual accepted stores");exact(parent.name(),64);}std::cout<<"CPP_PREFIXES "<<count<<'\n';
 }
 drain(ledger);
 {
  auto other=std::make_shared<Ledger>(limits);auto p=buffer(other,0,32);check(refuses([&]{ledger->create(Kind::Texture,0,Domain::Material,a,std::span(&p,1));}),"Foreign ledger backing-store authority refuses without borrowed aliases");p.reset();drain(other);
  auto p2=buffer(ledger,a,32);auto held=p2;ledger->clear(true);check(!p2&&!held&&ledger->terminal(),"Terminal GL cleanup leaves outstanding CPU leases valid with zero names");check(refuses([&]{ledger->create(Kind::Buffer,1,Domain::Material,a);}),"Terminal ledger cannot silently restart with forgotten debt");held.reset();p2.reset();
 }
 check(glGetError()==GL_NO_ERROR,"Lifecycle controls leave no unresolved GL errors");std::cout<<"PASS "<<checks<<" real GL store lifecycle/budget/failure controls\n";
}catch(const std::exception&e){fail_alloc=pending=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
