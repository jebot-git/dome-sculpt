// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-key-derivation.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-projection-identity.hpp"
#include "shapekey-attributes.hpp"
#include "shapekey-source-uv.hpp"
#include "polygon-conversion.hpp"
#include "text-utf8.hpp"
#include "source-paint-pairing.hpp"
#include <set>
#include <cstring>
namespace dome::detail {
namespace {
void stop(const std::function<bool()> &c){if(c&&c())throw std::runtime_error("Independent seam-key authoring cancelled");}
template<class T>bool own(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
template<class T>bool bytes(const std::vector<T>&a,const std::vector<T>&b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);}
bool role_bytes(const NormalFrames&a,const NormalFrames&b){return a.transport==b.transport&&a.generated_tangents==b.generated_tangents&&a.generated_uv==b.generated_uv&&a.origin_vertex_count==b.origin_vertex_count&&a.source_vertex_count==b.source_vertex_count&&a.source_corner_count==b.source_corner_count&&a.origin_vertices==b.origin_vertices&&a.source_vertices==b.source_vertices&&a.source_corners==b.source_corners&&bytes(a.normals,b.normals)&&bytes(a.tangents,b.tangents)&&a.targets.size()==b.targets.size()&&std::equal(a.targets.begin(),a.targets.end(),b.targets.begin(),[](const auto&x,const auto&y){return x.id==y.id&&bytes(x.normals,y.normals)&&bytes(x.tangents,y.tangents);});}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool vectors(std::span<const Vec3>a,std::span<const Vec3>b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(!same(a[i],b[i]))return false;return true;}
const FrameTargetIdentity &identity(const NormalFrameSet &f,uint64_t id){for(const auto&t:f.targets)if(t.id==id)return t;throw std::invalid_argument("Derived key lost a stable target identity");}
const NormalFrameTarget &attributes(const NormalFrames &f,uint64_t id){for(const auto&t:f.targets)if(t.id==id)return t;throw std::invalid_argument("Derived key lost its actual role target attributes");}
std::vector<Vec3> prior_positions(const SourceMaterialBindings &binding,uint64_t id){
  const auto &frames=*binding.normal_frames;const auto &target=identity(frames,id);if(!target.positions.empty())return target.positions;
  if(!target.origin_slot||!binding.uv_origin||!binding.uv_origin->deformation||!binding.uv_origin->deformation->morphs)throw std::invalid_argument("Derived key has no original native POSITION endpoint");
  const auto &source=binding.uv_origin->deformation->morphs->targets.at(*target.origin_slot).delta;
  if(!frames.position_domain)return source;
  std::vector<Vec3> out;out.reserve(frames.position_domain->source_frame_vertices().size());for(auto v:frames.position_domain->source_frame_vertices())out.push_back(source.at(v));return out;
}
std::vector<uint32_t> anchor_map(const SourceKeyDerivation &node){
  const auto &domain=*node.position_domain();if(node.before_material()->normal_frames->position_domain==node.position_domain()){
    std::vector<uint32_t> result(domain.basis()->positions.size());for(size_t i=0;i<result.size();++i)result[i]=uint32_t(i);return result;
  }return domain.origin_vertices();
}
std::vector<std::array<float,2>> role_coordinates(const SourceKeyDerivation &node,uint32_t coordinate,const std::function<bool()> &cancelled){
  auto values=frame_position_coordinates(*node.before_material(),coordinate,cancelled);const auto map=anchor_map(node);std::vector<std::array<float,2>> out;out.reserve(map.size());for(auto v:map)out.push_back(values.at(v));return out;
}
void key_budget(const DeformationState *d,size_t n,size_t targets){constexpr size_t cap=8'000'000;size_t count=0;auto add=[&](size_t v){if(v>cap-count)throw std::length_error("Independent key deformation preflight exceeds native quota");count+=v;};if(d){add(d->pose.size());for(const auto&p:d->pose)if(p.full_local_matrix)add(16);if(d->skin)for(const auto &v:d->skin->vertices)add(v.size());}add(targets);if(targets&&n>(cap-count)/targets)throw std::length_error("Independent key target POSITION arrays exceed native quota");}
std::string new_name(const MorphSet &m,const std::string &requested){auto used=[&](const std::string&s){return std::any_of(m.targets.begin(),m.targets.end(),[&](const auto&t){return t.name==s;});};if(!requested.empty()){if(requested.size()>256||requested.find('\0')!=std::string::npos||!text::valid_utf8(requested)||used(requested))throw std::invalid_argument("Choose a unique UTF-8 shapekey name (1..256 bytes)");return requested;}for(size_t i=1;i<=129;++i){auto name="Shape "+std::to_string(i);if(!used(name))return name;}throw std::length_error("Shapekey name budget exceeded");}
bool same_frames(const NormalFrameSet&a,const NormalFrameSet&b){
  if(!own(a.position_domain,b.position_domain)||a.skin_policy!=b.skin_policy||a.canonical_evaluation!=b.canonical_evaluation||a.current_targets!=b.current_targets||a.next_target!=b.next_target||a.targets.size()!=b.targets.size())return false;
  for(size_t i=0;i<a.targets.size();++i){const auto&x=a.targets[i],&y=b.targets[i];if(x.id!=y.id||x.origin_slot!=y.origin_slot||x.source_target!=y.source_target||!bytes(x.positions,y.positions))return false;}
  for(auto roles:{std::pair{a.pbr,b.pbr},std::pair{a.mtoon,b.mtoon},std::pair{a.canonical,b.canonical}})if(bool(roles.first)!=bool(roles.second)||(roles.first&&!role_bytes(*roles.first,*roles.second)))return false;
  return (a.pbr==a.mtoon)==(b.pbr==b.mtoon)&&(a.pbr==a.canonical)==(b.pbr==b.canonical)&&(a.mtoon==a.canonical)==(b.mtoon==b.canonical);
}
}
bool key_recipe_sampling_matches(const SourceNormalPreparation &a,const SourceNormalPreparation &b){return own(a.original_paint,b.original_paint)&&own(a.original_material,b.original_material)&&own(a.original_appearance,b.original_appearance)&&own(a.original_deformation,b.original_deformation)&&own(a.sampling_domain,b.sampling_domain)&&own(a.uv_reprojection,b.uv_reprojection)&&a.receipt.encode()==b.receipt.encode()&&normal_projection_identity(*a.projection,*b.projection)&&normal_projection_identity(*a.initial_projection,*b.initial_projection);}
const std::vector<Vec3> &SourceKeyDerivation::positions(uint64_t id) const {if(!frames_)throw std::invalid_argument("Independent key endpoints have no owning frame registry");return identity(*frames_,id).positions;}
std::shared_ptr<const NormalFrameSet> SourceKeyDerivation::replay(const std::function<bool()> &cancelled) const {
  stop(cancelled);const auto &before=*material_->normal_frames;const auto &basis=*domain_->basis();const auto map=anchor_map(*this);const size_t count=targets_.size();key_budget(deformation_.get(),basis.positions.size(),count);
  constexpr size_t cap=128*1024*1024;size_t planned=domain_->storage_bytes();auto add=[&](size_t n,size_t size){if(n>(cap-planned)/size)throw std::length_error("Independent key attribute preflight exceeds native frame quota");planned+=n*size;};
  add(count,basis.positions.size()*sizeof(Vec3));std::set<const NormalFrames*> old;for(const auto &f:{before.pbr,before.mtoon,before.canonical})if(f&&old.insert(f.get()).second){add(1,f->storage_bytes());if(count>f->targets.size())add(count-f->targets.size(),basis.indices.size()*2*sizeof(Vec3));}
  auto out=std::make_shared<NormalFrameSet>(before);out->position_domain=domain_;out->targets.clear();out->current_targets=current_targets_;out->next_target=next_target_;
  for(const auto &t:targets_){stop(cancelled);FrameTargetIdentity record;record.id=t.id;if(t.reference)record=identity(before,*t.reference);record.id=t.id;record.positions=positions(t.id);out->targets.push_back(std::move(record));}
  struct Copy {const NormalFrames*source;uint32_t coordinate;std::shared_ptr<const NormalFrames> frame;};std::vector<Copy> copies;
  auto role=[&](const std::shared_ptr<const NormalFrames> &artist,ShapeFrameRole kind)->std::shared_ptr<const NormalFrames>{
    if(!artist)return {};const auto coordinate=shapekey_frame_coordinate(*material_,*artist,kind);for(const auto &copy:copies)if(copy.source==artist.get()&&copy.coordinate==coordinate)return copy.frame;
    auto frame=std::make_shared<NormalFrames>(*artist);frame->origin_vertex_count=uint32_t(basis.positions.size());frame->origin_vertices=basis.indices;frame->targets.clear();auto mesh=basis;if(!artist->tangents.empty())mesh.uv=role_coordinates(*this,coordinate,cancelled);
    for(const auto &t:targets_){stop(cancelled);const auto &endpoint=positions(t.id);std::vector<Vec3> previous(mesh.positions.size());const NormalFrameTarget *old_key=nullptr;if(t.reference){auto old_position=prior_positions(*material_,*t.reference);for(size_t v=0;v<map.size();++v)previous[v]=old_position.at(map[v]);old_key=&attributes(*artist,*t.reference);}
      NormalFrameTarget target;if(vectors(previous,endpoint)){if(old_key)target=*old_key;target.id=t.id;}
      else {auto old_positions=mesh.positions,new_positions=mesh.positions;for(size_t v=0;v<map.size();++v){old_positions[v]+=previous[v];new_positions[v]+=endpoint[v];}target=author_key_attributes(mesh,*artist,old_key,old_positions,new_positions,t.id,cancelled);}
      frame->targets.push_back(std::move(target));
    }frame->validate();copies.push_back({artist.get(),coordinate,frame});return frame;
  };
  out->pbr=role(before.pbr,ShapeFrameRole::Pbr);out->mtoon=role(before.mtoon,ShapeFrameRole::MToon);out->canonical=role(before.canonical,ShapeFrameRole::Canonical);out->validate();stop(cancelled);return out;
}
void SourceKeyDerivation::validate(bool deep,const std::function<bool()> &cancelled) const {
  if(!paint_||!material_||!material_->normal_frames||!domain_||!frames_||targets_.size()>128||current_targets_.size()>128||!own(frames_->position_domain,domain_)||frames_->current_targets!=current_targets_||frames_->next_target!=next_target_||frames_->targets.size()!=targets_.size())throw std::invalid_argument("Incomplete owning independent key derivation");
  // Even mutable before-material aliases cannot create an unbounded recursive
  // source graph during a standalone/deep validation.
  thread_local std::vector<const SourceKeyDerivation*> stack;if(stack.size()>=32||std::find(stack.begin(),stack.end(),this)!=stack.end())throw std::length_error("Independent key derivation exceeds bounded acyclic history");stack.push_back(this);struct Pop{std::vector<const SourceKeyDerivation*>&s;~Pop(){s.pop_back();}}pop{stack};
  if(!own(paint_->mesh_owner(),domain_->current_geometry()))throw std::invalid_argument("Independent key derivation borrowed a different current geometry owner");
  const auto &old_domain=material_->normal_frames->position_domain;if(!own(old_domain,domain_)&&!own(old_domain,domain_->previous()))throw std::invalid_argument("Independent key domain lost its actual before-frame authority");
  if(!material_->uv_origin||!material_->uv_origin->paint||!own(domain_->source_frame_basis(),material_->uv_origin->paint->mesh_owner()))throw std::invalid_argument("Independent key domain changed its actual original attribute coordinate owner");
  const auto n=domain_->basis()->positions.size();std::set<uint64_t> ids;for(size_t i=0;i<targets_.size();++i){const auto&t=targets_[i];if(!t.id||t.id>=next_target_||!ids.insert(t.id).second||frames_->targets[i].positions.size()!=n||frames_->targets[i].id!=t.id)throw std::invalid_argument("Independent key target registry differs from its derived POSITION authority");if(t.reference){if(*t.reference!=t.id)throw std::invalid_argument("Independent key reference needs an explicit Duplicate/Mix provenance operation");identity(*material_->normal_frames,*t.reference);}else if(std::any_of(material_->normal_frames->targets.begin(),material_->normal_frames->targets.end(),[&](const auto &prior){return prior.id==t.id;}))throw std::invalid_argument("Independent key cannot relabel an original artist target as newly authored zero");if(deep)for(auto v:frames_->targets[i].positions)if(!finite(v))throw std::invalid_argument("Independent key POSITION authority is not finite");}
  if(targets_.size()<material_->normal_frames->targets.size()||next_target_<material_->normal_frames->next_target||next_target_-material_->normal_frames->next_target!=targets_.size()-material_->normal_frames->targets.size())throw std::invalid_argument("Independent key stable identity allocation differs from its actual before registry");
  const auto &prior_slots=material_->normal_frames->current_targets;if(current_targets_.size()<prior_slots.size()||!std::equal(prior_slots.begin(),prior_slots.end(),current_targets_.begin()))throw std::invalid_argument("Independent key operation cannot silently reorder or drop prior native target slots");
  for(const auto &prior:material_->normal_frames->targets)if(!ids.contains(prior.id))throw std::invalid_argument("Independent key operation omitted retained original target authority");
  material_->validate(deep);source_paint_pairing(paint_.get(),material_.get(),appearance_.get(),false);if(deep&&material_->color_preparation)material_->color_preparation->validate_current(paint_->mesh(),appearance_.get());normal_frame_current_authority(*material_,paint_->mesh(),deformation_.get(),cancelled);
  if(!material_->normal_preparation&&appearance_){const auto original=material_->uv_origin->appearance;if(!original||!own(original->asset,appearance_->asset)||!own(original->original_bytes,appearance_->original_bytes)||original->source_mesh!=appearance_->source_mesh||original->source_primitive!=appearance_->source_primitive||original->material!=appearance_->material||appearance_->source_vertices.size()!=paint_->mesh().positions.size())throw std::invalid_argument("Generic key anchor borrowed another original primitive appearance");
    for(size_t c=0;c<domain_->basis()->indices.size();++c){if((c&1023)==0)stop(cancelled);const auto v=domain_->basis()->indices[c];if(appearance_->source_vertices.at(domain_->current_vertices().at(v))!=original->source_vertices.at(domain_->source_frame_vertices().at(v)))throw std::invalid_argument("Generic key appearance map differs from its genuine source-frame corners");}}
  if(material_->normal_preparation){const auto &recipe=*material_->normal_preparation;
    if(!paint_->normal_projection()||!normal_projection_identity(*paint_->normal_projection(),*recipe.projection))throw std::invalid_argument("Independent key anchor lost its actual prepared normal field");
    recipe.validate_current(paint_->mesh(),appearance_.get(),cancelled);if(recipe.sampling_domain)recipe.sampling_domain->validate_paint(*paint_,*material_,deep,cancelled);
  }else if(paint_->normal_projection())throw std::invalid_argument("Independent key anchor has unbound normal sampling planes");
  domain_->validate(deep,cancelled);if(deformation_)deformation_->validate(paint_->mesh());if(deep&&!same_frames(*frames_,*replay(cancelled)))throw std::invalid_argument("Independent key attributes differ from genuine source-role authoring replay");
}
bool needs_independent_key_domain(const SculptObject &o,const ShapeKeyEdit &edit){
  if(!o.material_bindings||!o.material_bindings->normal_frames||!o.material_bindings->uv_origin)return false;
  if(o.material_bindings->key_derivation)return true;
  if(edit.action!=ShapeKeyAction::Replace)return false;const auto &current=o.painted->mesh(),&origin=frame_position_basis(*o.material_bindings);if(edit.positions.size()!=current.positions.size())throw std::invalid_argument("Independent key requires complete current REST positions");
  std::vector<Vec3> values(origin.positions.size());std::vector<uint8_t> seen(values.size());for(size_t c=0;c<current.indices.size();++c){auto a=current.indices[c],b=origin.indices.at(c);auto delta=edit.positions[a]-current.positions[a];if(seen[b]&&!same(values[b],delta))return true;values[b]=delta;seen[b]=1;}return false;
}
ShapeKeyResult author_derived_normal_shapekey(Scene &scene,uint64_t object,const ShapeKeyEdit &edit,const std::function<bool()> &cancelled){
  stop(cancelled);const auto &source=scene.object(object);const auto &binding=source.material_bindings;normal_frame_current_authority(*binding,source.painted->mesh(),source.deformation.get(),cancelled);const auto recipe=binding->normal_preparation;if(recipe)recipe->validate_current(source.painted->mesh(),source.appearance.get(),cancelled);
  if(edit.action!=ShapeKeyAction::Replace&&edit.action!=ShapeKeyAction::Create&&edit.action!=ShapeKeyAction::Rename)throw std::invalid_argument("Independent seam-key Duplicate/CaptureMix/Mirror require their own explicit target-reference adapter; original source retained");
  const auto &current=source.painted->mesh();const auto *original_keys=source.deformation&&source.deformation->morphs?&*source.deformation->morphs:nullptr;const size_t count=original_keys?original_keys->targets.size():0;const bool create=edit.action==ShapeKeyAction::Create;if(!create&&edit.target>=count)throw std::invalid_argument("Choose an existing shapekey");if(create&&count>=128)throw std::length_error("Shapekey target budget exceeded");
  key_budget(source.deformation.get(),current.positions.size(),count+(create?1:0));
  const auto domain=derive_frame_position_domain(source.painted->mesh_owner(),*binding,cancelled);key_budget(source.deformation.get(),domain->basis()->positions.size(),binding->normal_frames->targets.size()+(create?1:0));
  // FrameSet target POSITION vectors are the sole retained endpoint storage.
  // Derivation records retain only identity/reference provenance, not a second
  // independently mutable copy of every vertex array.
  auto deform=std::make_shared<DeformationState>(source.deformation?*source.deformation:DeformationState(current));if(!deform->morphs)deform->morphs=MorphSet(current);
  const auto slot=create?count:edit.target;const auto name=edit.action==ShapeKeyAction::Replace?deform->morphs->targets[slot].name:new_name(*deform->morphs,edit.name);
  if(edit.action==ShapeKeyAction::Rename){if(deform->expressions)deform->expressions->rename_shape(*deform->morphs,deform->morphs->targets[slot].name,name);else deform->morphs->targets[slot].name=name;Scene next=scene;next.object(object).deformation=deform;next.validate();stop(cancelled);scene=std::move(next);return{slot,true};}
  auto node=std::shared_ptr<SourceKeyDerivation>(new SourceKeyDerivation);node->owner_=node;const auto old=binding->key_derivation;
  const bool reuse=old&&old->position_domain()==domain&&old->before_paint()->mesh_owner()==source.painted->mesh_owner()&&bool(recipe)==bool(old->before_material()->normal_preparation)&&(!recipe||key_recipe_sampling_matches(*recipe,*old->before_material()->normal_preparation));
  auto staged=std::make_shared<NormalFrameSet>(*binding->normal_frames);staged->position_domain=domain;
  if(reuse){node->paint_=old->paint_;node->material_=old->material_;node->deformation_=old->deformation_;node->appearance_=old->appearance_;node->targets_=old->targets_;}
  else {node->paint_=source.painted->snapshot();node->material_=binding;node->deformation_=source.deformation;node->appearance_=source.appearance;node->domain_=domain;staged->targets.clear();const auto map=anchor_map(*node);for(const auto &t:binding->normal_frames->targets){node->targets_.push_back({t.id,t.id});FrameTargetIdentity record=t;auto old_positions=prior_positions(*binding,t.id);record.positions.clear();record.positions.reserve(map.size());for(auto v:map)record.positions.push_back(old_positions.at(v));staged->targets.push_back(std::move(record));}}
  node->domain_=domain;node->current_targets_=binding->normal_frames->current_targets;node->next_target_=binding->normal_frames->next_target;
  uint64_t id;if(create){if(node->targets_.size()>=128||node->next_target_==UINT64_MAX)throw std::length_error("Independent key stable identity budget exceeded");id=node->next_target_++;node->current_targets_.push_back(id);node->targets_.push_back({id,{}});staged->targets.push_back({id,{},{},std::vector<Vec3>(domain->basis()->positions.size())});}else id=node->current_targets_.at(slot);
  auto target=std::find_if(staged->targets.begin(),staged->targets.end(),[&](const auto&t){return t.id==id;});auto endpoint=current.positions;if(!create)endpoint=edit.positions;
  for(size_t v=0;v<domain->current_vertices().size();++v){if((v&1023)==0)stop(cancelled);auto vertex=domain->current_vertices()[v];target->positions[v]=endpoint.at(vertex)-current.positions.at(vertex);}
  staged->current_targets=node->current_targets_;staged->next_target=node->next_target_;node->frames_=std::move(staged);
  node->frames_=node->replay(cancelled);node->validate(true,cancelled);
  deform->morphs->set_target(source.painted->mesh(),name,endpoint);if(create&&!deform->morph_weights.empty())deform->morph_weights.push_back(0);deform->validate(source.painted->mesh());std::shared_ptr<const SourceNormalPreparation> next_recipe;if(recipe){auto prepared=std::make_shared<SourceNormalPreparation>(*recipe);prepared->target_frames=node->frames_;prepared->key_derivation=node;next_recipe=std::move(prepared);}
  auto next_binding=std::make_shared<SourceMaterialBindings>(*binding);next_binding->normal_frames=node->frames_;next_binding->normal_preparation=next_recipe;next_binding->key_derivation=node;next_binding->validate();normal_frame_current_authority(*next_binding,source.painted->mesh(),deform.get(),cancelled);
  Scene next=scene;auto &output=next.object(object);output.material_bindings=next_binding;output.deformation=deform;if(!reuse)output.shape_source=capture_polygon_origin(scene,object);next.validate();stop(cancelled);scene=std::move(next);return{slot,true};
}
} // namespace dome::detail
