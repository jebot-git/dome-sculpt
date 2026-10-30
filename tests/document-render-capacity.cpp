// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/render.hpp"
#include "graphics.h"
#include "render-buffer-size.hpp"
#include <cstdlib>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0,array_bytes=0,index_bytes=0,array_calls=0,index_calls=0;
bool capture=false,inject_error=false;
void check(bool yes,const char*label){if(!yes)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}
struct Home {
 std::filesystem::path path;
 Home(){auto pattern=(std::filesystem::temp_directory_path()/"dome-capacity-XXXXXX").string();std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);auto value=mkdtemp(name.data());if(!value)throw std::runtime_error("Cannot isolate capacity fixture");path=value;
 setenv("HOME",path.c_str(),1);for(auto [key,dir]:std::initializer_list<std::pair<const char*,const char*>>{{"XDG_CONFIG_HOME","config"},{"XDG_CACHE_HOME","cache"},{"XDG_STATE_HOME","state"},{"XDG_DATA_HOME","data"},{"XDG_RUNTIME_DIR","runtime"}}){auto p=path/dir;std::filesystem::create_directory(p);setenv(key,p.c_str(),1);}}
 ~Home(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
Mesh quad(size_t count){Mesh m;m.positions.assign(count,{0,0,0});m.normals.assign(count,{0,0,1});m.uv.assign(count,{.5f,.5f});
 m.positions[0]={-.75f,-.75f,0};m.positions[1]={.75f,-.75f,0};m.positions[2]={.75f,.75f,0};m.positions[3]={-.75f,.75f,0};
 m.uv[0]={0,0};m.uv[1]={1,0};m.uv[2]={1,1};m.uv[3]={0,1};m.indices={0,1,2,0,2,3};m.validate();return m;}
Scene scene(size_t vertices){Scene s;
 for(int i=0;i<3;++i){auto id=i?s.add("Independent ordinary mesh",Volume{}):s.active().id;auto&o=s.object(id);o.painted=std::make_shared<PaintedMesh>(quad(vertices),64);o.mesh_active=true;o.material.color={1,1,1,1};o.material.unlit=true;}
 s.validate();return s;}
Scene indexed_scene(){Scene s;constexpr uint32_t side=600;
 for(int object=0;object<3;++object){Mesh m;m.positions.reserve(size_t(side+1)*(side+1));m.normals.reserve(m.positions.capacity());m.uv.reserve(m.positions.capacity());
  for(uint32_t y=0;y<=side;++y)for(uint32_t x=0;x<=side;++x){m.positions.push_back({-.75f+1.5f*x/side,-.75f+1.5f*y/side,0});m.normals.push_back({0,0,1});m.uv.push_back({float(x)/side,float(y)/side});}
  m.indices.reserve(size_t(side)*side*6);for(uint32_t y=0;y<side;++y)for(uint32_t x=0;x<side;++x){auto a=y*(side+1)+x,b=a+1,c=a+side+2,d=a+side+1;m.indices.insert(m.indices.end(),{a,b,c,a,c,d});}
  auto id=object?s.add("Independent indexed grid",Volume{}):s.active().id;auto&o=s.object(id);o.painted=std::make_shared<PaintedMesh>(std::move(m),64);o.mesh_active=true;o.material.color={1,1,1,1};o.material.unlit=true;
 }s.validate();return s;}
struct Target {
 GLuint f=0,c=0,d=0;static constexpr int size=32;
 Target(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Incomplete capacity framebuffer");}
 ~Target(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}
};
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
std::shared_ptr<const MeshSnapshot> ready(Document&doc){
 auto until=std::chrono::steady_clock::now()+std::chrono::seconds(120);
 while(!doc.wait_mesh(std::chrono::milliseconds(25))){auto error=doc.frame().state->error;if(!error.empty())throw std::runtime_error("Capacity Document: "+error);if(std::chrono::steady_clock::now()>=until)throw std::runtime_error("Capacity Document mesh timeout");}
 if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);return doc.frame().surface;}
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void*data,GLenum usage){
 if(capture){if(target==GL_ARRAY_BUFFER){array_bytes+=size;++array_calls;}if(target==GL_ELEMENT_ARRAY_BUFFER){index_bytes+=size;++index_calls;}}
 if(capture&&inject_error&&target==GL_ARRAY_BUFFER&&size>0){inject_error=false;__real_glBufferData(target,-1,data,usage);return;}
 __real_glBufferData(target,size,data,usage);
}
int main(int argc,char**argv)try{
 Home home;bool old=argc>1&&std::string(argv[1])=="--expect-rejection";
 const size_t limit=size_t(std::numeric_limits<GLsizeiptr>::max());
 check(render_detail::buffer_bytes(limit,1)==limit&&render_detail::buffer_sum(limit-1,1)==limit,"Per-buffer size checks accept the exact GLsizeiptr boundary");
 bool overflow=false;try{render_detail::buffer_bytes(limit/56+1,56);}catch(const std::length_error&){overflow=true;}
 check(overflow,"Per-buffer multiplication rejects overflow without allocating an oversized vector");
 overflow=false;try{render_detail::buffer_sum(limit,1);}catch(const std::length_error&){overflow=true;}
 check(overflow,"Combined blocks of a single VBO reject address overflow without capping independent buffers");
 NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Capacity fixture executes on requested graphics device");
 Target target;SurfaceRenderer renderer;Document doc;
 auto render=[&](std::shared_ptr<const MeshSnapshot> mesh){array_bytes=index_bytes=array_calls=index_calls=0;capture=true;try{renderer.draw(std::move(mesh),{},identity,{0,0,2},Target::size,Target::size,true);}catch(...){capture=false;throw;}capture=false;std::vector<uint8_t> pixels(Target::size*Target::size*4);glReadPixels(0,0,Target::size,Target::size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 doc.replace(scene(4));auto small=ready(doc);auto baseline=render(small);
 check(baseline[(Target::size/2*Target::size+Target::size/2)*4]==255,"Small ordinary three-object scene produces a nonblank independent pixel baseline");
 constexpr size_t per_object=1'600'004,total=3*per_object;
 doc.replace(scene(per_object));std::shared_ptr<const MeshSnapshot> large;
 try{large=ready(doc);}catch(const std::exception&e){if(!old||std::string(e.what()).find("Packed surface preview exceeds native renderer geometry limits")==std::string::npos)throw;std::cout<<"EXPECTED PRE-FIX FAILURE "<<e.what()<<std::endl;return 0;}
 if(old)throw std::runtime_error("Expected old c0 Document aggregate rejection did not occur");
 check(large->mesh.positions.size()==total&&large->mesh.indices.size()==18&&large->ranges.size()==3,"Actual Document packs all4.8M ordinary vertices and preserves independent object ranges");
 check(large->source_normals.empty()&&large->canonical_normals.empty()&&large->source_tangents.empty()&&large->sculpt_masks.empty()&&large->selected_weights.empty(),"Ordinary painted meshes acquire no retained-frame or SDF visualization arrays");
 auto sources=doc.copy_scene();bool intact=sources.objects().size()==3;
 for(const auto&o:sources.objects())intact&=o.mesh_active&&o.volume->voxels().empty()&&o.painted&&o.painted->mesh().positions.size()==per_object&&o.painted->mesh().indices.size()==6&&!o.appearance&&!o.material_bindings&&!o.deformation;
 check(intact&&std::all_of(large->ranges.begin(),large->ranges.end(),[](const auto&r){return !r.source_frame&&!r.canonical_frame;}),"Actual Scene retains all independently valid ordinary sources without adding SDF or retained-frame flags");
 auto uploaded=renderer.buffer_stats().geometry_uploads;auto pixels=render(large);
 check(pixels==baseline,"Large ordinary aggregate preserves the exact small-scene baseline pixels");
 constexpr size_t bytes=total*56;
 check(bytes>256ull*1024*1024&&array_calls==1&&array_bytes==bytes&&index_calls==1&&index_bytes==18*sizeof(uint32_t)&&renderer.buffer_stats().vertex_bytes==bytes,"Real geometry upload exceeds256MiB without an invented aggregate scene cap");
 std::cout<<"MEASURE ordinary vertices "<<total<<" actual vertex upload bytes "<<array_bytes<<" actual index upload bytes "<<index_bytes<<std::endl;
 check(renderer.buffer_stats().geometry_uploads==uploaded+1,"The large publication completes exactly one geometry upload");
 check(render(large)==baseline&&array_calls==0&&index_calls==0,"Second-eye rendering reuses all large geometry without glBufferData traffic");
 auto before_failure=renderer.buffer_stats().geometry_uploads;inject_error=true;bool failed=false;
 try{render(small);}catch(const std::runtime_error&e){if(std::string(e.what()).find("Surface vertex buffer upload failed (GL error 1281)")==std::string::npos)throw;failed=true;std::cout<<"EXPECTED DRIVER FAILURE "<<e.what()<<std::endl;}
 check(failed&&!inject_error&&renderer.buffer_stats().geometry_uploads==before_failure,"A real invalid-size driver upload is reported before successful geometry publication");
 check(glGetError()==GL_NO_ERROR,"Upload failure consumes its GL error and leaves a recoverable error queue");
 check(render(small)==baseline&&array_calls==1&&index_calls==1,"A failed upload invalidates the cache and retries the same valid source successfully");
 large.reset();doc.replace(indexed_scene());auto indexed=ready(doc);
 check(indexed->mesh.positions.size()==1'083'603&&indexed->mesh.indices.size()==6'480'000,"Actual Document preserves independently valid ordinary grids above6M aggregate indices");
 check(render(indexed)==baseline&&index_calls==1&&index_bytes==6'480'000*sizeof(uint32_t),"The driver receives every index above the former aggregate cap and preserves baseline pixels");
 std::cout<<"MEASURE indexed vertices "<<indexed->mesh.positions.size()<<" indices "<<indexed->mesh.indices.size()<<" actual vertex upload bytes "<<array_bytes<<" actual index upload bytes "<<index_bytes<<std::endl;
 check(render(indexed)==baseline&&array_calls==0&&index_calls==0,"Second-eye rendering reuses the complete above6M index buffer");
 check(glGetError()==GL_NO_ERROR,"Capacity and recovery rendering leave the GL error queue clean");
 std::cout<<checks<<" Document capacity checks passed"<<std::endl;
}catch(const std::exception&e){capture=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
