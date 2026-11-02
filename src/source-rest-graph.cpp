// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-rest-graph.hpp"
#include "source-rigid-pose.hpp"
#include "import-skin.hpp"
#include "dome/polygon-scene.hpp"
#include <set>
namespace dome::detail {
namespace {
void require(bool good,const char *message){if(!good)throw std::invalid_argument(message);}
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Source rest graph capture cancelled");}
template<class T> bool owner(const std::shared_ptr<const T> &a,const std::shared_ptr<const T> &b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
bool member(const std::shared_ptr<const SourceNodePlacement> &a,const std::shared_ptr<const SourceNodePlacement> &b){return a&&b&&owner(a->model,b->model)&&a->source_instance==b->source_instance;}
const PolygonOrigin *first_source(const SculptObject &object){return object.rig_source.get();}
}
std::shared_ptr<const SourceNodePlacement> source_rest_identity(const Scene &scene,const SculptObject &object){
 if(!object.mesh_active||!object.painted||!object.appearance||!object.appearance->asset||object.appearance->source_node<0)return {};
 const auto &appearance=*object.appearance;auto groups=scene.group_path(object.group);
 for(auto at=groups.rbegin();at!=groups.rend();++at){const auto &placement=at->source_placement;if(placement&&placement->model&&owner(placement->model->asset,appearance.asset)&&owner(placement->model->original_bytes,appearance.original_bytes)&&placement->source_node==uint32_t(appearance.source_node))return placement;}
 return {};
}
std::vector<uint64_t> source_rest_targets(const Scene &scene,uint64_t selected){
 const auto identity=source_rest_identity(scene,scene.object(selected));require(bool(identity),"Select an authenticated original source instance for rest editing");std::vector<uint64_t> targets;
 for(const auto &object:scene.objects())if(member(identity,source_rest_identity(scene,object)))targets.push_back(object.id);
 require(!targets.empty()&&targets.size()<=256,"Original source instance exceeds native object bounds");return targets;
}
SourcePoseResolution SourceRestGraph::pose_agreement(uint32_t node)const{
 if(!model||node>=model->asset->nodes.size())throw std::out_of_range("Source pose node identity");SourcePoseResolution result;auto at=pose_witnesses.find(node);if(at==pose_witnesses.end())return result;
 const auto &witnesses=at->second;require(!witnesses.empty(),"Empty source pose witness table");const auto &first=witnesses.front().local;
 bool equal=true;for(const auto &w:witnesses){equal&=w.local==first;if(w.local!=rest_local.at(node))result.edited_objects.push_back(w.object);}
 if(equal){result.agreement=SourcePoseAgreement::Shared;result.shared=first;return result;}
 std::optional<RigMatrix> edited;bool conflict=false;for(const auto &w:witnesses)if(w.local!=rest_local.at(node)){if(edited&&*edited!=w.local)conflict=true;else edited=w.local;}
 result.agreement=conflict?SourcePoseAgreement::ConflictingViews:SourcePoseAgreement::OneEditedView;return result;
}
SourceRestGraph capture_source_rest_graph(const Scene &scene,uint64_t selected,const std::function<bool()> &cancelled){
 stop(cancelled);const auto identity=source_rest_identity(scene,scene.object(selected));require(bool(identity),"Rest editing has no authenticated original model/instance");identity->validate();SourceRestGraph graph;graph.model=identity->model;graph.instance=identity->source_instance;
 const auto &asset=*graph.model->asset;require(!asset.nodes.empty()&&asset.nodes.size()<=100000,"Original node graph exceeds inherited import metadata bounds");
 graph.parents.reserve(asset.nodes.size());graph.rest_local.reserve(asset.nodes.size());for(const auto &node:asset.nodes){stop(cancelled);graph.parents.push_back(node.parent);graph.rest_local.push_back(node.local);}
 std::vector<uint64_t> rest_owners(asset.nodes.size());size_t skins=0,work=asset.nodes.size();
 for(const auto &object:scene.objects()){
  stop(cancelled);const auto part_identity=source_rest_identity(scene,object);if(!member(identity,part_identity))continue;
  if(graph.parts.size()==256)throw std::length_error("Source rest part count exceeded");const auto &appearance=*object.appearance;
  require(owner(appearance.asset,graph.model->asset)&&owner(appearance.original_bytes,graph.model->original_bytes),"Part raw model owner differs from its source placement");
  require(appearance.source_node>=0&&uint32_t(appearance.source_node)==part_identity->source_node,"Source mesh node identity changed");const auto node=uint32_t(appearance.source_node);const auto &raw=asset.nodes.at(node);
  require(appearance.source_mesh>=0&&raw.mesh==appearance.source_mesh,"Source node/mesh identity changed");
  SourceRestPart part;part.object=object.id;part.mesh_node=node;part.original_skin=raw.skin;part.appearance=object.appearance;part.deformation=object.deformation;part.mesh=object.painted->mesh_owner();part.world=scene.world_matrix(object.id);
  const bool skinned=object.deformation&&object.deformation->skin;
  require(skinned==(raw.skin>=0),"Native skin ownership differs from the immutable source node");
  if(!skinned){require(appearance.source_joint_nodes.empty()&&(!object.deformation||!object.deformation->skeleton),"Rigid original part cannot invent a native skeleton");graph.parts.push_back(std::move(part));continue;}
  ++skins;const auto &d=*object.deformation;require(bool(d.skeleton),"Source skin has no skeleton");d.validate(*part.mesh);const auto &s=*d.skeleton;
  require(s.space_matrix&&*s.space_matrix==imported_inverse(raw.world),"Source rest requires the actual original mesh inverse space");
  require(s.inverse_bind_override&&s.inverse_bind_override->size()==s.bones.size(),"Source skin lacks explicit bone-aligned bind authority");
  require(appearance.source_joint_nodes.size()==s.bones.size(),"Source joint table differs from the current native skeleton");
  part.joint_nodes=appearance.source_joint_nodes;part.rest_local=s.local_matrices();part.pose_local=s.local_matrices(d.pose);part.rest_world=s.world();part.pose_world=s.world(d.pose);
  std::set<uint32_t> unique;for(auto source:part.joint_nodes)require(source<asset.nodes.size()&&unique.insert(source).second,"Invalid or repeated original joint node identity");
  const auto &original_skin=asset.skins.at(size_t(raw.skin));for(auto joint:original_skin.joints)require(unique.contains(joint),"Current native table discarded an authored skin joint");
  const auto *origin=first_source(object);
  if(origin){require(origin->appearance&&owner(origin->appearance->asset,graph.model->asset)&&owner(origin->appearance->original_bytes,graph.model->original_bytes)&&origin->appearance->source_node==appearance.source_node&&origin->deformation&&origin->deformation->skeleton,"First rig source cannot authenticate the current original-node view");}
  for(size_t slot=0;slot<s.bones.size();++slot){
   stop(cancelled);if(++work>50000000)throw std::length_error("Source rest metadata work budget exceeded");const auto source=part.joint_nodes[slot];const auto parent=s.bones[slot].parent<0?-1:int(part.joint_nodes.at(size_t(s.bones[slot].parent)));
   require(s.bones[slot].rest_matrix.has_value(),"Original joint lost its exact REST local authority");
   if(!origin)require(parent==asset.nodes[source].parent&&part.rest_local[slot]==asset.nodes[source].local,"Unwitnessed original-node REST/parent edits require a retained first rig source");
   if(rest_owners[source])require(graph.parents[source]==parent&&graph.rest_local[source]==part.rest_local[slot],"Linked source skins disagree on original-node REST authority");
   else {rest_owners[source]=object.id;graph.parents[source]=parent;graph.rest_local[source]=part.rest_local[slot];}
   graph.pose_witnesses[source].push_back({object.id,part.pose_local[slot]});
  }
  graph.parts.push_back(std::move(part));
 }
 require(skins>0,"Original rest editing requires at least one authenticated source skin");graph.rest_world.resize(asset.nodes.size());std::vector<uint8_t> state(asset.nodes.size()),changed_path(asset.nodes.size());
 auto visit=[&](auto &&self,uint32_t node,size_t depth)->void{
  stop(cancelled);require(depth<=256,"Source rest ancestry exceeds inherited256 levels");if(state[node]==2)return;require(state[node]==0,"Cycle in current original-node REST authority");state[node]=1;const auto parent=graph.parents[node];require(parent>=-1&&parent<int(asset.nodes.size()),"Source rest parent identity is out of range");
  if(parent>=0)self(self,uint32_t(parent),depth+1);
  changed_path[node]=graph.parents[node]!=asset.nodes[node].parent||graph.rest_local[node]!=asset.nodes[node].local||(parent>=0&&changed_path[size_t(parent)]);
  if(!changed_path[node])graph.rest_world[node]=asset.nodes[node].world;
  else if(!rest_owners[node]){
   // Nodes absent from all skin tables retain their actual original REST world
   // placement. The derived local compensates a fitted/reparented ancestor;
   // rigid meshes/cameras/metadata nodes never silently move with a new pivot.
   graph.rest_world[node]=asset.nodes[node].world;
   graph.rest_local[node]=parent<0?graph.rest_world[node]:(AffineTransform(graph.rest_world[size_t(parent)]).inverse()*AffineTransform(graph.rest_world[node])).float_matrix();
  }else graph.rest_world[node]=parent<0?graph.rest_local[node]:(AffineTransform(graph.rest_world[size_t(parent)])*AffineTransform(graph.rest_local[node])).float_matrix();
  state[node]=2;
 };
 for(size_t node=0;node<asset.nodes.size();++node)visit(visit,uint32_t(node),1);stop(cancelled);return graph;
}
SourceRestPose source_rest_pose(const SourceRestGraph &graph,const std::function<bool()> &cancelled){
 stop(cancelled);require(bool(graph.model),"Missing source rest model");SourceRestPose result;result.local=graph.rest_local;
 for(const auto &[node,_]:graph.pose_witnesses){stop(cancelled);const auto resolved=graph.pose_agreement(node);require(resolved.agreement==SourcePoseAgreement::Shared&&resolved.shared.has_value(),"Source skins have distinct current pose views; resolve that authority explicitly before a shared edit");result.local.at(node)=*resolved.shared;}
 result.world.resize(result.local.size());std::vector<uint8_t> state(result.local.size());
 auto visit=[&](auto &&self,size_t node,size_t depth)->void{stop(cancelled);require(depth<=256,"Source pose ancestry exceeds256 levels");if(state[node]==2)return;require(state[node]==0,"Cyclic source current pose graph");state[node]=1;const auto parent=graph.parents.at(node);require(parent>=-1&&parent<int(result.local.size()),"Source current pose parent is out of range");if(parent>=0)self(self,size_t(parent),depth+1);
  if(result.local[node]==graph.rest_local[node]&&(parent<0||result.world[size_t(parent)]==graph.rest_world[size_t(parent)]))result.world[node]=graph.rest_world[node];
  else result.world[node]=parent<0?result.local[node]:(AffineTransform(result.world[size_t(parent)])*AffineTransform(result.local[node])).float_matrix();state[node]=2;};
 for(size_t node=0;node<result.local.size();++node)visit(visit,node,1);return result;
}
std::vector<SceneGroup> source_rest_groups(const SourceRestGraph &graph,const SourceRestPose &pose,std::vector<SceneGroup> groups,const std::function<bool()> &cancelled){
 require(graph.model&&pose.local.size()==graph.model->asset->nodes.size(),"Missing complete source attachment frame");
 for(auto &group:groups){stop(cancelled);const auto &placement=group.source_placement;if(!placement||!owner(placement->model,graph.model)||placement->source_instance!=graph.instance)continue;
  const auto node=placement->source_node;const auto &raw=graph.model->asset->nodes.at(node).local;if(pose.local.at(node)==raw)continue;
  const auto native=affine_similarity(group.transform);const auto edit=group.affine_edit?group.affine_edit->matrix():AffineTransform{};
  group.affine_edit=std::make_shared<const NativeAffineEdit>(edit*native*AffineTransform(pose.local[node])*AffineTransform(raw).inverse()*native.inverse());
 }return groups;
}
} // namespace dome::detail
