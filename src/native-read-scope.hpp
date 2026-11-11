// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <stdexcept>
#include <utility>
namespace dome::detail {
// Propagate the existing synchronous file-reader cancellation authority into
// native decoding without changing public model definitions or storing a
// callback in a published Scene. All enclosing readers retain cancellation
// authority. Own the callback so a temporary callable cannot leave a dangling
// ambient pointer, and skip only an already executing callback during reentry.
class NativeReadScope {
  inline static thread_local NativeReadScope *current_=nullptr;
  NativeReadScope *previous_;
  std::function<bool()> cancelled_;
  bool invoking_=false;
public:
  explicit NativeReadScope(std::function<bool()> cancelled)
      :previous_(current_),cancelled_(std::move(cancelled)){current_=this;}
  ~NativeReadScope(){current_=previous_;}
  NativeReadScope(const NativeReadScope&)=delete;
  NativeReadScope&operator=(const NativeReadScope&)=delete;
  static void check(){
    for(auto *scope=current_;scope;scope=scope->previous_) {
      if(scope->invoking_||!scope->cancelled_)continue;
      scope->invoking_=true;
      try {
        const bool cancelled=scope->cancelled_();scope->invoking_=false;
        if(cancelled)throw std::runtime_error("Project loading cancelled");
      }catch(...){scope->invoking_=false;throw;}
    }
  }
};
}
