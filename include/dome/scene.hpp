// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include "navigation.hpp"
#include "paint.hpp"
#include <memory>
namespace dome {
struct Material {
  std::string name = "Clay";
  std::array<float, 4> color{.58f, .30f, .15f, 1};
  float metallic = 0, roughness = .8f;
  std::array<float, 3> emissive{};
  bool toon = false, double_sided = false;
  void validate() const;
};
struct SculptObject {
  uint64_t id = 0;
  std::string name = "Sculpt";
  CanvasTransform transform{};
  bool visible = true, locked = false;
  Material material;
  bool mesh_active = false;
  std::shared_ptr<PaintedMesh> painted;
  std::shared_ptr<Volume> volume = std::make_shared<Volume>();
};
struct ObjectSurface {
  uint64_t id = 0;
  std::string name;
  CanvasTransform transform{};
  Material material;
  Mesh mesh;
  std::shared_ptr<const TextureMaps> textures{};
};
// Scene copies retain object volumes for command history. Use snapshot() for
// detached immutable geometry on meshing and file workers.
class Scene {
public:
  Scene();
  const std::vector<SculptObject> &objects() const { return objects_; }
  SculptObject &object(uint64_t);
  const SculptObject &object(uint64_t) const;
  SculptObject &active() { return object(active_); }
  const SculptObject &active() const { return object(active_); }
  void select(uint64_t);
  uint64_t add(std::string name, Volume, CanvasTransform = {});
  uint64_t duplicate(uint64_t);
  void erase(uint64_t);
  Scene snapshot() const;
  void validate() const;
  void save(const std::filesystem::path &) const;
  static Scene load(const std::filesystem::path &);
  nlohmann::json encode() const;
  static Scene decode(const nlohmann::json &);
  std::vector<ObjectSurface>
  surfaces(bool atlas = false,
           const std::function<bool()> &cancelled = {}) const;

private:
  std::vector<SculptObject> objects_;
  uint64_t active_ = 1, next_ = 2;
};
void validate_transform(CanvasTransform);
void export_glb(const std::vector<ObjectSurface> &,
                const std::filesystem::path &);
} // namespace dome
