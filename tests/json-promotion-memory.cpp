// SPDX-License-Identifier: GPL-3.0-or-later
// Preserve JSON's basic valid-state guarantee while null becomes a container.
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>
#ifdef NDEBUG
#error This mutation-unwind regression requires JSON invariants enabled.
#endif
namespace {
thread_local bool denying = false, hit = false;
thread_local size_t allocations = 0, boundary = SIZE_MAX;
using Json = nlohmann::json;
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void allocation_check() {
  if (denying && ++allocations >= boundary) {
    hit = true;
    throw std::bad_alloc();
  }
}
struct Inputs {
  std::string key = std::string(80, 'k');
  Json payload = std::string(128, 'v');
  Json source = {{"first", 1}, {"second", "value"}};
  Json::object_t::value_type pair{key, payload};
};
void mutate(Json &value, Inputs &input, unsigned mode) {
  switch (mode) {
  case 0:
    value[size_t(3)] = 7;
    break;
  case 1:
    value[input.key] = 7;
    break;
  case 2:
    value[std::string_view(input.key)] = 7;
    break;
  case 3:
    value.push_back(static_cast<const Json &>(input.payload));
    break;
  case 4:
    value.push_back(std::move(input.payload));
    break;
  case 5:
    value.push_back(input.pair);
    break;
  case 6:
    value.emplace_back(12);
    break;
  case 7:
    value.emplace(input.key, 42);
    break;
  case 8:
    value.update(input.source.begin(), input.source.end());
    break;
  case 9:
    value["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"] =
        1;
    break;
  default:
    value["nested"][size_t(1)]["name"] = input.payload;
    break;
  }
}
Json initial(unsigned mode) {
  return mode < 9 ? Json() : Json{{"anchor", {{"exact", {1, 2, 3}}}}};
}
bool valid(const Json &value) {
  if (value.is_object()) {
    auto object = value.get_ptr<const Json::object_t *>();
    if (!object)
      return false;
    for (const auto &[key, child] : *object) {
      (void)key;
      if (!valid(child))
        return false;
    }
  } else if (value.is_array()) {
    auto array = value.get_ptr<const Json::array_t *>();
    if (!array)
      return false;
    for (const auto &child : *array)
      if (!valid(child))
        return false;
  } else if (value.is_string())
    return value.get_ptr<const Json::string_t *>() != nullptr;
  else if (value.is_binary())
    return value.get_ptr<const Json::binary_t *>() != nullptr;
  return true;
}
} // namespace
[[gnu::noinline]] void *operator new(size_t size) {
  allocation_check();
  if (auto value = std::malloc(size ? size : 1))
    return value;
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t size) {
  return ::operator new(size);
}
[[gnu::noinline]] void operator delete(void *value) noexcept {
  std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value) noexcept {
  std::free(value);
}
[[gnu::noinline]] void operator delete(void *value, size_t) noexcept {
  std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value, size_t) noexcept {
  std::free(value);
}
int main(int argc, char **argv) try {
  const unsigned first = argc == 2 ? std::stoul(argv[1]) : 0;
  const unsigned last = argc == 2 ? first + 1 : 11;
  require(first < 11 && argc <= 2,
          "Optional diagnostic case must be 0 through 10");
  size_t total = 0;
  for (unsigned mode = first; mode < last; ++mode) {
    auto value = initial(mode);
    Inputs input;
    allocations = 0;
    boundary = SIZE_MAX;
    denying = true;
    mutate(value, input, mode);
    denying = false;
    const auto measured = allocations;
    const Json expected = value;
    require(measured > 0, "Every actual mutator exposes allocation boundaries");
    for (size_t at = 1; at <= measured; ++at) {
      value = initial(mode);
      Inputs attempt;
      allocations = 0;
      boundary = at;
      hit = false;
      denying = true;
      bool caught = false;
      try {
        mutate(value, attempt, mode);
      } catch (const std::bad_alloc &) {
        caught = true;
      }
      denying = false;
      require(hit && caught, "Every measured persistent denial is actually "
                             "reached and propagates");
      // Later insertion can leave valid intermediate containers; no blanket
      // strong-transaction guarantee is claimed for JSON mutation itself.
      require(valid(value), "Refusal cannot expose a typed null heap owner");
      require(Json::from_cbor(Json::to_cbor(value)) == value,
              "Every refused mutation retains a serializable valid JSON state");
      if (mode >= 9)
        require(value.at("anchor") == initial(mode).at("anchor"),
                "Nested null promotion preserves all pre-existing values");
      Inputs retry;
      mutate(value, retry, mode);
      require(
          value == expected,
          "Retry after every real refusal reaches the exact intended value");
    }
    total += measured;
    std::cout
        << "PASS null promotion " << mode << ": " << measured
        << " persistent allocation denials, valid state and exact retry\n";
  }
  std::cout << total << " assertion-enabled null-promotion boundaries passed\n";
  return 0;
} catch (const std::exception &error) {
  denying = false;
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
