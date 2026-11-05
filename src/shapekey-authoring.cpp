#include "dome/texture-sets.hpp"
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
#include "shapekey-normal-preparation.hpp"
#include "shapekey-source-uv.hpp"
#include "shapekey-attributes.hpp"
#include "normal-key-derivation.hpp"
#include "native-paint-frame-source.hpp"
#include "key-mirror-correspondence.hpp"
#include <vector>
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
// Capture the native shader's face UV derivative instead of smoothing adjacent
// face tangents with Mikk. FixedAttributes stores these genuine corner values;
// no imported tangent authority or CanonicalMikk interpretation is claimed.
std::vector<Tangent> native_derivative_tangents(const Mesh &mesh,
    const NormalFrames &base,const NormalFrameTarget *key,
    std::span<const Vec3> delta,const std::function<bool()> &cancelled) {
 std::vector<Tangent> result;result.reserve(mesh.indices.size());
 auto position=[&](uint32_t v){return mesh.positions[v]+(delta.empty()?Vec3{}:delta[v]);};
 for(size_t c=0;c<mesh.indices.size();c+=3){if((c&1023)==0)stop(cancelled);
  const auto i=mesh.indices[c],j=mesh.indices[c+1],k=mesh.indices[c+2];
  const auto e1=position(j)-position(i),e2=position(k)-position(i);
  const auto a=mesh.uv[i],b=mesh.uv[j],d=mesh.uv[k];
  const float u1=b[0]-a[0],v1=b[1]-a[1],u2=d[0]-a[0],v2=d[1]-a[1],det=u1*v2-u2*v1;
  const float scale=std::sqrt((u1*u1+v1*v1)*(u2*u2+v2*v2));
  if(!std::isfinite(det)||std::abs(det)<=1e-8f*std::max(scale,1e-20f))
   throw std::invalid_argument("Native normal coordinate derivatives are singular; prepare a valid normal basis before key authoring");
  const auto raw_t=(e1*v2-e2*v1)/det,raw_b=(e2*u1-e1*u2)/det;
  for(size_t corner=c;corner<c+3;++corner){auto n=base.normals[corner];if(key&&!key->normals.empty())n+=key->normals[corner];n=normalized(n);
   auto t=raw_t-n*dot(n,raw_t);const float size=length(t);
   if(!finite(t)||!std::isfinite(size)||size<1e-10f)throw std::invalid_argument("Native normal derivative tangent is singular");
   t=t/size;const float hand=dot(cross(n,t),raw_b)<0?-1.f:1.f;result.push_back({t.x,t.y,t.z,hand});
  }
 }
 stop(cancelled);return result;
}
std::shared_ptr<const NormalFrameSet> capture_native_key_frames(const PaintedMesh &paint,
    const DeformationState *deformation,const SourceMaterialBindings &binding,
    const std::function<bool()> &cancelled) {
 const auto &origin=*binding.uv_origin;const auto &rest=paint.mesh();
 // A native capture is a real new frame. It may not borrow a differently
 // parameterized first source as if that were the current geometry/UV owner.
 const auto &owner=origin.paint->mesh();
 if(origin.paint->source_maps()!=paint.source_maps()||owner.indices!=rest.indices||owner.uv!=rest.uv||!equal(owner.positions,rest.positions))
  throw std::invalid_argument("Native key frame capture requires its current owning coordinate domain");
 const auto &maps=origin.paint->source_maps();
 if(maps&&maps->normal.image&&maps->normal.uv_transform!=std::array<float,6>{1,0,0,1,0,0})
  throw std::invalid_argument("Prepare source normals before capturing a transformed native normal frame");
 auto capture=[&](detail::ShapeFrameRole role) {
  NormalFrames selection;
  const auto coordinate=detail::shapekey_frame_coordinate(binding,selection,role);
  auto mesh=rest;mesh.uv=detail::shapekey_frame_coordinates(binding,coordinate,cancelled);
  auto candidate=detail::normal_bake_frame_candidate(mesh,deformation,nullptr,
      {128*1024*1024,128*1024*1024,cancelled});
  auto frame=std::make_shared<NormalFrames>(*candidate.frames->pbr);
  frame->generated_uv=coordinate;
  const bool normal_map=role==detail::ShapeFrameRole::Pbr?bool(maps&&maps->normal.image):
      bool(origin.material&&origin.material->parameters.normal_enabled&&origin.material->textures[size_t(MToonSlot::Normal)].index>=0);
  if(normal_map){
   frame->tangents=native_derivative_tangents(mesh,*frame,nullptr,{},cancelled);
   for(size_t slot=0;slot<frame->targets.size();++slot){auto &key=frame->targets[slot];auto tangents=native_derivative_tangents(mesh,*frame,&key,deformation->morphs->targets[slot].delta,cancelled);key.tangents.clear();key.tangents.reserve(tangents.size());
    for(size_t c=0;c<tangents.size();++c){if((c&1023)==0)stop(cancelled);const auto t=tangents[c],b=frame->tangents[c];if(t[3]!=b[3])throw std::invalid_argument("Native normal key changes immutable tangent handedness");key.tangents.push_back({t[0]-b[0],t[1]-b[1],t[2]-b[2]});}
   }
  }
  frame->validate();
  auto frames=std::make_shared<NormalFrameSet>(*candidate.frames);
  frames->pbr=frame;frames->mtoon=frame;return frames;
 };
 auto frames=capture(detail::ShapeFrameRole::Pbr);
 const bool typed_normal=origin.material&&origin.material->parameters.normal_enabled&&origin.material->textures[size_t(MToonSlot::Normal)].index>=0;
 if(binding.has_mtoon&&(detail::shapekey_frame_coordinate(binding,NormalFrames{},detail::ShapeFrameRole::MToon)!=*frames->pbr->generated_uv||(typed_normal&&!(maps&&maps->normal.image))))
  frames->mtoon=capture(detail::ShapeFrameRole::MToon)->pbr;
 frames->validate();return frames;
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
  o.material_bindings=origin.material_bindings;o.retopology=origin.retopology;o.polygon=origin.polygon;o.shape_source=origin.shape_source;o.rig_source=origin.rig_source;o.bake_recipe=origin.bake_recipe;o.texture_sets=origin.texture_sets?origin.texture_sets->snapshot():nullptr;
  next.validate();stop(cancelled);scene=std::move(next);return {0,true};
 }
 if(edit.action==ShapeKeyAction::Mirror&&(!std::isfinite(edit.mirror_plane)||std::abs(edit.mirror_plane)>1e6||size_t(edit.mirror_frames)>2))throw std::invalid_argument("Invalid explicit Mirror plane or frame interpretation");
 const auto &rest=source.painted->mesh();rest.validate();
 const MorphSet empty(rest);const auto &keys=source.deformation&&source.deformation->morphs?*source.deformation->morphs:empty;
 const size_t count=keys.targets.size();
 bool existing=edit.action==ShapeKeyAction::Duplicate||edit.action==ShapeKeyAction::Mirror||edit.action==ShapeKeyAction::Replace||edit.action==ShapeKeyAction::Rename;
 if(existing&&edit.target>=count)throw std::invalid_argument("Choose an existing shapekey");
 if(edit.action==ShapeKeyAction::Replace && edit.positions.size()!=rest.positions.size())throw std::invalid_argument("Shapekey edits must preserve the complete REST vertex domain");
 if(edit.action==ShapeKeyAction::Replace){bool unchanged=true;for(size_t v=0;v<rest.positions.size();++v){auto old=rest.positions[v]+keys.targets[edit.target].delta[v];unchanged&=old.x==edit.positions[v].x&&old.y==edit.positions[v].y&&old.z==edit.positions[v].z;}if(unchanged)return {edit.target,false};}
 if(edit.action==ShapeKeyAction::Rename && edit.name==keys.targets[edit.target].name)return {edit.target,false};
 const bool replace=edit.action==ShapeKeyAction::Replace||edit.action==ShapeKeyAction::Rename;
 const size_t slot=replace?edit.target:count;
 if(!replace && count>=128)throw std::length_error("Shapekey target budget exceeded");
 std::string name=edit.action==ShapeKeyAction::Replace?keys.targets[slot].name:key_name(keys,edit.name);
 // Capturing a native32 frame is a distinct operation from its historical
 // writable UV basis. Preserve both real owners in a detached paired state.
 if(source.material_bindings&&source.material_bindings->native_paint_sampling&&!source.material_bindings->native_paint_frames){
  auto captured=detail::capture_native_paint_frames(source,{128*1024*1024,128*1024*1024,cancelled});
  Scene paired=scene;auto &o=paired.object(object);const auto first=source.shape_source?source.shape_source:capture_polygon_origin(scene,object);
  auto b=std::make_shared<SourceMaterialBindings>(*source.material_bindings);b->native_paint_frames=captured;b->uv_origin=captured->origin();b->normal_frames=captured->frames();b->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;b->validate();o.material_bindings=b;o.shape_source=first;
  auto result=detail::author_derived_normal_shapekey(paired,object,edit,cancelled);
  paired.object(object).shape_source=first;paired.validate();stop(cancelled);scene=std::move(paired);return result;
 }
 if(detail::needs_independent_key_domain(source,edit,cancelled))return detail::author_derived_normal_shapekey(scene,object,edit,cancelled);
 if(edit.action==ShapeKeyAction::Mirror&&edit.mirror_frames!=ShapeKeyMirrorFrames::VertexPositions&&(edit.mirror_plane!=0||edit.mirror_frames!=ShapeKeyMirrorFrames::ExactAuthored))throw std::invalid_argument("Explicit-plane frame conversion requires genuine owning native frame authority");
 if(source.material_bindings&&source.material_bindings->normal_preparation)
  return detail::author_prepared_normal_shapekey(scene,object,edit,cancelled);
 auto deform=std::make_shared<DeformationState>(source.deformation?*source.deformation:DeformationState(rest));if(!deform->morphs)deform->morphs=MorphSet(rest);
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
  std::vector<uint32_t> mirror;std::vector<Vec3> vertex_delta;
  if(edit.action==ShapeKeyAction::Mirror){if(!std::isfinite(edit.mirror_tolerance)||edit.mirror_tolerance<=0||edit.mirror_tolerance>1)throw std::invalid_argument("Invalid local mirror tolerance");
   if(edit.mirror_frames==ShapeKeyMirrorFrames::VertexPositions){detail::KeyMirrorLimits limits;limits.cancelled=cancelled;vertex_delta=detail::reflected_key_vertex_deltas(rest,deform->morphs->targets[edit.target].delta,{edit.mirror_plane},edit.mirror_tolerance,limits);}
   else mirror=mirror_vertices(rest,edit.mirror_tolerance,cancelled);
  }
  for(size_t v=0;v<positions.size();++v){if((v&1023)==0)stop(cancelled);auto d=vertex_delta.empty()?deform->morphs->targets[edit.target].delta[mirror.empty()?v:mirror[v]]:vertex_delta[v];if(!mirror.empty())d.x=-d.x;positions[v]+=d;}
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
  if(source.material_bindings&&source.material_bindings->has_mtoon){origin->material=source.material_bindings->material;origin->roles=source.material_bindings->roles;}
  if(source.material_bindings && source.material_bindings->uv_origin)binding->uv_origin=source.material_bindings->uv_origin;
  else binding->uv_origin=std::move(origin);
  binding->has_mtoon=bool(source.material_bindings)&&source.material_bindings->has_mtoon;
  binding->normal_frames=capture_native_key_frames(*o.painted,source.deformation.get(),*binding,cancelled);
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
 struct Copy {const NormalFrames *source;uint32_t coordinate;std::shared_ptr<const NormalFrames> frame;};
 std::vector<Copy> copies;
 auto author=[&](const std::shared_ptr<const NormalFrames>&old,detail::ShapeFrameRole role)->std::shared_ptr<const NormalFrames>{
  if(!old)return {};
  const bool geometric=edit.action==ShapeKeyAction::Mirror||edit.action==ShapeKeyAction::Replace;
  const auto coordinate=geometric?detail::shapekey_frame_coordinate(*binding,*old,role):0;
  for(const auto &copy:copies)if(copy.source==old.get()&&copy.coordinate==coordinate)return copy.frame;
  auto frame=std::make_shared<NormalFrames>(*old);NormalFrameTarget key;key.id=id;
  if(edit.action==ShapeKeyAction::Duplicate){key=frame_target(*old,frames->current_targets[edit.target]);key.id=id;}
  else if(edit.action==ShapeKeyAction::CaptureMix){
   for(size_t t=0;t<count;++t){float w=deform->morph_weights.empty()?0:deform->morph_weights[t];if(!w)continue;const auto &k=frame_target(*old,frames->current_targets[t]);
    auto add=[&](auto &out,const auto &values){if(values.empty())return;if(out.empty())out.resize(values.size());for(size_t c=0;c<values.size();++c){if((c&1023)==0)stop(cancelled);out[c]+=values[c]*w;}};
    add(key.normals,k.normals);add(key.tangents,k.tangents);
   }
  }else if(edit.action==ShapeKeyAction::Mirror||edit.action==ShapeKeyAction::Replace){
   auto mesh=binding->uv_origin->paint->mesh();
   if(!old->tangents.empty())mesh.uv=detail::shapekey_frame_coordinates(*binding,coordinate,cancelled);
   std::vector<Vec3> edited=mesh.positions;
   const auto &authority=replace?identity->positions:frames->targets.back().positions;
   for(size_t v=0;v<edited.size();++v)edited[v]+=authority[v];
   key=detail::author_key_attributes(mesh,*old,replace?&frame_target(*old,id):nullptr,old_positions,edited,id,cancelled);
  }
  if(replace){auto target=std::find_if(frame->targets.begin(),frame->targets.end(),[&](const auto&t){return t.id==id;});*target=std::move(key);}
  else frame->targets.push_back(std::move(key));
  frame->validate();copies.push_back({old.get(),coordinate,frame});return frame;
 };
 frames->pbr=author(frames->pbr,detail::ShapeFrameRole::Pbr);frames->mtoon=author(frames->mtoon,detail::ShapeFrameRole::MToon);frames->canonical=author(frames->canonical,detail::ShapeFrameRole::Canonical);frames->validate();binding->normal_frames=frames;
 deform->morphs->set_target(rest,name,positions);if(!replace&&!deform->morph_weights.empty())deform->morph_weights.push_back(0);
 deform->validate(rest);binding->validate();detail::normal_frame_current_authority(*binding,rest,deform.get(),cancelled);
 o.deformation=deform;o.material_bindings=binding;next.validate();stop(cancelled);scene=std::move(next);return {slot,true};
}
} // namespace dome
