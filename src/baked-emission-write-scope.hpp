// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace dome {class PaintImageArchive;class Scene;}
namespace dome::detail {
// Observe completion of the actual owning Scene writer, including retained
// source graphs. Opaque material JSON never grants a FLOAT feature here.
class SceneEmissionFeatureProbe final {
  friend class BakedEmissionWriteScope;
  inline static thread_local SceneEmissionFeatureProbe *current_=nullptr;
  SceneEmissionFeatureProbe *previous_;
  const dome::Scene *scene_;
  bool emitted_=false,completed_=false;
public:
  explicit SceneEmissionFeatureProbe(const dome::Scene &scene):previous_(current_),scene_(&scene){current_=this;}
  ~SceneEmissionFeatureProbe(){current_=previous_;}
  SceneEmissionFeatureProbe(const SceneEmissionFeatureProbe&)=delete;
  bool float_emitted()const noexcept{return completed_&&emitted_;}
};
// Observe actual serialization into ONE Scene's real image archive. This
// is a writer implementation detail, never inferred from emitted opaque JSON.
// Enter before validation to isolate nested encodes; bind only when the owning
// serialization archive exists. Discarded encode/hash calls with local/null
// archives cannot manufacture a feature. Nothing survives exception unwind.
class BakedEmissionWriteScope final {
  inline static thread_local BakedEmissionWriteScope *current_=nullptr;
  BakedEmissionWriteScope *previous_;
  const PaintImageArchive *archive_=nullptr;
  bool emitted_=false,source_bake_=false;
public:
  BakedEmissionWriteScope():previous_(current_){current_=this;}
  ~BakedEmissionWriteScope(){current_=previous_;}
  BakedEmissionWriteScope(const BakedEmissionWriteScope&)=delete;
  void begin(const PaintImageArchive &archive)noexcept{archive_=&archive;emitted_=source_bake_=false;}
  bool emitted()const noexcept{return emitted_;}
  void publish(const dome::Scene &scene)const noexcept{
    if(auto *probe=SceneEmissionFeatureProbe::current_;probe&&probe->scene_==&scene){probe->emitted_=emitted_;probe->completed_=true;}
  }
  bool source_bake_emitted()const noexcept{return source_bake_;}
  static void source_bake_emitted(const PaintImageArchive *archive)noexcept{
    if(current_&&archive&&current_->archive_==archive)current_->source_bake_=true;
  }
  static void field_emitted(const PaintImageArchive *archive)noexcept{
    if(current_&&archive&&current_->archive_==archive)current_->emitted_=true;
  }
};
} // namespace dome::detail
