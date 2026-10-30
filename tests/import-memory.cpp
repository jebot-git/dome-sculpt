// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/import-authoring.hpp"
#include "dome/import-source.hpp"
#include "dome/material-bindings.hpp"
#include "import-skin-fixtures.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <unistd.h>
using namespace dome;
namespace {
thread_local bool armed = false, hit = false;
thread_local size_t calls = 0, fail_at = SIZE_MAX;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Fixture read failed");
  return {std::istreambuf_iterator<char>(f), {}};
}
Scene seed() {
  Mesh m;
  m.positions = {
      {-.2f, -.2f, 0}, {.2f, -.2f, 0}, {-.2f, .2f, 0}, {.2f, .2f, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{.1f, .1f}, {.9f, .1f}, {.1f, .9f}, {.9f, .9f}};
  m.indices = {0, 1, 2, 1, 3, 2};
  Scene s;
  s.active().mesh_active = true;
  s.active().name = "Existing Žluťoučký source";
  s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  PaintSettings p;
  p.color = {.1f, .7f, .2f, .8f};
  Brush b;
  b.radius = .4f;
  b.normal = {0, 0, 1};
  s.active().painted->begin_stroke();
  s.active().painted->stamp(b, p);
  s.active().painted->end_stroke();
  s.active().group = s.add_group("Retained hierarchy");
  return s;
}
} // namespace
void *operator new(size_t n) {
  if (armed && ++calls >= fail_at) {
    hit = true;
    throw std::bad_alloc();
  }
  if (auto *p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
int main(int argc, char **argv) try {
  check(argc == 2, "Choose flat, textured or skinned import fixture");
  const std::string mode = argv[1];
  check(mode == "flat" || mode == "textured" || mode == "skinned",
        "Unknown import fixture");
  auto dir = std::filesystem::temp_directory_path() /
             ("dome-import-memory-" + mode + "-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir);
  struct Remove {
    std::filesystem::path p;
    ~Remove() {
      std::error_code ec;
      std::filesystem::remove_all(p, ec);
    }
  } remove{dir};
  auto path = dir / (mode + ".glb");
  auto f = mode == "skinned" ? import_fixture::skinned(true)
                             : import_fixture::Fixture{};
  if (mode == "flat") {
    auto &primitive = f.j["meshes"][0]["primitives"][0];
    primitive["attributes"].erase("NORMAL");
    primitive["targets"][0].erase("NORMAL");
    while (f.bin.size() % 4)
      f.bin.push_back(0);
    int p = f.values({-.18f, -.18f, 0, .18f, -.18f, .03f, .18f, .18f, .07f,
                      -.18f, .18f, -.05f},
                     3, "VEC3");
    f.j["accessors"][p]["min"] = {-.18f, -.18f, -.05f};
    f.j["accessors"][p]["max"] = {.18f, .18f, .07f};
    primitive["attributes"]["POSITION"] = p;
    f.j["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
    f.j["images"] = import_fixture::Json::array();
    f.j["textures"] = import_fixture::Json::array();
    f.j["samplers"] = import_fixture::Json::array();
  }
  std::filesystem::path resource;
  if (mode == "textured") {
    path.replace_extension(".gltf");
    resource = dir / "texture-source.bin";
    f.j["buffers"] = {
        {{"byteLength", f.bin.size()}, {"uri", resource.filename().string()}}};
    {
      std::ofstream out(resource, std::ios::binary);
      out.write(reinterpret_cast<const char *>(f.bin.data()), f.bin.size());
      check(bool(out), "Fixture external buffer write failed");
    }
    {
      std::ofstream out(path, std::ios::binary);
      out << f.j.dump();
      check(bool(out), "Fixture original glTF write failed");
    }
  } else
    f.save(path);
  const auto file_before = bytes(path);
  const auto resource_before =
      resource.empty() ? std::vector<uint8_t>{} : bytes(resource);
  auto files_exact = [&] {
    return bytes(path) == file_before &&
           (resource.empty() || bytes(resource) == resource_before);
  };
  Scene input = seed();
  const auto before = input.encode();
  const auto paint = input.active().painted;
  const auto volume = input.active().volume;
  const auto owner = volume->geometry_owner();
  const auto revision = paint->revision();
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  auto run = [&](ImportAuthoringOptions o) {
    return prepare_authoring_import(input, path, std::move(o));
  };
  auto warm = run(options);
  check(!warm.objects.empty(), "Fixture must actually import");
  for (auto id : warm.objects) {
    const auto &o = warm.scene.object(id);
    if (mode == "flat")
      check(o.appearance &&
                !o.appearance->asset->primitives[0].normals_authored &&
                o.painted->mesh().positions.size() == 6,
            "Flat fixture must use actual generated absent-NORMAL corner "
            "authority");
    if (mode == "textured")
      check(o.appearance && !o.appearance->decoded_images->empty() &&
                o.painted->source_maps() && o.appearance->source_receipt &&
                o.appearance->source_receipt->format ==
                    ImportSourceFormat::Gltf &&
                *o.appearance->source_receipt->root.bytes == file_before &&
                o.appearance->source_receipt->resources.size() == 1 &&
                o.appearance->source_receipt->resources[0].name ==
                    resource.filename().string() &&
                *o.appearance->source_receipt->resources[0].bytes ==
                    resource_before,
            "Texture fixture must decode and retain actual PNG material maps");
    if (mode == "skinned")
      check(o.deformation && o.deformation->skin && o.deformation->morphs &&
                o.deformation->skeleton->inverse_bind_override,
            "Skin fixture must retain actual authored IBM and morph data");
  }
  const auto expected = warm.scene.encode();
  warm = {};
  calls = 0;
  fail_at = SIZE_MAX;
  armed = true;
  {
    auto measured = run(options);
  }
  armed = false;
  const auto count = calls;
  check(count > 100, "Whole helper must exercise real allocations");
  for (size_t site = 1; site <= count; ++site) {
    calls = 0;
    fail_at = site;
    hit = false;
    armed = true;
    bool refused = false;
    try {
      auto result = run(options);
    } catch (const std::bad_alloc &) {
      refused = true;
    }
    armed = false;
    check(hit && refused, "Every measured whole import allocation boundary "
                          "must refuse under persistent denial");
    check(input.encode() == before && input.active().painted == paint &&
              input.active().volume == volume &&
              volume->geometry_owner() == owner &&
              paint->revision() == revision,
          "Failed direct import changed original Scene, painted pixels, COW "
          "owner or revision");
    check(files_exact(), "Failed direct import changed original file bytes");
    auto retry = run(options);
    check(retry.scene.encode() == expected,
          "Retry after persistent import OOM differs from complete expected "
          "result");
  }
  size_t observations = 0;
  options.limits.cancelled = [&] {
    ++observations;
    return false;
  };
  {
    auto measured = run(options);
  }
  options.limits.cancelled = {};
  const auto cancellation_count = observations;
  for (size_t site = 1; site <= cancellation_count; ++site) {
    observations = 0;
    auto cancelled_options = options;
    cancelled_options.limits.cancelled = [&] { return ++observations >= site; };
    bool refused = false;
    try {
      auto result = run(cancelled_options);
    } catch (const std::exception &) {
      refused = true;
    }
    check(refused && observations >= site && input.encode() == before &&
              files_exact(),
          "Cancellation at a whole import callback must refuse without partial "
          "source/file mutation");
  }
  check(run(options).scene.encode() == expected,
        "Final retry after whole helper cancellation must be exact");
  std::cout
      << mode << " direct prepare_authoring_import: " << count
      << " persistent allocation boundaries +" << cancellation_count
      << " cancellation boundaries passed; source/file/owner/retry exact\n";
  return 0;
} catch (const std::exception &e) {
  armed = false;
  std::cerr << "FAIL " << e.what() << " calls=" << calls << " site=" << fail_at
            << '\n';
  return 1;
}
