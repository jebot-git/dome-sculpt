// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
namespace dome::detail {
// Current recipient authority for one selected field. It owns detached paint
// metadata and exact source/group owners; it does not grant any normal/source
// interpretation or serialize/copy the Scene graph on the UI thread.
class TextureSetStrokeLease {
public:
  uint64_t object()const noexcept{return before_.id;}
  TextureSetId set()const noexcept{return set_;}
  const std::shared_ptr<const PaintedMesh>&paint()const noexcept{return paint_;}
  void authenticate_owner(const std::shared_ptr<const TextureSetStrokeLease>&)const;
  void authenticate_scene(const Scene&)const;
private:
  friend std::shared_ptr<const TextureSetStrokeLease> capture_texture_set_recipient(const Scene&,uint64_t,const TextureSetLimits&);
  TextureSetStrokeLease(SculptObject,std::vector<SceneGroup>);
  TextureSetStrokeLease(const TextureSetStrokeLease&)=delete;
  mutable std::weak_ptr<const TextureSetStrokeLease>self_;
  SculptObject before_;
  std::vector<SceneGroup>groups_;
  Volume::GeometryOwner volume_geometry_;
  uint64_t volume_revision_=0,volume_layer_=0;float voxel_=0,band_=0;
  TextureSetId set_=0;
  std::shared_ptr<const PaintedMesh>paint_;
};
std::shared_ptr<const TextureSetStrokeLease> capture_texture_set_recipient(
    const Scene&,uint64_t,const TextureSetLimits & = {});
std::shared_ptr<const TextureSetStrokeLease> capture_texture_set_stroke(
    const Scene&,uint64_t,const TextureSetLimits & = {});
// Clones one current recipient field/collection and preserves its other fields.
// The draft may be in-progress: snapshot() deliberately seals current COW values
// without leaking stroke scratch into the immutable preview/accepted Scene.
void adopt_texture_set_stroke(Scene&,const std::shared_ptr<const TextureSetStrokeLease>&,
    const PaintedMesh&,const TextureSetLimits & = {});
} // namespace dome::detail
