// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-native-pose.hpp"
#include "source-paint-pairing.hpp"
#include "normal-frame-runtime.hpp"
#include <bit>
namespace dome::detail {
namespace {
struct Budget {
  const CanonicalNativePoseOptions &o;size_t work=0,bytes=4096;
  void stop() const {if(o.cancelled&&o.cancelled())throw std::runtime_error("Original native pose evaluation cancelled");}
  void step(size_t n=1){if(n>o.max_work-work)throw std::length_error("Original native pose work budget exceeded");work+=n;stop();}
  void charge(size_t n,size_t unit){if(bytes>o.max_working_bytes||n>(o.max_working_bytes-bytes)/unit)throw std::length_error("Original native pose prospective memory budget exceeded");bytes+=n*unit;}
};
bool bits(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool bits(Vec3 a,Vec3 b){return bits(a.x,b.x)&&bits(a.y,b.y)&&bits(a.z,b.z);}
}
std::shared_ptr<const CanonicalNativePose> CanonicalNativePose::create(
    const SculptObject &object,const CanonicalNativePoseOptions &options){
  Budget b{options};
  if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||!options.max_work||options.max_work>256*1024*1024)
    throw std::invalid_argument("Invalid original native pose resource policy");
  b.stop();const auto &binding=object.material_bindings;const auto &paint=object.painted;
  if(!object.mesh_active||!paint||paint->in_stroke()||object.appearance||!binding||
      !binding->canonical_attributes||binding->key_derivation||object.shape_source||object.rig_source||
      binding->normal_frames!=binding->canonical_attributes->frames())
    throw std::invalid_argument("Original native pose requires its exact supported canonical source and current frame authority");
  const auto origin=binding->canonical_attributes;
  if(!origin->valid_owner(origin))throw std::invalid_argument("Original native pose source lacks its authentic conversion owner");
  const auto rest=paint->mesh_owner();const auto &root=origin->before_paint()->mesh();
  if(rest->positions.size()>2'000'000||rest->indices.size()>6'000'000||root.positions.size()>2'000'000)
    throw std::length_error("Original native pose geometry exceeds its bounded domain");
  // Current output/smoothing scratch, two skin meshes, flattened mapping,
  // and conservative detached Mikk corner workspace are charged up front.
  b.charge(rest->positions.size(),sizeof(Vec3)*6+sizeof(std::array<float,2>)*3+sizeof(uint32_t)*2+8);
  b.charge(rest->indices.size(),sizeof(Vec3)*4+sizeof(Tangent)*3+sizeof(uint32_t)*3+sizeof(std::array<float,2>));
  b.charge(root.positions.size(),sizeof(Vec3));
  const auto &d=object.deformation;
  if(d){
    if(d->skeleton){b.charge(d->skeleton->bones.size(),sizeof(Bone)+sizeof(RigMatrix)*4+64);for(const auto &bone:d->skeleton->bones)b.charge(bone.name.size(),1);}
    if(d->skin){b.charge(d->skin->vertices.size(),sizeof(VertexWeights));for(const auto &v:d->skin->vertices)b.charge(v.size(),sizeof(Influence));}
    if(d->morphs){b.charge(d->morphs->targets.size(),sizeof(MorphTarget));for(const auto &t:d->morphs->targets){b.charge(t.name.size(),1);b.charge(t.delta.size(),sizeof(Vec3));}}
    b.charge(d->pose.size(),sizeof(JointTransform));b.charge(d->morph_weights.size(),sizeof(float));
  }
  b.step(rest->positions.size()+rest->indices.size());
  binding->validate();source_paint_pairing(paint.get(),binding.get(),nullptr,true);
  normal_frame_current_authority(*binding,*rest,d.get(),options.cancelled);
  if(d)d->validate(*rest);
  auto out=std::shared_ptr<CanonicalNativePose>(new CanonicalNativePose);
  out->origin_=origin;out->uv_=binding->canonical_uv;out->rest_=rest;out->root_vertices_.resize(rest->positions.size());
  if(out->uv_){out->uv_->validate_current(*rest,options.cancelled);out->root_vertices_=out->uv_->layout()->source_vertices;}
  else{
    if(rest->positions.size()!=root.positions.size()||rest->indices!=root.indices||rest->uv!=root.uv)
      throw std::invalid_argument("Original native pose topology/UV change lacks actual correspondence");
    for(size_t v=0;v<rest->positions.size();++v){if((v&1023)==0)b.step();if(!bits(rest->positions[v],root.positions[v])||!bits(rest->normals[v],root.normals[v]))throw std::invalid_argument("Original native pose rest geometry changed without authority");out->root_vertices_[v]=uint32_t(v);}
  }
  if(d){
    // Only these channels participate in native geometry evaluation. Copy
    // their values, not mutable input aliases or unrelated expression/UI data.
    auto copied=std::make_shared<DeformationState>(*rest);copied->skeleton=d->skeleton;copied->skin=d->skin;copied->morphs=d->morphs;copied->pose=d->pose;copied->morph_weights=d->morph_weights;out->evaluation_=std::move(copied);
  }
  out->posed_=*rest;
  bool active=false;
  if(out->evaluation_&&out->evaluation_->morphs&&!out->evaluation_->morph_weights.empty()){
    const auto &e=*out->evaluation_;
    for(size_t t=0;t<e.morph_weights.size();++t){const float w=e.morph_weights[t];if(w==0)continue;active=true;b.step(rest->positions.size());
      for(size_t v=0;v<rest->positions.size();++v){if((v&1023)==0)b.stop();out->posed_.positions[v]+=e.morphs->targets[t].delta[v]*w;}
    }
  }
  if(active){
    // Native smoothing belongs to the original topology, not the duplicated
    // editable UV vertices. Preserve the same face order and accumulation.
    std::vector<Vec3> normals(root.positions.size());
    for(size_t c=0;c<rest->indices.size();c+=3){if((c&1023)==0)b.step();const auto a=rest->indices[c],v=rest->indices[c+1],w=rest->indices[c+2];
      const auto n=cross(out->posed_.positions[v]-out->posed_.positions[a],out->posed_.positions[w]-out->posed_.positions[a]);
      normals[out->root_vertices_[a]]+=n;normals[out->root_vertices_[v]]+=n;normals[out->root_vertices_[w]]+=n;
    }
    for(auto &n:normals)n=normalized(n);
    for(size_t v=0;v<rest->positions.size();++v){if((v&1023)==0)b.step();out->posed_.normals[v]=normals[out->root_vertices_[v]];}
  }
  if(out->evaluation_&&out->evaluation_->skeleton&&out->evaluation_->skin&&
      (!out->evaluation_->pose.empty()||out->evaluation_->skeleton->inverse_bind_override)){
    b.step(rest->positions.size());const auto &e=*out->evaluation_;
    out->posed_=deform_mesh(out->posed_,*e.skin,*e.skeleton,e.pose);
  }
  for(size_t v=0;v<rest->positions.size();++v){if((v&1023)==0)b.step();out->posed_.uv[v]=root.uv.at(out->root_vertices_[v]);}
  out->posed_.validate();out->tangents_=tangent_corners(out->posed_,options.cancelled);
  out->working_bytes_=b.bytes;out->self_=out;b.stop();return out;
}
} // namespace dome::detail
