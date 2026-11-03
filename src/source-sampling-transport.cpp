// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-sampling-transport.hpp"
#include <algorithm>

namespace dome::detail {
std::shared_ptr<const PaintSourceMaps> transport_source_sampling(
    const Mesh &before,const PaintSourceMaps &source,const Mesh &target,
    std::span<const uint32_t> vertices,const SourceSamplingLimits &limits) {
  const auto cancel=[&] {
    if(limits.cancelled&&limits.cancelled())
      throw std::runtime_error("Original sampling transport cancelled");
  };
  cancel();
  if(!limits.max_vertices||limits.max_vertices>2'000'000||
     !limits.max_triangles||limits.max_triangles>2'000'000||
     !limits.max_coordinate_bytes||limits.max_coordinate_bytes>128*1024*1024||
     !limits.max_work||limits.max_work>64'000'000)
    throw std::invalid_argument("Invalid original sampling transport bounds");
  if(before.positions.size()>limits.max_vertices||target.positions.size()>limits.max_vertices||
     before.indices.size()/3>limits.max_triangles||target.indices.size()/3>limits.max_triangles)
    throw std::length_error("Original sampling geometry exceeds its budget");
  // Bound validation as well as output remapping before traversing any large
  // input arrays. A small declared work quota must not be spent after a full
  // source-coordinate scan has already happened.
  uint64_t validation_work=uint64_t(before.positions.size())+before.normals.size()+before.uv.size()+before.indices.size()+
    before.uv_pins.size()+before.uv_seams.size()+target.positions.size()+target.normals.size()+target.uv.size()+
    target.indices.size()+target.uv_pins.size()+target.uv_seams.size();
  if(source.extra_uv_sets.size()>6)throw std::invalid_argument("Original sampling coordinate table exceeds bounds");
  if(source.uv1){if(source.uv1->size()!=before.positions.size())throw std::invalid_argument("Original sampling UV1 size differs");validation_work+=source.uv1->size();}
  for(const auto &[set,values]:source.extra_uv_sets){
    if(set<2||set>7||!values||values->size()!=before.positions.size())throw std::invalid_argument("Original sampling extra-coordinate size differs");
    validation_work+=values->size();
  }
  if(source.empty()||before.uv.size()!=before.positions.size()||
     target.uv.size()!=target.positions.size()||vertices.size()!=target.positions.size()||
     before.indices.size()!=target.indices.size())
    throw std::invalid_argument("Original sampling requires actual images and complete ordered correspondence");
  const std::array<const PaintSourceTexture*,5> slots{
    &source.color,&source.metal_rough,&source.emission,&source.normal,&source.occlusion};
  const bool needs_zero=std::any_of(slots.begin(),slots.end(),
    [](const auto *view){return view->image&&view->texcoord==0;});
  uint32_t carrier=1;
  if(needs_zero) {
    while(carrier<8&&(carrier==1?bool(source.uv1):source.extra_uv_sets.contains(carrier)))++carrier;
    if(carrier==8)throw std::length_error("Original UV0 needs an independent free coordinate carrier");
  }
  const size_t sets=size_t(bool(source.uv1))+source.extra_uv_sets.size()+size_t(needs_zero);
  // Charge every new coordinate array before allocating any of them. Images
  // are retained by immutable identity, never duplicated or downsampled here.
  const uint64_t bytes=uint64_t(sets)*vertices.size()*sizeof(std::array<float,2>);
  const uint64_t work=validation_work+uint64_t(sets+1)*vertices.size()+target.indices.size();
  if(bytes>limits.max_coordinate_bytes||work>limits.max_work)
    throw std::length_error("Original sampling transport exceeds coordinate/work budget");
  before.validate();target.validate();source.validate_surface(before);cancel();
  auto equal=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
  for(size_t v=0;v<vertices.size();++v) {
    if((v&1023)==0)cancel();const auto old=vertices[v];
    if(old>=before.positions.size()||!equal(target.positions[v],before.positions[old])||
       !equal(target.normals[v],before.normals[old]))
      throw std::invalid_argument("Original sampling transport cannot change geometry or normals");
  }
  for(size_t c=0;c<target.indices.size();++c) {
    if((c&1023)==0)cancel();
    if(vertices[target.indices[c]]!=before.indices[c])
      throw std::invalid_argument("Original sampling transport cannot reorder, lose or duplicate source corners");
  }
  auto remap=[&](const auto &coordinates) {
    auto output=std::make_shared<std::vector<std::array<float,2>>>();
    output->reserve(vertices.size());
    for(size_t v=0;v<vertices.size();++v) {
      if((v&1023)==0)cancel();const auto q=coordinates[vertices[v]];
      if(!std::isfinite(q[0])||!std::isfinite(q[1])||std::abs(q[0])>10000||std::abs(q[1])>10000)
        throw std::invalid_argument("Original sampling coordinate exceeds the native finite bound");
      output->push_back(q);
    }
    return std::shared_ptr<const std::vector<std::array<float,2>>>(std::move(output));
  };
  auto output=std::make_shared<PaintSourceMaps>(source);
  if(source.uv1)output->uv1=remap(*source.uv1);
  for(auto &[set,coordinates]:output->extra_uv_sets)coordinates=remap(*source.extra_uv_sets.at(set));
  if(needs_zero) {
    auto coordinates=remap(before.uv);
    if(carrier==1)output->uv1=std::move(coordinates);
    else output->extra_uv_sets.emplace(carrier,std::move(coordinates));
    for(auto view:{&output->color,&output->metal_rough,&output->emission,&output->normal,&output->occlusion})
      if(view->image&&view->texcoord==0)view->texcoord=int(carrier);
  }
  output->validate_surface(target);cancel();return output;
}
}
