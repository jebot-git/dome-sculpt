// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-shapes-fixtures.hpp"
#include "prepared-normal-consumer.hpp"
#include "colored-vrm-bake-fixture.hpp"
namespace {
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Prepared authoring failed: "+d.frame().state->error);}
size_t author(Document &d){
 const auto before=d.copy_scene();const auto count=before.active().deformation->morphs->targets.size();
 ShapeKeyEdit edit;edit.name="Prepared graphical key";d.author_shape(edit);ready(d);edit.action=ShapeKeyAction::Replace;edit.target=count;edit.positions=before.active().painted->mesh().positions;edit.positions[2].z+=.08f;edit.positions[1].x+=.04f;d.author_shape(edit);ready(d);
 const auto after=d.copy_scene();check(after.active().painted->encode()==before.active().painted->encode(),"Real authoring preserves every intrinsic/detail/paint plane before graphical qualification");
 check(after.active().shape_source&&after.active().material_bindings->normal_preparation!=before.active().material_bindings->normal_preparation,"Real worker publishes a newly authenticated receipt with retained complete prepared source");
 return count;
}
void pose(Scene &s,size_t target,int mode,float weight){auto d=*s.active().deformation;d.morph_weights.assign(d.morphs->targets.size(),0);d.morph_weights[target]=weight;d.morph_weights[0]=weight*.25f;d.pose.clear();for(const auto &b:d.skeleton->bones)d.pose.push_back(b.rest);
 if(mode){d.pose[0].scale=mode==1?Vec3{1.1f,.85f,.65f}:Vec3{.85f,1.05f,1.4f};const float a=mode==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};d.pose[1].scale=mode==1?Vec3{.9f,1.1f,1.4f}:Vec3{1.15f,.9f,.8f};}s.set_deformation(s.active().id,d);
}
void generic(const std::filesystem::path &prefix,bool n_only,SurfaceRenderer &native,Fbo &f,prepared_normal_consumer::Renderer &consumer){
 Document d;d.replace(prepared_shape_fixture::model(n_only));ready(d);const auto slot=author(d);auto project=prefix;project+=n_only?"-n-only.dsculpt":"-native.dsculpt";d.save(project);Document reopened;reopened.open(project);ready(reopened);const auto scene=reopened.copy_scene();check(scene.encode()==d.copy_scene().encode(),"New authored normal model reopens before independent file rendering");
 Pixels zero;
 for(int mode=0;mode<3;++mode)for(float weight:{0.f,.35f,1.f}){auto current=scene;pose(current,slot,mode,weight);Document posed;posed.replace(current);ready(posed);auto path=prefix;path+=(n_only?"-n-only":"-native")+std::string("-p")+std::to_string(mode)+"-w"+std::to_string(int(weight*100))+".glb";posed.export_asset(path).get();prepared_normal_consumer::Model file(path);
  const auto wanted=draw(native,posed.frame().surface);f.bind();consumer.render(file.data);const auto actual=read();const int error=delta(wanted,actual);size_t covered=0;for(size_t n=3;n<actual.size();n+=4)covered+=actual[n]>0;
  std::cout<<"PREPARED_KEY_PBR n_only="<<n_only<<" pose="<<mode<<" weight="<<weight<<" maxerror="<<error<<" covered="<<covered<<'\n';check(covered>1000&&error<=4,"Independent SampleRenderer-policy GLB consumer matches newly authored normal keys through simultaneous morphs and mixed skin");
  if(mode==0&&weight==0)zero=wanted;if(mode==0&&weight==1)check(delta(zero,wanted)>2,"New normal key produces visible geometry/direction changes rather than a vacuous file comparison");
  if(mode==0&&weight==1){auto &normal=file.data->meshes[0].primitives[0].material->normal_texture;const auto old=normal.texture;normal.texture=nullptr;f.bind();consumer.render(file.data);check(delta(actual,read())>2,"Independent pixels visibly depend on real prepared normal detail");normal.texture=old;}
 }
}
void typed_model(const std::filesystem::path &prefix,SurfaceRenderer &native,Fbo &f,prepared_normal_consumer::Renderer &consumer){
 Document d;d.replace(colored_vrm_fixture::model(true,true,true));ObjectBakeSettings settings;settings.prepare_colors=settings.prepare_normals=true;settings.front_mm=settings.back_mm=500;settings.ao_samples=0;settings.padding=4;settings.allow_partial=true;d.bake_from_object(d.copy_scene().objects().back().id,settings);ready(d);
 auto current=d.copy_scene();auto binding=std::make_shared<SourceMaterialBindings>(*current.active().material_bindings);binding->material.parameters.normal_scale=.65f;current.active().material_bindings=binding;d.replace(current);ready(d);const auto slot=author(d);auto project=prefix;project+="-artist.dsculpt";d.save(project);Document reopened;reopened.open(project);ready(reopened);const auto scene=reopened.copy_scene();
 check(scene.active().material_bindings->material.parameters.normal_scale==.65f,"Prepared artist authoring retains current normal strength while replaying original raw sampling input");
 Pixels zero;
 for(int mode=0;mode<3;++mode)for(float weight:{0.f,.35f,1.f}){auto posed_scene=scene;pose(posed_scene,slot,mode,weight);Document posed;posed.replace(posed_scene);ready(posed);auto path=prefix;path+="-artist-p"+std::to_string(mode)+"-w"+std::to_string(int(weight*100))+".vrm";posed.export_asset(path,AssetExportFormat::Avatar).get();prepared_normal_consumer::Model file(path);
  const auto wanted=draw(native,posed.frame().surface);f.bind();consumer.render(file.data,&file.json.at("extensions").at("VRM"));const auto actual=read();const auto error=delta(wanted,actual);size_t covered=0;for(size_t n=3;n<actual.size();n+=4)covered+=actual[n]>0;
  std::cout<<"PREPARED_KEY_MTOON pose="<<mode<<" weight="<<weight<<" maxerror="<<error<<" covered="<<covered<<'\n';check(covered>1000&&error<=4,"Independent raw-TBN VRM0 consumer matches newly reauthored artist N/T and prepared detail");
  if(mode==0&&weight==0)zero=wanted;if(mode==0&&weight==1)check(delta(zero,wanted)>2,"Artist normal key visibly changes the actual typed viewport");
  auto fallback=posed_scene;auto pbr=std::make_shared<SourceMaterialBindings>(*fallback.active().material_bindings);pbr->has_mtoon=false;pbr->roles={};fallback.active().material_bindings=pbr;fallback.active().material.toon=false;Document p; p.replace(fallback);ready(p);const auto expected=draw(native,p.frame().surface);f.bind();consumer.render(file.data);const auto pbr_error=delta(expected,read());std::cout<<"PREPARED_KEY_VRM_PBR pose="<<mode<<" weight="<<weight<<" maxerror="<<pbr_error<<'\n';check(pbr_error<=4&&expected!=wanted,"The same actual VRM preserves its distinct SampleRenderer-policy PBR fallback after artist key authoring");
 }
}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());NativeGraphics graphics;std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<'\n';check(argc<3||std::string(argv[2])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Prepared normal-key consumers run on the requested graphics device");Fbo f;f.bind();SurfaceRenderer native;prepared_normal_consumer::Renderer consumer;generic(prefix,false,native,f,consumer);generic(prefix,true,native,f,consumer);typed_model(prefix,native,f,consumer);std::cout<<"PASS "<<checks<<" prepared-normal shape graphics checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
