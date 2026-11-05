// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-paint-frame-source.hpp"
#include "normal-frame-runtime.hpp"
#include <bit>
namespace dome::detail {namespace {
void stop(const std::function<bool()> &c){if(c&&c())throw std::runtime_error("Native paint frame capture cancelled");}
template<class T>bool own(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool same(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool same(Vec3 a,Vec3 b){return same(a.x,b.x)&&same(a.y,b.y)&&same(a.z,b.z);}
bool base_equal(const NormalFrames &a,const NormalFrames &b,const std::function<bool()> &cancelled){
 if(a.transport!=b.transport||a.generated_tangents!=b.generated_tangents||a.generated_uv!=b.generated_uv||a.normals.size()!=b.normals.size()||a.tangents.size()!=b.tangents.size()||a.source_vertex_count!=b.source_vertex_count||a.source_corner_count!=b.source_corner_count||a.source_vertices!=b.source_vertices||a.source_corners!=b.source_corners)return false;
 for(size_t c=0;c<a.normals.size();++c){if((c&1023)==0)stop(cancelled);if(!same(a.normals[c],b.normals[c]))return false;}
 for(size_t c=0;c<a.tangents.size();++c){if((c&1023)==0)stop(cancelled);for(size_t k=0;k<4;++k)if(!same(a.tangents[c][k],b.tangents[c][k]))return false;}
 return true;
}
void base_equal(const NormalFrameSet &a,const NormalFrameSet &b,const std::function<bool()> &cancelled){
 if(a.skin_policy!=b.skin_policy||a.canonical_evaluation!=b.canonical_evaluation||(a.pbr==a.mtoon)!=(b.pbr==b.mtoon)||(a.pbr==a.canonical)!=(b.pbr==b.canonical))throw std::invalid_argument("Native frame capture changed its base role interpretation");
 for(auto pair:{std::pair{a.pbr,b.pbr},std::pair{a.mtoon,b.mtoon},std::pair{a.canonical,b.canonical}})
  if(bool(pair.first)!=bool(pair.second)||(pair.first&&!base_equal(*pair.first,*pair.second,cancelled)))throw std::invalid_argument("Native frame base attributes differ from their independent current capture");
}
template<class Emit>void frame_metadata(const NormalFrameSet &set,Emit emit){
 emit(uint64_t(set.skin_policy));emit(uint64_t(set.canonical_evaluation));emit(set.next_target);emit(set.current_targets.size());for(auto id:set.current_targets)emit(id);
 emit(set.targets.size());for(const auto &t:set.targets){emit(t.id);emit(t.origin_slot.has_value());emit(t.origin_slot.value_or(0));emit(t.source_target.has_value());emit(t.source_target.value_or(0));emit(t.positions.size());}
 for(const auto &role:{set.pbr,set.mtoon,set.canonical}){emit(bool(role));if(!role)continue;
  emit(uint64_t(role->transport));emit(role->generated_tangents);emit(role->generated_uv.has_value());emit(role->generated_uv.value_or(0));emit(role->origin_vertex_count);emit(role->source_vertex_count);emit(role->source_corner_count);
  emit(role->origin_vertices.size());emit(role->source_vertices.size());emit(role->source_corners.size());emit(role->normals.size());emit(role->tangents.size());emit(role->targets.size());for(const auto &t:role->targets){emit(t.id);emit(t.normals.size());emit(t.tangents.size());}
 }
}
void preflight(const PaintedMesh &paint,const DeformationState *deform,const NormalBakeFrameOptions &options){
 if(!options.max_bytes||options.max_bytes>128*1024*1024||!options.max_work||options.max_work>256*1024*1024)throw std::invalid_argument("Invalid native frame capture resource bounds");
 size_t bytes=sizeof(NativePaintFrameSource)+2*sizeof(PaintedMesh)+sizeof(SourceMaterialBindings)+sizeof(SourceUvOrigin)+65536;constexpr size_t total=256*1024*1024;
 auto add=[&](size_t n,size_t stride){if(n>(total-bytes)/stride)throw std::length_error("Native frame capture exceeds owning metadata/model budget before copies");bytes+=n*stride;};
 for(const auto &layer:paint.layers()){add(2,sizeof(PaintLayer));add(2,layer.name.size()+1);add(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),2*sizeof(std::shared_ptr<const void>));}
 if(deform){
  deform->resource_entries();add(1,sizeof(DeformationState));add(deform->pose.size(),sizeof(JointTransform));add(deform->morph_weights.size(),sizeof(float));
  if(deform->skin){add(deform->skin->vertices.size(),sizeof(std::vector<Influence>));for(const auto &row:deform->skin->vertices)add(row.size(),sizeof(Influence));}
  if(deform->skeleton){add(deform->skeleton->bones.size(),sizeof(Bone));for(const auto &b:deform->skeleton->bones)add(b.name.size()+1,1);if(deform->skeleton->inverse_bind_override)add(deform->skeleton->inverse_bind_override->size(),sizeof(RigMatrix));}
  if(deform->humanoid)for(const auto &[name,joint]:deform->humanoid->joints)add(1,128+name.size());
  if(deform->landmarks)for(const auto &[name,value]:deform->landmarks->points)add(1,128+name.size());
  if(deform->expressions)for(const auto &[name,values]:deform->expressions->visemes){add(1,128+name.size());add(values.size(),sizeof(VisemeBinding));for(const auto &v:values)add(v.shape.size()+1,1);}
 }
 // The initial source has no morph targets. Charge both the actual bounded
 // capture and independent replay peak before snapshots/model copies.
 const auto n=paint.mesh().indices.size(),v=paint.mesh().positions.size();
 if(!n||!v)throw std::invalid_argument("Native frame capture requires a nonempty indexed current source");
 constexpr size_t corner_bytes=3*sizeof(uint32_t)+sizeof(Vec3)+sizeof(Tangent)+2*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t)+sizeof(Tangent));
 constexpr size_t vertex_bytes=4*sizeof(Vec3)+2*sizeof(std::array<float,2>)+sizeof(uint8_t);
 if(n>options.max_bytes/corner_bytes||v>(options.max_bytes-n*corner_bytes)/vertex_bytes)throw std::length_error("Native frame capture exceeds explicit frame scratch before copies");
 add(2,n*corner_bytes+v*vertex_bytes);stop(options.cancelled);
}
}
void NativePaintFrameSource::seal_metadata(){
 size_t count=0;auto measure=[&](uint64_t){if(++count>8192)throw std::length_error("Native frame scalar certificate exceeds bounded metadata");};frame_metadata(*base_,measure);frame_metadata(*frames_,measure);metadata_.reserve(count);auto take=[&](uint64_t value){metadata_.push_back(value);};frame_metadata(*base_,take);frame_metadata(*frames_,take);
 role_owners_={base_->pbr,base_->mtoon,base_->canonical,frames_->pbr,frames_->mtoon,frames_->canonical};domain_owners_={base_->position_domain,frames_->position_domain};
}
void NativePaintFrameSource::check_metadata() const {
 if(metadata_.empty()||metadata_.size()>8192)throw std::invalid_argument("Native frame source has no private scalar/owner certificate");
 const std::array<std::shared_ptr<const NormalFrames>,6> roles{base_->pbr,base_->mtoon,base_->canonical,frames_->pbr,frames_->mtoon,frames_->canonical};for(size_t n=0;n<roles.size();++n)if(!own(roles[n],role_owners_[n]))throw std::invalid_argument("Native frame source changed its sealed normal role owner");
 if(!own(base_->position_domain,domain_owners_[0])||!own(frames_->position_domain,domain_owners_[1]))throw std::invalid_argument("Native frame source changed its sealed POSITION domain owner");
 size_t n=0;auto compare=[&](uint64_t value){if(n>=metadata_.size()||value!=metadata_[n++])throw std::invalid_argument("Native frame source changed its sealed scalar metadata");};frame_metadata(*base_,compare);frame_metadata(*frames_,compare);if(n!=metadata_.size())throw std::invalid_argument("Native frame scalar certificate changed dimensions");
}
bool NativePaintFrameSource::valid_owner(const std::shared_ptr<const NativePaintFrameSource>&p)const noexcept{return p.get()==this&&!self_.expired()&&!self_.owner_before(p)&&!p.owner_before(self_);}
void NativePaintFrameSource::validate(bool deep,const std::function<bool()> &cancelled)const{
 stop(cancelled);
 if(!paint_||!material_||!origin_||!base_||!frames_||!material_->native_paint_sampling||material_->native_paint_frames||material_->normal_frames||material_->key_derivation||material_->normal_preparation||material_->canonical_attributes||material_->canonical_uv||material_->has_mtoon||!own(origin_->paint,paint_)||origin_->appearance||origin_->material||!origin_->creator_kind.empty()||origin_->creator_version||!origin_->creator_parameters.empty())throw std::invalid_argument("Native frame source lost its actual unprepared before paint/material/origin");
 if(origin_->deformation&&origin_->deformation->morphs&&!origin_->deformation->morphs->targets.empty())throw std::invalid_argument("Native initial frame source requires its explicit morph-domain adapter");
 for(const auto&r:origin_->roles)if(r.image)throw std::invalid_argument("Native frame source cannot borrow an original role image");
 if(base_->position_domain||!base_->targets.empty()||!base_->current_targets.empty()||base_->next_target!=1)throw std::invalid_argument("Native initial frame source has fabricated key identity history");
 if(keys_&&(!keys_->valid_owner(keys_)||!own(keys_->frames(),frames_)||!keys_->before_material()||!keys_->before_material()->native_paint_frames||!own(keys_->before_material()->native_paint_frames->origin(),origin_)))throw std::invalid_argument("Native key frame source changed its sealed before/current owner association");
 if(!metadata_.empty())check_metadata();
 if(!deep){if(metadata_.empty())throw std::invalid_argument("Unsealed native frame capture cannot enter cheap publication");return;}
 material_->validate(true);base_->validate(true);frames_->validate(true);
 if(!material_->native_paint_sampling->matches_current(*paint_))throw std::invalid_argument("Native frame capture borrowed another paint geometry attachment");
 if(keys_){if(!keys_->valid_owner(keys_)||!own(keys_->frames(),frames_)||!keys_->before_material()||!keys_->before_material()->native_paint_frames||!own(keys_->before_material()->native_paint_frames->origin(),origin_))throw std::invalid_argument("Native key frames lost their own before capture and current derivation");keys_->validate(deep,cancelled);}
 else if(!own(frames_,base_))throw std::invalid_argument("Changed native frame/key data requires its actual derivation");
 if(deep){
  if(origin_->deformation)origin_->deformation->validate(paint_->mesh());
  auto expected=normal_bake_frame_candidate(paint_->mesh(),origin_->deformation.get(),nullptr,{128*1024*1024,128*1024*1024,cancelled}).frames;
  base_equal(*base_,*expected,cancelled);
  for(const auto &frame:{base_->pbr,base_->mtoon,base_->canonical})if(frame&&(frame->origin_vertex_count!=paint_->mesh().positions.size()||frame->origin_vertices!=paint_->mesh().indices))throw std::invalid_argument("Native initial frame capture lost exact current origin corners");
  base_equal(*frames_,*base_,cancelled);
 }
 stop(cancelled);
}
void NativePaintFrameSource::validate_binding(const SourceMaterialBindings &binding,bool deep,const std::function<bool()> &cancelled)const{
 if(binding.has_mtoon||binding.color_preparation||binding.normal_preparation||binding.canonical_attributes||binding.canonical_uv||!binding.sampling_reparameterized)throw std::invalid_argument("Native frame seal cannot reinterpret another current material role");
 for(const auto &role:binding.roles)if(role.image)throw std::invalid_argument("Native frame seal cannot authorize an unrelated role image");
 if(!valid_owner(binding.native_paint_frames)||!own(binding.uv_origin,origin_)||!own(binding.normal_frames,frames_)||!own(binding.key_derivation,keys_)||binding.normal_frame!=SourceMaterialNormalFrame::OriginalAttributes||!binding.native_paint_sampling)throw std::invalid_argument("Native frame material changed its sealed current owner/interpretation");
 auto node=binding.native_paint_sampling;bool found=false;for(size_t depth=0;node&&depth<32;++depth){if(own(node,material_->native_paint_sampling)){found=true;break;}const auto &before=node->before_material();node=before?before->native_paint_sampling:nullptr;}
 if(!found)throw std::invalid_argument("Native normal capture and writable paint have unrelated histories");
 validate(deep,cancelled);
}
std::shared_ptr<const NativePaintFrameSource> capture_native_paint_frames(const SculptObject &source,const NormalBakeFrameOptions &options){
 stop(options.cancelled);
 if(!native_paint_retention_supported(source)||!source.material_bindings||!source.material_bindings->native_paint_sampling||source.material_bindings->normal_frames)throw std::invalid_argument("Native key capture needs its genuine current supported writable paint");
 preflight(*source.painted,source.deformation.get(),options);
 auto node=std::shared_ptr<NativePaintFrameSource>(new NativePaintFrameSource);
 // Freeze a scrubbed snapshot as a genuinely const object. Both transient
 // metadata copies (but no shared immutable tile buffers) are precharged.
 node->paint_=std::make_shared<const PaintedMesh>(*source.painted->snapshot());node->material_=std::make_shared<const SourceMaterialBindings>(*source.material_bindings);
 SourceUvOrigin origin;origin.paint=node->paint_;if(source.deformation)origin.deformation=std::make_shared<const DeformationState>(*source.deformation);node->origin_=std::make_shared<const SourceUvOrigin>(std::move(origin));
 node->base_=normal_bake_frame_candidate(node->paint_->mesh(),node->origin_->deformation.get(),nullptr,options).frames;node->frames_=node->base_;node->self_=node;node->validate(true,options.cancelled);node->seal_metadata();return node;
}
std::shared_ptr<const NativePaintFrameSource> rebind_native_paint_frames(const std::shared_ptr<const NativePaintFrameSource>&before,std::shared_ptr<const NormalFrameSet> frames,std::shared_ptr<const SourceKeyDerivation> keys,const std::function<bool()> &cancelled){
 if(!before||!before->valid_owner(before))throw std::invalid_argument("Native key rebind needs its genuine capture");
 before->validate(true,cancelled);
 auto node=std::shared_ptr<NativePaintFrameSource>(new NativePaintFrameSource);node->paint_=before->paint_;node->material_=before->material_;node->origin_=before->origin_;node->base_=before->base_;node->frames_=std::move(frames);node->keys_=std::move(keys);node->self_=node;node->validate(true,cancelled);node->seal_metadata();return node;
}
void seal_native_paint_frames(SourceMaterialBindings &binding,const std::function<bool()> &cancelled){if(binding.native_paint_frames)binding.native_paint_frames=rebind_native_paint_frames(binding.native_paint_frames,binding.normal_frames,binding.key_derivation,cancelled);}
std::shared_ptr<const NativePaintFrameSource> NativePaintFrameCodec::replay(std::shared_ptr<const PaintedMesh> paint,std::shared_ptr<const SourceMaterialBindings> material,std::shared_ptr<const SourceUvOrigin> origin,std::shared_ptr<const NormalFrameSet> base,std::shared_ptr<const NormalFrameSet> frames,std::shared_ptr<const SourceKeyDerivation> keys,const std::function<bool()> &cancelled){
 if(!paint||!material||!origin||!own(origin->paint,paint))throw std::invalid_argument("Native frame record lost exact paint/origin alias");
 preflight(*paint,origin->deformation.get(),{128*1024*1024,128*1024*1024,cancelled});
 auto node=std::shared_ptr<NativePaintFrameSource>(new NativePaintFrameSource);node->paint_=std::move(paint);node->material_=std::move(material);node->origin_=std::move(origin);node->base_=std::move(base);node->frames_=std::move(frames);node->keys_=std::move(keys);node->self_=node;node->validate(true,cancelled);node->seal_metadata();return node;
}
} // namespace dome::detail
