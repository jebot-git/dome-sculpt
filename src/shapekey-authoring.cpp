// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/shapekey-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/humanoid-creator.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "polygon-conversion.hpp"
#include "polygon-paint.hpp"
#include "text-utf8.hpp"
#include "dome/tangents.hpp"
#include <map>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Shapekey authoring cancelled");}
bool equal(std::span<const Vec3> a,std::span<const Vec3> b){
 if(a.size()!=b.size())return false;
 for(size_t i=0;i<a.size();++i)if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].z!=b[i].z)return false;
 return true;
}
std::string key_name(const MorphSet &keys,const std::string &requested){
 auto used=[&](const auto &name){return std::any_of(keys.targets.begin(),keys.targets.end(),[&](const auto&t){return t.name==name;});};
 if(!requested.empty()){
  if(requested.size()>256||requested.find('\0')!=std::string::npos||!text::valid_utf8(requested)||used(requested))throw std::invalid_argument("Choose a unique UTF-8 shapekey name (1..256 bytes)");
  return requested;
 }
 for(unsigned i=1;i<=129;++i){auto n="Shape "+std::to_string(i);if(!used(n))return n;}
 throw std::length_error("Shapekey name budget exceeded");
}
const NormalFrameTarget &frame_target(const NormalFrames &frame,uint64_t id){
 auto i=std::find_if(frame.targets.begin(),frame.targets.end(),[&](const auto&t){return t.id==id;});
 if(i==frame.targets.end())throw std::invalid_argument("Shapekey normal target identity is missing");
 return *i;
}
NormalFrameTarget authored_attributes(const Mesh &rest,const NormalFrames &artist,
    const NormalFrameTarget *old_key,std::span<const Vec3> old_positions,
    std::span<const Vec3> new_positions,uint64_t id,const std::function<bool()> &cancelled){
 // Rotate the previous artist endpoint by the geometric frame's change. This
 // preserves smoothing/normal offsets; it does not label generated N/T as the
 // original authored channels. REST and every other key remain immutable.
 MorphSet before(rest);before.set_target(rest,"geometric-reference",old_positions);
 auto mesh=before.evaluate(rest,std::array<float,1>{1});
 NormalFrames geometric;geometric.origin_vertex_count=uint32_t(rest.positions.size());geometric.origin_vertices=rest.indices;
 geometric.normals.reserve(rest.indices.size());for(auto v:rest.indices)geometric.normals.push_back(mesh.normals[v]);
 if(!artist.tangents.empty()){
  geometric.tangents=tangent_corners(mesh,cancelled);geometric.generated_tangents=true;geometric.generated_uv=0;
 }
 auto after=detail::normal_bake_author_native_target(rest,geometric,new_positions,id,{128*1024*1024,128*1024*1024,cancelled}).attributes;
 NormalFrameTarget result;result.id=id;result.normals.reserve(rest.indices.size());if(!artist.tangents.empty())result.tangents.reserve(rest.indices.size());
 for(size_t c=0;c<rest.indices.size();++c){if((c&1023)==0)stop(cancelled);
  auto n0=geometric.normals[c],n1=n0+after.normals[c];Vec3 t0,t1;float handedness=1;
  if(!artist.tangents.empty()){
   const auto &t=geometric.tangents[c];t0={t[0],t[1],t[2]};t1=t0+after.tangents[c];handedness=t[3];
  }else{
   auto old_edge=old_positions[rest.indices[(c/3)*3+1]]-old_positions[rest.indices[(c/3)*3]];
   auto new_edge=new_positions[rest.indices[(c/3)*3+1]]-new_positions[rest.indices[(c/3)*3]];
   t0=normalized(old_edge-n0*dot(old_edge,n0));t1=normalized(new_edge-n1*dot(new_edge,n1));
  }
  auto b0=cross(n0,t0)*handedness,b1=cross(n1,t1)*handedness;
  auto transport=[&](Vec3 v){return t1*dot(v,t0)+b1*dot(v,b0)+n1*dot(v,n0);};
  auto previous=artist.normals[c];if(old_key&&!old_key->normals.empty())previous+=old_key->normals[c];
  result.normals.push_back(transport(previous)-artist.normals[c]);
  if(!artist.tangents.empty()){
   const auto &base=artist.tangents[c];Vec3 previous_t{base[0],base[1],base[2]};
   if(old_key&&!old_key->tangents.empty())previous_t+=old_key->tangents[c];
   result.tangents.push_back(transport(previous_t)-Vec3{base[0],base[1],base[2]});
  }
 }
 return result;
}

}
ShapeKeyResult author_shapekey(Scene &scene,uint64_t object,const ShapeKeyEdit &edit,
 const std::function<bool()> &cancelled){
 stop(cancelled);const auto &source=scene.object(object);
 if(!source.visible||source.locked||!source.mesh_active||!source.painted)
  throw std::invalid_argument("Select a visible, unlocked painting mesh for shapekey authoring");
 if(edit.action<ShapeKeyAction::Create||edit.action>ShapeKeyAction::Restore)throw std::invalid_argument("Unknown shapekey action");
 if(edit.action==ShapeKeyAction::Restore){
  if(!source.shape_source)throw std::invalid_argument("This mesh has no pre-shapekey authoring source");
  const auto &origin=*source.shape_source;Scene next=scene;auto &o=next.object(object);
  o.volume=std::make_shared<Volume>(origin.volume->geometry_snapshot());o.painted=origin.paint?origin.paint->snapshot():nullptr;
  o.mesh_active=origin.mesh_active;o.material=origin.material;o.deformation=origin.deformation;o.appearance=origin.appearance;
  o.material_bindings=origin.material_bindings;o.retopology=origin.retopology;o.polygon=origin.polygon;o.shape_source=origin.shape_source;
  next.validate();stop(cancelled);scene=std::move(next);return {0,true};
 }
 const auto &rest=source.painted->mesh();rest.validate();
 auto deform=std::make_shared<DeformationState>(source.deformation?*source.deformation:DeformationState(rest));
 if(!deform->morphs)deform->morphs=MorphSet(rest);
 const size_t count=deform->morphs->targets.size();
 bool existing=edit.action==ShapeKeyAction::Duplicate||edit.action==ShapeKeyAction::Mirror||edit.action==ShapeKeyAction::Replace||edit.action==ShapeKeyAction::Rename;
 if(existing&&edit.target>=count)throw std::invalid_argument("Choose an existing shapekey");
 if(edit.action==ShapeKeyAction::Replace && edit.positions.size()!=rest.positions.size())throw std::invalid_argument("Shapekey edits must preserve the complete REST vertex domain");
 if(edit.action==ShapeKeyAction::Replace){std::vector<Vec3> old;old.reserve(rest.positions.size());for(size_t v=0;v<rest.positions.size();++v)old.push_back(rest.positions[v]+deform->morphs->targets[edit.target].delta[v]);if(equal(old,edit.positions))return {edit.target,false};}
 if(edit.action==ShapeKeyAction::Rename && edit.name==deform->morphs->targets[edit.target].name)return {edit.target,false};
 const bool replace=edit.action==ShapeKeyAction::Replace||edit.action==ShapeKeyAction::Rename;
 const size_t slot=replace?edit.target:count;
 if(!replace && count>=128)throw std::length_error("Shapekey target budget exceeded");
 std::string name=edit.action==ShapeKeyAction::Replace?deform->morphs->targets[slot].name:key_name(*deform->morphs,edit.name);
 Scene next=scene;auto &o=next.object(object);
 if(!o.shape_source)o.shape_source=capture_polygon_origin(scene,object);
 if(edit.action==ShapeKeyAction::Rename){
  auto old=deform->morphs->targets[slot].name;
  if(deform->expressions)deform->expressions->rename_shape(*deform->morphs,old,name);
  else deform->morphs->targets[slot].name=name;
  deform->validate(rest);o.deformation=deform;next.validate();stop(cancelled);scene=std::move(next);return {slot,true};
 }
 std::vector<Vec3> positions=rest.positions;
 if(edit.action==ShapeKeyAction::Duplicate||edit.action==ShapeKeyAction::Mirror){
  std::vector<uint32_t> mirror;
  if(edit.action==ShapeKeyAction::Mirror){if(!std::isfinite(edit.mirror_tolerance)||edit.mirror_tolerance<=0||edit.mirror_tolerance>1)throw std::invalid_argument("Invalid local mirror tolerance");mirror=mirror_vertices(rest,edit.mirror_tolerance,cancelled);}
  for(size_t v=0;v<positions.size();++v){if((v&1023)==0)stop(cancelled);auto d=deform->morphs->targets[edit.target].delta[mirror.empty()?v:mirror[v]];if(!mirror.empty())d.x=-d.x;positions[v]+=d;}
 }else if(edit.action==ShapeKeyAction::CaptureMix){
  for(size_t t=0;t<count;++t){float weight=deform->morph_weights.empty()?0:deform->morph_weights[t];for(size_t v=0;v<positions.size();++v){if((v&1023)==0)stop(cancelled);positions[v]+=deform->morphs->targets[t].delta[v]*weight;}}
 }else if(edit.action==ShapeKeyAction::Replace)positions=edit.positions;
 stop(cancelled);
 auto binding=source.material_bindings?std::make_shared<SourceMaterialBindings>(*source.material_bindings):std::make_shared<SourceMaterialBindings>();
 if(!binding->normal_frames){
  if(source.appearance)throw std::invalid_argument("Prepare the retained original normal attributes before authoring source shapekeys");
  if(source.painted->baked()&&!source.painted->baked()->maps.normal.rgba.empty())throw std::invalid_argument("Prepare retained normal frames before authoring keys on this legacy baked normal surface");
  if(source.painted->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored){
   PaintSurfaceOptions options;options.cancelled=cancelled;
   try{o.painted=polygon_paint_geometry(*source.painted,rest,options);}
   catch(const std::invalid_argument&e){throw std::invalid_argument(std::string("Unwrap polygon UVs before shapekey frame authoring: ")+e.what());}
  }
  auto origin=std::make_shared<SourceUvOrigin>();origin->paint=o.painted->snapshot();origin->deformation=source.deformation;
  if(source.material_bindings && source.material_bindings->uv_origin)binding->uv_origin=source.material_bindings->uv_origin;
  else binding->uv_origin=std::move(origin);
  binding->has_mtoon=bool(source.material_bindings)&&source.material_bindings->has_mtoon;
  binding->normal_frames=detail::normal_bake_frame_candidate(rest,source.deformation.get(),nullptr,{128*1024*1024,128*1024*1024,cancelled}).frames;
  binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
  if(!source.material_bindings){binding->native_reference.color=source.material.color;binding->native_reference.emission=source.material.emissive;binding->native_reference.alpha=int(source.material.alpha);binding->native_reference.alpha_cutoff=source.material.alpha_cutoff;binding->native_reference.double_sided=source.material.double_sided;}
 }
 detail::normal_frame_current_authority(*binding,rest,source.deformation.get(),cancelled);
 auto frames=std::make_shared<NormalFrameSet>(*binding->normal_frames);
 uint64_t id=0;
 if(replace)id=frames->current_targets.at(slot);
 else{if(frames->targets.size()>=128||frames->next_target==UINT64_MAX)throw std::length_error("Retained shapekey identity budget exceeded");id=frames->next_target++;}
 std::vector<Vec3> delta(positions.size());for(size_t v=0;v<delta.size();++v){if((v&1023)==0)stop(cancelled);delta[v]=positions[v]-rest.positions[v];}
 std::vector<Vec3> old_positions=binding->uv_origin->paint->mesh().positions;
 if(replace){auto old_delta=detail::normal_bake_target_origin_positions(rest,binding->uv_origin->paint->mesh(),deform->morphs->targets[slot].delta,{128*1024*1024,128*1024*1024,cancelled});for(size_t v=0;v<old_positions.size();++v)old_positions[v]+=old_delta[v];}
 auto origin_delta=detail::normal_bake_target_origin_positions(rest,binding->uv_origin->paint->mesh(),delta,{128*1024*1024,128*1024*1024,cancelled});
 const auto identity=std::find_if(frames->targets.begin(),frames->targets.end(),[&](const auto&t){return t.id==id;});
 if(replace)identity->positions=origin_delta;
 else{frames->targets.push_back({id,{},{},std::move(origin_delta)});frames->current_targets.push_back(id);}
 std::map<const NormalFrames*,std::shared_ptr<const NormalFrames>> copies;
 auto author=[&](const std::shared_ptr<const NormalFrames>&old)->std::shared_ptr<const NormalFrames>{
  if(!old)return {};
  if(auto i=copies.find(old.get());i!=copies.end())return i->second;
  auto frame=std::make_shared<NormalFrames>(*old);NormalFrameTarget key;key.id=id;
  if(edit.action==ShapeKeyAction::Duplicate){key=frame_target(*old,frames->current_targets[edit.target]);key.id=id;}
  else if(edit.action==ShapeKeyAction::CaptureMix){
   for(size_t t=0;t<count;++t){float w=deform->morph_weights.empty()?0:deform->morph_weights[t];if(!w)continue;const auto &k=frame_target(*old,frames->current_targets[t]);
    auto add=[&](auto &out,const auto &values){if(values.empty())return;if(out.empty())out.resize(values.size());for(size_t c=0;c<values.size();++c){if((c&1023)==0)stop(cancelled);out[c]+=values[c]*w;}};
    add(key.normals,k.normals);add(key.tangents,k.tangents);
   }
  }else if(edit.action==ShapeKeyAction::Mirror||edit.action==ShapeKeyAction::Replace){
   const auto &mesh=binding->uv_origin->paint->mesh();std::vector<Vec3> edited=mesh.positions;
   const auto &authority=replace?identity->positions:frames->targets.back().positions;
   for(size_t v=0;v<edited.size();++v)edited[v]+=authority[v];
   key=authored_attributes(mesh,*old,replace?&frame_target(*old,id):nullptr,old_positions,edited,id,cancelled);
  }
  if(replace){auto target=std::find_if(frame->targets.begin(),frame->targets.end(),[&](const auto&t){return t.id==id;});*target=std::move(key);}
  else frame->targets.push_back(std::move(key));
  frame->validate();copies[old.get()]=frame;return frame;
 };
 frames->pbr=author(frames->pbr);frames->mtoon=author(frames->mtoon);frames->canonical=author(frames->canonical);frames->validate();binding->normal_frames=frames;
 deform->morphs->set_target(rest,name,positions);if(!replace&&!deform->morph_weights.empty())deform->morph_weights.push_back(0);
 deform->validate(rest);binding->validate();detail::normal_frame_current_authority(*binding,rest,deform.get(),cancelled);
 o.deformation=deform;o.material_bindings=binding;next.validate();stop(cancelled);scene=std::move(next);return {slot,true};
}
} // namespace dome
