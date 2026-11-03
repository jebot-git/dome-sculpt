// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-rest-scene.hpp"
#include "polygon-conversion.hpp"
#include <set>
namespace dome::detail {
namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Source rig authoring cancelled");}
void require(bool value,const char *message){if(!value)throw std::invalid_argument(message);}
void admission(const Scene &scene,const SourceRestGraph &graph,uint64_t selected,const RigRestOptions &options){
 options.validate();require(options.linked,"Original-node REST edits require complete linked source authority");
 std::set<uint64_t> expected,actual;for(const auto &part:graph.parts){expected.insert(part.object);const auto &object=scene.object(part.object);require(!object.locked&&object.mesh_active&&object.painted,"A linked source part is locked or has no original mesh");if(part.object==selected)require(object.visible,"Select a visible original source rig");}
 if(!options.linked_objects.empty()){for(auto id:options.linked_objects)require(actual.insert(id).second,"Repeated linked source target identity");require(actual==expected,"Captured linked targets must include exactly this original model/instance, including its rigid parts");}
}
void update_landmarks(DeformationState &deformation){if(deformation.landmarks&&deformation.humanoid){const auto segments=deformation.skeleton->segments();for(auto &[role,point]:deformation.landmarks->points){auto at=deformation.humanoid->joints.find(role);if(at!=deformation.humanoid->joints.end())point=segments.at(at->second)[0];}}}
AffineTransform frame(const Scene &scene,const SourceRestGraph &graph,const SourceRestPose &pose,uint64_t object,const std::function<bool()> &cancelled){
 const auto part=std::find_if(graph.parts.begin(),graph.parts.end(),[&](const auto &part){return part.object==object;});require(part!=graph.parts.end(),"Rigid attachment lost its actual original part identity");
 AffineTransform value;std::optional<AffineTransform> source_node;
 for(const auto &group:scene.group_path(scene.object(object).group)){stop(cancelled);value=value*group_local_matrix(group);const auto &placement=group.source_placement;if(placement&&placement->model==graph.model&&placement->source_instance==graph.instance&&placement->source_node==part->mesh_node)source_node=value;}
 require(source_node.has_value(),"Rigid attachment lost its native source-node placement endpoint");const auto node=part->mesh_node;
 // The rigid implicit bind obeys exactly the same equation as skin binds.
 // P * inverse(R_current) * R_original is invariant under pure REST fitting,
 // including a mesh on the edited joint itself and an authored reparent.
 // Extra native subgroups below the source node remain relative artist offsets
 // in the actual object world, rather than being folded into the pose prefix.
 if(pose.world.at(node)==graph.rest_world.at(node))return *source_node;
 const auto original=AffineTransform(graph.model->asset->nodes.at(node).world);
 return *source_node*original.inverse()*AffineTransform(pose.world.at(node))*AffineTransform(graph.rest_world.at(node)).inverse()*original;
}
void rigid_delta(const Scene &before,Scene &after,const SourceRestGraph &graph,const SourceRestPose &old_pose,const SourceRestGraph &next_graph,const SourceRestPose &new_pose,const std::function<bool()> &cancelled,double maximum_world_error=.00002){
 const bool neutral=old_pose.local==graph.rest_local&&new_pose.local==next_graph.rest_local;
 for(const auto &part:graph.parts){stop(cancelled);if(part.original_skin>=0)continue;const auto old=frame(before,graph,old_pose,part.object,cancelled),now=frame(after,next_graph,new_pose,part.object,cancelled);if(old.matrix()==now.matrix())continue;
  if(neutral&&old_pose.world.at(part.mesh_node)==new_pose.world.at(part.mesh_node)){
   // Exact authority, not a small-motion deadzone: both complete local pose
   // tables equal their own REST, and this rigid MODEL global is unchanged.
   // Compensating float locals may recompose slightly differently. Keep the
   // artist transform byte-exact after checking the declared reconstruction
   // bound. Any authored nonneutral delta, however small, takes the full path.
   for(size_t k=0;k<16;++k)require(std::abs(old.matrix()[k]-now.matrix()[k])<=maximum_world_error,"Neutral rigid attachment exceeds its declared WORLD reconstruction bound");continue;
  }
  const auto world=now*old.inverse()*before.world_matrix(part.object);auto &target=after.object(part.object);target.affine_edit=std::make_shared<const NativeAffineEdit>(after.parent_matrix(part.object).inverse()*world*affine_similarity(target.transform).inverse());
 }
}
void restore(SculptObject &object,const PolygonOrigin &origin){
 object.volume=std::make_shared<Volume>(origin.volume->geometry_snapshot());object.painted=origin.paint?origin.paint->snapshot():nullptr;object.material=origin.material;
 object.deformation=origin.deformation;object.appearance=origin.appearance;object.material_bindings=origin.material_bindings;object.retopology=origin.retopology;object.polygon=origin.polygon;object.shape_source=origin.shape_source;object.rig_source=origin.rig_source;object.mesh_active=origin.mesh_active;
 // Live id/name/group/artist placement are retained. The source capsule is not
 // permission to reparent arbitrary current scene objects.
}
Quat rotation(Vec3 d){auto axis=[](Vec3 a,float degrees){const auto v=degrees*.008726646259971648f;return Quat{std::cos(v),a.x*std::sin(v),a.y*std::sin(v),a.z*std::sin(v)};};return axis({0,0,1},d.z)*axis({0,1,0},d.y)*axis({1,0,0},d.x);}
}
bool author_source_rig_rest(Scene &scene,uint64_t selected,std::span<const RigRestJointEdit> edits,RigRestOptions options,const std::function<bool()> &cancelled,std::span<const SourceRestParentEdit> parents){
 stop(cancelled);const auto graph=capture_source_rest_graph(scene,selected,cancelled);admission(scene,graph,selected,options);const auto old_pose=source_rest_pose(graph,cancelled);const auto plan=plan_source_rest(graph,selected,edits,options,cancelled,parents);if(!plan.changed)return false;
 Scene next=scene;for(const auto &part:graph.parts){stop(cancelled);if(!next.object(part.object).rig_source)next.object(part.object).rig_source=capture_polygon_origin(scene,part.object);}
 for(const auto &skin:plan.skins){stop(cancelled);auto &target=next.object(skin.object);auto deformation=std::make_shared<DeformationState>(*target.deformation);deformation->skeleton=skin.skeleton;deformation->pose=skin.pose;update_landmarks(*deformation);deformation->validate(target.painted->mesh());target.deformation=std::move(deformation);
  if(target.appearance->source_joint_nodes!=skin.source_nodes){auto appearance=std::make_shared<CreatorAppearance>(*target.appearance);appearance->source_joint_nodes=skin.source_nodes;target.appearance=std::move(appearance);}
 }
 // Pivot fitting must retain current rigid world placement exactly, rather than
 // applying a tiny float reconstruction delta to the user's artist transform.
 const auto checked=capture_source_rest_graph(next,selected,cancelled);const auto new_pose=source_rest_pose(checked,cancelled);
 for(const auto &part:graph.parts)if(part.original_skin<0){const auto a=frame(scene,graph,old_pose,part.object,cancelled),b=frame(next,checked,new_pose,part.object,cancelled);for(size_t k=0;k<16;++k)require(std::abs(a.matrix()[k]-b.matrix()[k])<=options.maximum_world_error,"REST fitting cannot preserve the original rigid attachment within the explicit WORLD bound");}
 next.validate();stop(cancelled);scene=std::move(next);return true;
}
void restore_source_rig_rest(Scene &scene,uint64_t selected,RigRestOptions options,const std::function<bool()> &cancelled){
 stop(cancelled);const auto graph=capture_source_rest_graph(scene,selected,cancelled);admission(scene,graph,selected,options);const auto old_pose=source_rest_pose(graph,cancelled);Scene next=scene;
 for(const auto &part:graph.parts){stop(cancelled);const auto &object=scene.object(part.object);require(bool(object.rig_source),"A linked source part has no first rig-fitting source");restore(next.object(part.object),*object.rig_source);}
 const auto restored=capture_source_rest_graph(next,selected,cancelled);const auto restored_pose=source_rest_pose(restored,cancelled);rigid_delta(scene,next,graph,old_pose,restored,restored_pose,cancelled,options.maximum_world_error);next.validate();stop(cancelled);scene=std::move(next);
}
void edit_source_rest_pose(Scene &scene,uint64_t selected,const RigEdit &edit,const std::function<bool()> &cancelled){
 stop(cancelled);require(edit.linked_pose,"Fitted original-node poses require an explicit linked edit");const auto graph=capture_source_rest_graph(scene,selected,cancelled);RigRestOptions options;admission(scene,graph,selected,options);const auto old_pose=source_rest_pose(graph,cancelled);
 const auto &object=scene.object(selected);require(object.deformation&&object.deformation->skeleton&&object.deformation->skin,"Select an authored source skin for joint editing");const auto &before=*object.deformation;std::optional<uint32_t> edited_node;std::optional<RigMatrix> edited_local;
 if(edit.joint){const auto &adjustment=*edit.joint;require(adjustment.joint<before.skeleton->bones.size()&&finite(adjustment.degrees)&&finite(adjustment.translation)&&std::max({std::abs(adjustment.degrees.x),std::abs(adjustment.degrees.y),std::abs(adjustment.degrees.z)})<=180&&std::max({std::abs(adjustment.translation.x),std::abs(adjustment.translation.y),std::abs(adjustment.translation.z)})<=10,"Invalid bounded source joint adjustment");
  edited_node=object.appearance->source_joint_nodes.at(adjustment.joint);auto joint=before.pose.empty()?before.skeleton->bones.at(adjustment.joint).rest:before.pose.at(adjustment.joint);
  // New REST may be sheared. Edit the actual absolute current local, retaining
  // the original editable TRS references instead of resetting to those axes.
  joint.full_local_matrix=(edit.reset_pose?graph.rest_local:old_pose.local).at(*edited_node);joint=adjust_joint_pose(std::move(joint),rotation(adjustment.degrees),adjustment.translation);edited_local=joint.full_local_matrix;
 }
 if(edit.morph)require(before.morphs&&edit.morph->target<before.morphs->targets.size()&&std::isfinite(edit.morph->weight)&&std::abs(edit.morph->weight)<=1,"Invalid source shapekey target or weight");
 Scene next=scene;for(const auto &part:graph.parts){stop(cancelled);if(part.original_skin<0)continue;const bool touches=edit.reset_pose||(edited_node&&std::find(part.joint_nodes.begin(),part.joint_nodes.end(),*edited_node)!=part.joint_nodes.end());if(!touches&&!(part.object==selected&&edit.morph))continue;
  auto deformation=std::make_shared<DeformationState>(*part.deformation);if(edit.reset_pose)deformation->pose.clear();if(edited_node){auto at=std::find(part.joint_nodes.begin(),part.joint_nodes.end(),*edited_node);if(at!=part.joint_nodes.end()){if(deformation->pose.empty())for(const auto &bone:deformation->skeleton->bones)deformation->pose.push_back(bone.rest);deformation->pose[size_t(at-part.joint_nodes.begin())].full_local_matrix=*edited_local;}}
  if(part.object==selected&&edit.morph){if(deformation->morph_weights.empty())deformation->morph_weights.resize(deformation->morphs->targets.size());deformation->morph_weights[edit.morph->target]=edit.morph->weight;}
  deformation->validate(*part.mesh);next.object(part.object).deformation=std::move(deformation);
 }
 const auto checked=capture_source_rest_graph(next,selected,cancelled);const auto new_pose=source_rest_pose(checked,cancelled);rigid_delta(scene,next,graph,old_pose,checked,new_pose,cancelled);next.validate();stop(cancelled);scene=std::move(next);
}
} // namespace dome::detail
