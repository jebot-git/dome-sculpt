// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/render.hpp"
#include "dome/source-paint.hpp"
#include "dome/tangents.hpp"
#include "baked-emission-domain.hpp"
#include "render-paint-overlay.hpp"
#include "render-source-pack.hpp"
#include "graphics.h"
#include <iostream>
#include <cmath>
#include <string_view>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*why){if(!value)throw std::runtime_error(why);++checks;}
Mesh quad(){Mesh m;m.positions={{-.75f,-.75f,0},{.75f,-.75f,0},{.75f,.75f,0},{-.75f,.75f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct Target{GLuint f=0,c=0,d=0;Target(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,96,96,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,96,96);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Real renderer fixture framebuffer complete");}~Target(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}};
void stamp(PaintedMesh&p,Vec3 point,float radius,std::array<float,3>rgb,PaintEmissionUnits units=PaintEmissionUnits::Physical){PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission_units=units;ink.emission=rgb;ink.front_only=false;Brush brush;brush.from=brush.to=point;brush.normal={0,0,1};brush.radius=radius;brush.strength=.8f;p.begin_stroke();p.stamp(brush,ink);check(p.end_stroke(),"Actual emission paint stroke commits");}
struct Sample{std::array<double,3> rgb{};double coverage=0;};
// Independent original sparse-layer consumer: no production overlay sampler,
// retained field interpolation, baked maps, projection or material resolver.
Sample sample(const PaintedMesh&p,double u,double v,std::array<float,3>background,double strength=1){const int r=p.resolution();const double x=u*r-.5,y=v*r-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));Sample out;
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,r-1),py=std::clamp(iy+dy,0,r-1);std::array<double,3>rgb{background[0],background[1],background[2]};double coverage=0;
  for(const auto&layer:p.layers())if(layer.visible&&layer.opacity){const auto q=p.pixel(layer.id,px,py);const auto e=p.emission_pixel(layer.id,px,py);const double a=double(e.coverage)*double(q.data[13])/255.*layer.opacity;for(int k=0;k<3;++k)rgb[k]=rgb[k]*(1-a)+(double(e.legacy[k])*strength+e.physical[k])*a;coverage=coverage*(1-a)+a;}
  const double w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(int k=0;k<3;++k)out.rgb[k]+=rgb[k]*w;out.coverage+=coverage*w;
 }return out;}
const Image image_pattern{3,2,{
 255,0,48,255, 32,255,0,255, 50,70,255,255,
 60,180,255,255, 255,24,70,255, 7,40,10,255}};
// Independent oracle for mirrored U and repeated V, including the discrete
// nearest knots and sRGB decoding before interpolating original image taps.
std::array<float,3> original_image(double u,double v,bool nearest,bool typed,std::array<float,3> factor){
 u=1-u;
 if(typed){
  const double x=u*2.25,y=v*1.75,c=std::cos(.5),s=std::sin(.5);
  u=c*x-s*y-.375;v=s*x+c*y-.25;
 }else{u=3*u-.5;v=2*v-.375;}
 u-=2*std::floor(u/2);u=1-std::abs(1-u);
 v-=std::floor(v);
 const double x=u*3-(nearest?0:.5),y=v*2-(nearest?0:.5);
 const int ix=int(std::floor(x)),iy=int(std::floor(y));
 std::array<double,3> rgb{};
 for(int dy=0;dy<(nearest?1:2);++dy)for(int dx=0;dx<(nearest?1:2);++dx){
  const int p=std::clamp(ix+dx,0,2),q=(iy+dy+2)%2;
  const double weight=nearest?1:(dx?x-ix:1-x+ix)*(dy?y-iy:1-y+iy);
  for(int k=0;k<3;++k){
   const double encoded=image_pattern.rgba[(q*3+p)*4+k]/255.;
   const double linear=encoded<=.04045?encoded/12.92:std::pow((encoded+.055)/1.055,2.4);
   rgb[k]+=weight*linear*factor[k];
  }
 }
 return {float(rgb[0]),float(rgb[1]),float(rgb[2])};
}
std::shared_ptr<SourceMaterialBindings> binding(const Material&m,float strength=1){auto b=std::make_shared<SourceMaterialBindings>();b->has_mtoon=true;b->material=MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});b->material.parameters.base_color=m.color;b->material.parameters.shade_color={0,0,0};const float peak=std::max({1.f,m.emissive[0],m.emissive[1],m.emissive[2]});b->material.parameters.emission={m.emissive[0]/peak,m.emissive[1]/peak,m.emissive[2]/peak};b->material.parameters.emission_strength=peak*strength;b->material.parameters.cull=MToonCull::Off;b->native_reference.color=m.color;b->native_reference.emission=m.emissive;b->native_reference.double_sided=m.double_sided;b->validate();return b;}
std::shared_ptr<MeshSnapshot> snapshot(const PaintedMesh &paint,Material material,
                                       bool toon,bool retained) {
 auto snapshot=std::make_shared<MeshSnapshot>();
 snapshot->mesh=paint.mesh();
 const auto corners=tangent_corners(snapshot->mesh);
 snapshot->tangents.resize(snapshot->mesh.positions.size());
 std::vector<bool> seen(snapshot->mesh.positions.size());
 for(size_t corner=0;corner<corners.size();++corner) {
  const auto vertex=snapshot->mesh.indices[corner];
  if(seen[vertex])
   check(snapshot->tangents[vertex]==corners[corner],
         "Actual quad corner tangents have unambiguous vertex transport");
  snapshot->tangents[vertex]=corners[corner];
  seen[vertex]=true;
 }
 snapshot->pbr_view=!toon;
 SurfaceRange range{};
 range.object=1;
 range.first=0;
 range.count=snapshot->mesh.indices.size();
 range.material=material;
 range.pbr_view=!toon;
 if(retained)range.textures=paint.preview(material);
 if(toon)range.material_bindings=binding(material,retained?5:1);
 snapshot->ranges.push_back(std::move(range));
 return snapshot;
}
std::vector<uint8_t> draw(SurfaceRenderer&r,Target&t,const std::shared_ptr<MeshSnapshot>&s){glBindFramebuffer(GL_FRAMEBUFFER,t.f);r.draw(s,{},identity,{0,0,2},96,96,true);std::vector<uint8_t>pixels(96*96*4);glReadPixels(0,0,96,96,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());check(glGetError()==GL_NO_ERROR,"Actual material renderer completed without GL errors");return pixels;}
}
int main(int argc,char**argv)try{
 bool hardware=false,mapped=false,original=false,nearest=false,typed_source=false;
 for(int i=1;i<argc;++i){
  const std::string_view flag=argv[i];
  if(flag=="--hardware")hardware=true;
  else if(flag=="--mapped")mapped=true;
  else if(flag=="--image")original=mapped=true;
  else if(flag=="--nearest-image")nearest=original=mapped=true;
  else if(flag=="--typed-image")typed_source=original=mapped=true;
  else throw std::invalid_argument("Unknown retained HDR render fixture argument");
 }
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_RENDERER="<<device<<std::endl;check(!hardware||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Requested device executes real GPU path");
 Scene scene;auto &source_object=scene.active();const auto source=source_object.id;source_object.mesh_active=true;source_object.material.color={0,0,0,1};source_object.material.emissive={.02f,.04f,.08f};source_object.material.roughness=.75f;
 if(original){
  auto maps=std::make_shared<PaintSourceMaps>();
  maps->emission.image=std::make_shared<const Image>(image_pattern);
  maps->emission.texcoord=1;maps->emission.uv_transform={3,0,0,2,-.5f,-.375f};
  maps->emission.wrap_s=33648;maps->emission.wrap_t=10497;
  maps->emission.mag_filter=maps->emission.min_filter=nearest?9728:9729;
  auto atlas=quad(),raw=atlas;
  auto uv1=std::make_shared<std::vector<std::array<float,2>>>(raw.uv);
  for(auto &uv:*uv1)uv[0]=1-uv[0];
  maps->uv1=uv1;
  for(auto &uv:atlas.uv)uv={.125f+.75f*uv[0],.0625f+.875f*uv[1]};
  const std::array<uint32_t,4> vertices{0,1,2,3};
  source_object.painted=prepare_source_paint(capture_source_sampling(raw,maps,64),std::move(atlas),vertices);
  source_object.material.emissive={1.7f,2.3f,.85f};
  if(typed_source){
   auto authored=binding(source_object.material,1.5f);
   const auto index=size_t(MToonSlot::Emission);
   auto &slot=authored->material.textures[index];
   slot.index=0;slot.texcoord=1;slot.encoding=MToonEncoding::Srgb;
   slot.scale={2.25f,1.75f};slot.offset={-.375f,-.25f};slot.rotation=.5f;
   authored->roles[index]=maps->emission;
   authored->roles[index].texcoord=0;
   authored->roles[index].uv_transform={1,0,0,1,0,0};
   authored->validate();
   source_object.material_bindings=std::move(authored);
  }
 }else source_object.painted=std::make_shared<PaintedMesh>(quad(),64);
 stamp(*source_object.painted,{-.25f,0,0},.37f,{2,.1f,.03f});source_object.painted->add_layer("Original source detail");stamp(*source_object.painted,{.24f,.13f,0},.23f,{.05f,.8f,.3f});source_object.painted->layer_properties("Original source detail",true,.6f);const auto original_source=source_object.painted;const auto background=source_object.material.emissive;
 if(original){source_object.painted->add_layer("Original relative emission");stamp(*source_object.painted,{.03f,-.1f,0},.31f,{.15f,.08f,.31f},PaintEmissionUnits::MaterialRelative);}
 auto target_mesh=quad();
 if(mapped)for(auto &uv:target_mesh.uv)uv={.625f-.5f*uv[0]+.125f*uv[1],.25f+.5f*uv[1]};
 const auto target=scene.add("Actual higher-resolution target",Volume{});auto &target_object=scene.object(target);target_object.mesh_active=true;target_object.painted=std::make_shared<PaintedMesh>(std::move(target_mesh),512);target_object.material=scene.object(source).material;scene.select(target);ObjectBakeSettings settings;settings.padding=0;settings.ao_samples=0;settings.front_mm=settings.back_mm=2;const auto report=bake_object(scene,source,settings);check(report.projected>0,"Actual Bake produces retained source authority");auto paint=scene.active().painted;const auto domain=paint->baked()->emission_domain;check(domain&&domain->field()->resolution()==64&&paint->resolution()==512,"Original64 HDR knots are independent from current512 paint");
 if(mapped){check(domain->mapped(),"Mapped rendering exercises the independent target atlas");std::cout<<"HDR_MAPPED mirrored and sheared UV0\n";}
 if(original){check(domain->image()&&domain->image_mapping()!=domain->mapping(),"Render retains separate source image and paint spaces");std::cout<<"HDR_IMAGE nearest="<<nearest<<" typed_source="<<typed_source<<'\n';}
 const auto packet=detail::PaintOverlayFactory::capture_native_emission(*paint);detail::PaintOverlayCompiler compiler;compiler.prepare({{packet,true}});const auto&resource=compiler.at(packet,true);check(resource.retained_emission&&resource.bytes==detail::PaintOverlayCompiler::texture_bytes(packet)&&compiler.image_count()>=6,"Retained original-size texture is owned and budgeted");GLuint fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,resource.retained_emission,0);glReadBuffer(GL_COLOR_ATTACHMENT0);std::vector<float>raw(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,raw.data());double coefficient_error=0;for(size_t i=0;i<4096;++i){const auto&e=domain->field()->values()[i];for(size_t k=0;k<3;++k)coefficient_error=std::max(coefficient_error,std::abs(double(raw[i*4+k])-e.absolute[k]));check(raw[i*4+3]==e.coverage,"Retained immutable coverage coefficient survives GPU extraction");}check(coefficient_error==0,"GPU extracts original FLOAT coefficients exactly without resampling");glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);
 const auto layout=domain->texture_layout();
 MToonSourcePacks packs;MToonSourcePacks::Inputs inputs{};inputs[7]=MToonSourcePacks::Input::retained(resource);packs.prepare({inputs},0,0,{});check(packs.at(inputs).levels[98][1]==int(layout.width)&&packs.at(inputs).levels[98][2]==int(layout.height),"MToon eighth view retains the owning texture layout in the same sampler unit");
 Material material=scene.active().material;material.emissive={17,8,3};Target output;SurfaceRenderer renderer,reference;renderer.set_frame_time(0);reference.set_frame_time(0);int maximum=0;
 for(bool edited:{false,true}){if(edited){paint=paint->snapshot();paint->add_layer("Current512 physical paint");stamp(*paint,{0,-.1f,0},.29f,{.1f,.9f,.02f});paint->layer_properties("Current512 physical paint",true,.45f);}
  for(bool toon:{false,true}){const auto actual=draw(renderer,output,snapshot(*paint,material,toon,true));int phase_error=0;for(int y:{29,37,48,59,67})for(int x:{29,37,48,59,67}){const double u=(x+.5-12)/72.,v=(y+.5-12)/72.;
   auto below=sample(*original_source,original?.125+.75*u:u,original?.0625+.875*v:v,
                     original?std::array<float,3>{}:background,typed_source?2.3*1.5:1);
   if(original){
    auto factor=background;
    if(typed_source)for(float &value:factor)value*=1.5f;
    const auto image=original_image(u,v,nearest,typed_source,factor);
    for(int k=0;k<3;++k)below.rgb[k]+=image[k]*(1-below.coverage);
   }
   const auto above=sample(*paint,mapped?.625-.5*u+.125*v:u,mapped?.25+.5*v:v,{});
   auto expected_material=material;for(int k=0;k<3;++k)expected_material.emissive[k]=float(below.rgb[k]*(1-above.coverage)+above.rgb[k]);const auto expected=draw(reference,output,snapshot(*paint,expected_material,toon,false));for(int k=0;k<3;++k)phase_error=std::max(phase_error,std::abs(int(actual[(y*96+x)*4+k])-int(expected[(y*96+x)*4+k])));}
   std::cout<<"HDR_MATERIAL edited="<<edited<<" mtoon="<<toon<<" rgb_error="<<phase_error<<std::endl;maximum=std::max(maximum,phase_error);check(phase_error<=2,"Independent original64/current512 layer equation matches actual PBR/MToon material pixels");
  }
 }
 std::cout<<"HDR_GPU coefficient_error="<<coefficient_error<<" maximum_RGB_error="<<maximum<<" checks="<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
