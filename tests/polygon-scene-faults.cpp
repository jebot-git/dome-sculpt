// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "dome/polygon-scene.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
thread_local bool armed = false, hit = false;
thread_local size_t calls = 0, failure = 0;
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace
void *operator new(size_t size) {
  if (armed && ++calls == failure) {
    failure = 0;
    hit = true;
    throw std::bad_alloc();
  }
  if (auto p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
int main(int argc, char **argv) try {
  bool source_material = false, encode = false;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--source")
      source_material = true;
    else if (arg == "--encode")
      encode = true;
    else
      throw std::invalid_argument("Unknown fault fixture option");
  }
  // --source and --encode preserve the actual failing dependency reproducers.
  // The default CTest fixture qualifies the complete plain retained-DAG
  // decoder.

  auto original = std::make_shared<PolygonAuthority>();
  original->topology = PolygonMesh::grid();
  auto paint =
      std::make_shared<PaintedMesh>(original->topology.triangulate().mesh, 64);
  auto source = std::make_shared<PolygonOrigin>();
  source->volume = std::make_shared<const Volume>();
  source->paint = paint->snapshot();
  if (source_material) {
    auto maps = std::make_shared<PaintSourceMaps>();
    maps->color.image =
        std::make_shared<const Image>(Image{1, 1, {31, 63, 127, 255}});
    source->paint =
        PaintedMesh::from_maps(original->topology.triangulate().mesh, maps, 64);
    auto uv_origin = std::make_shared<SourceUvOrigin>();
    uv_origin->paint = source->paint;
    auto binding = std::make_shared<SourceMaterialBindings>();
    binding->has_mtoon = false;
    binding->sampling_reparameterized = true;
    binding->uv_origin = uv_origin;
    source->material_bindings = binding;
  }
  source->mesh_active = true;
  source->name = "Before reconversion";
  source->polygon = original;
  auto latest = std::make_shared<PolygonAuthority>(*original);
  latest->origin = source;
  Scene scene;
  scene.active().mesh_active = true;
  scene.active().painted = paint;
  scene.active().polygon = latest;
  const auto encoded = scene.encode();
  const auto before = nlohmann::json::to_cbor(encoded);
  size_t total = 0;
  {
    auto run = [&] {
      if (encode)
        (void)scene.encode();
      else
        (void)Scene::decode(encoded);
    };
    calls = failure = 0;
    armed = true;
    run();
    armed = false;
    const auto count = calls;
    require(count > 50, "Expected real source graph allocation boundaries");
    for (size_t boundary = 1; boundary <= count; ++boundary) {
      if (const char *selected =
              std::getenv("DOME_POLYGON_ALLOCATION_BOUNDARY"))
        if (boundary != std::strtoul(selected, nullptr, 10))
          continue;
      std::cerr << (encode ? "encode" : "decode") << " boundary " << boundary
                << "/" << count << "\n";
      failure = boundary;
      calls = 0;
      hit = false;
      armed = true;
      bool refused = false;
      try {
        run();
      } catch (const std::bad_alloc &) {
        refused = true;
      }
      armed = false;
      require(hit && refused,
              "Every measured source graph allocation must refuse atomically");
      require(
          nlohmann::json::to_cbor(encoded) == before &&
              nlohmann::json::to_cbor(scene.encode()) == before,
          "Fault changed the serialized source or live immutable authority");
      ++total;
    }
    std::cout << (encode ? "encode" : "decode") << " " << total
              << " exact allocation failures passed (measured " << count
              << ")\n";
  }
  require(Scene::decode(encoded).encode() == encoded,
          "Retry must restore exact DAG after every allocation failure");
  std::cout << total
            << " Scene12 allocation boundaries passed; final retry exact\n";
  return 0;
} catch (const std::exception &e) {
  armed = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
