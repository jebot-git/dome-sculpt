// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/native-authoring.hpp"
namespace dome::detail {
// Only internally derived original surfaces enter this capture. A portable
// compiler consumes these immutable originals and cannot manufacture their
// relationship to the native Scene by setting a flag on changed surfaces.
class NativeAuthoringCapture final {
  std::shared_ptr<const Scene> scene_;
  std::vector<ObjectSurface> originals_;
  std::vector<uint64_t> ids_;
  NativeAuthoringExport::Scope scope_;
  NativeAuthoringCapture(std::shared_ptr<const Scene>,std::vector<ObjectSurface>,
                         std::vector<uint64_t>,NativeAuthoringExport::Scope);
public:
  static std::shared_ptr<const NativeAuthoringCapture>
  create(const NativeAuthoringExport &,const std::function<bool()> &);
  static std::shared_ptr<const NativeAuthoringCapture>
  create(const NativeAuthoringExport &,const std::function<bool()> &,const SourceUvBakeOptions &);
  const Scene &scene() const { return *scene_; }
  const std::shared_ptr<const Scene> &scene_owner() const { return scene_; }
  const std::vector<ObjectSurface> &originals() const { return originals_; }
  const std::vector<uint64_t> &ids() const { return ids_; }
  NativeAuthoringExport::Scope scope() const { return scope_; }
};
}
