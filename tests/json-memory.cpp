// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp> // Must agree with the full pinned header/ABI.
using nlohmann::json;
namespace {
bool denied = false;
size_t attempts = 0, blocks = 0, checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << '\n';
}
template <class T> struct CountingAllocator {
  using value_type = T;
  CountingAllocator() noexcept = default;
  template <class U> CountingAllocator(const CountingAllocator<U> &) noexcept {}
  T *allocate(size_t n) {
    auto *p = std::allocator<T>{}.allocate(n);
    ++blocks;
    return p;
  }
  void deallocate(T *p, size_t n) noexcept {
    --blocks;
    std::allocator<T>{}.deallocate(p, n);
  }
  template <class U>
  bool operator==(const CountingAllocator<U> &) const noexcept {
    return true;
  }
};
using TrackedString =
    std::basic_string<char, std::char_traits<char>, CountingAllocator<char>>;
using Tracked =
    nlohmann::basic_json<std::map, std::vector, TrackedString, bool, int64_t,
                         uint64_t, double, CountingAllocator,
                         nlohmann::adl_serializer,
                         std::vector<uint8_t, CountingAllocator<uint8_t>>>;
template <class J> void destroy(J *root, const char *message) {
  attempts = 0;
  denied = true;
  delete root;
  denied = false;
  check(attempts == 0, message);
}
template <class J> void deep(const char *message) {
  auto *root = new J(J::array());
  auto *node = root;
  for (size_t i = 0; i < 100000; ++i) {
    if (i % 2 == 0) {
      node->push_back(J::object());
      node = &node->back();
    } else {
      (*node)["child"] = J::array();
      node = &(*node)["child"];
    }
  }
  destroy(root, message);
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
  static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 &&
                NLOHMANN_JSON_VERSION_MINOR == 12 &&
                NLOHMANN_JSON_VERSION_PATCH == 0);
  check(json::meta()["version"]["string"].get<std::string>() == "3.12.0",
        "Pinned full and forward headers preserve JSON3.12 API/version");
  auto *values = new json(json::array(
      {nullptr, true, false, -17, UINT64_MAX, 1.25, std::string(512, 'x'),
       json::binary({0, 1, 255}, 29), json(json::value_t::discarded),
       json::array(), json::object()}));
  check((*values)[7].is_binary() && (*values)[7].get_binary().subtype() == 29 &&
            (*values)[8].is_discarded(),
        "Binary subtype and discarded/scalar authority retained before "
        "destruction");
  destroy(values, "All primitive, binary, discarded and empty container "
                  "destruction makes zero allocation attempts");
  auto *wide = new json(json::object());
  for (size_t i = 0; i < 20000; ++i)
    (*wide)[std::string(40, 'k') + std::to_string(i)] =
        json::array({std::string(128, 'v'), int(i)});
  destroy(wide, "Wide object keys and nested leaf containers destroyed under "
                "permanent allocation denial");
  deep<json>("100000 alternating array/map levels destroyed without allocation "
             "or recursive overflow");
  auto *ordered = new nlohmann::ordered_json{{"z", 1}, {"a", 2}, {"middle", 3}};
  check(ordered->dump() == "{\"z\":1,\"a\":2,\"middle\":3}",
        "Ordered-object insertion order is unchanged");
  destroy(ordered,
          "Ordered object key/value cleanup makes zero allocation attempts");
  deep<nlohmann::ordered_json>("100000 ordered-map/array levels destroyed "
                               "without allocation or recursive overflow");
  check(blocks == 0, "Tracking allocator starts with no live storage");
  auto *tracked = new Tracked(Tracked::object());
  for (int i = 0; i < 256; ++i) {
    TrackedString key(64, 'k');
    auto suffix = std::to_string(i);
    key += TrackedString(suffix.begin(), suffix.end());
    (*tracked)[key] =
        Tracked::array({TrackedString(128, 'v'), Tracked::binary({1, 2, 3}),
                        Tracked::object()});
  }
  check(blocks > 1000, "Actual heap-backed keys, strings, binary and container "
                       "allocations were observed");
  destroy(tracked, "Custom allocator object cleanup succeeds with every new "
                   "allocation denied");
  check(blocks == 0, "All object keys and value/container allocator blocks are "
                     "deallocated exactly once");
  json canonical = {{"a", {1, true, nullptr}}, {"z", "é"}};
  const std::vector<uint8_t> cbor = {0xa2, 0x61, 0x61, 0x83, 0x01, 0xf5,
                                     0xf6, 0x61, 0x7a, 0x62, 0xc3, 0xa9};
  check(json::to_cbor(canonical) == cbor && json::from_cbor(cbor) == canonical,
        "CBOR encoding and reading match fixed independent legacy bytes");
  check(json::parse("{\"z\":\"é\",\"a\":[1,true,null]}") == canonical,
        "Legacy UTF-8 object/array parsing preserves values and canonical key "
        "order");
  std::cout << checks << " pinned JSON memory/format checks passed\n";
  return 0;
} catch (const std::exception &e) {
  denied = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
