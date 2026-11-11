// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-gpu-arena-layout.hpp"
#include <iostream>
#include <string>
using namespace dome::render_detail;
namespace {
size_t checks=0;
void check(bool yes,const char *text){if(!yes)throw std::runtime_error(text);++checks;}
template<class F>bool rejects(F fn){try{fn();return false;}catch(const std::exception&){return true;}}
}
int main()try{
  SculptArenaSlots accepted(32);const auto a=*accepted.reserve(7),b=*accepted.reserve(11);
  check(a==SculptArenaSlots::Span{0,7}&&b==SculptArenaSlots::Span{7,11},"Actual disjoint first-fit reservations");
  auto abandoned=accepted;check(abandoned.reserve(12)->first==18,"Staging cannot overwrite occupied accepted ranges");
  check(!abandoned.reserve(3),"Fragmentation/reserve exhaustion is explicit");
  check(accepted.reserve(14)->first==18,"Abandoned plans do not alter accepted allocator");
  auto next=accepted;next.release(b);check(next.reserve(11)->first==7,"Only committed retired span becomes reusable");
  check(!accepted.reserve(1),"Retired staging span still occupied by prior accepted snapshot");
  next.release(a);next.release({7,11});next.release({18,14});
  check(next.reserve(32)==SculptArenaSlots::Span{0,32},"Adjacent holes coalesce without padding loss");
  check(rejects([&]{next.release({31,2});}),"Release rejects overflow/outside owner");
  SculptArenaSlots holes(10);auto all=*holes.reserve(10);holes.release({2,3});
  check(rejects([&]{holes.release({3,2});}),"Double-free/overlap rejects instead of aliasing accepted storage");
  check(sculpt_arena_capacity(1000)==1250,"Actual25percent reserve forecast");
  check(sculpt_arena_storage(1250,2000)==43000,"Position normal default mask and index reserve all charged");
  check(rejects([&]{sculpt_arena_peak(43000,43000,85999);}),"Old plus new peak refuses before allocation, even if final fits");
  sculpt_arena_peak(43000,43000,86000);check(true,"Inclusive exact old/new limit accepted");
  check(rejects([&]{sculpt_arena_multiply(size_t(-1),4);})&&rejects([&]{sculpt_arena_add(size_t(-1),1);}),"Prospective arithmetic refuses wraparound");
  // Exhaustive tiny allocator permutation/fragmentation comparison against an
  // independent per-unit bitmap. A transaction starts from an occupied base.
  for(size_t n=1;n<=16;++n)for(size_t bits=0;bits<(size_t(1)<<n);++bits){
    SculptArenaSlots slots(n);std::vector<bool> used(n,true);slots.reserve(n);
    for(size_t j=0;j<n;++j)if(bits&(size_t(1)<<j)){slots.release({j,1});used[j]=false;}
    for(size_t wanted=1;wanted<=n+1;++wanted){
      auto trial=slots;const auto got=trial.reserve(wanted);std::optional<size_t> expected;
      for(size_t j=0;j+wanted<=n;++j){bool free=true;for(size_t k=j;k<j+wanted;++k)free=free&&!used[k];if(free){expected=j;break;}}
      check(bool(got)==bool(expected)&&(!got||got->first==*expected),"Allocator differs from independent exhaustive bitmap oracle");
    }
  }
  std::cout<<"PASS "<<checks<<" actual arena allocation/transaction/budget controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
