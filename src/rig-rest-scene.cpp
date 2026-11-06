#include "dome/texture-sets.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-rest.hpp"
#include "polygon-conversion.hpp"
#include "rig-rest-transport.hpp"
#include "source-rigid-pose.hpp"
#include "source-rest-scene.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Rest-rig editing cancelled");}
uint64_t boundary(const Scene &scene,const SculptObject &object){const auto path=scene.group_path(object.group);for(auto at=path.rbegin();at!=path.rend();++at)if(at->humanoid)return at->id;return object.group;}
const DeformationState &editable(const SculptObject &o,bool selected){
 if((selected&&!o.visible)||o.locked||!o.mesh_active||!o.painted||!o.deformation||!o.deformation->skeleton)
  throw std::invalid_argument("Rest fitting requires unlocked original meshes with a skeleton");
 o.deformation->validate(o.painted->mesh());return *o.deformation;
}
void restore(SculptObject &object,const PolygonOrigin &origin){
 object.volume=std::make_shared<Volume>(origin.volume->geometry_snapshot());
 object.painted=origin.paint?origin.paint->snapshot():nullptr;object.material=origin.material;
 object.deformation=origin.deformation;object.appearance=origin.appearance;object.material_bindings=origin.material_bindings;
 object.retopology=origin.retopology;object.polygon=origin.polygon;object.shape_source=origin.shape_source;object.rig_source=origin.rig_source;object.bake_recipe=origin.bake_recipe;object.texture_sets=origin.texture_sets;object.mesh_active=origin.mesh_active;
 // Identity/name/current placement are intentionally retained. Source placement
 // is immutable provenance; restoring data must not rewire the live hierarchy.
}
}

std::vector<uint64_t> rig_rest_linked_objects(const Scene &scene,uint64_t id){
 const auto &source=scene.object(id);if(!source.deformation||!source.deformation->skeleton)throw std::invalid_argument("Select an existing rig for linked fitting");if(detail::source_rest_identity(scene,source))return detail::source_rest_targets(scene,id);const auto &deformation=*source.deformation;const auto group=boundary(scene,source);std::vector<uint64_t> result{id};
 const auto original=[](const SculptObject&o){return o.rig_source?o.rig_source->deformation:o.deformation;};
 if(!group)return result;
 for(const auto &o:scene.objects()){
  if(o.id==id||boundary(scene,o)!=group||!o.deformation||!o.deformation->skeleton)continue;
  const bool native_owner=original(o)==original(source);
  const bool source_owner=source.appearance&&o.appearance&&source.appearance->asset&&source.appearance->asset==o.appearance->asset&&source.appearance->original_bytes==o.appearance->original_bytes&&source.appearance->source_joint_nodes==o.appearance->source_joint_nodes;
  if((native_owner||source_owner)&&o.deformation->skeleton->encode()==deformation.skeleton->encode())result.push_back(o.id);
 }
 if(result.size()>256)throw std::length_error("Linked rig selection exceeds 256 objects");return result;
}
bool author_rig_rest(Scene &scene,uint64_t id,std::span<const RigRestJointEdit> edits,RigRestOptions options,const std::function<bool()> &cancelled){
 stop(cancelled);options.validate();const auto &selected=scene.object(id);const auto &original=editable(selected,true);
 if(edits.empty())return false;
 // Original-node rigs have different joint slot tables and rigid descendants.
 // They require the normalized source graph adapter, never name/slot guessing.
 if(detail::source_rest_identity(scene,selected))return detail::author_source_rig_rest(scene,id,edits,options,cancelled);
 auto proposal=transport_rig_rest(*original.skeleton,original.pose,scene.world_matrix(id),edits,options,cancelled);
 if(!proposal.changed)return false;
 const auto key=original.skeleton->encode();const auto group=boundary(scene,selected);
 const auto linked=options.linked_objects.empty()?rig_rest_linked_objects(scene,id):options.linked_objects;
 {std::set<uint64_t> seen;for(auto target:linked){if(!seen.insert(target).second)throw std::invalid_argument("Repeated linked rig target identity");const auto &o=scene.object(target);if(target!=id&&(!group||boundary(scene,o)!=group||!o.deformation||!o.deformation->skeleton||o.deformation->skeleton->encode()!=key))throw std::invalid_argument("Explicit linked target is outside the captured rig group/REST identity");}}

 Scene next=scene;bool changed=false;
 for(const auto &object:scene.objects()){
  stop(cancelled);if(object.id!=id){
   if(!options.linked||std::find(linked.begin(),linked.end(),object.id)==linked.end())continue;
   if(detail::source_rig_instance(scene,object))throw std::invalid_argument("Grouped native and original-node rig identities cannot be mixed implicitly");
   if(selected.appearance&&object.appearance&&selected.appearance->source_joint_nodes!=object.appearance->source_joint_nodes)throw std::invalid_argument("Grouped rig parts have different original joint identities");
  }
  const auto &before=editable(object,object.id==id);
  auto fitted=object.id==id?proposal:detail::finish_rig_rest_transport(*before.skeleton,before.pose,proposal.skeleton,scene.world_matrix(object.id),options,cancelled);
  if(!fitted.changed)continue;
  auto deformation=std::make_shared<DeformationState>(before);deformation->skeleton=std::move(fitted.skeleton);deformation->pose=std::move(fitted.pose);
  if(deformation->landmarks&&deformation->humanoid){const auto heads=deformation->skeleton->segments();for(auto &[role,point]:deformation->landmarks->points){auto joint=deformation->humanoid->joints.find(role);if(joint!=deformation->humanoid->joints.end())point=heads.at(joint->second)[0];}}
  deformation->validate(object.painted->mesh());auto &target=next.object(object.id);
  if(!target.rig_source)target.rig_source=capture_polygon_origin(scene,object.id);
  target.deformation=std::move(deformation);changed=true;
 }
 if(!changed)return false;next.validate();stop(cancelled);scene=std::move(next);return true;
}
void restore_rig_source(Scene &scene,uint64_t id,RigRestOptions options,const std::function<bool()> &cancelled){
 options.validate();if(detail::source_rest_identity(scene,scene.object(id))){detail::restore_source_rig_rest(scene,id,options,cancelled);return;}
 // An explicit original-source Restore may legitimately remove a rig that did
 // not exist before binding. Never infer any linked membership from absence.
 {
  const auto &selected=scene.object(id);
  if(selected.rig_source&&(!selected.rig_source->deformation||!selected.rig_source->deformation->skeleton)){
   stop(cancelled);editable(selected,true);
   if(options.linked)throw std::invalid_argument("Restore an originally unskinned source on the active part; turn off linked fitting");
   const auto &origin=*selected.rig_source;
   if(!origin.volume||!origin.mesh_active||!origin.paint)throw std::invalid_argument("Original unskinned source capsule is incomplete");
   Scene next=scene;restore(next.object(id),origin);next.validate();stop(cancelled);scene=std::move(next);return;
  }
 }
 const auto linked_objects=options.linked_objects.empty()?rig_rest_linked_objects(scene,id):options.linked_objects;
 stop(cancelled);const auto &selected=scene.object(id);editable(selected,true);
 if(!selected.rig_source||!selected.rig_source->deformation||!selected.rig_source->deformation->skeleton)throw std::invalid_argument("This mesh has no original rig-fitting source");
 const auto group=boundary(scene,selected);const auto key=selected.rig_source->deformation->skeleton->encode();Scene next=scene;
 for(const auto &object:scene.objects()){
  stop(cancelled);if(object.id!=id&&(!options.linked||std::find(linked_objects.begin(),linked_objects.end(),object.id)==linked_objects.end()||!group||boundary(scene,object)!=group||!object.rig_source||!object.rig_source->deformation||!object.rig_source->deformation->skeleton||object.rig_source->deformation->skeleton->encode()!=key))continue;
  editable(object,object.id==id);restore(next.object(object.id),*object.rig_source);
 }
 next.validate();stop(cancelled);scene=std::move(next);
}
void restore_rig_source(Scene &scene,uint64_t id,bool linked,const std::function<bool()> &cancelled){RigRestOptions options;options.linked=linked;restore_rig_source(scene,id,std::move(options),cancelled);}
} // namespace dome
