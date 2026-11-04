// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_typed_source_controls
#include "mtoon-render.cpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#undef main
#include "dome/scene-file.hpp"
#include <fstream>
#include <cstring>
namespace {
std::filesystem::path shader_directory;
size_t shader_number=0,gpu_copies=0;bool gpu_only=true;
struct Coefficients {std::array<double,3> legacy{},physical{};double coverage=0;};
Coefficients independent_field(const PaintedMesh &paint,float u,float v) {
  check(!paint.baked(),"Independent native coefficient oracle does not infer baked field semantics");
  const int r=int(paint.resolution());
  auto texel=[&](int x,int y){Coefficients out;x=std::clamp(x,0,r-1);y=std::clamp(y,0,r-1);const size_t tile=size_t(y/32)*(r/32)+size_t(x/32),pixel=size_t(y%32)*32+size_t(x%32);
    for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity>0)if(const auto &owner=layer.tiles.at(tile)){
      const auto &q=owner->pixels[pixel].data;const auto emission=layer.emission_tiles.empty()?nullptr:layer.emission_tiles.at(tile);
      const double opacity=double(q[13])/255*layer.opacity,coverage=opacity*(emission?emission->pixels[pixel].coverage:double(q[12])/255);
      for(size_t c=0;c<3;++c){const double legacy=emission?emission->pixels[pixel].legacy[c]:mtoon_srgb_to_linear(q[6+c]/255.f),physical=emission?emission->pixels[pixel].physical[c]:0;
        out.legacy[c]=out.legacy[c]*(1-coverage)+legacy*coverage;out.physical[c]=out.physical[c]*(1-coverage)+physical*coverage;}
      out.coverage=out.coverage*(1-coverage)+coverage;
    }return out;};
  const double x=std::clamp(double(u),0.,1.)*r-.5,y=std::clamp(double(v),0.,1.)*r-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));const double fx=x-ix,fy=y-iy;
  const auto a=texel(ix,iy),b=texel(ix+1,iy),c=texel(ix,iy+1),d=texel(ix+1,iy+1);Coefficients out;
  for(size_t k=0;k<3;++k){out.legacy[k]=std::lerp(std::lerp(a.legacy[k],b.legacy[k],fx),std::lerp(c.legacy[k],d.legacy[k],fx),fy);out.physical[k]=std::lerp(std::lerp(a.physical[k],b.physical[k],fx),std::lerp(c.physical[k],d.physical[k],fx),fy);}
  out.coverage=std::lerp(std::lerp(a.coverage,b.coverage,fx),std::lerp(c.coverage,d.coverage,fx),fy);return out;
}
std::array<float,2> pixel_uv(const MeshSnapshot &frame,int x=48,int y=48) {
 const float px=(2.f*x+1)/Fbo::size-1,py=(2.f*y+1)/Fbo::size-1;
 for(size_t face=0;face<frame.mesh.indices.size();face+=3){const auto ia=frame.mesh.indices[face],ib=frame.mesh.indices[face+1],ic=frame.mesh.indices[face+2];const auto a=frame.mesh.positions[ia],b=frame.mesh.positions[ib],c=frame.mesh.positions[ic];const float denominator=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(std::abs(denominator)<1e-8f)continue;
  const float wa=((b.y-c.y)*(px-c.x)+(c.x-b.x)*(py-c.y))/denominator,wb=((c.y-a.y)*(px-c.x)+(a.x-c.x)*(py-c.y))/denominator,wc=1-wa-wb;
  if(std::min({wa,wb,wc})>=-1e-5f)return {frame.mesh.uv[ia][0]*wa+frame.mesh.uv[ib][0]*wb+frame.mesh.uv[ic][0]*wc,frame.mesh.uv[ia][1]*wa+frame.mesh.uv[ib][1]*wb+frame.mesh.uv[ic][1]*wc};
 }throw std::runtime_error("Independent pixel oracle has no actual visible triangle");
}
std::array<float,4> nearest(const Image &image,std::array<float,2> uv,MToonEncoding encoding) {
 const auto x=std::clamp(int(std::floor(uv[0]*image.width)),0,int(image.width)-1),y=std::clamp(int(std::floor(uv[1]*image.height)),0,int(image.height)-1);std::array<float,4> raw{};for(size_t c=0;c<4;++c)raw[c]=image.rgba[(size_t(y)*image.width+size_t(x))*4+c]/255.f;return mtoon_decode_texel(encoding,raw);
}
std::array<int,4> independent_pixel(const Scene &scene,const MeshSnapshot &frame) {
 const auto &object=scene.active();const auto &binding=*object.material_bindings;const auto uv=pixel_uv(frame);const auto fields=independent_field(*object.painted,uv[0],uv[1]);
 const auto base=nearest(*binding.roles[0].image,uv,binding.material.textures[0].encoding),emission=nearest(*binding.roles[3].image,uv,binding.material.textures[3].encoding);MToonSamples samples;
 for(size_t c=0;c<4;++c)samples.base[c]=object.material.color[c]*base[c];
 std::array<float,3> radiance{};for(size_t c=0;c<3;++c){const double background=binding.physical_emission?binding.physical_emission->at(c):double(object.material.emissive[c])*binding.material.parameters.emission_strength;
  radiance[c]=float(background*emission[c]*(1-fields.coverage)+fields.legacy[c]*binding.material.parameters.emission_strength+fields.physical[c]);}samples.emission={radiance[0],radiance[1],radiance[2]};
 auto neutral=binding.material;neutral.parameters.base_color={1,1,1,1};neutral.parameters.emission={1,1,1};neutral.parameters.emission_strength=1;return expected(neutral,samples);
}
std::shared_ptr<SourceMaterialBindings> binding(bool legacy,const Material &native,
    const std::shared_ptr<const Image> &base,const std::shared_ptr<const Image> &emission) {
  auto out=std::make_shared<SourceMaterialBindings>();out->material=typed(legacy);
  out->native_reference={native.color,native.emissive,int(native.alpha),native.alpha_cutoff,native.double_sided};
  out->material.parameters.base_color=native.color;out->material.parameters.emission={native.emissive[0],native.emissive[1],native.emissive[2]};
  out->material.textures[0].index=0;out->roles[0].image=base;out->material.textures[3].index=1;out->roles[3].image=emission;
  for(size_t role:{0u,3u})out->roles[role].min_filter=out->roles[role].mag_filter=GL_NEAREST;
  out->validate();return out;
}
void ink(PaintedMesh &paint,PaintSettings settings) {
  Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=.8f;settings.front_only=false;
  paint.begin_stroke();paint.stamp(brush,settings);check(paint.end_stroke(),"Actual native physical brush creates sparse float emission");
}
Scene native_typed_physical(bool legacy,float opacity=.63f) {
  Scene scene;scene.active().visible=false;const auto id=scene.add("Native typed float emission",Volume{});scene.select(id);
  auto &object=scene.active();object.mesh_active=true;object.material=white();object.material.unlit=false;
  object.material.color={.6f,.7f,.5f,1};object.material.emissive={.15f,.03f,.08f};
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,{255,40,80,255,40,240,80,255,80,40,255,255,220,180,40,255}});
  maps->emission.image=std::make_shared<const Image>(Image{2,2,{180,40,20,255,20,80,200,255,40,200,80,255,220,100,20,255}});
  maps->color.min_filter=maps->color.mag_filter=maps->emission.min_filter=maps->emission.mag_filter=GL_NEAREST;
  object.painted=PaintedMesh::from_maps(quad(),maps,64);
  auto holder=binding(legacy,object.material,maps->color.image,maps->emission.image);holder->material.parameters.emission_strength=legacy?1:1.5f;object.material_bindings=holder;
  PaintSettings settings;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission={2.2f,.03f,1.3f};ink(*object.painted,settings);
  object.painted->layer_properties("Layer 1",true,opacity);return scene;
}
void ready_native(Document &doc) {check(doc.wait_mesh(std::chrono::seconds(30))&&doc.frame().surface,"Actual Document publishes its own complete typed native preview");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
}
extern "C" {
void __real_glShaderSource(GLuint,GLsizei,const GLchar *const*,const GLint*);
void __wrap_glShaderSource(GLuint shader,GLsizei count,const GLchar *const*strings,const GLint *lengths) {
 if(!shader_directory.empty()){GLint type=0;glGetShaderiv(shader,GL_SHADER_TYPE,&type);std::ofstream file(shader_directory/(std::to_string(++shader_number)+(type==GL_FRAGMENT_SHADER?".frag":".vert")));for(GLsizei n=0;n<count;++n)file.write(strings[n],lengths&&lengths[n]>=0?lengths[n]:std::streamsize(std::strlen(strings[n])));}
 __real_glShaderSource(shader,count,strings,lengths);
}
void __real_glGetTexImage(GLenum,GLint,GLenum,GLenum,void*);
void __wrap_glGetTexImage(GLenum target,GLint level,GLenum format,GLenum type,void *offset) {GLint bound=0;glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&bound);gpu_only&=bound!=0;++gpu_copies;__real_glGetTexImage(target,level,format,type,offset);}
}
int main(int argc,char **argv)try {
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);shader_directory=out/"shaders";std::filesystem::create_directories(shader_directory);
  NativeGraphics graphics;std::cout<<"GPU "<<glGetString(GL_VENDOR)<<" / "<<glGetString(GL_RENDERER)<<std::endl;Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);
  for(bool legacy:{true,false}) {auto scene=native_typed_physical(legacy);const auto exact=scene.encode();scene.save(out/(legacy?"native-v0.dsculpt":"native-v1.dsculpt"));Document doc;doc.replace(scene);ready_native(doc);
    try {draw(renderer,doc.frame().surface);}catch(...){check(doc.copy_scene().encode()==exact,"Actual typed preview refusal preserves complete float/source/native Scene");throw;}
    check(doc.copy_scene().encode()==exact,"Actual typed float preview preserves all source owners and float project bytes");}
  for(bool legacy:{true,false})for(float strength:legacy?std::vector<float>{1}:std::vector<float>{0,1,1.5f,3}){
    auto scene=native_typed_physical(legacy,.15f);auto binding=std::make_shared<SourceMaterialBindings>(*scene.active().material_bindings);binding->material.parameters.emission_strength=strength;scene.active().material_bindings=binding;
    Document doc;doc.replace(scene);ready_native(doc);fbo.bind();const auto pixels=draw(renderer,doc.frame().surface);const auto want=independent_pixel(scene,*doc.frame().surface),got=at(pixels);std::cout<<"NATIVE_MTOON_ORACLE legacy="<<legacy<<" strength="<<strength<<" actual="<<got[0]<<","<<got[1]<<","<<got[2]<<" expected="<<want[0]<<","<<want[1]<<","<<want[2]<<std::endl;check(close(got,want,2),"Independent native MToon field arithmetic preserves physical radiance and actual typed strength within unchanged2 bound");
    const auto stats=renderer.texture_stats();const auto geometry=renderer.buffer_stats();fbo.bind();check(draw(renderer,doc.frame().surface)==pixels,"Actual second-eye native typed image is exact");check(renderer.texture_stats().uploads==stats.uploads&&renderer.texture_stats().pack_builds==stats.pack_builds&&renderer.buffer_stats().geometry_uploads==geometry.geometry_uploads,"Native typed second eye reuses actual packet/geometry with no upload or repacking");
  }
  check(gpu_copies>0&&gpu_only,"Every production native typed field/source copy stays in GPU buffers");
  std::cout<<"PASS "<<checks<<" native typed physical controls\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
