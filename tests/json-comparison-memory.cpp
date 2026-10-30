// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string_view>
#include <type_traits>
using nlohmann::json;
namespace {
bool denied = false;
size_t attempts = 0, checks = 0, propagated = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << '\n';
}
template <class F> void allocation_case(F f, const char *m) {
  attempts = 0;
  denied = true;
  bool caught = false;
  try {
    f();
  } catch (const std::bad_alloc &) {
    caught = true;
  }
  denied = false;
  check(caught && attempts > 0, m);
  ++propagated;
}
template <class F> void numeric_case(F f, const char *m) {
  attempts = 0;
  denied = true;
  bool result = f();
  denied = false;
  check(result && attempts == 0, m);
}
} // namespace
void *operator new(size_t n) {
  if (denied) {
    ++attempts;
    throw std::bad_alloc();
  }
  if (auto *p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
int main() try {
  static_assert(!noexcept(std::declval<const json &>() == "literal"));
  static_assert(!noexcept("literal" == std::declval<const json &>()));
  static_assert(!noexcept(std::declval<const json &>() < "literal"));
  static_assert(!noexcept("literal" < std::declval<const json &>()));
  static_assert(noexcept(std::declval<const json &>() == 7));
  static_assert(noexcept(7 < std::declval<const json &>()));
  json value = "literal";
  const auto before = value;
  const char *pointer = "literal";
  const std::string owned = "literal";
  const std::string_view view = "literal";
  check(value == pointer && pointer == value && !(value != pointer) &&
            value < "z" && "a" < value && value > "a" && "z" > value,
        "String comparison values and operand ordering remain unchanged");
  allocation_case(
      [&] { (void)(value == pointer); },
      "JSON == C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer == value); },
      "C string == JSON propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(value != pointer); },
      "JSON != C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer != value); },
      "C string != JSON propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(value < pointer); },
      "JSON < C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer < value); },
      "C string < JSON propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(value <= pointer); },
      "JSON <= C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer <= value); },
      "C string <= JSON propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(value > pointer); },
      "JSON > C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer > value); },
      "C string > JSON propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(value >= pointer); },
      "JSON >= C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer >= value); },
      "C string >= JSON propagates bad_alloc under persistent denial");
  allocation_case([&] { (void)(value == owned); },
                  "Existing owning-string conversion remains catchable");
  allocation_case([&] { (void)(value == view); },
                  "Existing string-view conversion remains catchable");
#if __cplusplus >= 202002L
  allocation_case(
      [&] { (void)(value <=> pointer); },
      "JSON <=> C string propagates bad_alloc under persistent denial");
  allocation_case(
      [&] { (void)(pointer <=> value); },
      "C string <=> JSON propagates bad_alloc under persistent denial");
#endif
  check(value == before && owned == "literal" && view == "literal",
        "Every failed comparison leaves both operands unchanged");
  json number = 7;
  numeric_case(
      [&] { return number == 7 && 7 == number && number != 8 && 8 != number; },
      "Numeric equality/inequality still requires no allocation");
  numeric_case(
      [&] { return number < 8 && 6 < number && number <= 7 && 7 <= number; },
      "Numeric lower ordering still requires no allocation");
  numeric_case(
      [&] { return number > 6 && 8 > number && number >= 7 && 7 >= number; },
      "Numeric upper ordering still requires no allocation");
  json boolean = true, empty = nullptr;
  numeric_case(
      [&] {
        return boolean == true && true == boolean && empty == nullptr &&
               nullptr == empty;
      },
      "Boolean/null scalar exception guarantees remain nonallocating");
  std::cout << checks << " scalar comparison checks; " << propagated
            << " persistent-denial exceptions caught\n";
  return 0;
} catch (const std::exception &e) {
  denied = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
