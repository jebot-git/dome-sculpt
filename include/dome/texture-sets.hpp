// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include <span>
namespace dome {
using TextureSetId = uint32_t;
namespace detail { class TextureSetFieldSource; struct TextureSetAccess; struct TextureSetPaintAccess; class TextureSetArchive; }
struct TextureSetInitialField {Material material;uint32_t resolution=512;};
struct TextureSetLimits {
  size_t maximum_sets_per_object=64,maximum_sets_per_scene=256;
  size_t maximum_triangles=2'000'000,maximum_corners=6'000'000;
  size_t maximum_work=64'000'000,maximum_bytes=256*1024*1024;
  std::function<bool()> cancelled;
  void validate() const;
};
// Geometry-only, immutable ordered identities. This owns no paint, material,
// cache or Scene backlink. Polygon faces/corners retain their actual IDs;
// triangle-only meshes get independent IDs at the first set operation.
class TextureSetTopology {
public:
  const std::shared_ptr<const Mesh>& geometry()const noexcept{return geometry_;}
  std::span<const uint64_t> triangle_faces()const noexcept{return faces_;}
  std::span<const uint64_t> triangle_corners()const noexcept{return corners_;}
  bool polygon_ids()const noexcept{return polygon_ids_;}
private:
  friend struct detail::TextureSetAccess;
  friend class detail::TextureSetArchive;
  TextureSetTopology()=default;
  TextureSetTopology(const TextureSetTopology&)=default;
  std::shared_ptr<const Mesh> geometry_;
  std::vector<uint64_t> faces_,corners_;
  bool polygon_ids_=false;
};
// Set membership is an exact subset of the full logical triangle sequence.
// Raster samples keep those original triangle/barycentric IDs after filtering.
// The total member count across a collection is exactly the triangle count.
class TextureSetFaceDomain {
public:
  TextureSetId set()const noexcept{return set_;}
  const std::shared_ptr<const TextureSetTopology>& topology()const noexcept{return topology_;}
  std::span<const uint32_t> triangles()const noexcept{return triangles_;}
  bool contains(uint32_t triangle)const noexcept;
  bool matches(const std::shared_ptr<const Mesh>&)const noexcept;
  void authenticate_owner(const std::shared_ptr<const TextureSetFaceDomain>&)const;
private:
  friend struct detail::TextureSetAccess;
  friend class detail::TextureSetArchive;
  friend class TextureSetCollection;
  TextureSetFaceDomain()=default;
  TextureSetFaceDomain(const TextureSetFaceDomain&)=default;
  mutable std::weak_ptr<const TextureSetFaceDomain>self_;
  std::shared_ptr<const TextureSetTopology> topology_;
  std::shared_ptr<const std::vector<TextureSetId>> assignments_;
  std::vector<uint32_t> triangles_;
  TextureSetId set_=0;
};
struct TextureSetRecord {
  TextureSetId id=0;
  std::string name;
  std::shared_ptr<const PaintedMesh> painted;
  Material material;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  std::shared_ptr<const ObjectBakeRecipe> bake_recipe;
  std::shared_ptr<const TextureSetFaceDomain> domain;
  std::shared_ptr<const detail::TextureSetFieldSource> source;
};
// Set0 uses the existing SculptObject paint/material/source fields. Additional
// records never replace that geometry authority when the active set changes.
// IDs are object-local material-slot identities, unrelated to display names or
// original material table numbers on a different imported object instance.
class TextureSetCollection {
public:
  TextureSetId active()const noexcept{return active_;}
  uint64_t next_id()const noexcept{return next_;}
  const std::shared_ptr<const TextureSetTopology>& topology()const noexcept{return topology_;}
  const std::shared_ptr<const TextureSetFaceDomain>& primary_domain()const noexcept{return primary_;}
  const std::shared_ptr<const detail::TextureSetFieldSource>& primary_source()const noexcept{return primary_source_;}
  std::span<const TextureSetId> triangle_sets()const noexcept{return slots_?std::span<const TextureSetId>(*slots_):std::span<const TextureSetId>{};}
  const std::vector<TextureSetRecord>& additional()const noexcept{return additional_;}
  const TextureSetRecord& at(TextureSetId)const;
  void validate(const SculptObject&,bool deep=true,const TextureSetLimits & = {})const;
private:
  friend struct detail::TextureSetAccess;
  friend class detail::TextureSetArchive;
  friend void initialize_texture_sets(Scene&,uint64_t,const Mesh&,std::span<const TextureSetInitialField>,std::span<const TextureSetId>,const TextureSetLimits&);
  friend TextureSetId create_texture_set(Scene&,uint64_t,std::span<const uint64_t>,std::string,uint32_t,const TextureSetLimits&);
  friend bool assign_texture_set_faces(Scene&,uint64_t,TextureSetId,std::span<const uint64_t>,const TextureSetLimits&);
  friend bool select_texture_set(Scene&,uint64_t,TextureSetId);
  friend bool rename_texture_set(Scene&,uint64_t,TextureSetId,std::string,const TextureSetLimits&);
  friend bool edit_texture_set(Scene&,uint64_t,TextureSetId,const std::function<bool(PaintedMesh&,Material&)> &,const TextureSetLimits&);
  TextureSetCollection()=default;
  TextureSetCollection(const TextureSetCollection&)=default;
  std::shared_ptr<const TextureSetTopology> topology_;
  std::shared_ptr<const TextureSetFaceDomain> primary_;
  std::shared_ptr<const detail::TextureSetFieldSource> primary_source_;
  std::shared_ptr<const std::vector<TextureSetId>> slots_;
  std::vector<TextureSetRecord> additional_;
  TextureSetId active_=0;
  uint64_t next_=1;
};
struct TextureSetView {
  TextureSetId id=0;
  std::string name;
  std::shared_ptr<const PaintedMesh> painted;
  Material material;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  std::shared_ptr<const ObjectBakeRecipe> bake_recipe;
  std::shared_ptr<const TextureSetFaceDomain> domain;
  std::shared_ptr<const detail::TextureSetFieldSource> source;
};
enum class TextureSetAction { Create, AssignFaces, Select, Rename, Material, LayerOpacity, LayerOrder, AddLayer, DeleteLayer, NextLayer, ToggleLayer };
// Native/desktop/XR requests operate on persistent logical face IDs and one
// explicit material slot of the active mesh, never an object/group alias.
struct TextureSetEdit {
  TextureSetAction action=TextureSetAction::Select;
  TextureSetId set=0;
  std::vector<uint64_t> faces;
  std::string name;
  uint32_t resolution=0;
  dome::Material material;
  uint64_t layer=0;
  float value=0;
};
TextureSetView texture_set_view(const SculptObject&,TextureSetId);
TextureSetView active_texture_set(const SculptObject&);
// Detached operations: every allocation, membership/owner check and raster
// construction precedes replacing the caller Scene. A failed/cancelled call
// leaves all objects, geometry, layers, high-water IDs and sources untouched.
// Creates genuine plain current geometry on a new empty actor. Membership is
// minted before any paint raster, so separate material slots may share UV0
// without claiming original-source authority or duplicating the mesh.
void initialize_texture_sets(Scene&,uint64_t,const Mesh&,
    std::span<const TextureSetInitialField>,std::span<const TextureSetId>,const TextureSetLimits & = {});
TextureSetId create_texture_set(Scene&,uint64_t object,std::span<const uint64_t> faces,
                               std::string name,uint32_t resolution=0,
                               const TextureSetLimits & = {});
bool assign_texture_set_faces(Scene&,uint64_t object,TextureSetId,
                              std::span<const uint64_t> faces,const TextureSetLimits & = {});
bool select_texture_set(Scene&,uint64_t object,TextureSetId);
// Mutate a private COW draft, validate the actual full Scene/source context, and
// seal a second snapshot before replacing a record. No mutable draft can be
// retained through a const collection/history alias. Returning false discards
// the draft; unfinished strokes, failures and cancellation never publish it.
bool edit_texture_set(Scene&,uint64_t object,TextureSetId,
    const std::function<bool(PaintedMesh&,Material&)> &,const TextureSetLimits & = {});
bool rename_texture_set(Scene&,uint64_t,TextureSetId,std::string,const TextureSetLimits & = {});
} // namespace dome
