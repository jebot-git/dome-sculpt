// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-placement.hpp"
#include <set>
namespace dome {
SceneGroup &Scene::group(uint64_t id) {
  auto it = std::find_if(groups_.begin(), groups_.end(), [&](auto &g) { return g.id == id; });
  if (it == groups_.end()) throw std::out_of_range("Unknown scene group");
  return *it;
}
const SceneGroup &Scene::group(uint64_t id) const {
  return const_cast<Scene *>(this)->group(id);
}
std::vector<SceneGroup> Scene::group_path(uint64_t id) const {
  std::vector<SceneGroup> path;
  std::set<uint64_t> visited;
  while (id) {
    if (!visited.insert(id).second || visited.size() > 128)
      throw std::invalid_argument("Cyclic or excessive scene group hierarchy");
    auto &g = group(id);
    path.push_back(g);
    id = g.parent;
  }
  std::reverse(path.begin(), path.end());
  return path;
}
CanvasTransform ObjectSurface::world_transform() const {
  if(affine_edit || std::any_of(groups.begin(),groups.end(),[](const auto&g){return bool(g.source_placement)||bool(g.affine_edit);}))
    throw std::logic_error("This surface has exact affine placement; use world_matrix");
  CanvasTransform world{};
  for (const auto &g : groups) world = ob::compose(world, g.transform);
  return ob::compose(world, transform);
}
CanvasTransform Scene::world_transform(uint64_t id) const {
  auto &o = object(id);
  if(has_affine_placement(id))throw std::logic_error("This object has exact affine placement; use world_matrix");
  CanvasTransform world{};
  for (const auto &g : group_path(o.group)) world = ob::compose(world, g.transform);
  return ob::compose(world, o.transform);
}
AffineTransform ObjectSurface::world_matrix() const {
  AffineTransform world;
  for(const auto &g:groups)world=world*group_local_matrix(g);
  auto local=affine_similarity(transform);
  if(affine_edit)local=affine_edit->matrix()*local;
  return world*local;
}
AffineTransform Scene::parent_matrix(uint64_t id) const {
  AffineTransform world;
  for(const auto &g:group_path(object(id).group))world=world*group_local_matrix(g);
  return world;
}
AffineTransform Scene::world_matrix(uint64_t id) const {
  return parent_matrix(id)*object_local_matrix(object(id));
}
bool Scene::has_affine_placement(uint64_t id) const {
  return dome::has_affine_placement(object(id),group_path(object(id).group));
}
uint64_t Scene::add_group(std::string name, CanvasTransform transform,
                          uint64_t parent, std::shared_ptr<const HumanoidControls> recipe) {
  if (name.empty() || name.size() > 256 || name.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid scene group name");
  validate_transform(transform);
  if (parent) group(parent);
  if (groups_.size() >= 128 || next_group_ == UINT64_MAX)
    throw std::length_error("Scene group limit reached");
  if (recipe) {
    recipe->validate();
    recipe = std::make_shared<const HumanoidControls>(*recipe);
  }
  auto path=group_path(parent);
  if(std::any_of(path.begin(),path.end(),[](const auto&g){return bool(g.source_placement)||bool(g.affine_edit);})) {
    AffineTransform world;for(const auto &g:path)world=world*group_local_matrix(g);
    (void)(world*affine_similarity(transform));
  } else {
    CanvasTransform world{};for(const auto &g:path)world=ob::compose(world,g.transform);
    validate_transform(ob::compose(world,transform));
  }
  uint64_t id = next_group_++;
  groups_.push_back({id, parent, std::move(name), transform, std::move(recipe)});
  return id;
}
void Scene::erase_group(uint64_t id, bool erase_objects) {
  group(id);
  std::set<uint64_t> removed;
  for (const auto &g : groups_)
    for (const auto &ancestor : group_path(g.id))
      if (ancestor.id == id) { removed.insert(g.id); break; }
  if (erase_objects) {
    if (next_ == UINT64_MAX && std::all_of(objects_.begin(), objects_.end(),
         [&](auto &o) { return removed.contains(o.group); }))
      throw std::length_error("Cannot retain an empty sculpt: object IDs exhausted");
    std::erase_if(objects_, [&](auto &o) { return removed.contains(o.group); });
    if (objects_.empty()) add("Empty sculpt", Volume{});
    if (std::none_of(objects_.begin(), objects_.end(), [&](auto &o) { return o.id == active_; }))
      active_ = objects_.front().id;
  } else {
    // Compute every world transform while the entire old hierarchy still exists.
    struct Flat {uint64_t id;CanvasTransform transform;std::shared_ptr<const NativeAffineEdit> edit;};
    std::vector<Flat> flat;
    for (auto &o : objects_)
      if (removed.contains(o.group)) {
        if(has_affine_placement(o.id)) {
          auto edit=std::make_shared<const NativeAffineEdit>(world_matrix(o.id)*affine_similarity(o.transform).inverse());
          flat.push_back({o.id,o.transform,std::move(edit)});
        } else flat.push_back({o.id,world_transform(o.id),{}});
      }
    for (auto &f : flat) {
      auto &o = object(f.id); o.transform=f.transform;o.affine_edit=std::move(f.edit);o.group=0;
    }
  }
  std::erase_if(groups_, [&](auto &g) { return removed.contains(g.id); });
}
} // namespace dome
