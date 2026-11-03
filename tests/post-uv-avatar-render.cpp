// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/shapekey-authoring.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/creation.hpp"
#include "dome/vrm.hpp"
#include "prepared-normal-consumer.hpp"
namespace frame_consumer=prepared_normal_consumer;
namespace {
Pixels key_draw(SurfaceRenderer &renderer,std::shared_ptr<const MeshSnapshot> mesh){auto projection=identity;projection[10]=-1;renderer.draw(std::move(mesh),{},projection,{0,0,2},Fbo::size,Fbo::size,true);return read();}
void controlled_lighting(Scene &scene,uint64_t group){
 for(const auto &item:scene.objects())if(item.group==group){auto &o=scene.object(item.id);auto b=std::make_shared<SourceMaterialBindings>(*o.material_bindings);auto &m=b->material;
  // Control unrelated lighting/color roles, retaining the ACTUAL original
  // normal texture, sampler, factor and artist frame authority without a
  // constant-normal replacement. Raw original assets remain untouched.
  for(size_t slot=0;slot<b->roles.size();++slot)if(slot!=size_t(MToonSlot::Normal)){m.textures[slot].index=-1;b->roles[slot]={};}
  m.parameters.base_color={.6f,.4f,.2f,1};m.parameters.shade_color={.1f,.05f,.02f};m.parameters.toony=.5f;m.parameters.shift=-.2f;m.parameters.indirect_intensity=0;m.parameters.light_color_attenuation=0;m.parameters.emission={};m.parameters.rim_color={};m.parameters.outline=MToonOutline::None;m.parameters.cull=MToonCull::Back;m.parameters.scroll={};m.parameters.rotation_speed=0;b->validate();o.material_bindings=b;o.material.color=b->native_reference.color;o.material.emissive=b->native_reference.emission;o.material.alpha=Material::AlphaMode::Opaque;
 }
}
void avatar_graphics(const std::filesystem::path &prefix,const char *baseline,SurfaceRenderer &native,Fbo &framebuffer,frame_consumer::Renderer &consumer){
 Scene scene;const auto group=append_humanoid(scene,humanoid_baseline_defaults(baseline),"Authored CC0 avatar",{},256);scene.group(group).transform.scale=.8f;scene.group(group).transform.position.y=-.7f;controlled_lighting(scene,group);
 auto part=std::find_if(scene.objects().begin(),scene.objects().end(),[&](const auto &o){return o.group==group&&o.deformation&&o.deformation->morphs&&!o.deformation->morphs->targets.empty()&&o.deformation->morphs->targets.size()<127&&o.name.find("Face_00_SKIN")!=std::string::npos;});
 check(part!=scene.objects().end(),"Actual bundled CC0 VRoid supplies artist morph identities for native authoring");const auto object=part->id;scene.select(object);{auto &o=scene.active();auto prepared=prepare_source_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material});o.painted=prepared.paint;o.material_bindings=prepared.material_bindings;scene.validate();}
 Document uv_document;uv_document.replace(scene);check(uv_document.wait_mesh(std::chrono::seconds(120)),"Actual complete avatar initializes native normalUV worker");
 for(int pass=0;pass<2;++pass){uv_document.begin_uv();check(uv_document.wait_uv(std::chrono::seconds(120)),"Actual avatar normalUV draft initializes");uv_document.edit_uv(UvAction::Move,0,(pass?-1.f:1.f)/256.f);check(uv_document.wait_uv(std::chrono::seconds(120)),"Actual avatar exact texel translation validates");uv_document.end_uv(true);check(uv_document.wait_asset(std::chrono::seconds(120))&&uv_document.wait_mesh(std::chrono::seconds(120))&&uv_document.frame().state->error.empty(),"Actual avatar normalUV translation commits with genuine intrinsic planes");}
 scene=uv_document.copy_scene();
 const auto before=scene.encode();const auto original_frames=scene.active().material_bindings->normal_frames;const auto original_appearance=scene.active().appearance;const auto original_targets=scene.active().deformation->morphs->targets.size();
 const auto rest=scene.active().painted->mesh();size_t vertex=rest.indices[2];for(size_t v=0;v<rest.positions.size();++v)if(rest.positions[v].z>rest.positions[vertex].z)vertex=v;
 ShapeKeyEdit edit;edit.name="Native authored detail";const auto created=author_shapekey(scene,object,edit);edit.action=ShapeKeyAction::Replace;edit.target=created.target;edit.positions=rest.positions;for(size_t v=0;v<rest.positions.size();++v)edit.positions[v].y+=.02f*(1.f+.5f*std::sin(rest.positions[v].x*80.f));author_shapekey(scene,object,edit);
 edit.target=0;edit.positions=rest.positions;for(size_t v=0;v<rest.positions.size();++v)edit.positions[v]+=scene.active().deformation->morphs->targets[0].delta[v];edit.positions[vertex].x+=.0025f;author_shapekey(scene,object,edit);
 const auto frames=scene.active().material_bindings->normal_frames;
 check(scene.active().shape_source->material_bindings->normal_frames==original_frames&&scene.active().appearance==original_appearance&&frames->targets.back().source_target==std::nullopt&&frames->current_targets[0]==original_frames->current_targets[0],"Actual VRoid new/replaced keys preserve raw appearance, original artist arrays, stable expression IDs and native-only target provenance");
 auto project=prefix;project+=std::string("-")+baseline+".dsculpt";scene.save(project);const auto stored=scene.encode();scene=Scene::load(project);check(scene.encode()==stored,"Actual nineteen-part avatar reopens authored targets with complete source capsule and frame identities");
 bool mixed=false;for(const auto &o:scene.objects())if(o.group==group&&o.deformation&&o.deformation->skin)for(const auto &weights:o.deformation->skin->vertices)mixed|=weights.size()>1;check(mixed,"Actual CC0 avatar graphical fixture retains genuine mixed-joint vertex influences");
 std::array<Pixels,3> zero_images;
 for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){auto current=scene;
  if(std::getenv("DOME_SHAPE_DIAGNOSTIC_ONLY")&&(pose!=2||weight!=0))continue;
  for(const auto &item:current.objects())if(item.group==group){auto d=*item.deformation;d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);
   if(pose){const auto hips=d.humanoid->joints.at("hips"),spine=d.humanoid->joints.at("spine");d.pose[hips].scale={1.15f,.9f,.7f};d.pose[spine].scale={.85f,1.05f,1.2f};if(pose==2){const float half_angle=std::getenv("DOME_SHAPE_BOUNDARY_POSE")?.075f:.08f;d.pose[spine].rotation=Quat{std::cos(half_angle),0,0,std::sin(half_angle)};}}
   if(d.morphs){d.morph_weights.assign(d.morphs->targets.size(),0);if(item.id==object){d.morph_weights[0]=weight;d.morph_weights[original_targets]=weight*.6f;}}current.set_deformation(item.id,std::move(d));
  }
  Document doc;doc.replace(current);check(doc.wait_mesh(std::chrono::seconds(60))&&doc.frame().state->error.empty(),"Actual avatar worker transports new and replaced artist keys together through current nonuniform joint poses");
  auto objects=current.preview_surfaces();std::erase_if(objects,[&](const auto &o){return o.groups.empty()||o.groups.back().id!=group;});auto path=prefix;path+=std::string("-")+baseline+"-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".vrm";
  const auto report=export_vrm(objects,path);frame_consumer::Model parsed(path);check(report.normal_targets_recomputed==0&&parsed.data->meshes_count==19,"Actual exported CC0 VRM carries authored NORMAL/TANGENT targets without export-time geometric replacement");
  const auto wanted=key_draw(native,doc.frame().surface);framebuffer.bind();consumer.render(parsed.data,&parsed.json.at("extensions").at("VRM"),*objects[0].appearance->source_basis_quaternion,true);const auto pixels=read();const auto error=delta(wanted,pixels);size_t covered=0;for(size_t n=3;n<pixels.size();n+=4)covered+=pixels[n]>0;

  if(weight==0)zero_images[pose]=wanted;else if(weight==1)check(delta(zero_images[pose],wanted)>0,"Authored real-avatar targets change visible normal-map/geometry pixels under each qualified pose");

  std::cout<<"POST_UV_AUTHORED_VROID baseline="<<baseline<<" pose="<<pose<<" weight="<<weight<<" maxerror="<<error<<" covered="<<covered<<'\n';check(covered>300&&error<=4,"Independent parsed VRM0 graphical consumer matches newly authored actual avatar geometry and artist direction targets");
 }
 const auto immutable=scene.encode();auto incompatible=scene;auto binding=std::make_shared<SourceMaterialBindings>(*incompatible.active().material_bindings);auto set=std::make_shared<NormalFrameSet>(*binding->normal_frames);auto distinct=std::make_shared<NormalFrames>(*set->mtoon);distinct->normals[0]=normalized(distinct->normals[0]+Vec3{.05f,0,0});set->mtoon=distinct;binding->normal_frames=set;incompatible.active().material_bindings=binding;
 auto out=prefix;out+=std::string("-")+baseline+"-incompatible.vrm";check(rejects([&]{incompatible.validate();export_vrm(incompatible.preview_surfaces(),out);})&&scene.encode()==immutable&&!std::filesystem::exists(out),"Mismatched prepared target-frame authority refuses before output while the authenticated source remains exact");

 ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(scene,object,restore);check(scene.encode()==before,"Actual avatar Restore after restart/export recovers all original expressions, source frames, paint and deformation exactly");
}
}
#ifndef DOME_POST_UV_AVATAR_RENDER_NO_MAIN
int main(int argc,char **argv)try{
 if(argc<2)return 2;std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<3||std::string(argv[2])!="--hardware"||device.find("A770")!=std::string::npos,"Actual prepared VRoid consumer uses requested A770");Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;frame_consumer::Renderer consumer;
 avatar_graphics(prefix,"vroid-beta-female",native,framebuffer,consumer);avatar_graphics(prefix,"vroid-beta-male",native,framebuffer,consumer);
 std::cout<<"PASS "<<checks<<" actual post-UV prepared VRoid consumer checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}

#endif
