// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-normal-uv.hpp"
#include <bit>
namespace dome::detail {
namespace {
void stop(const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Converted normal UV correspondence cancelled");
}
bool same(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool same(Vec3 a,Vec3 b){return same(a.x,b.x)&&same(a.y,b.y)&&same(a.z,b.z);}
bool same(std::array<float,2> a,std::array<float,2> b){return same(a[0],b[0])&&same(a[1],b[1]);}
size_t forecast(const Mesh &target,const CanonicalNormalUvOptions &o){
  if(!o.max_working_bytes||o.max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid converted normal UV memory policy");
  if(target.positions.empty()||target.positions.size()>2'000'000||target.indices.empty()||
      target.indices.size()>6'000'000||target.normals.size()!=target.positions.size()||
      target.uv.size()!=target.positions.size()||
      (!target.uv_pins.empty()&&target.uv_pins.size()!=target.positions.size())||
      (!target.uv_seams.empty()&&target.uv_seams.size()!=target.indices.size()))
    throw std::length_error("Converted normal UV geometry exceeds its bounded correspondence policy");
  // Include copied geometry/layout, flattened IDs/coordinates, subsequent
  // sampling-domain streams, and detached composition scratch before allocating.
  const size_t bytes=sizeof(CanonicalNormalUv)+sizeof(SourceNormalUvLayout)+sizeof(Mesh)+
    sizeof(NormalSamplingDomain)+2048+target.positions.size()*(sizeof(Vec3)*2+
      sizeof(std::array<float,2>)*3+sizeof(uint32_t)*2)+
    target.indices.size()*sizeof(uint32_t)*3+(target.uv_pins.size()+target.uv_seams.size())*2;
  if(bytes>o.max_working_bytes)throw std::length_error("Converted normal UV correspondence exceeds its prospective memory budget");
  return bytes;
}
void correspondence(const Mesh &before,const Mesh &target,std::span<const uint32_t> source,
    const std::function<bool()> &cancelled){
  if(source.size()!=target.positions.size()||target.indices.size()!=before.indices.size())
    throw std::invalid_argument("Converted normal UV edit requires exact ordered corner correspondence");
  before.validate();target.validate();
  for(size_t v=0;v<source.size();++v){
    if((v&1023)==0)stop(cancelled);
    if(source[v]>=before.positions.size()||!same(target.positions[v],before.positions[source[v]])||
        !same(target.normals[v],before.normals[source[v]]))
      throw std::invalid_argument("Converted normal UV edit changed authored POSITION or NORMAL");
    for(float q:target.uv[v])if(!std::isfinite(q)||q<0||q>1)
      throw std::invalid_argument("Converted normal UV atlas must lie in the unit square");
  }
  for(size_t c=0;c<target.indices.size();++c){
    if((c&1023)==0)stop(cancelled);
    if(source[target.indices[c]]!=before.indices[c])
      throw std::invalid_argument("Converted normal UV edit changed ordered source triangle identity");
  }
}
}
std::shared_ptr<const CanonicalNormalUv> CanonicalNormalUv::create(
    std::shared_ptr<const CanonicalNormalConversion> origin,const Mesh &target,
    std::span<const uint32_t> source,const CanonicalNormalUvOptions &o){
  stop(o.cancelled);forecast(target,o);
  if(!origin||!origin->valid_owner(origin))throw std::invalid_argument("Converted normal UV edit requires its actual conversion owner");
  origin->validate();return build(std::move(origin),target,source,o);
}
std::shared_ptr<const CanonicalNormalUv> CanonicalNormalUv::build(
    std::shared_ptr<const CanonicalNormalConversion> origin,const Mesh &target,
    std::span<const uint32_t> root_vertices,const CanonicalNormalUvOptions &o){
  stop(o.cancelled);const auto bytes=forecast(target,o);const auto &root=origin->before_paint()->mesh();
  correspondence(root,target,root_vertices,o.cancelled);
  auto layout=std::make_shared<SourceNormalUvLayout>();
  layout->source_vertices.assign(root_vertices.begin(),root_vertices.end());
  layout->indices=target.indices;layout->uv=target.uv;layout->pins=target.uv_pins;layout->seams=target.uv_seams;
  auto result=std::shared_ptr<CanonicalNormalUv>(new CanonicalNormalUv);
  result->origin_=std::move(origin);result->layout_=std::move(layout);result->mesh_=std::make_shared<const Mesh>(target);
  result->coordinates_.reserve(root_vertices.size());
  for(size_t v=0;v<root_vertices.size();++v){if((v&1023)==0)stop(o.cancelled);result->coordinates_.push_back(root.uv.at(root_vertices[v]));}
  result->bytes_=bytes;result->owner_=result;stop(o.cancelled);return result;
}
std::shared_ptr<const CanonicalNormalUv> CanonicalNormalUv::reparameterize(const Mesh &target,
    std::span<const uint32_t> source,const CanonicalNormalUvOptions &o) const {
  stop(o.cancelled);forecast(target,o);validate_current(*mesh_,o.cancelled);
  correspondence(*mesh_,target,source,o.cancelled);
  std::vector<uint32_t> flattened;flattened.reserve(source.size());
  for(size_t v=0;v<source.size();++v){if((v&1023)==0)stop(o.cancelled);flattened.push_back(layout_->source_vertices.at(source[v]));}
  return build(origin_,target,flattened,o);
}
void CanonicalNormalUv::validate_current(const Mesh &current,const std::function<bool()> &cancelled) const {
  stop(cancelled);const auto owner=owner_.lock();
  if(!valid_owner(owner)||!origin_||!origin_->valid_owner(origin_)||!layout_||!mesh_||!bytes_||
      coordinates_.size()!=mesh_->positions.size())throw std::invalid_argument("Incomplete actual converted normal UV owner");
  current.validate();
  if(current.positions.size()!=mesh_->positions.size()||current.uv.size()!=mesh_->uv.size()||current.indices!=mesh_->indices||
      current.uv_pins!=mesh_->uv_pins||current.uv_seams!=mesh_->uv_seams)
    throw std::invalid_argument("Converted normal UV owner does not match the current geometry/layout");
  for(size_t v=0;v<current.positions.size();++v){
    if((v&1023)==0)stop(cancelled);
    if(!same(current.positions[v],mesh_->positions[v])||!same(current.normals[v],mesh_->normals[v])||!same(current.uv[v],mesh_->uv[v]))
      throw std::invalid_argument("Converted normal UV current POSITION/NORMAL/UV differs from its authentic owner");
  }
  stop(cancelled);
}
} // namespace dome::detail
