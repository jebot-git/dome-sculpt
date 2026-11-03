// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "normal-color-uv-render.cpp"
#undef DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "dome/rig-rest.hpp"
#include "normal-frame-export.hpp"
#include <fstream>
namespace union_authoring {
void move_rest(Scene &scene,uint64_t id){
 const auto &d=*scene.object(id).deformation;auto world=scene.world_matrix(id)*AffineTransform(d.skeleton->world().front());auto p=world.point({0,0,0});
 RigRestJointEdit edit;edit.joint=0;edit.world_position=Vec3{float(p[0]+.021),float(p[1]-.013),float(p[2])};RigRestOptions options;options.linked=false;
 if(!author_rig_rest(scene,id,{&edit,1},options))throw std::runtime_error("Combined native rig edit did not change its REST source");
}
uint64_t raw_coordinates(Scene &scene,uint64_t active){
 auto id=scene.duplicate(active);auto &object=scene.object(id);object.name="Owned raw UV1 and UV2 source";object.appearance.reset();object.material_bindings.reset();object.polygon.reset();object.shape_source.reset();object.rig_source.reset();object.material.unlit=true;
 auto mesh=object.painted->mesh();auto maps=std::make_shared<PaintSourceMaps>();auto image=std::make_shared<const Image>(Image{2,2,{255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255}});
 maps->color.image=image;maps->color.texcoord=2;maps->emission.image=image;maps->emission.texcoord=1;
 auto uv1=std::make_shared<std::vector<std::array<float,2>>>(mesh.uv),uv2=std::make_shared<std::vector<std::array<float,2>>>(mesh.uv);
 for(auto &p:*uv1)p={p[1]*.8f+.05f,p[0]*.7f+.1f};for(auto &p:*uv2)p={p[0]*.65f+.13f,p[1]*.6f+.17f};maps->uv1=uv1;maps->extra_uv_sets.emplace(2,uv2);
 object.painted=PaintedMesh::from_maps(std::move(mesh),maps,256);scene.select(active);return id;
}
void skin_products(const DeformationState &a,const DeformationState &b){
 const auto x=a.skeleton->skin_matrices(a.pose),y=b.skeleton->skin_matrices(b.pose);if(x.size()!=y.size())throw std::runtime_error("Combined rig edit changed joint identities");
 for(size_t j=0;j<x.size();++j)for(size_t k=0;k<16;++k)if(std::abs(double(x[j][k])-y[j][k])>2e-5)throw std::runtime_error("Combined rig edit changed actual current skin product");
}
void ink(Document &doc,const Scene &scene){
 const auto &o=scene.active();auto mesh=o.deformation->preview(o.painted->mesh());size_t face=0;while(face<mesh.indices.size()/3&&mesh.normals[mesh.indices[face*3]].z<.8f)++face;
 if(face==mesh.indices.size()/3)throw std::runtime_error("Combined posed paint target missing");Vec3 p{};for(size_t k=0;k<3;++k)p+=mesh.positions[mesh.indices[face*3+k]]/3.f;
 auto matrix=scene.world_matrix(o.id);auto q=matrix.point({p.x,p.y,p.z});auto n=matrix.normal({mesh.normals[mesh.indices[face*3]].x,mesh.normals[mesh.indices[face*3]].y,mesh.normals[mesh.indices[face*3]].z});p={float(q[0]),float(q[1]),float(q[2])};Vec3 normal=normalized(Vec3{float(n[0]),float(n[1]),float(n[2])});
 PaintSettings settings;settings.color={1,0,0,1};doc.paint_settings(settings);NativeStrokeSample sample;sample.origin=p+normal;sample.direction=normal*-1;sample.brush.radius=.22f;sample.brush.blend=0;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual native paint is admitted after lossless UV and exact-pose rig editing");doc.end_native_stroke(true);if(!doc.wait_native_stroke())throw std::runtime_error("Combined native paint did not finish");settled(doc);
}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<vendor<<' '<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("UHD")==std::string::npos),"Combined master fixture uses its requested actual graphics device");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 auto base=joint::prepare(true,Material::AlphaMode::Opaque);const auto active=base.active().id;const auto raw=union_authoring::raw_coordinates(base,active);base.object(raw).visible=false;base.validate();const auto original=base.encode();
 for(int pose=0;pose<3;++pose){
  auto before=base.snapshot();posed(before,pose,.35f);auto fitted=before.snapshot();fitted.object(raw).visible=true;
  const auto raw_before=fitted.object(raw).painted->source_maps();union_authoring::move_rest(fitted,raw);fitted.object(raw).visible=false;union_authoring::move_rest(fitted,active);fitted.select(active);
  check(fitted.active().rig_source&&fitted.object(raw).rig_source,"Actual separate native rest edits capture both complete first rig sources");
  union_authoring::skin_products(*before.active().deformation,*fitted.active().deformation);
  check(std::any_of(fitted.active().deformation->pose.begin(),fitted.active().deformation->pose.end(),[](const auto &p){return bool(p.full_local_matrix);}),"Combined fitting owns genuine compensated exact-pose matrices");
  Document doc;doc.replace(fitted);settled(doc);doc.shading(false);settled(doc);doc.begin_uv();check(doc.wait_uv(),"Actual master UV worker opens the fitted prepared source");doc.edit_uv(UvAction::RetainNormals);
  const auto &mesh=fitted.active().painted->mesh();size_t face=0;while(face<mesh.indices.size()/3&&mesh.normals[mesh.indices[face*3]].z<.9f)++face;std::array<float,2> p{};for(size_t k=0;k<3;++k){auto uv=mesh.uv[mesh.indices[face*3+k]];p[0]+=uv[0]/3;p[1]+=uv[1]/3;}
  doc.edit_uv(UvAction::Pick,p[0],p[1]);doc.edit_uv(UvAction::Scale,.7f);doc.edit_uv(UvAction::Rotate,1.57079632679f);check(doc.wait_uv()&&doc.frame().state->uv->valid,"Fitted master source validates the actual fractional scale and turn");doc.end_uv(true);
  const auto project=directory/("fitted-"+std::to_string(pose)+".dsculpt");doc.save_async(project).get();settled(doc);auto accepted=doc.copy_scene();auto encoded=accepted.encode();auto restarted=Scene::load(project);
  check(encoded.at("version")==26&&encoded.at("polygon_sources").at("version")==6&&encoded.at("source_materials").at("version")==9&&restarted.encode()==encoded,"One actual native Save/restart composes Scene26 raw coordinates, rig history6 and independent material9 domains");
  const auto &b=*restarted.active().material_bindings;auto colour=b.color_preparation->paint_sampling;auto normal=b.normal_preparation->sampling_domain;
  check(colour&&normal&&normal->metadata_sampling_domain==restarted.active().painted->paint_sampling_domain()&&colour->before_deformation()==normal->before_deformation&&std::any_of(normal->before_deformation->pose.begin(),normal->before_deformation->pose.end(),[](const auto &p){return bool(p.full_local_matrix);}),"Independent colour/normal nodes retain the same exact-pose before-state without borrowing either authority");
  const auto &raw_current=restarted.object(raw);check(raw_current.rig_source&&raw_current.rig_source->paint->source_maps()->color.texcoord==2&&*raw_current.painted->source_maps()->extra_uv_sets.at(2)==*raw_before->extra_uv_sets.at(2)&&*raw_current.rig_source->paint->source_maps()->uv1==*raw_before->uv1,"Current and retained first-rig maps preserve exact UV1/UV2 arrays and original role IDs");
  auto surfaces=accepted.surfaces();auto surface=std::find_if(surfaces.begin(),surfaces.end(),[&](const auto &s){return s.id==active;});check(surface!=surfaces.end(),"Expanded-frame test uses the actual current authoring surface");auto expanded=detail::normal_frame_export(*surface,detail::SourceMorphSpace::Native,{},true);check(expanded.surface.rig_source==accepted.active().rig_source,"Paint/frame corner expansion preserves its exact complete rig-source capsule");
  auto malformed=encoded;malformed["version"]=24;check(rejects([&]{Scene::decode(malformed);}),"Older outer contexts cannot hide exact poses and genuine extra-coordinate rig histories");
  check(doc.undo()&&doc.copy_scene().encode()==fitted.encode()&&doc.redo()&&doc.copy_scene().encode()==encoded,"Combined UV worker retains exactly one global Undo/Redo and all first rig sources");settled(doc);
  for(float weight:{0.f,.35f,1.f}){
   auto a=before.snapshot(),c=restarted.snapshot();for(auto *s:{&a,&c}){auto d=*s->active().deformation;d.morph_weights[0]=weight;s->set_deformation(active,std::move(d));}
   Document old,current;old.replace(a);current.replace(c);settled(old);settled(current);old.shading(false);current.shading(false);settled(old);settled(current);
   const auto x=draw(renderer,old.frame().surface),y=draw(renderer,current.frame().surface);std::cout<<"UNION_NATIVE pose="<<pose<<" morph="<<weight<<" max="<<delta(x,y)<<std::endl;check(delta(x,y)<=2,"Exact-pose rig transport and lossless UV preserve actual partial-Bake pixels under morph mixing");
   auto output=directory/("fitted-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");current.export_asset(output).get();prepared_normal_consumer::Model model(output);fbo.bind();independent.render(model.data);auto z=read();std::cout<<"UNION_GLB max="<<delta(y,z)<<std::endl;check(delta(y,z)<=3,"Independent current GLB preserves rig factoring, authored frames and independent paint/normal sampling");
  }
  if(pose==1){auto prior=doc.copy_scene();const auto rgba=prior.active().painted->preview(prior.active().material)->pbr_color.rgba;union_authoring::ink(doc,prior);auto changed=doc.copy_scene();check(changed.active().painted->preview(changed.active().material)->pbr_color.rgba!=rgba&&changed.active().rig_source==prior.active().rig_source&&changed.active().painted->baked()->normal_authority==prior.active().painted->baked()->normal_authority,"Post-UV native paint changes real PBR pixels while preserving first rig and independent normal event");auto path=directory/"post-paint.dsculpt";doc.save_async(path).get();check(Scene::load(path).encode()==changed.encode()&&doc.undo()&&doc.copy_scene().encode()==prior.encode()&&doc.redo()&&doc.copy_scene().encode()==changed.encode(),"Combined exact-pose/domain paint has actual ordered restart and one Undo/Redo");settled(doc);}
  auto restored=restarted.snapshot();restore_rig_source(restored,active,false);check(restored.active().painted->encode()==before.active().painted->encode()&&restored.active().deformation->encode(restored.active().painted->mesh())==before.active().deformation->encode(before.active().painted->mesh()),"Explicit rig Restore retains its complete genuine pre-rest painting/normal/morph state");
 }
 check(base.encode()==original,"All combined authoring and malformed attempts retain the exact original scene");std::cout<<"PASS "<<checks<<" coherent source authoring checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
