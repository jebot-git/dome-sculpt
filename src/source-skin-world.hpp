// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "affine-surface.hpp"
#include "source-rest-graph.hpp"
#include "import-skin.hpp"
namespace dome::detail {
// Source skinning is MODEL-joint-world * IBM. The imported float mesh-space
// inverse is a compatibility representation, not an extra glTF mesh transform.
// Recover this relationship only from the owning source graph/instance/path;
// never infer it from a matrix value or a matching joint name alone.
inline std::optional<std::vector<Vec3>> source_skin_world_positions(
    const ObjectSurface &surface,const std::function<bool()> &cancelled={},
    size_t maximum_bytes=256u*1024u*1024u) {
  const auto &d=surface.deformation;const auto &appearance=surface.appearance;
  if(!d||!d->skin||!d->skeleton||!appearance||!appearance->asset||
      appearance->source_node<0||!d->skeleton->space_matrix||!d->skeleton->inverse_bind_override)return {};
  const auto &asset=*appearance->asset;const auto node=uint32_t(appearance->source_node);
  if(node>=asset.nodes.size()||asset.nodes[node].skin<0)return {};
  const auto &skeleton=*d->skeleton;
  if(*skeleton.space_matrix!=imported_inverse(asset.nodes[node].world))return {};
  std::shared_ptr<const SourceNodePlacement> identity;
  for(const auto &group:surface.groups)if(group.source_placement&&
      group.source_placement->source_node==node&&
      group.source_placement->model&&group.source_placement->model->asset==appearance->asset&&
      group.source_placement->model->original_bytes==appearance->original_bytes)
    identity=group.source_placement;
  if(!identity)return {};
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Source skin world evaluation cancelled");};stop();
  std::vector<uint32_t> ancestry;int parent=int(node);
  while(parent>=0){stop();if(ancestry.size()==256||size_t(parent)>=asset.nodes.size())throw std::invalid_argument("Source skin world ancestry is cyclic or excessive");ancestry.push_back(uint32_t(parent));parent=asset.nodes[size_t(parent)].parent;}
  std::reverse(ancestry.begin(),ancestry.end());size_t at=0;
  for(const auto &group:surface.groups)if(const auto &placement=group.source_placement){
    if(placement->model!=identity->model||placement->source_instance!=identity->source_instance||
       at==ancestry.size()||placement->source_node!=ancestry[at++])
      throw std::invalid_argument("Source skin world placement lost its authenticated ancestry");
  }
  if(at!=ancestry.size())throw std::invalid_argument("Source skin world placement lacks complete original ancestry");
  if(surface.mesh.positions.size()>maximum_bytes/sizeof(Vec3))throw std::length_error("Source skin world position budget exceeded");
  const auto position_bytes=surface.mesh.positions.size()*sizeof(Vec3);
  size_t scratch_bytes=position_bytes;
  auto charge=[&](size_t count,size_t stride){if(count>(maximum_bytes-scratch_bytes)/stride)throw std::length_error("Source skin world scratch budget exceeded");scratch_bytes+=count*stride;};
  charge(surface.mesh.positions.size(),sizeof(Vec3));charge(surface.mesh.normals.size(),sizeof(Vec3));charge(surface.mesh.indices.size(),sizeof(uint32_t));charge(surface.mesh.uv.size(),sizeof(std::array<float,2>));charge(surface.mesh.uv_pins.size(),1);charge(surface.mesh.uv_seams.size(),1);
  charge(skeleton.bones.size(),sizeof(AffineTransform)*2+sizeof(RigMatrix)*3+sizeof(uint8_t));
  std::vector<RigMatrix> local;
  {
    auto graph=capture_source_rest_graph(std::span<const ObjectSurface>(&surface,1),surface.id,maximum_bytes-position_bytes,cancelled);
    local=std::move(graph.parts.at(0).pose_local);
  }
  AffineTransform original_world;for(auto source:ancestry)original_world=original_world*AffineTransform(asset.nodes[source].local);
  const auto model_to_world=placement_frame(surface).world()*original_world.inverse();
  std::vector<AffineTransform> world(local.size());std::vector<uint8_t> done(local.size());
  auto visit=[&](auto &&self,size_t bone,size_t depth)->void{
    stop();if(done[bone]==2)return;if(done[bone]||depth>256)throw std::invalid_argument("Source skin world joint hierarchy is cyclic or excessive");done[bone]=1;
    auto matrix=AffineTransform(local[bone]);const auto parent=skeleton.bones[bone].parent;
    if(parent>=0){self(self,size_t(parent),depth+1);matrix=world[size_t(parent)]*matrix;}world[bone]=matrix;done[bone]=2;
  };
  const auto binds=skeleton.inverse_bind();std::vector<AffineTransform> matrices;matrices.reserve(local.size());
  for(size_t bone=0;bone<local.size();++bone){visit(visit,bone,1);matrices.push_back(model_to_world*world[bone]*AffineTransform(binds[bone]));}
  auto morphed=surface.mesh;if(d->morphs&&!d->morph_weights.empty())morphed=d->morphs->evaluate(surface.mesh,d->morph_weights);
  std::vector<Vec3> positions;positions.reserve(morphed.positions.size());
  for(size_t vertex=0;vertex<morphed.positions.size();++vertex){if((vertex&1023)==0)stop();AffineVector position{};
    bool influenced=false;for(const auto &influence:d->skin->vertices[vertex]){if(!influence.weight)continue;influenced=true;const auto p=matrices.at(influence.joint).point(affine_vector(morphed.positions[vertex]));for(size_t axis=0;axis<3;++axis)position[axis]+=p[axis]*influence.weight;}
    if(!influenced)position=placement_frame(surface).world().point({0,0,0}); // Preserve existing native zero-influence policy.
    positions.push_back(affine_position(position));
  }
  stop();return positions;
}
} // namespace dome::detail
