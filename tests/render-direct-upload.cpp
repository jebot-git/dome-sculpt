// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#include <cstring>
#include <filesystem>
#include <fstream>
namespace {
struct Block {GLuint buffer;GLintptr offset;GLsizeiptr bytes;const void *source;};
bool capture=false,unknown=false;
std::vector<const void*> sources;
std::vector<Block> transfers;
size_t allocations=0;
void allow(const auto &values){if(!values.empty())sources.push_back(values.data());}
void source(const MeshSnapshot&s){
 sources.clear();allow(s.mesh.positions);allow(s.mesh.normals);allow(s.mesh.uv);
 allow(s.tangents);allow(s.source_normals);allow(s.canonical_normals);allow(s.source_tangents);
 allow(s.source_colors);allow(s.normal_coordinates);allow(s.paint_coordinates);
 allow(s.selected_weights);allow(s.sculpt_masks);allow(s.mesh.indices);
}
std::shared_ptr<MeshSnapshot> large(){
 auto s=std::make_shared<MeshSnapshot>();
 for(size_t y=0;y<80;++y)for(size_t x=0;x<80;++x){
  const float a=-.75f+float(x)*1.5f/80,b=-.75f+float(y)*1.5f/80;
  s->mesh.positions.insert(s->mesh.positions.end(),{{a,b,0},{a+1.5f/80,b,0},{a,b+1.5f/80,0}});
 }
 s->mesh.normals.assign(s->mesh.positions.size(),{0,0,1});
 for(size_t i=0;i<s->mesh.positions.size();++i)s->mesh.indices.push_back(uint32_t(i));
 s->generation=8;s->geometry_identity=std::make_shared<const int>(1);
 s->ranges.push_back({1,0,s->mesh.indices.size(),white(),{},{},0});return s;
}
void bytes(const std::filesystem::path&p,const Pixels&data){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));check(bool(f),"Framebuffer evidence written");}
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage){
 if(capture&&(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER)){
  ++allocations;
  if(data&&std::find(sources.begin(),sources.end(),data)==sources.end())unknown=true;
 }
 __real_glBufferData(target,size,data,usage);
}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr size,const void *data){
 if(capture&&target==GL_ARRAY_BUFFER){
  GLint buffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
  transfers.push_back({GLuint(buffer),offset,size,data});
  if(std::find(sources.begin(),sources.end(),data)==sources.end())unknown=true;
 }
 __real_glBufferSubData(target,offset,size,data);
}
int main(int argc,char**argv)try{
 check(argc==3,"Usage: render-direct-upload reference|direct output-directory");
 const bool direct=std::string(argv[1])=="direct";check(direct||std::string(argv[1])=="reference","Known upload proof mode");
 std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
 NativeGraphics graphics;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<'\n';
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;auto scene=large();
 auto render=[&](const std::shared_ptr<MeshSnapshot>&s,const std::string&name){
  source(*s);unknown=false;transfers.clear();allocations=0;capture=true;
  try{renderer.draw(s,{},identity,{0,0,2},Fbo::size,Fbo::size,true);}catch(...){capture=false;throw;}
  capture=false;const auto image=read();bytes(output/(name+".rgba"),image);
  if(direct){
   check(!unknown,"All geometry/scalar uploads use actual immutable snapshot arrays");
   GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);
   for(const auto&b:transfers){
    std::vector<uint8_t> actual(size_t(b.bytes));glBindBuffer(GL_ARRAY_BUFFER,b.buffer);
    glGetBufferSubData(GL_ARRAY_BUFFER,b.offset,b.bytes,actual.data());
    check(!std::memcmp(actual.data(),b.source,size_t(b.bytes)),"Actual GPU block equals its independent source array byte-for-byte");
   }
   glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous));
  }
  return image;
 };
 const auto baseline=render(scene,"plain");
 check(std::any_of(baseline.begin(),baseline.end(),[](auto v){return v!=0;}),"Large mesh actually draws nonblank pixels");
 if(direct)check(renderer.buffer_stats().vertex_bytes==scene->mesh.positions.size()*2*sizeof(Vec3),"Plain sculpt geometry stores positions/normals without duplicated default attributes");
 const auto again=render(scene,"stereo");check(again==baseline&&allocations==0&&transfers.empty(),"Second eye retains pixels and issues no geometry/scalar transfer");
 auto complete=std::make_shared<MeshSnapshot>(*scene);complete->geometry_identity=std::make_shared<const int>(2);
 const auto n=complete->mesh.positions.size();complete->mesh.uv.assign(n,{.25f,.75f});complete->tangents.assign(n,{1,0,0,1});
 complete->source_normals.assign(n,{0,0,1});complete->canonical_normals.assign(n,{0,1,0});complete->source_tangents.assign(n,{0,1,0,-1});complete->source_colors.assign(n,{1,1,1,1});
 const auto full=render(complete,"frames");check(full==baseline,"Optional source frames/colors preserve the authored unlit picture");
 auto partial=std::make_shared<MeshSnapshot>(*complete);partial->geometry_identity=std::make_shared<const int>(3);partial->canonical_normals.clear();partial->source_tangents.clear();partial->mesh.uv.clear();partial->tangents.clear();
 const auto fewer=render(partial,"partial");check(fewer==baseline,"Absent optional streams preserve the default material picture");
 glVertexAttrib2f(2,8,9);glVertexAttrib2f(3,8,9);glVertexAttrib4f(5,0,1,0,-1);glVertexAttrib4f(8,0,1,0,-1);
 check(render(partial,"constants")==baseline&&allocations==0&&transfers.empty(),"Interleaved context state cannot change cached default stream pixels");
 if(direct){
  float values[4]{};glGetVertexAttribfv(5,GL_CURRENT_VERTEX_ATTRIB,values);check(values[0]==1&&values[1]==0&&values[2]==0&&values[3]==1,"Missing native tangent restores its actual per-draw default");
  glGetVertexAttribfv(8,GL_CURRENT_VERTEX_ATTRIB,values);check(values[0]==1&&values[1]==0&&values[2]==0&&values[3]==1,"Missing source tangent restores its actual per-draw default");
 }
 auto masked=std::make_shared<MeshSnapshot>(*partial);masked->sculpt_masks.assign(n,.5f);render(masked,"mask");
 if(direct)for(int field=0;field<6;++field){
  auto bad=std::make_shared<MeshSnapshot>(*scene);bad->geometry_identity=std::make_shared<const int>(20+field);
  switch(field){
   case 0:bad->tangents.assign(n-1,{1,0,0,1});break;
   case 1:bad->source_normals.assign(n-1,{0,0,1});break;
   case 2:bad->canonical_normals.assign(n-1,{0,0,1});break;
   case 3:bad->source_tangents.assign(n-1,{1,0,0,1});break;
   case 4:bad->source_colors.assign(n-1,{1,1,1,1});break;
   case 5:bad->normal_coordinates.assign(n-1,{0,0,0,0});break;
  }
  check(rejects([&]{renderer.draw(bad,{},identity,{0,0,2},Fbo::size,Fbo::size,true);}),"Malformed optional stream count refuses before out-of-range source reads");
  check(render(scene,"recovered-"+std::to_string(field))==baseline,"A real valid draw recovers exact cached/default pixels after stream refusal");
 }
 check(glGetError()==GL_NO_ERROR,"Direct-buffer rendering leaves actual GL state error-free");
 std::cout<<"PASS "<<checks<<" direct-upload checks\n";return 0;
}catch(const std::exception&e){capture=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
