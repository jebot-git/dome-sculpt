// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-paint-preview-fixture.hpp"
#include "dome/render.hpp"
#include "render-current-paint-preview.hpp"
#include "graphics.h"
#include <EGL/eglext.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
using namespace current_fixture;
namespace {
bool production=false,pending=false;int fence_fault=0,query_fault=0,buffer_fault=0;size_t forbidden_reads=0,blocking_waits=0,finishes=0;
}
extern "C" {
void __real_glGetTexImage(GLenum,GLint,GLenum,GLenum,void*);
void __wrap_glGetTexImage(GLenum a,GLint b,GLenum c,GLenum d,void*e){if(production)++forbidden_reads;__real_glGetTexImage(a,b,c,d,e);}
void __real_glReadPixels(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
void __wrap_glReadPixels(GLint a,GLint b,GLsizei c,GLsizei d,GLenum e,GLenum f,void*g){if(production)++forbidden_reads;__real_glReadPixels(a,b,c,d,e,f,g);}
void __real_glGetBufferSubData(GLenum,GLintptr,GLsizeiptr,void*);
void __wrap_glGetBufferSubData(GLenum a,GLintptr b,GLsizeiptr c,void*d){if(production)++forbidden_reads;__real_glGetBufferSubData(a,b,c,d);}
GLenum __real_glClientWaitSync(GLsync,GLbitfield,GLuint64);
GLenum __wrap_glClientWaitSync(GLsync a,GLbitfield b,GLuint64 c){if(production&&c)++blocking_waits;if(query_fault){--query_fault;return __real_glClientWaitSync(a,2,0);}if(pending)return GL_TIMEOUT_EXPIRED;return __real_glClientWaitSync(a,b,c);}
GLsync __real_glFenceSync(GLenum,GLbitfield);
GLsync __wrap_glFenceSync(GLenum a,GLbitfield b){if(fence_fault){--fence_fault;return __real_glFenceSync(a,1);}return __real_glFenceSync(a,b);}
void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
void __wrap_glBufferData(GLenum a,GLsizeiptr b,const void*c,GLenum d){if(buffer_fault){--buffer_fault;__real_glBufferData(a,-1,c,d);}else __real_glBufferData(a,b,c,d);}
void __real_glFinish();void __wrap_glFinish(){if(production)++finishes;__real_glFinish();}
}
namespace {
using Compiler=dome::detail::CurrentPaintPreviewCompiler;
using Pixels=std::vector<uint8_t>;
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct Fbo{GLuint f=0,c=0,d=0;static constexpr int size=96;Fbo(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Independent render framebuffer complete");}~Fbo(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}void bind(){glBindFramebuffer(GL_FRAMEBUFFER,f);}};
Pixels draw(SurfaceRenderer&r,const std::shared_ptr<const MeshSnapshot>&s,bool toon=false,bool light=false){production=true;try{r.draw(s,{},identity,{0,0,2},96,96,true,toon,light);}catch(...){production=false;throw;}production=false;Pixels p(96*96*4);glReadPixels(0,0,96,96,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
int delta(const Pixels&a,const Pixels&b){check(a.size()==b.size(),"Reference pixel shape agrees");int error=0;for(size_t i=0;i<a.size();++i)error=std::max(error,std::abs(int(a[i])-int(b[i])));return error;}
void prepare(Compiler&c,std::vector<std::shared_ptr<const dome::detail::CurrentGridPaintPreview>>p,size_t budget=1ull<<30){production=true;try{c.prepare(p,budget);}catch(...){production=false;throw;}production=false;}
Pixels bytes(GLuint texture,uint32_t r){Pixels result(size_t(r)*r*4);glBindTexture(GL_TEXTURE_2D,texture);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,result.data());check(glGetError()==GL_NO_ERROR,"Validation-only byte texture read succeeds");return result;}
std::vector<float> floats(GLuint texture,uint32_t r){std::vector<float>result(size_t(r)*r*4);glBindTexture(GL_TEXTURE_2D,texture);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,result.data());check(glGetError()==GL_NO_ERROR,"Validation-only FLOAT texture read succeeds");return result;}
long double decode(long double x){return x<=.04045L?x/12.92L:std::pow((x+.055L)/1.055L,2.4L);}
void compare(Compiler&c,const Scene&s,const std::shared_ptr<const dome::detail::CurrentGridPaintPreview>&p){
 const auto&r=c.at(p);const auto&paint=*s.active().painted;const auto size=paint.resolution();const auto old=p->physical_emission()?dome::detail::EmissionExportAccess::other_channels(paint,s.active().material,{}):reference(s);
 check(bytes(r.textures[0],size)==old->color.rgba,"Every canonical GPU color RGBA byte matches independent old CPU image");check(bytes(r.textures[1],size)==old->metal_rough.rgba,"Every canonical GPU ORM byte matches independent old CPU image");
 if(!r.physical){check(bytes(r.textures[2],size)==old->emission.rgba,"Every canonical GPU emission byte matches independent old CPU image");return;}
 const auto legacy=floats(r.textures[2],size),coverage=floats(r.textures[3],size),physical=floats(r.textures[4],size);double maximum=0;bool tiny=false,hdr=false;
 for(uint32_t y=0;y<size;++y)for(uint32_t x=0;x<size;++x){std::array<long double,3>l{},h{};long double alpha=0;const auto at=(size_t(y)*size+x)*4;const auto pad=p->layers()->padding()->at(size_t(y)*size+x);if(pad>=0){auto px=uint32_t(pad)%size,py=uint32_t(pad)/size;for(const auto&layer:paint.layers())if(layer.visible&&layer.opacity>0){auto q=paint.pixel(layer.id,px,py);auto value=paint.emission_pixel(layer.id,px,py);long double a=q.data[13]/255.L*layer.opacity*value.coverage;for(size_t k=0;k<3;++k){l[k]=l[k]*(1-a)+value.legacy[k]*a;h[k]=h[k]*(1-a)+value.physical[k]*a;}alpha=alpha*(1-a)+a;}}
  for(size_t k=0;k<4;++k){const std::array<long double,3>values{k<3?l[k]:alpha,k==3?alpha:0,k<3?h[k]:alpha};const std::array<float,3>actual{legacy[at+k],coverage[at+k],physical[at+k]};for(size_t plane=0;plane<3;++plane){double error=std::abs(double(values[plane])-actual[plane]);maximum=std::max(maximum,error/std::max(1e-8,std::abs(double(values[plane]))));check(error<=std::max(1e-12,std::abs(double(values[plane]))*4e-6),"Independent native FLOAT emission/coverage oracle agrees at every padded texel");}}
  tiny|=physical[at+3]>0&&physical[at+3]<1.f/255;hdr|=physical[at]>1e-4f;
 }
 check(tiny&&hdr,"SubUNORM8 real coverage and HDR-derived radiance survive independent FLOAT planes");std::cout<<"FLOAT_MAX_RELATIVE "<<maximum<<'\n';
}
void identity_check(bool hardware){
 const auto renderer=std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));std::cout<<"GL_RENDERER "<<renderer<<'\n';if(!hardware)return;
 auto query=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));auto device_string=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));EGLAttrib device=0;check(query&&device_string&&query(eglGetCurrentDisplay(),EGL_DEVICE_EXT,&device),"Strict Arc test authenticates actual current EGLDevice");const char*node=device_string(reinterpret_cast<EGLDeviceEXT>(device),EGL_DRM_RENDER_NODE_FILE_EXT);check(node&&std::string(node)=="/dev/dri/renderD129","Strict Arc EGLDevice exposes expected DRM rendernode");std::string vendor,id;const auto sys=std::filesystem::path("/sys/class/drm/renderD129/device");std::ifstream(sys/"vendor")>>vendor;std::ifstream(sys/"device")>>id;check(std::filesystem::canonical(sys).filename()=="0000:03:00.0"&&vendor=="0x8086"&&id=="0x56a0"&&renderer.find("Arc")!=std::string::npos&&renderer.find("UHD")==std::string::npos,"Strict Arc controls verify EGL/DRM/PCI and actual GL renderer together");std::cout<<"EGL_NODE "<<node<<" PCI 0000:03:00.0 "<<vendor<<' '<<id<<'\n';
}
}
int main(int argc,char**argv)try{
 NativeGraphics graphics;identity_check(argc>1&&std::string_view(argv[1])=="--hardware");set_paint_composite_cache_budget(0);
 for(bool physical:{false,true}){auto scene=fixture(64,physical);auto native=scene.encode();Document d;auto frame=published(d,scene);auto p=owner(*frame);Compiler compiler;
  glEnable(GL_BLEND);glEnable(GL_SCISSOR_TEST);glViewport(2,3,17,19);prepare(compiler,{p});GLint viewport[4]{};glGetIntegerv(GL_VIEWPORT,viewport);check(glIsEnabled(GL_BLEND)&&glIsEnabled(GL_SCISSOR_TEST)&&viewport[0]==2&&viewport[1]==3&&viewport[2]==17&&viewport[3]==19,"Current-grid compiler restores actual caller GL state");glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);compare(compiler,scene,p);
  auto previous=compiler.stats();prepare(compiler,{p,p});auto unchanged=compiler.stats();check(previous.builds==unchanged.builds&&previous.payload_bytes==unchanged.payload_bytes&&previous.padding_bytes==unchanged.padding_bytes&&previous.directory_bytes==unchanged.directory_bytes&&previous.quantization_bytes==unchanged.quantization_bytes,"Repeated unchanged contributions issue zero builds or native input/directory/padding uploads");
  Fbo f;SurfaceRenderer current,old;for(bool toon:{false,true})for(bool light:{false,true})for(bool unlit:{false,true})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){auto candidate=scene;candidate.active().material.unlit=unlit;candidate.active().material.alpha=alpha;candidate.active().material.alpha_cutoff=.6f;auto actual=published(d,candidate);auto legacy=legacy_frame(*actual,candidate);f.bind();auto a=draw(current,actual,toon,light);f.bind();auto b=draw(old,legacy,toon,light);auto error=delta(a,b);if(error>1)std::cerr<<"render physical="<<physical<<" toon="<<toon<<" light="<<light<<" unlit="<<unlit<<" alpha="<<int(alpha)<<" error="<<error<<'\n';check(error<=1,"Actual GPU current-grid renderer agrees with legacy image/physical reference across PBR/simple-Toon/alpha/material modes");f.bind();check(draw(current,actual,toon,light)==a,"Unchanged actual frame renders identically without readback");}
  check(scene.encode()==native,"GPU compilation/rendering preserves complete native layers and hidden/FLOAT metadata");
 }
 {
  auto scene=fixture(512,false);auto p=dome::detail::CurrentGridPaintPreview::for_render(scene.active())->current_grid_preview;Compiler c;prepare(c,{p});auto original=bytes(c.at(p).textures[0],512);auto first=c.stats();PaintSettings ink;ink.color={.9f,.2f,.7f,.41f};stroke(*scene.active().painted,ink,{.101f,.041f,0},.8f,.005f);auto q=dome::detail::CurrentGridPaintPreview::for_render(scene.active())->current_grid_preview;
  pending=true;prepare(c,{q});auto changed=c.stats();check(changed.payload_bytes>first.payload_bytes&&changed.payload_bytes-first.payload_bytes<=4*16384&&changed.common_tiles-first.common_tiles<=4,"Actual sparse512 edit uploads only changed native COW tiles");check(changed.padding_bytes==first.padding_bytes&&changed.quantization_bytes==first.quantization_bytes,"Sparse edit does not reupload unchanged atlas padding or canonical quantization table");check(changed.retired_bytes>0&&changed.requested_bytes>=changed.retired_bytes,"Old requested stores stay charged while latest fence is pending");
  auto third=scene;third.active().material.color[2]=.19f;auto r=dome::detail::CurrentGridPaintPreview::for_render(third.active())->current_grid_preview;const auto live=c.at(q).textures;check(refuses([&]{prepare(c,{r},changed.requested_bytes);})&&c.at(q).textures==live,"Pending old/new retirement debt refuses new stores before invalidating published cache");compare(c,scene,q);pending=false;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(c.stats().retired_bytes&&std::chrono::steady_clock::now()<deadline){prepare(c,{q});std::this_thread::yield();}check(c.stats().retired_bytes==0&&c.stats().requested_bytes<changed.requested_bytes,"Zero-time completed fence collects old stores and releases exact requested-store debt");
  for(int fault=0;fault<3;++fault){if(fault==0)fence_fault=1;if(fault==1)query_fault=1;if(fault==2)buffer_fault=1;auto before=c.at(q).textures;check(refuses([&]{prepare(c,{r});})&&c.at(q).textures==before,"Real GL fence/query/allocation rejection preserves preceding published resource");check(bytes(c.at(q).textures[0],512)==reference(scene)->color.rgba,"Rejected GPU staging leaves prior texture image exact");prepare(c,{r});compare(c,third,r);prepare(c,{q});}
  check(original!=bytes(c.at(q).textures[0],512),"Sparse COW update produces actual changed GPU pixels");
 }
 check(forbidden_reads==0&&blocking_waits==0&&finishes==0,"Actual current-grid producer/cache/renderer performs no GPU readback, blocking wait or glFinish; reads occur only in validation");check(glGetError()==GL_NO_ERROR,"GPU controls leave no outstanding GL error");std::cout<<"PASS "<<checks<<" current-grid GPU/reference/cache/resource checks\n";
}catch(const std::exception&e){production=pending=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
