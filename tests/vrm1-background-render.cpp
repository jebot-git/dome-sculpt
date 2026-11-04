// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "dome/vrm-expression.hpp"
#include "vrm1-background-consumer.hpp"
#include <fstream>
int main(int argc,char **argv)try{
 if(argc!=3&&argc!=4)return 2;
 const std::filesystem::path fixture=argv[1],out=argv[2];std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"PBR_DEVICE vendor="<<vendor<<" renderer="<<device<<std::endl;if(argc==4){check(std::string_view(argv[3])=="--hardware","Only the explicit strict hardware flag is accepted");check(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos,"Actual PCI03 Intel Arc A770 renderer is required");}Scene baseline=Scene::load(fixture/"baseline.dsculpt");const auto original=baseline.encode();const auto id=baseline.active().id;
 role_consumer::Model model(fixture/"source.vrm");Fbo fbo;SurfaceRenderer native;native.set_output_encoding(SurfaceOutputEncoding::Linear);V1BackgroundConsumer independent;Document doc;doc.replace(baseline.snapshot());doc.shading(false);
 check(doc.wait_mesh(),"Actual source V1 PBR view meshes");auto frame=doc.frame();Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};for(auto p:frame.surface->mesh.positions){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}const auto center=(hi+lo)*.5f;const float scale=1.4f/std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});Mat4 camera{scale,0,0,0,0,scale,0,0,0,0,scale,0,-center.x*scale,-center.y*scale,-center.z*scale,1};
 auto compare=[&](Scene current,float weight,const std::string &name,bool sensitive){
  const auto exact=current.encode();const auto &object=current.object(id);doc.replace(current.snapshot());doc.shading(false);
  check(doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.frame().state->pbr_view&&doc.frame().surface->pbr_view,"Actual immutable V1 PBR role and current worker context agree");frame=doc.frame();
  fbo.bind();native.draw(frame.surface,{},camera,{0,0,2},96,96,true);const auto actual=read();const auto geometry=native.buffer_stats();const auto textures=native.texture_stats();fbo.bind();native.draw(frame.surface,{},camera,{0,0,2},96,96,true);
  check(read()==actual&&native.buffer_stats().geometry_uploads==geometry.geometry_uploads&&native.texture_stats().uploads==textures.uploads,"No-op immutable PBR redraw changes no pixels and uploads no geometry/images");
  independent.expression=weight;independent.requested=object.material;independent.absolute_background=object.material_bindings->physical_emission;
  const auto source=object.painted->source_sampling_original()->source_maps();independent.current_base=source->color.uv_transform;independent.current_emission=source->emission.uv_transform;
  fbo.bind();const auto expected=independent.render(model,0,camera,object);const auto error=delta(actual,expected);size_t visible=0;for(size_t i=3;i<actual.size();i+=4)visible+=actual[i]!=0;
  std::cout<<"PBR_BACKGROUND case="<<name<<" weight="<<weight<<" max="<<error<<" visible="<<visible<<std::endl;
  for(const auto &[label,pixels]:std::array<std::pair<const char*,Pixels>,2>{{{"native",actual},{"independent",expected}}}){auto png=Image{96,96,pixels}.png();std::ofstream file(out/(std::string(label)+"-"+name+".png"),std::ios::binary);file.write(reinterpret_cast<const char*>(png.data()),png.size());}
  check(visible>300&&error<=4,"Independent V1 PBR background/color/emission/alpha preserves paint at unchanged four-level bound");
  if(sensitive){
   auto sensitivity=[&](bool &flag,const char *label){flag=true;fbo.bind();const auto wrong=independent.render(model,0,camera,object);flag=false;const int difference=delta(expected,wrong);std::cout<<"PBR_SENSITIVITY "<<label<<" max="<<difference<<std::endl;check(difference>2,label);};
   sensitivity(independent.wrong_uv,"Original source UV expression affects the independent PBR image");
   sensitivity(independent.wrong_paint,"Moving fixed native paint with source UV is visibly wrong");
   sensitivity(independent.wrong_tint,"Multiplying existing paint by expression color is visibly wrong");
   sensitivity(independent.wrong_sampler,"Replacing real source samplers is visibly wrong");
   if(!object.painted->has_physical_emission())sensitivity(independent.omit_strength,"Legacy painted emission keeps authored source strength");
   if(object.painted->has_physical_emission())sensitivity(independent.scale_physical,"Absolute physical paint is not scaled by typed strength");
  }
  check(doc.copy_scene().encode()==exact,"PBR role and render preserve complete native source/paint state");
  const auto save=out/(name+".dsculpt");doc.save_async(save).get();auto reopened=Scene::load(save);check(reopened.encode()==exact,"PBR Save/restart retains every source, typed factor and sparse paint owner value");
  doc.replace(reopened);doc.shading(false);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Reopened source-owned PBR context publishes");fbo.bind();native.draw(doc.frame().surface,{},camera,{0,0,2},96,96,true);check(read()==actual,"Reopened native PBR image is pixel-exact");
 };
 for(int mode=0;mode<3;++mode)for(float weight:{0.f,.35f,1.f}){
  Vrm1ExpressionWeights weights;weights.preset["happy"]=weight;auto current=bake_vrm1_expression_pose(baseline,id,weights);auto &object=current.object(id);object.material.alpha=mode==1?Material::AlphaMode::Mask:mode==2?Material::AlphaMode::Blend:Material::AlphaMode::Opaque;object.material.alpha_cutoff=.5f;
  compare(std::move(current),weight,"m"+std::to_string(mode)+"-w"+std::to_string(weight),mode==0&&weight==1);
 }
 Vrm1ExpressionWeights weights;weights.preset["happy"]=1;auto physical=bake_vrm1_expression_pose(baseline,id,weights);auto &object=physical.object(id);object.painted=object.painted->snapshot();
 Brush brush;for(auto p:object.painted->mesh().positions)brush.from+=p;brush.from=brush.from/float(object.painted->mesh().positions.size());brush.to=brush.from;brush.radius=2;brush.strength=.7f;
 PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={.005f,.5f,100};ink.front_only=false;
 object.painted->begin_stroke();object.painted->stamp(brush,ink);check(object.painted->end_stroke()&&object.painted->has_physical_emission(),"Genuine sparse physical stroke keeps tiny and bright values beside legacy painted emission");
 compare(physical.snapshot(),1,"physical-mixed",true);
 for(const auto value:{std::array<float,3>{0,0,0},std::array<float,3>{.005f,100,.3f}}){auto background=physical.snapshot();auto &o=background.object(id);auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);binding->physical_emission=value;binding->validate();o.material_bindings=binding;compare(std::move(background),1,value[1]?"absolute-background":"explicit-zero-background",false);}
 check(baseline.encode()==original,"PBR preview leaves complete baseline native owner exact");std::cout<<"PASS "<<checks<<" independent V1 PBR background checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
