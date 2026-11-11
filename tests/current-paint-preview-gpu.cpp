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
struct Copy {GLuint from,to;GLint64 source_size,destination_size;GLintptr source_offset,destination_offset;GLsizeiptr bytes;};
struct Upload {GLuint buffer;GLintptr offset;GLsizeiptr bytes;};
std::vector<Copy> copies;std::vector<Upload> uploads;
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
void __real_glCopyBufferSubData(GLenum,GLenum,GLintptr,GLintptr,GLsizeiptr);
void __wrap_glCopyBufferSubData(GLenum a,GLenum b,GLintptr c,GLintptr d,GLsizeiptr e){if(production){GLint from=0,to=0;GLint64 source=0,destination=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&from);glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING,&to);glGetBufferParameteri64v(a,GL_BUFFER_SIZE,&source);glGetBufferParameteri64v(b,GL_BUFFER_SIZE,&destination);copies.push_back({GLuint(from),GLuint(to),source,destination,c,d,e});}__real_glCopyBufferSubData(a,b,c,d,e);}
void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
void __wrap_glBufferSubData(GLenum a,GLintptr b,GLsizeiptr c,const void*d){if(production&&a==GL_COPY_WRITE_BUFFER){GLint buffer=0;glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING,&buffer);uploads.push_back({GLuint(buffer),b,c});}__real_glBufferSubData(a,b,c,d);}
void __real_glFinish();void __wrap_glFinish(){if(production)++finishes;__real_glFinish();}
}
namespace {
using Compiler=dome::detail::CurrentPaintPreviewCompiler;
using Pixels=std::vector<uint8_t>;
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct Fbo{GLuint f=0,c=0,d=0;static constexpr int size=96;Fbo(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Independent render framebuffer complete");}~Fbo(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}void bind(){glBindFramebuffer(GL_FRAMEBUFFER,f);}};
Pixels draw(SurfaceRenderer&r,const std::shared_ptr<const MeshSnapshot>&s,bool toon=false,bool light=false,std::span<const ObjectInfo>display={}){production=true;try{r.draw(s,{},identity,{0,0,2},96,96,true,toon,light,display);}catch(...){production=false;throw;}production=false;Pixels p(96*96*4);glReadPixels(0,0,96,96,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
int delta(const Pixels&a,const Pixels&b){check(a.size()==b.size(),"Reference pixel shape agrees");int error=0;for(size_t i=0;i<a.size();++i)error=std::max(error,std::abs(int(a[i])-int(b[i])));return error;}
void prepare(Compiler&c,std::vector<std::shared_ptr<const dome::detail::CurrentGridPaintPreview>>p,size_t budget=1ull<<30){production=true;try{c.prepare(p,budget);}catch(...){production=false;throw;}production=false;}
Pixels bytes(GLuint texture,uint32_t r){Pixels result(size_t(r)*r*4);glBindTexture(GL_TEXTURE_2D,texture);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,result.data());check(glGetError()==GL_NO_ERROR,"Validation-only byte texture read succeeds");return result;}
std::vector<float> floats(GLuint texture,uint32_t r){std::vector<float>result(size_t(r)*r*4);glBindTexture(GL_TEXTURE_2D,texture);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,result.data());check(glGetError()==GL_NO_ERROR,"Validation-only FLOAT texture read succeeds");return result;}
long double decode(long double x){return x<=.04045L?x/12.92L:std::pow((x+.055L)/1.055L,2.4L);}
void compare_object(Compiler&c,const SculptObject&o,const std::shared_ptr<const dome::detail::CurrentGridPaintPreview>&p,bool require_radiance=true){
 const auto&r=c.at(p);const auto&paint=*o.painted;const auto size=paint.resolution();const auto old=p->physical_emission()?dome::detail::EmissionExportAccess::other_channels(paint,o.material,{}):paint.has_physical_emission()?dome::detail::EmissionExportAccess::preview(paint,o.material,{}):paint.composite(o.material);
 check(bytes(r.textures[0],size)==old->color.rgba,"Every canonical GPU color RGBA byte matches independent old CPU image");check(bytes(r.textures[1],size)==old->metal_rough.rgba,"Every canonical GPU ORM byte matches independent old CPU image");
 if(!r.physical){check(bytes(r.textures[2],size)==old->emission.rgba,"Every canonical GPU emission byte matches independent old CPU image");return;}
 const auto legacy=floats(r.textures[2],size),coverage=floats(r.textures[3],size),physical=floats(r.textures[4],size);double maximum=0;bool tiny=false,hdr=false;
 for(uint32_t y=0;y<size;++y)for(uint32_t x=0;x<size;++x){std::array<long double,3>l{},h{};long double alpha=0;const auto at=(size_t(y)*size+x)*4;const auto pad=p->layers()->padding()->at(size_t(y)*size+x);if(pad>=0){auto px=uint32_t(pad)%size,py=uint32_t(pad)/size;for(const auto&layer:paint.layers())if(layer.visible&&layer.opacity>0){auto q=paint.pixel(layer.id,px,py);auto value=paint.emission_pixel(layer.id,px,py);long double a=q.data[13]/255.L*layer.opacity*value.coverage;for(size_t k=0;k<3;++k){l[k]=l[k]*(1-a)+value.legacy[k]*a;h[k]=h[k]*(1-a)+value.physical[k]*a;}alpha=alpha*(1-a)+a;}}
  for(size_t k=0;k<4;++k){const std::array<long double,3>values{k<3?l[k]:alpha,k==3?alpha:0,k<3?h[k]:alpha};const std::array<float,3>actual{legacy[at+k],coverage[at+k],physical[at+k]};for(size_t plane=0;plane<3;++plane){double error=std::abs(double(values[plane])-actual[plane]);maximum=std::max(maximum,error/std::max(1e-8,std::abs(double(values[plane]))));check(error<=std::max(1e-12,std::abs(double(values[plane]))*4e-6),"Independent native FLOAT emission/coverage oracle agrees at every padded texel");}}
  tiny|=physical[at+3]>0&&physical[at+3]<1.f/255;hdr|=physical[at]>1;
 }
 if(require_radiance)check(tiny&&hdr,"SubUNORM8 real coverage and HDR-derived radiance survive independent FLOAT planes");std::cout<<"FLOAT_MAX_RELATIVE "<<maximum<<'\n';
}
void compare(Compiler&c,const Scene&s,const std::shared_ptr<const dome::detail::CurrentGridPaintPreview>&p){compare_object(c,s.active(),p);}
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
  auto tinted=std::make_shared<MeshSnapshot>(*frame);tinted->source_colors.assign(tinted->mesh.positions.size(),{.7f,.3f,.8f,.53f});auto old_tint=legacy_frame(*tinted,scene);for(bool weighted:{false,true}){if(weighted){tinted->selected_weights.assign(tinted->mesh.positions.size(),.37f);old_tint->selected_weights=tinted->selected_weights;}std::array<ObjectInfo,1>display{};display[0].id=scene.active().id;display[0].visible=true;display[0].ghost=true;display[0].selected=true;display[0].hovered=true;f.bind();auto a=draw(current,tinted,true,false,display);f.bind();check(delta(a,draw(old,old_tint,true,false,display))<=1,"Actual current-grid route preserves source vertex color/alpha, selection ghost and weighted view semantics");}
  check(scene.encode()==native,"GPU compilation/rendering preserves complete native layers and hidden/FLOAT metadata");
 }
 {
  auto scene=fixture(512,false);auto p=dome::detail::CurrentGridPaintPreview::for_render(scene.active())->current_grid_preview;Compiler c;prepare(c,{p});auto original=bytes(c.at(p).textures[0],512);auto first=c.stats();PaintSettings ink;ink.color={.9f,.2f,.7f,.41f};stroke(*scene.active().painted,ink,{.101f,.041f,0},.8f,.005f);auto q=dome::detail::CurrentGridPaintPreview::for_render(scene.active())->current_grid_preview;
  pending=true;prepare(c,{q});auto changed=c.stats();check(changed.payload_bytes>first.payload_bytes&&changed.payload_bytes-first.payload_bytes<=4*16384&&changed.common_tiles-first.common_tiles<=4,"Actual sparse512 edit uploads only changed native COW tiles");check(changed.padding_bytes==first.padding_bytes&&changed.quantization_bytes==first.quantization_bytes,"Sparse edit does not reupload unchanged atlas padding or canonical quantization table");check(changed.retired_bytes>0&&changed.requested_bytes>=changed.retired_bytes,"Old requested stores stay charged while latest fence is pending");
  auto third=scene;third.active().material.color[2]=.19f;auto r=dome::detail::CurrentGridPaintPreview::for_render(third.active())->current_grid_preview;const auto live=c.at(q).textures;check(refuses([&]{prepare(c,{r},changed.requested_bytes);})&&c.at(q).textures==live,"Pending old/new retirement debt refuses new stores before invalidating published cache");compare(c,scene,q);pending=false;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(c.stats().retired_bytes&&std::chrono::steady_clock::now()<deadline){prepare(c,{q});std::this_thread::yield();}check(c.stats().retired_bytes==0&&c.stats().requested_bytes<changed.requested_bytes,"Zero-time completed fence collects old stores and releases exact requested-store debt");
  for(int fault=0;fault<3;++fault){if(fault==0)fence_fault=1;if(fault==1)query_fault=1;if(fault==2)buffer_fault=1;auto before=c.at(q).textures;check(refuses([&]{prepare(c,{r});})&&c.at(q).textures==before,"Real GL fence/query/allocation rejection preserves preceding published resource");check(bytes(c.at(q).textures[0],512)==reference(scene)->color.rgba,"Rejected GPU staging leaves prior texture image exact");prepare(c,{r});compare(c,third,r);prepare(c,{q});}
  check(original!=bytes(c.at(q).textures[0],512),"Sparse COW update produces actual changed GPU pixels");
 }
 {
  // One genuine Scene owns five actual duplicate actors sharing the same rest
  // mesh/padding, with distinct native COW common tiles. No fabricated source
  // bindings, synthetic packet, private paint write or copied image authority.
  Scene fields;fields.active().mesh_active=true;fields.active().painted=std::make_shared<PaintedMesh>(quad(),64);const auto first=fields.active().id;
  std::vector<uint64_t>actors;std::vector<std::shared_ptr<const dome::detail::CurrentGridPaintPreview>>packets;std::vector<nlohmann::json>native;
  for(size_t i=0;i<5;++i){if(i)fields.duplicate(first);auto&o=fields.active();PaintSettings ink;ink.color={float(i+1)/7,.19f,.73f,.31f};stroke(*o.painted,ink,{-.2f,-.2f,0},.7f,.04f);actors.push_back(o.id);native.push_back(o.painted->encode());packets.push_back(dome::detail::CurrentGridPaintPreview::for_render(o)->current_grid_preview);}
  const auto exact=fields.encode();Compiler growth;auto initial=packets;initial.pop_back();prepare(growth,initial);auto before=growth.stats();check(before.common_tiles==4&&before.payload_bytes==4*16384,"Four actual sparse actor fields fill exactly four common input slots");
  const auto copy_start=copies.size(),upload_start=uploads.size();prepare(growth,packets);const auto after=growth.stats();check(copies.size()==copy_start+1,"A fifth real COW field executes exactly one genuine GPU store copy");const auto copy=copies.back();
  check(copy.from&&copy.to&&copy.from!=copy.to&&copy.source_size==3072+4*16384&&copy.destination_size==3072+8*16384&&copy.source_offset==0&&copy.destination_offset==0&&copy.bytes==copy.source_size,"Actual GL stores grow capacity4 to8 and copy every old native byte/table entirely on the GPU");
  size_t common_uploads=0,common_bytes=0;for(size_t i=upload_start;i<uploads.size();++i)if(uploads[i].buffer==copy.to){++common_uploads;common_bytes+=size_t(uploads[i].bytes);check(uploads[i].offset==3072+4*16384,"Growth uploads only the genuinely new fifth native slot");}
  check(common_uploads==1&&common_bytes==16384&&after.payload_bytes-before.payload_bytes==16384&&after.common_tiles-before.common_tiles==1&&after.quantization_bytes==before.quantization_bytes&&after.padding_bytes==before.padding_bytes,"Copied old slots and canonical table/padding are never reuploaded through the CPU");
  check(fields.encode()==exact,"GPU input growth preserves the entire genuine five-actor native Scene");
  // Existing resolved images could conceal a broken copy. Force new real
  // material resolves for all old slot owners, retaining the same COW inputs.
  const auto resolve_upload=uploads.size(),resolve_copy=copies.size();std::vector<std::shared_ptr<const dome::detail::CurrentGridPaintPreview>>revised;
  for(size_t i=0;i<actors.size();++i){auto&o=fields.object(actors[i]);if(i<4)o.material.color={.12f+float(i)*.13f,.27f,.63f,.81f};revised.push_back(dome::detail::CurrentGridPaintPreview::for_render(o)->current_grid_preview);}
  prepare(growth,revised);check(growth.stats().builds==after.builds+4&&growth.stats().payload_bytes==after.payload_bytes&&copies.size()==resolve_copy,"Every copied old slot is re-resolved after growth without another copy or native payload upload");
  check(std::none_of(uploads.begin()+resolve_upload,uploads.end(),[&](const auto&u){return u.buffer==copy.to;}),"Actual GL API records prove no CPU reupload of copied old slots during re-resolve");
  for(size_t i=0;i<actors.size();++i){compare_object(growth,fields.object(actors[i]),revised[i]);check(fields.object(actors[i]).painted->encode()==native[i],"Every old/new slot retains exact native paint after real GPU-copy replay");}
 }
 for(bool hidden:{false,true}){
  Scene scene=hidden?fixture(64,true):Scene{};if(hidden)scene.active().painted->layer_properties("Hidden physical owner",false,.73f);else{scene.active().mesh_active=true;scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);scene.active().material.emissive={100,.000001f,3};}
  const auto exact=scene.encode();auto p=dome::detail::CurrentGridPaintPreview::for_render(scene.active())->current_grid_preview;check(p->physical_emission()!=hidden,"Bare HDR background keeps FLOAT interpretation while hidden FLOAT payload preserves visible legacy quantization");Compiler c;prepare(c,{p});compare_object(c,scene.active(),p,false);Document d;auto actual=published(d,scene);Fbo f;SurfaceRenderer current,old;f.bind();auto pixels=draw(current,actual);f.bind();check(delta(pixels,draw(old,legacy_frame(*actual,scene)))<=1,"Bare HDR background and hidden-FLOAT cases match actual independent legacy renderer");check(scene.encode()==exact,"Bare HDR/hidden-FLOAT GPU views preserve every native value and metadata");
 }
 check(forbidden_reads==0&&blocking_waits==0&&finishes==0,"Actual current-grid producer/cache/renderer performs no GPU readback, blocking wait or glFinish; reads occur only in validation");check(glGetError()==GL_NO_ERROR,"GPU controls leave no outstanding GL error");std::cout<<"PASS "<<checks<<" current-grid GPU/reference/cache/resource checks\n";
}catch(const std::exception&e){production=pending=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
