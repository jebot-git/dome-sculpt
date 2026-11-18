// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-tool.hpp"
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/normal-frames.hpp"
#include "dome/rig-edit.hpp"
#include "document-authority.hpp"
#include "import-controller-driver.hpp"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*label){if(!value)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<'\n';}
template<class F>bool refuses(F action){try{action();return false;}catch(const std::exception&){return true;}}
float difference(const Mesh&a,const Mesh&b){if(a.positions.size()!=b.positions.size())throw std::runtime_error("Unexpected vertex count");float maximum=0;for(size_t i=0;i<a.positions.size();++i)maximum=std::max(maximum,length(a.positions[i]-b.positions[i]));return maximum;}
void source_retained(const CreatedHumanoid&base,const CreatedHumanoid&next){
 check(next.parts.size()==19||next.parts.size()==20,"Creator outputs complete genuine source parts with only an optional new groom");
 for(size_t i=0;i<19;++i){const auto&a=base.parts[i],&b=next.parts[i];
  if(a.mesh.indices!=b.mesh.indices||a.mesh.uv!=b.mesh.uv||a.deformation.skin->vertices!=b.deformation.skin->vertices||
     a.appearance->original_bytes!=b.appearance->original_bytes||a.appearance->asset!=b.appearance->asset||a.appearance->decoded_images!=b.appearance->decoded_images||
     a.appearance->source_vertices!=b.appearance->source_vertices||a.appearance->source_joint_nodes!=b.appearance->source_joint_nodes||a.appearance->source_morph_targets!=b.appearance->source_morph_targets||
     a.appearance->material_json!=b.appearance->material_json||a.appearance->mtoon_json!=b.appearance->mtoon_json||*a.appearance->vrm_json!=*b.appearance->vrm_json)
   throw std::runtime_error("Creator changed original topology/UV/skin/source/image/material/morph authority");
  const auto &ar=*a.deformation.skeleton,&br=*b.deformation.skeleton;
  if(ar.bones.size()!=br.bones.size())throw std::runtime_error("Creator changed source rig size");
  for(size_t joint=0;joint<ar.bones.size();++joint)if(ar.bones[joint].name!=br.bones[joint].name||ar.bones[joint].parent!=br.bones[joint].parent)throw std::runtime_error("Creator changed authored joint hierarchy");
  if(bool(a.deformation.morphs)!=bool(b.deformation.morphs))throw std::runtime_error("Creator dropped source morphs");
  if(a.deformation.morphs){if(a.deformation.morphs->targets.size()!=b.deformation.morphs->targets.size())throw std::runtime_error("Creator changed morph identities");for(size_t target=0;target<a.deformation.morphs->targets.size();++target)if(a.deformation.morphs->targets[target].name!=b.deformation.morphs->targets[target].name)throw std::runtime_error("Creator renamed an original morph");}
  b.normal_frames->validate();for(auto n:b.mesh.normals)if(!std::isfinite(n.x)||!std::isfinite(n.y)||!std::isfinite(n.z)||std::abs(length(n)-1)>1e-3f)throw std::runtime_error("Creator produced invalid normal transport");
 }
 check(true,"Every original source vertex/UV/skin/joint/morph/image/material identity survives the actual control output");
}
void controls(const std::filesystem::path&root){
 nlohmann::json evidence=nlohmann::json::array();
 for(const char*id:{"vroid-beta-female","vroid-beta-male"}){
  const auto original_controls=humanoid_baseline_defaults(id);const auto original=create_humanoid(original_controls);
  for(size_t parameter=0;parameter<humanoid_creator_parameter_count;++parameter){
   auto c=original_controls;const auto descriptor=humanoid_creator_parameter(c,parameter);
   const float value=parameter==0?c.height+.10f:parameter>=17?.35f:.45f;
   set_humanoid_creator_parameter(c,parameter,value);auto changed=create_humanoid(c);source_retained(original,changed);
   float geometry=0,color=0;for(size_t part=0;part<19;++part){geometry=std::max(geometry,difference(original.parts[part].mesh,changed.parts[part].mesh));for(size_t k=0;k<4;++k)color=std::max(color,std::abs(original.parts[part].color[k]-changed.parts[part].color[k]));}
   if(parameter<14)check(geometry>1e-5f,"Exposed body/face/style parameter changes genuine CC0 geometry");
   else if(parameter<17){check(changed.parts.size()==20&&geometry==0,"Hair controls add an original fitted groom without rewriting any source hair/body vertex");}
   else check(color>.01f&&geometry==0,"Exposed tint changes actual source material factors while retaining source geometry");
   evidence.push_back({{"baseline",id},{"parameter",descriptor.label},{"value",value},{"maximum_source_position_delta",geometry},{"maximum_material_factor_delta",color},{"parts",changed.parts.size()}});
  }
  auto styled=humanoid_preset(HumanoidPreset::Anime,id);styled.hair_length=.35f;styled.hair_volume=.25f;styled.fringe=.2f;styled.skin_color={.8f,.6f,.5f,1};styled.iris_color={.4f,.6f,.9f,1};styled.hair_color={.3f,.2f,.15f,1};
  const auto model=create_humanoid(styled);source_retained(original,model);size_t moving=0,morphs=0;
  for(const auto&part:model.parts){auto deformation=part.deformation;const auto rest=deformation.preview(part.mesh);check(difference(rest,part.mesh)<2e-5f,"Rest skin/inverse binds preserve generated body and groom positions");
   for(const auto&m:deformation.skeleton->skin_matrices({}))for(size_t k=0;k<16;++k)if(std::abs(m[k]-(k%5==0?1.f:0.f))>2e-5f)throw std::runtime_error("Generated rest skin is not identity");
   deformation.pose.clear();for(const auto&b:deformation.skeleton->bones)deformation.pose.push_back(b.rest);const auto head=deformation.humanoid->joints.at("head");deformation.pose[head]=adjust_joint_pose(deformation.pose[head],arcball({0,1,0},normalized({.08f,1,.02f})),{});
   if(deformation.morphs){for(size_t t=0;t<deformation.morphs->targets.size();++t){bool nonzero=false;for(auto v:deformation.morphs->targets[t].delta)nonzero|=length(v)>1e-5f;if(nonzero){deformation.morph_weights.assign(deformation.morphs->targets.size(),0);deformation.morph_weights[t]=.3f;++morphs;break;}}}
   const auto preview=deformation.preview(part.mesh);preview.validate();check(preview.indices==part.mesh.indices&&preview.uv==part.mesh.uv,"Real posed/morphed creator output keeps its stable topology and authored UVs");moving+=difference(preview,part.mesh)>1e-5f;
  }
  check(moving>2&&morphs>0,"Real source head skin and authored facial targets remain usable after combined body/face/hair settings");
  Scene scene;append_humanoid(scene,styled,"Creator output");const auto path=root/(std::string(id)+"-styled.glb");export_glb(scene.surfaces(true),path);
  cgltf_options options{};cgltf_data*raw=nullptr;check(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,"Independent cgltf parses actual styled creator export");std::unique_ptr<cgltf_data,decltype(&cgltf_free)>data(raw,cgltf_free);check(cgltf_load_buffers(&options,data.get(),path.c_str())==cgltf_result_success&&cgltf_validate(data.get())==cgltf_result_success,"Independent glTF consumer validates actual styled geometry, skins, morphs and image package");
  size_t primitives=0,skinned=0,targets=0;for(size_t n=0;n<data->nodes_count;++n)if(data->nodes[n].mesh){skinned+=bool(data->nodes[n].skin);for(size_t p=0;p<data->nodes[n].mesh->primitives_count;++p){++primitives;targets+=data->nodes[n].mesh->primitives[p].targets_count;}}
  check(primitives==20&&skinned>=19&&targets>0,"Export retains all real source primitives plus the authored groom and actual skin/morph payloads");
 }
 std::ofstream(root/"creator-control-effects.json")<<evidence.dump(2)<<'\n';
}
void ready(Document&d){if(!d.wait_asset(std::chrono::seconds(180))||!d.wait_mesh(std::chrono::seconds(180)))throw std::runtime_error("Creator worker/surface timed out");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
std::atomic_bool gate{false};std::mutex gate_mutex;std::condition_variable changed;bool entered=false,released=false;
void arm(){std::lock_guard lock(gate_mutex);entered=released=false;gate=true;}
void await_gate(){std::unique_lock lock(gate_mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[]{return entered;}))throw std::runtime_error("Creator Save barrier timed out");}
void release(){std::lock_guard lock(gate_mutex);released=true;changed.notify_all();}
Scene authored(){
 Scene scene;append_humanoid(scene,humanoid_preset(HumanoidPreset::Toon),"Existing 作家 avatar");
 auto &object=scene.active();object.painted=object.painted->snapshot();object.painted->add_layer("Existing artist paint");const auto&m=object.painted->mesh();bool painted=false;
 for(size_t i=0;i<m.indices.size()&&!painted;i+=3){const auto a=m.positions[m.indices[i]],b=m.positions[m.indices[i+1]],c=m.positions[m.indices[i+2]];Brush brush;brush.from=brush.to=(a+b+c)/3;brush.radius=.02f;PaintSettings ink;ink.color={.9f,.1f,.3f,1};ink.flow=1;object.painted->begin_stroke();object.painted->stamp(brush,ink);painted=object.painted->end_stroke();}
 check(painted,"Actual existing CC0 avatar fixture contains genuine artist-painted pixels");
 RigEdit edit;edit.joint=JointAdjustment{object.deformation->humanoid->joints.at("head"),{0,5,0},{}};edit_rig(scene,object.id,edit);scene.validate();return scene;
}
void old_avatar_retained(const Scene&before,const Scene&after){
 for(const auto&o:before.objects())if(o.mesh_active)check(detail::same_object_authority(o,after.object(o.id)),"New avatar creation preserves every immutable old mesh/paint/source/rest/skin/morph owner");
 for(const auto&g:before.groups()){const auto &next=after.group(g.id);check(g.name==next.name&&g.parent==next.parent&&detail::same_transform(g.transform,next.transform)&&detail::same_humanoid_controls(g.humanoid,next.humanoid),"Variation retains genuine original group transform and creator recipe exactly");}
}
void workspace(const std::filesystem::path&root){
 Document doc;const auto source=authored();doc.replace(source);ready(doc);const auto baseline=doc.copy_scene().encode();ImportDriver driver(doc);driver.open();for(int i:{8,10,2})driver.sector(i);
 check(doc.frame().state->creator&&doc.frame().state->menu_page==33,"Actual controller opens Objects / Avatars / Creator controls");
 check(doc.frame().state->creator->can_copy_selected&&!doc.frame().state->can_undo,"Creator exposes real saved settings while keeping global history separate");
 driver.sector(7);auto view=doc.frame().state->creator;check(view->settings.controls.encode()==source.group(source.active().group).humanoid->encode()&&view->settings.name.find("variation")!=std::string::npos,"Controller copies genuine saved creator parameters into a NEW-avatar draft");
 const auto copied=view->settings;driver.sector(5);check(doc.frame().state->creator->settings.controls.anime==.25f,"Actual controller Anime preset uses the existing bounded VRoid backend values");check(doc.undo()&&doc.frame().state->creator->settings.controls.encode()==copied.controls.encode(),"Local creator Undo restores the exact copied controls without touching the avatar");check(doc.redo()&&doc.frame().state->creator->settings.controls.anime==.25f,"Local creator Redo restores the exact style draft");
 driver.sector(6);check(doc.frame().state->controls.parameter==VrParameter::CreatorValue,"Actual controller opens the shared large creator value slider");const float height=doc.frame().state->creator->settings.controls.height;driver.ray(.85f);driver.panel_button(false);check(doc.frame().state->creator->settings.controls.height==height,"Cancel restores the exact prior creator value without any Scene/history edit");
 driver.sector(6);driver.ray(.7f);driver.panel_button(true);check(std::abs(doc.frame().state->creator->settings.controls.height-1.9f)<1e-6f,"Controller ray applies a real bounded metric body parameter");
 for(int i=0;i<14;++i)driver.sector(1);
 for(float value:{.2f,.15f,.1f}){driver.sector(6);driver.ray(value);driver.panel_button(true);driver.sector(1);}
 check(doc.frame().state->creator->settings.controls.hair_length==.2f&&doc.frame().state->creator->settings.controls.hair_volume==.15f&&doc.frame().state->creator->settings.controls.fringe==.1f,"Actual controller sliders author length, volume and fringe in the native groom settings");
 for(int i=0;i<6;++i)driver.sector(1);driver.sector(6);driver.ray(.3f);driver.panel_button(true);
 check(std::abs(doc.frame().state->creator->settings.controls.hair_color[0]-.3f)<1e-6f,"Actual controller slider authors a retained hair material tint");
 check(doc.copy_scene().encode()==baseline,"All body/face/hair drafts leave genuine prior geometry, paint, source and rig untouched");
 driver.sector(11);auto saved=doc.save_async(root/"created.dsculpt");saved.get();ready(doc);const auto accepted_scene=doc.copy_scene();const auto accepted=accepted_scene.encode();old_avatar_retained(source,accepted_scene);
 check(accepted_scene.objects().size()==source.objects().size()+20&&!doc.frame().state->creator,"Controller Apply creates a complete distinct avatar through the native ordered worker");check(Scene::load(root/"created.dsculpt").encode()==accepted,"Save ordered immediately after Apply/reopen retains complete old and new creator content");check(doc.undo()&&doc.copy_scene().encode()==baseline,"Exactly one global Undo removes only the newly applied avatar");ready(doc);check(doc.redo()&&doc.copy_scene().encode()==accepted,"Exactly one global Redo restores the complete created avatar");ready(doc);
 doc.begin_humanoid_creator();auto old=doc.frame().state->creator;doc.end_humanoid_creator(false);doc.begin_humanoid_creator();auto current=doc.frame().state->creator;
 check(refuses([&]{doc.configure_humanoid_creator(old->settings,old->session);})&&doc.frame().state->creator==current,"Retired creator session cannot modify a new draft on the same source");check(refuses([&]{doc.end_humanoid_creator(false,old->session);})&&doc.frame().state->creator==current,"Retired cancellation cannot close another creator session");doc.end_humanoid_creator(false);
 doc.begin_humanoid_creator();auto epoch=doc.frame().state->creator;doc.hold_object(doc.frame().state->active_object);check(refuses([&]{doc.configure_humanoid_creator(epoch->settings,epoch->session);}),"Changed edit authority refuses an older creator draft");doc.end_humanoid_creator(false);doc.release_held_object();
 doc.begin_humanoid_creator();driver.frame.focused=false;driver.send();check(!doc.frame().state->creator&&doc.copy_scene().encode()==accepted,"Real focus loss cancels an unapplied creator draft without any geometry/history mutation");driver.frame.focused=true;driver.send();
 Document pending;ready(pending);const auto empty=pending.copy_scene().encode();struct Release{~Release(){release();}}release_guard;
 arm();auto blocked=pending.save_async(root/"before-cancel.dsculpt");await_gate();pending.begin_humanoid_creator();pending.end_humanoid_creator(true);pending.cancel_asset_work();release();blocked.get();ready(pending);check(pending.copy_scene().encode()==empty&&!pending.frame().state->can_undo,"Accepted queued creator Cancel preserves the complete source and global history");
 arm();blocked=pending.save_async(root/"before-replace.dsculpt");await_gate();pending.begin_humanoid_creator();pending.end_humanoid_creator(true);pending.replace(Volume(.07f));const auto replacement=pending.copy_scene().encode();release();blocked.get();ready(pending);check(pending.copy_scene().encode()==replacement&&!pending.frame().state->can_undo,"Stale accepted creator worker cannot overwrite a replacement generation");
 HumanoidCreatorSettings legacy;legacy.controls=humanoid_baseline_defaults("makehuman-hm08");check(refuses([&]{legacy.validate();}),"The creator never offers or falls back to removed MakeHuman geometry");
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(gate.exchange(false)){std::unique_lock lock(gate_mutex);entered=true;changed.notify_all();if(!changed.wait_for(lock,std::chrono::seconds(60),[]{return released;}))return -1;}return __real_fsync(fd);}
int main(int argc,char**argv)try{
 std::cout<<std::unitbuf;const auto root=argc>2?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("creator-workspace-"+std::to_string(getpid()));std::filesystem::create_directories(root);
 if(argc>1&&std::string_view(argv[1])=="--controls")controls(root);
 else if(argc>1&&std::string_view(argv[1])=="--fixture"){authored().save(root/"creator-input.dsculpt");}
 else workspace(root);
 std::cout<<"CREATOR CHECKS "<<checks<<'\n';return 0;
}catch(const std::exception&e){release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
