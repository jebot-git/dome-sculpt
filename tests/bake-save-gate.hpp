// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/bake.hpp"
#include "dome/document.hpp"
#include <condition_variable>
#include <mutex>

namespace bake_save_gate {

// Hold the real worker at Bake entry until Save depends on it. A Save
// submitted after an already failed Bake legitimately saves the prior scene.
class BakeEntry {
public:
  void arm() {
    std::lock_guard lock(mutex_);
    armed_=true;
    entered_=false;
    released_=false;
  }
  void pause() {
    std::unique_lock lock(mutex_);
    if(!armed_)return;
    armed_=false;
    entered_=true;
    changed_.notify_all();
    changed_.wait(lock,[this]{return released_;});
  }
  void wait() {
    std::unique_lock lock(mutex_);
    if(!changed_.wait_for(lock,std::chrono::seconds(30),[this]{return entered_;}))
      throw std::runtime_error("Document worker did not enter the real Bake");
  }
  void release() {
    std::lock_guard lock(mutex_);
    armed_=false;
    released_=true;
    changed_.notify_all();
  }
private:
  std::mutex mutex_;
  std::condition_variable changed_;
  bool armed_=false,entered_=false,released_=false;
};

inline BakeEntry entry;

inline std::future<void> queue(dome::Document &document,uint64_t source,
    const dome::ObjectBakeSettings &settings,const std::filesystem::path &path) {
  entry.arm();
  struct Release { ~Release(){entry.release();} } release;
  document.bake_from_object(source,settings);
  entry.wait();
  return document.save_async(path);
}
} // namespace bake_save_gate

extern "C" dome::ObjectBakeReport __real__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE(
    dome::Scene &,uint64_t,dome::ObjectBakeSettings,const std::function<bool()> &,
    const std::function<void(float)> &);
extern "C" dome::ObjectBakeReport __wrap__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE(
    dome::Scene &scene,uint64_t source,dome::ObjectBakeSettings settings,
    const std::function<bool()> &cancelled,const std::function<void(float)> &progress) {
  bake_save_gate::entry.pause();
  return __real__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE(
      scene,source,settings,cancelled,progress);
}
