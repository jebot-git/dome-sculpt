// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr-fixture-clock.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
int main()try {
  constexpr std::array<std::pair<uint64_t,XrTime>,12> boundaries{{
    {182,4'276'000'000LL},{183,4'294'000'000LL},{184,4'312'000'000LL},
    {185,4'330'000'000LL},{186,4'348'000'000LL},{187,4'366'000'000LL},
    {188,4'384'000'000LL},{189,4'402'000'000LL},{190,4'420'000'000LL},
    {274,5'932'000'000LL},{288,6'184'000'000LL},{318,6'724'000'000LL}}};
  size_t checks=0;
  for(const auto [frame,expected]:boundaries){
    const auto actual=dome::test::xr_fixture_time(frame);
    if(actual!=expected)throw std::runtime_error("OpenXR fixture timestamp changed at its explicit boundary");
    ++checks;std::cout<<"CLOCK frame="<<frame<<" nanoseconds="<<actual<<'\n';
  }
  auto previous=dome::test::xr_fixture_time(0);
  for(uint64_t frame=1;frame<=6000;++frame){
    const auto current=dome::test::xr_fixture_time(frame);
    if(current-previous!=18'000'000)throw std::runtime_error("OpenXR fixture timestamp went backwards or changed its original18ms period");
    previous=current;++checks;
  }
  const auto old184=uint32_t(1'000'000'000u+uint32_t(184)*18'000'000u);
  if(old184!=17'032'704u||XrTime(old184)>=dome::test::xr_fixture_time(183))
    throw std::runtime_error("Regression no longer witnesses the original unsigned32 wrap");
  ++checks;
  if(dome::test::xr_fixture_time(std::numeric_limits<uint32_t>::max())<=0)
    throw std::runtime_error("Complete unsigned32 physical-frame range must fit signed OpenXR time");
  ++checks;std::cout<<"XR FIXTURE CLOCK CHECKS "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
