// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace dome {class PaintImageArchive;}
namespace dome::detail {
// Observe actual serialization into ONE Scene's real image archive. This
// is a writer implementation detail, never inferred from emitted opaque JSON.
// Enter before validation to isolate nested encodes; bind only when the owning
// serialization archive exists. Discarded encode/hash calls with local/null
// archives cannot manufacture a feature. Nothing survives exception unwind.
class BakedEmissionWriteScope final {
  inline static thread_local BakedEmissionWriteScope *current_=nullptr;
  BakedEmissionWriteScope *previous_;
  const PaintImageArchive *archive_=nullptr;
  bool emitted_=false;
public:
  BakedEmissionWriteScope():previous_(current_){current_=this;}
  ~BakedEmissionWriteScope(){current_=previous_;}
  BakedEmissionWriteScope(const BakedEmissionWriteScope&)=delete;
  void begin(const PaintImageArchive &archive)noexcept{archive_=&archive;emitted_=false;}
  bool emitted()const noexcept{return emitted_;}
  static void field_emitted(const PaintImageArchive *archive)noexcept{
    if(current_&&archive&&current_->archive_==archive)current_->emitted_=true;
  }
};
} // namespace dome::detail
