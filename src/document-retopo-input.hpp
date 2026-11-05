// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome {
// A model-lock-owned queue admission. The complete state lives in the sole
// retopology TU; neither Document nor the general Transaction gains a field.
class Document::RetopoInputScope {
  struct State;
  Document &d_;
  std::unique_ptr<State> state_;
public:
  explicit RetopoInputScope(Document &, bool active);
  RetopoInputScope(const RetopoInputScope &) = delete;
  ~RetopoInputScope() noexcept;
  void commit() noexcept;
  bool active() const noexcept { return bool(state_); }
  void retire_inputs() noexcept;
};
}
