// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_source_overlay_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/source-paint.hpp"
#include "dome/material-bindings.hpp"
#include "paint-overlay-packet.hpp"
namespace {
void ink(PaintedMesh &paint,PaintSettings setting){Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=2;b.strength=.8f;setting.front_only=false;paint.begin_stroke();paint.stamp(b,setting);check(paint.end_stroke(),"Real sparse native paint changes the typed writable grid");}
std::shared_ptr<SourceMaterialBindings> binding(bool legacy,const Material &native,const std::shared_ptr<const Image> &base,const std::shared_ptr<const Image> &emission={}){
 auto out=std::make_shared<SourceMaterialBindings>();out->material=typed(legacy);out->native_reference={native.color,native.emissive,int(native.alpha),native.alpha_cutoff,native.double_sided};
 out->material.parameters.base_color=native.color;out->material.parameters.shade_color={.3f,.4f,.5f};out->material.parameters.emission={native.emissive[0],native.emissive[1],native.emissive[2]};
 if(base){out->material.textures[0].index=0;out->roles[0].image=base;out->roles[0].min_filter=out->roles[0].mag_filter=GL_NEAREST;}
 if(emission){out->material.textures[3].index=1;out->roles[3].image=emission;out->roles[3].min_filter=out->roles[3].mag_filter=GL_NEAREST;}
 out->validate();return out;
}
std::shared_ptr<MeshSnapshot> frame(const Mesh &mesh,const std::shared_ptr<const TextureMaps> &textures,const Material &m,const std::shared_ptr<const SourceMaterialBindings> &holder){
 auto s=std::make_shared<MeshSnapshot>();s->mesh=mesh;s->ranges.push_back({1,0,mesh.indices.size(),m,textures,{},0});s->ranges[0].material_bindings=holder;return s;
}
}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';
 check(argc<2||std::string_view(argv[1])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos),"Requested typed hardware run requires actual A770");
 auto raw=quad();raw.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};const auto target=quad();const std::array<uint32_t,4> vertices{0,1,2,3};
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);int maximum=0;
 const auto split=std::make_shared<const Image>(Image{2,2,{255,40,0,0,0,255,70,255,40,20,255,180,220,180,40,255}});
 for(bool legacy:{true,false})for(bool original_one:{false,true})for(bool pbr_zero:{false,true}){
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=split;maps->color.min_filter=maps->color.mag_filter=GL_NEAREST;
  if(original_one||!pbr_zero){maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(std::vector<std::array<float,2>>{{.1f,.2f},{.4f,.2f},{.4f,.8f},{.1f,.8f}});}
  if(!pbr_zero)maps->color.texcoord=1;
  PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,maps,64,policy);const auto frozen=before->encode();auto paint=prepare_source_paint(before,target,vertices);
  Material m=white();m.color={.65f,.8f,.4f,.85f};m.alpha=Material::AlphaMode::Mask;m.alpha_cutoff=.5f;
  auto holder=binding(legacy,m,split);holder->material.parameters.scroll={.23f,-.17f};holder->material.textures[0].texcoord=original_one?1:0;
  auto raw_marker=before->preview(m);auto original=frame(raw,raw_marker,m,holder),adopted=frame(target,paint->preview(m),m,holder);
  const auto &packet=adopted->ranges[0].textures->overlay;
  check(packet&&packet->empty()&&*packet->original_uv0()==raw.uv,"Unpainted Source4 retains exact logical UV0 independently of generated image carriers");
  check(bool(packet->original_uv1())==bool(maps->uv1),"Logical UV1 exists only when the actual pristine source owned it");
  for(double time:{0.,1.25}){
   renderer.set_frame_time(time);const auto want=draw(renderer,original),got=draw(renderer,adopted);const auto error=delta(want,got);maximum=std::max(maximum,error);check(error<=2,"V0/V1 original authored UV and animation survive atlas adoption without paint");
   MaterialSurfaceQuery a(original->mesh,original->ranges),b(adopted->mesh,adopted->ranges);
   for(float x:{-.4f,0.f,.4f})check(bool(a.raycast({x,.1f,1},{0,0,-1},2,time))==bool(b.raycast({x,.1f,1},{0,0,-1},2,time)),"Original logical UV and animated alpha agree in actual native material picking");
  }
  PaintSettings zero;zero.channel=PaintChannel::Emission;zero.emission={0,0,0};ink(*paint,zero);auto painted=frame(target,paint->preview(m),m,holder);
  check(!painted->ranges[0].textures->overlay->empty(),"Real sparse zero-emission paint activates the typed overlay without changing base roles");
  for(double time:{0.,1.25}){renderer.set_frame_time(time);const auto error=delta(draw(renderer,original),draw(renderer,painted));maximum=std::max(maximum,error);check(error<=2,"Active typed paint preserves independently sampled original base and animated alpha");}
  auto saved=paint->encode();auto reopened=PaintedMesh::decode(saved,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});
  check(draw(renderer,frame(target,reopened->preview(m),m,holder))==draw(renderer,painted)&&before->encode()==frozen,"Restart derives the same logical coordinates without changing serialized source authority");
  if(!maps->uv1){auto invalid=std::make_shared<SourceMaterialBindings>(*holder);invalid->material.textures[0].texcoord=1;auto bad=frame(target,paint->preview(m),m,invalid);check(rejects([&]{draw(renderer,bad);})&&rejects([&]{MaterialSurfaceQuery q(bad->mesh,bad->ranges);}),"A generated UV0 carrier cannot impersonate absent authored MToon UV1");}
 }
 renderer.set_frame_time(0);
 for(bool legacy:{true,false})for(bool alias:{false,true}){
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({128,192,64,128});maps->emission.image=image({100,80,50,255});
  PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,maps,64,policy);auto paint=prepare_source_paint(before,target,vertices);auto paired=PaintedMesh::from_maps(target,maps,64);
  Material m=white();m.color={.7f,.8f,.9f,.8f};m.emissive={.1f,.2f,.05f};auto holder=binding(legacy,m,maps->color.image,maps->emission.image);
  if(alias){holder->material.textures[1]=holder->material.textures[0];holder->roles[1]=holder->roles[0];}
  PaintSettings setting;setting.color={.2f,.7f,.3f,.15f};ink(*paint,setting);ink(*paired,setting);setting.channel=PaintChannel::Emission;setting.emission={.07f,.11f,.03f};ink(*paint,setting);ink(*paired,setting);
  paint->layer_properties("Fractional typed paint",true,.63f);paired->layer_properties("Fractional typed paint",true,.63f);
  for(auto mode:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
   m.alpha=mode;m.alpha_cutoff=.25f;auto actual=frame(target,paint->preview(m),m,holder),reference=frame(target,paired->composite(m),m,holder);
   const auto error=delta(draw(renderer,actual),draw(renderer,reference));maximum=std::max(maximum,error);std::cout<<"TYPED legacy="<<legacy<<" alias="<<alias<<" alpha="<<int(mode)<<" max="<<error<<'\n';check(error<=2,"V0/V1 sparse colour, shade alias, emission and alpha agree with established constant-source composite");
   MaterialSurfaceQuery a(actual->mesh,actual->ranges),b(reference->mesh,reference->ranges);check(bool(a.raycast({0,0,1},{0,0,-1},2))==bool(b.raycast({0,0,1},{0,0,-1},2)),"Typed painted alpha agrees with the established native picker");
  }
 }
 for(float strength:{0.f,1.f}){
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({255,255,255,255});PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;
  auto before=PaintedMesh::from_maps(raw,maps,64,policy);auto paint=prepare_source_paint(before,target,vertices);Material dark=white();dark.color={0,0,0,1};auto holder=binding(false,dark,maps->color.image);holder->material.parameters.shade_color={0,0,0};holder->material.parameters.emission_strength=strength;
  PaintSettings absolute;absolute.channel=PaintChannel::Emission;absolute.emission_units=PaintEmissionUnits::Physical;absolute.emission={100,.005f,0};ink(*paint,absolute);
  const auto pixel=at(draw(renderer,frame(target,paint->preview(dark),dark,holder)));check(pixel[0]>200&&pixel[1]>3&&pixel[2]<=1,"Typed physical emission survives strength0 and tiny green beside bright red without invented blue");
  holder->physical_emission={0,0,0};auto zero=frame(target,before->preview(dark),dark,holder);zero->mesh=raw;zero->ranges[0].material.emissive={1,1,1};check(close(at(draw(renderer,zero)),{0,0,0,255}),"Typed explicit zero absolute background overrides historical material fallback");
 }
 std::cout<<"PASS "<<checks<<" typed source-paint checks maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
