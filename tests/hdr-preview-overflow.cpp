// SPDX-License-Identifier: GPL-3.0-or-later
// Actual Document/native owners and production renderer, not an HDR data clamp.
#define main established_mtoon_render_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include <limits>
namespace {
struct FloatTarget {
 GLuint f=0,c=0,d=0;
 FloatTarget(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,96,96,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,96,96);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Actual linear FLOAT destination is complete");}
 ~FloatTarget(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}
 void bind(){glBindFramebuffer(GL_FRAMEBUFFER,f);}
 std::array<float,4> sample(int x=48,int y=48){std::array<float,4> p{};glReadPixels(x,y,1,1,GL_RGBA,GL_FLOAT,p.data());check(glGetError()==GL_NO_ERROR,"Diagnostic-only float pixel read succeeds");for(float v:p)check(std::isfinite(v),"Resolved preview contains only finite components");return p;}
};
Image constant_image(std::array<uint8_t,4> value){Image result{64,64,std::vector<uint8_t>(4096*4)};for(size_t i=0;i<result.rgba.size();++i)result.rgba[i]=value[i%4];return result;}
Mesh outlined_mesh(){auto m=quad();for(auto&p:m.uv)p[0]*=.45f;for(size_t v=0;v<4;++v)m.normals[v]=normalized(Vec3{m.positions[v].x,m.positions[v].y,0});for(size_t v=0;v<4;++v){m.positions.push_back(m.positions[v]);m.normals.push_back(m.normals[v]);auto uv=m.uv[v];uv[0]+=.55f;m.uv.push_back(uv);}m.indices.insert(m.indices.end(),{6,5,4,7,6,4});return m;}
Scene native_source(BakedEmissionSample emission,float strength,float background,bool covered,bool outline=false,float alpha=1){
 auto p=std::make_shared<PaintedMesh>(outline?outlined_mesh():quad(),64);BakedTextures base;base.maps.color=constant_image({0,0,0,255});base.maps.metal_rough=constant_image({255,255,0,255});base.maps.normal=constant_image({128,128,255,255});base.coverage.assign(4096,255);base.emission_field=BakedEmissionField::create(64,std::vector<BakedEmissionSample>(4096,emission));p->install_bake(base);
 Scene s;auto &o=s.active();o.mesh_active=true;o.painted=p;o.material.color={0,0,0,1};o.material.emissive={background,0,0};o.material.metallic=0;o.material.roughness=1;o.material.unlit=false;
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=true;binding->material=alpha==1?typed(false):MToonMaterial::from_vrm1({{"alphaMode","BLEND"},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});auto &m=binding->material.parameters;m.base_color={0,0,0,alpha};m.shade_color={0,0,0};m.emission={1,1,1};m.emission_strength=strength;
 if(outline){m.cull=MToonCull::Off;m.outline_cull=MToonCull::Front;m.outline=MToonOutline::World;m.outline_width=.1f;m.outline_color={0,1,0};m.outline_mix=0;}
 binding->validate();o.material_bindings=binding;
 if(covered){o.transform.position.z=.1f;auto id=s.add("Actual opaque-alpha blend foreground",Volume{});auto &front=s.object(id);front.mesh_active=true;front.painted=std::make_shared<PaintedMesh>(quad(),64);front.material.color={.5f,0,0,1};front.material.emissive={};front.material.unlit=true;front.material.alpha=Material::AlphaMode::Blend;}
 s.validate();return s;
}
std::shared_ptr<const MeshSnapshot> publish(Document&doc,const Scene&s){doc.replace(s);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual native field publishes through Document worker");doc.authored_shading();check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual authored material view publishes");auto f=doc.frame().surface;check(f&&!f->mesh.positions.empty(),"Actual worker publishes immutable packed geometry");return f;}
void render(SurfaceRenderer&r,const std::shared_ptr<const MeshSnapshot>&f,bool deferred){r.draw(f,{},identity,{0,0,2},96,96,true);if(deferred)r.resolve();}
void case_check(BakedEmissionSample sample,float strength,float background,bool covered,bool deferred,bool outline=false,float alpha=1,float expected_red=-1){
 auto s=native_source(sample,strength,background,covered,outline,alpha);const auto original=s.encode();check(Scene::decode(original).encode()==original,"Full native FLOAT field and material restart exactly");Document doc;auto frame=publish(doc,s);SurfaceRenderer r;r.set_frame_time(0);r.set_deferred_resolve(deferred);r.set_output_encoding(SurfaceOutputEncoding::Linear);FloatTarget target;target.bind();render(r,frame,deferred);auto pixel=target.sample();std::cout<<"CASE strength="<<strength<<" background="<<background<<" covered="<<covered<<" deferred="<<deferred<<" outline="<<outline<<" alpha="<<alpha<<" expected_red="<<expected_red<<" pixel="<<pixel[0]<<","<<pixel[1]<<","<<pixel[2]<<","<<pixel[3]<<std::endl;
 if(covered)check(std::abs(pixel[0]-.5f)<.004f&&pixel[1]==0&&pixel[2]==0&&pixel[3]==1,"Opaque-alpha foreground survives real HDR overdraw without NaN contamination");
 else if(expected_red>=0)check(std::abs(pixel[0]-expected_red)<std::max(.003f,expected_red*.003f),"Finite HDR times small alpha preserves independent physical product before HALF storage");
 else check(pixel[0]>=60000&&std::abs(pixel[2]-2)<.004f&&pixel[3]==1,"Large positive emission remains bright and preserves independent finite channels");
 if(outline){auto border=target.sample(9,48);check(border[0]<.01f&&border[1]>.99f&&border[2]<.01f&&border[3]==1,"Zero-lighting-mix green outline survives large-emission shading");}
 {Fbo display;r.set_output_encoding(SurfaceOutputEncoding::Srgb);display.bind();render(r,frame,deferred);auto p=at(read());if(covered)check(close(p,{188,0,0,255},2),"Actual sRGB foreground remains red after HDR background");else if(expected_red<0)check(p[0]>=250&&p[1]<=1&&p[2]>=250,"Actual sRGB presentation saturates bright radiance instead of turning black");}
 check(s.encode()==original&&doc.copy_scene().encode()==original,"Display handling preserves exact full native coefficients, material and scene authority");
}
}
int main(int argc,char**argv)try{
 NativeGraphics graphics;const auto device=std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));std::cout<<"ACTUAL_RENDERER "<<device<<'\n';if(argc>1&&std::string_view(argv[1])=="--hardware")check(device.find("Arc")!=std::string::npos&&device.find("UHD")==std::string::npos,"Requested actual Arc renderer is present");
 const float largest=std::numeric_limits<float>::max();const BakedEmissionSample normal{{},{100000,1e-7f,2},1},overflow{{1,0,0},{largest,1e-7f,2},1},partial{{},{0,0,0},.75f};
 check(rejects([&]{(void)overflow.resolve({},largest);}),"Native radiance overflow still refuses instead of clamping stored/exported data");
 for(bool deferred:{false,true}){
  for(float strength:{0.f,3.5f,largest})for(bool covered:{false,true})case_check(normal,strength,2,covered,deferred);
  for(bool covered:{false,true})case_check(overflow,largest,0,covered,deferred);
  case_check(overflow,largest,0,false,deferred,true);
  // Clamping finite RGB to HALFmax before alpha would incorrectly change1 to.655.
  case_check(normal,3.5f,0,false,deferred,false,1e-5f,1.f);
  // Old background2*F32max overflows, but its .25 uncovered fraction is finite.
  const float alpha=1e-37f;const float expected=float(.5L*static_cast<long double>(largest)*alpha);
  case_check(partial,largest,2,false,deferred,false,alpha,expected);
 }
 check(glGetError()==GL_NO_ERROR,"HDR display controls leave the actual GL error queue clean");std::cout<<"PASS "<<checks<<" actual HDR preview overflow, coverage, alpha, outline and native-retention controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
