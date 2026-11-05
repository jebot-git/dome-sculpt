// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/skin-authoring.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
AffineTransform matrix(const RigMatrix&a){std::array<double,16>m;std::copy(a.begin(),a.end(),m.begin());return AffineTransform(m);}
bool mixed(){return std::string_view(std::getenv("DOME_HEAT_UI_CASE")?std::getenv("DOME_HEAT_UI_CASE"):"")=="mixed";}
uint64_t reference(const Scene&s){uint64_t id=0;for(const auto&o:s.objects())if(o.id!=s.active().id&&o.deformation&&o.deformation->skeleton){if(id)throw std::runtime_error("GUI fixture reference is ambiguous");id=o.id;}if(!id)throw std::runtime_error("Genuine unique non-output current rig required");return id;}
double pose_error(const SculptObject&o,const DeformationState&d,const AffineTransform&world){
 const auto&m=o.painted->mesh();std::vector<Vec3>points=m.positions;if(d.morphs)for(size_t t=0;t<d.morph_weights.size();++t)for(size_t i=0;i<points.size();++i)points[i]+=d.morphs->targets[t].delta[i]*d.morph_weights[t];
 auto pose=d.pose;if(pose.empty())for(const auto&b:d.skeleton->bones)pose.push_back(b.rest);double maximum=0;
 for(int variant=0;variant<2;++variant){if(variant){auto head=d.humanoid->joints.at("head");pose.at(head).translation.z+=.05f;if(mixed()){pose.at(3).translation.x+=.025f;pose.at(4).translation.y-=.02f;}}
  auto actual_state=d;actual_state.pose=pose;const auto actual=actual_state.preview(m);const auto joint=d.skeleton->world(pose),inverse=d.skeleton->inverse_bind();
  for(size_t i=0;i<points.size();++i){std::array<double,3>wanted{};for(const auto&influence:d.skin->vertices[i]){auto q=(matrix(joint.at(influence.joint))*matrix(inverse.at(influence.joint))).point({points[i].x,points[i].y,points[i].z});for(size_t k=0;k<3;++k)wanted[k]+=q[k]*influence.weight;}const auto a=world.point({actual.positions[i].x,actual.positions[i].y,actual.positions[i].z}),b=world.point(wanted);maximum=std::max(maximum,std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]));}
 }return maximum;
}
}
int main(int argc,char**argv)try{
 const auto*full_path=std::getenv("DOME_HEAT_NATIVE_EXPECTED");if(!full_path)throw std::invalid_argument("Read-only independently qualified field required");const auto full=Scene::load(full_path);
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){
  const auto*input=std::getenv("DOME_HEAT_NATIVE_INPUT");if(!input)throw std::invalid_argument("Authentic unweighted native input required");auto scene=Scene::load(input);uint64_t selected=0;
  if(mixed())selected=2;else for(const auto&o:scene.objects()){const auto&f=full.object(o.id);if(!o.painted||!f.deformation||!f.deformation->skin||!f.deformation->humanoid||!o.deformation||o.deformation->skeleton||!o.deformation->morphs||o.deformation->morphs->targets.empty())continue;const auto head=f.deformation->humanoid->joints.at("head");bool yes=true;for(const auto&row:f.deformation->skin->vertices)yes&=row.size()==1&&row[0].joint==head&&row[0].weight==1;if(yes){selected=o.id;break;}}
  if(!selected)throw std::runtime_error("Actual unskinned current key target required");scene.select(selected);auto&o=scene.active();auto d=*o.deformation;if(d.skeleton||d.skin||!d.morphs||d.morphs->targets.empty())throw std::runtime_error("Genuine unweighted authored-key fixture required");d.morph_weights.assign(d.morphs->targets.size(),0);d.morph_weights[0]=.25f;scene.set_deformation(selected,d);
  const auto ref=reference(scene);auto posed=*scene.object(ref).deformation;auto head=posed.humanoid->joints.at("head");if(posed.pose.empty())for(const auto&b:posed.skeleton->bones)posed.pose.push_back(b.rest);posed.pose[head].translation.z+=.01f;scene.set_deformation(ref,posed);
  o.painted=o.painted->snapshot();o.painted->add_layer("Current authoring paint retained through bind");Brush brush;brush.from=brush.to=o.painted->mesh().positions.at(o.painted->mesh().indices.at(0));brush.normal={0,0,1};brush.radius=mixed()?.15f:.05f;PaintSettings ink;ink.front_only=false;ink.color={.12f,.57f,.3f,.47f};o.painted->begin_stroke();o.painted->stamp(brush,ink);if(!o.painted->end_stroke())throw std::runtime_error("GUI fixture requires actual nonempty authored colour paint");o.painted->layer_properties("Current authoring paint retained through bind",true,.63f);ink.channel=PaintChannel::Roughness;ink.roughness=.21f;o.painted->begin_stroke();o.painted->stamp(brush,ink);if(!o.painted->end_stroke())throw std::runtime_error("GUI fixture requires actual authored PBR paint");scene.save(argv[2]);return 0;
 }
 if(argc==4&&std::string_view(argv[1])=="--inspect"){
  const auto scene=Scene::load(argv[2]),before=Scene::load(argv[3]);const auto&o=scene.active();const auto&m=o.painted->mesh();const bool skin=o.deformation&&o.deformation->skin;bool matches=false,retained=false;double foreign=0,error=0;size_t blended=0;
  if(skin){const auto&d=*o.deformation;auto expected=*full.object(o.id).deformation;expected.morph_weights=before.object(o.id).deformation->morph_weights;expected.pose=before.object(reference(before)).deformation->pose;matches=expected.encode(m)==d.encode(m);for(size_t i=0;i<d.skin->vertices.size();++i){blended+=d.skin->vertices[i].size()>1;for(auto influence:d.skin->vertices[i])if(std::none_of(expected.skin->vertices[i].begin(),expected.skin->vertices[i].end(),[&](auto x){return x.joint==influence.joint;}))foreign=std::max(foreign,double(influence.weight));}auto restored=scene;restored.object(o.id).deformation=before.object(o.id).deformation;restored.object(o.id).rig_source=before.object(o.id).rig_source;retained=restored.encode()==before.encode();error=pose_error(o,d,scene.world_matrix(o.id));}
  const auto&key=*o.deformation;const bool active=key.morphs&&std::any_of(key.morph_weights.begin(),key.morph_weights.end(),[](auto w){return w!=0;});const auto&ref=*scene.object(reference(scene)).deformation;bool active_pose=false;for(size_t i=0;i<ref.pose.size();++i)active_pose|=(length(ref.pose[i].translation-ref.skeleton->bones[i].rest.translation)>0)||ref.pose[i].rotation.w!=ref.skeleton->bones[i].rest.rotation.w||ref.pose[i].rotation.x!=ref.skeleton->bones[i].rest.rotation.x||ref.pose[i].rotation.y!=ref.skeleton->bones[i].rest.rotation.y||ref.pose[i].rotation.z!=ref.skeleton->bones[i].rest.rotation.z;
  std::cout<<nlohmann::json{{"case",mixed()?"mixed":"head"},{"skinned",skin},{"active_morph",active},{"active_pose",active_pose},{"authored_paint",o.painted->has_paint_data()},{"foreign_weight",foreign},{"max_pose_m",error},{"oracle_poses",2},{"blended_rows",blended},{"field_matches",matches},{"retained_exact",retained},{"scene",scene.encode()}}.dump()<<std::endl;return 0;
 }return 2;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
