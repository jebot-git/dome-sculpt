// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame-position-domain.hpp"
#include "dome/material-bindings.hpp"
#include "dome/paint.hpp"
#include "shapekey-source-uv.hpp"
#include <set>
namespace dome::detail {
namespace {
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same_mesh(const Mesh&a,const Mesh&b){if(a.positions.size()!=b.positions.size()||a.normals.size()!=b.normals.size()||a.uv!=b.uv||a.indices!=b.indices||a.uv_pins!=b.uv_pins||a.uv_seams!=b.uv_seams)return false;for(size_t v=0;v<a.positions.size();++v)if(!same(a.positions[v],b.positions[v])||!same(a.normals[v],b.normals[v]))return false;return true;}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
}
size_t FramePositionDomain::storage_bytes() const {size_t total=0;std::set<const Mesh*> meshes;std::set<const FramePositionDomain*> domains;charge_storage(total,128*1024*1024,domains,meshes);return total;}
void FramePositionDomain::charge_storage(size_t &total,size_t limit,std::set<const FramePositionDomain*> &domains,std::set<const Mesh*> &meshes) const {
  auto add=[&](size_t n,size_t stride){if(total>limit||n>(limit-total)/stride)throw std::length_error("Native key position-domain storage exceeds budget");total+=n*stride;};
  std::set<const FramePositionDomain*> path;
  for(auto node=this;node;node=node->previous_.get()){
    if(path.size()>=32||!path.insert(node).second)throw std::length_error("Native key position-domain history exceeds bounded acyclic depth");
    if(!domains.insert(node).second)break;
    if(!node->basis_||!node->before_||!node->current_||!node->source_)throw std::invalid_argument("Missing native key position-domain geometry");
    for(const auto &mesh:{node->basis_,node->before_,node->current_,node->source_})if(meshes.insert(mesh.get()).second){
      if(mesh->positions.size()>2'000'000||mesh->normals.size()>2'000'000||mesh->uv.size()>2'000'000||mesh->indices.size()>6'000'000||mesh->uv_pins.size()>2'000'000||mesh->uv_seams.size()>6'000'000)throw std::length_error("Native key position-domain geometry exceeds bounds");
      add(mesh->positions.size()+mesh->normals.size(),sizeof(Vec3));add(mesh->uv.size(),sizeof(std::array<float,2>));add(mesh->indices.size(),sizeof(uint32_t));add(mesh->uv_pins.size()+mesh->uv_seams.size(),1);
    }
    add(node->current_vertices_.size()+node->origin_vertices_.size()+node->source_vertices_.size(),sizeof(uint32_t));
  }
}

void FramePositionDomain::validate(bool deep,const std::function<bool()> &cancelled) const {
  storage_bytes();const auto n=basis_->positions.size();if(!n||n>2'000'000||basis_->indices.size()>6'000'000||current_vertices_.size()!=n||origin_vertices_.size()!=n||source_vertices_.size()!=n)
    throw std::invalid_argument("Invalid native key position-domain dimensions");
  const FramePositionDomain *node=this;std::set<const FramePositionDomain*> nodes;for(size_t depth=0;node;node=node->previous_.get(),++depth)if(depth>=32||!nodes.insert(node).second)throw std::length_error("Native key position-domain history exceeds bounded acyclic depth");
  if(previous_){if(!owner(before_,previous_->basis_)||!owner(source_,previous_->source_))throw std::invalid_argument("Native key position domain borrowed a different prior/source geometry owner");}
  else if(!owner(before_,source_))throw std::invalid_argument("Initial native key position basis lost its genuine original geometry owner");
  if(!deep)return;
  CornerKeyExpansionLimits limits;limits.cancelled=cancelled;auto expected=expand_corner_key_basis(*current_,*before_,limits);
  if(!same_mesh(*basis_,expected.basis)||current_vertices_!=expected.current_vertices||origin_vertices_!=expected.origin_vertices)throw std::invalid_argument("Native key position-domain replay differs from ordered source corners");
  for(size_t v=0;v<n;++v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Native key position-domain validation cancelled");const auto index=previous_?previous_->source_vertices_.at(origin_vertices_[v]):origin_vertices_[v];if(source_vertices_[v]!=index||index>=source_->positions.size())throw std::invalid_argument("Native key position domain relabelled its original attribute coordinate owner");}
}
std::shared_ptr<const FramePositionDomain> derive_frame_position_domain(std::shared_ptr<const Mesh> current,
    const SourceMaterialBindings &binding,const std::function<bool()> &cancelled){
  if(!current||!binding.normal_frames||!binding.uv_origin||!binding.uv_origin->paint)throw std::invalid_argument("Native key expansion needs its current and original owning frame geometry");
  const auto prior=binding.normal_frames->position_domain;const auto original=binding.uv_origin->paint->mesh_owner();
  if(prior){prior->validate(true,cancelled);if(!owner(prior->source_frame_basis(),original))throw std::invalid_argument("Derived frame geometry belongs to another original native frame source");if(owner(prior->current_geometry(),current))return prior;}
  auto result=std::shared_ptr<FramePositionDomain>(new FramePositionDomain);result->current_=std::move(current);result->previous_=prior;result->source_=original;result->before_=prior?prior->basis():original;
  CornerKeyExpansionLimits limits;limits.cancelled=cancelled;auto expanded=expand_corner_key_basis(*result->current_,*result->before_,limits);
  result->current_vertices_=std::move(expanded.current_vertices);result->origin_vertices_=std::move(expanded.origin_vertices);result->basis_=std::make_shared<const Mesh>(std::move(expanded.basis));result->source_vertices_.reserve(result->origin_vertices_.size());
  for(size_t v=0;v<result->origin_vertices_.size();++v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Native key expansion cancelled");auto index=result->origin_vertices_[v];result->source_vertices_.push_back(prior?prior->source_frame_vertices().at(index):index);}
  result->validate(true,cancelled);return result;
}
const Mesh &frame_position_basis(const SourceMaterialBindings &binding){
  if(binding.normal_frames&&binding.normal_frames->position_domain)return *binding.normal_frames->position_domain->basis();
  if(!binding.uv_origin||!binding.uv_origin->paint)throw std::invalid_argument("Frame POSITION authority has no actual original geometry owner");return binding.uv_origin->paint->mesh();
}
std::vector<std::array<float,2>> frame_position_coordinates(const SourceMaterialBindings &binding,uint32_t coordinate,
    const std::function<bool()> &cancelled){
  auto values=shapekey_frame_coordinates(binding,coordinate,cancelled);if(!binding.normal_frames||!binding.normal_frames->position_domain)return values;
  const auto &domain=*binding.normal_frames->position_domain;std::vector<std::array<float,2>> result;result.reserve(domain.basis()->positions.size());for(size_t v=0;v<domain.source_frame_vertices().size();++v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Native key role-coordinate expansion cancelled");result.push_back(values.at(domain.source_frame_vertices()[v]));}return result;
}
} // namespace dome::detail
