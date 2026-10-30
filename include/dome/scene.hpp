// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "deformation.hpp"
#include "mesh.hpp"
#include "groups.hpp"
#include "navigation.hpp"
#include "paint.hpp"
#include <memory>
namespace dome {
struct PolygonColorSurface;
struct CreatorAppearance;
struct SourceMaterialBindings;
struct RetopoPatch;
struct PolygonAuthority;
struct Material {
  std::string name = "Clay";
  std::array<float, 4> color{.58f, .30f, .15f, 1};
  float metallic = 0, roughness = .8f;
  std::array<float, 3> emissive{};
  bool toon = false, double_sided = false;
  enum class AlphaMode { Automatic, Opaque, Mask, Blend };
  AlphaMode alpha = AlphaMode::Automatic;
  float alpha_cutoff = .5f;
  bool unlit = false;
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
  std::shared_ptr<const DeformationState> deformation{};
  uint64_t group = 0;
  std::shared_ptr<const CreatorAppearance> appearance{};
  std::shared_ptr<const SourceMaterialBindings> material_bindings{};
  std::shared_ptr<const RetopoPatch> retopology{};
  std::shared_ptr<const PolygonAuthority> polygon{};
};
bool source_is_dormant(const SculptObject &);
struct ObjectSurface {
  uint64_t id = 0;
  std::string name;
  CanvasTransform transform{};
  Material material;
  Mesh mesh;
  std::shared_ptr<const TextureMaps> textures{};
  std::shared_ptr<const DeformationState> deformation{};
  // Rest mesh stays untouched for export, authoring and topology validation.
  Mesh preview_mesh() const;
  std::vector<SceneGroup> groups{}; // Root-to-leaf path, local object TRS retained.
  CanvasTransform world_transform() const;
  std::shared_ptr<const CreatorAppearance> appearance{};
  std::shared_ptr<const SourceMaterialBindings> material_bindings{};
  // Generated volume geometry cannot use dormant source topology/UV metadata.
  // Export must refuse rather than silently discard the retained source data.
  bool source_dormant = false;
  std::shared_ptr<const PolygonColorSurface> polygon_colors{};
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
  const std::vector<SceneGroup> &groups() const { return groups_; }
  SceneGroup &group(uint64_t);
  const SceneGroup &group(uint64_t) const;
  uint64_t add_group(std::string, CanvasTransform = {}, uint64_t parent = 0,
                     std::shared_ptr<const HumanoidControls> = {});
  void erase_group(uint64_t, bool erase_objects = true);
  std::vector<SceneGroup> group_path(uint64_t) const;
  CanvasTransform world_transform(uint64_t object) const;
  uint64_t add(std::string name, Volume, CanvasTransform = {});
  uint64_t duplicate(uint64_t);
  void erase(uint64_t);
  void set_deformation(uint64_t, DeformationState);
  void clear_deformation(uint64_t);
  Scene snapshot() const;
  void validate(bool deep_sources = true) const;
  void save(const std::filesystem::path &) const;
  static Scene load(const std::filesystem::path &);
  static Scene load(const std::filesystem::path &, const std::function<bool()> &cancelled);
  nlohmann::json encode() const;
  static Scene decode(const nlohmann::json &);
  // Includes visible empty volume objects, before surface extraction can skip them.
  void require_exportable_sources() const;
  std::vector<ObjectSurface>
  surfaces(bool atlas = false,
           const std::function<bool()> &cancelled = {}) const;
  std::vector<ObjectSurface> preview_surfaces(const std::function<bool()> &cancelled = {}) const;
  // Optional immutable source geometry cache for render workers. Material, pose
  // and source ownership still come from this Scene snapshot.
  using SculptMesher = std::function<Mesh(uint64_t, const Volume &)>;
  std::vector<ObjectSurface> preview_surfaces(const std::function<bool()> &, const SculptMesher &) const;

private:
  std::vector<ObjectSurface> surfaces_impl(bool, bool, const std::function<bool()> &, const SculptMesher & = {}) const;
  std::vector<SculptObject> objects_;
  uint64_t active_ = 1, next_ = 2;
  std::vector<SceneGroup> groups_;
  uint64_t next_group_ = 1;
};
void validate_transform(CanvasTransform);
void export_glb(const std::vector<ObjectSurface> &,
                const std::filesystem::path &);
} // namespace dome
