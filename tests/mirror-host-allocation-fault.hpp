// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <new>

namespace mirror_failure {
thread_local long remaining=-1;
thread_local bool count=false;
thread_local size_t calls=0;

void check_allocation() {
  if(count)++calls;
  if(remaining>=0&&remaining--==0){remaining=0;throw std::bad_alloc();}
}
}

// Wrap the executable's C++ allocation calls. Closed GPU driver libraries
// have their own allocation and failure contracts.
extern "C" void *__real__Znwm(size_t);
extern "C" void *__real__Znam(size_t);
extern "C" void *__wrap__Znwm(size_t n) {
  mirror_failure::check_allocation();
  return __real__Znwm(n);
}
extern "C" void *__wrap__Znam(size_t n) {
  mirror_failure::check_allocation();
  return __real__Znam(n);
}
