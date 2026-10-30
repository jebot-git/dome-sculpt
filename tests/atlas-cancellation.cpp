// SPDX-License-Identifier: GPL-3.0-or-later
#include "atlas-detail.hpp"
#include "xatlas.h"
#include "import-authoring-fixtures.hpp"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <unistd.h>
using namespace dome;
namespace {
std::atomic<xatlas::ProgressFunc> forward{nullptr};
thread_local int category = -1, percentage = -1;
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << '\n';
}
template <class T> bool same(const std::vector<T> &a, const std::vector<T> &b) {
  return a.size() == b.size() &&
         (a.empty() ||
          std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}
bool same(const Mesh &a, const Mesh &b) {
  return same(a.positions, b.positions) && same(a.normals, b.normals) &&
         same(a.uv, b.uv) && same(a.indices, b.indices) &&
         same(a.uv_pins, b.uv_pins) && same(a.uv_seams, b.uv_seams);
}
bool trampoline(xatlas::ProgressCategory c, int p, void *data) {
  auto old_category = category, old_percentage = percentage;
  category = int(c);
  percentage = p;
  auto result = forward.load()(c, p, data);
  category = old_category;
  percentage = old_percentage;
  return result;
}
Mesh source() {
  Mesh mesh;
  // Keep this atlas fixture independent of the separate polygon editor.
  Mesh cube;
  const std::array<std::array<Vec3,4>,6> faces{{
      {{{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}}},
      {{{.5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f}}},
      {{{-.5f,-.5f,-.5f},{-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f}}},
      {{{.5f,-.5f,.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f}}},
      {{{-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f}}},
      {{{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,-.5f,.5f},{-.5f,-.5f,.5f}}}}};
  const std::array<Vec3,6> normals{{{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}}};
  const std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
  for(size_t face=0;face<faces.size();++face) {
    const auto base=uint32_t(cube.positions.size());
    for(size_t k=0;k<4;++k){cube.positions.push_back(faces[face][k]);cube.normals.push_back(normals[face]);cube.uv.push_back(uv[k]);}
    for(auto i:{0u,1u,2u,0u,2u,3u})cube.indices.push_back(base+i);
  }
  for (unsigned object = 0; object < 24; ++object) {
    auto base = uint32_t(mesh.positions.size());
    for (auto p : cube.positions)
      mesh.positions.push_back(
          p + Vec3{float(object % 6) * 2, float(object / 6) * 2, 0});
    mesh.normals.insert(mesh.normals.end(), cube.normals.begin(),
                        cube.normals.end());
    mesh.uv.insert(mesh.uv.end(), cube.uv.begin(), cube.uv.end());
    for (auto i : cube.indices)
      mesh.indices.push_back(base + i);
  }
  mesh.uv_pins.assign(mesh.positions.size(), 1);
  mesh.uv_seams.assign(mesh.indices.size(), 1);
  mesh.validate();
  return mesh;
}
bool cancelled(Mesh &mesh, const std::function<bool()> &predicate) {
  try {
    unwrap(mesh, 512, 2, predicate);
    return false;
  } catch (const std::runtime_error &e) {
    return std::string(e.what()).find("cancel") != std::string::npos;
  }
}
} // namespace
// Observe actual vendor categories without production hooks or fabricated work.
// The opaque user data remains unchanged and belongs to the production context.
void real_progress(xatlas::Atlas *, xatlas::ProgressFunc,
                   void *) asm("__real__ZN6xatlas19SetProgressCallbackEPNS_"
                               "5AtlasEPFbNS_16ProgressCategoryEiPvES3_");
void wrap_progress(xatlas::Atlas *atlas, xatlas::ProgressFunc fn,
                   void *data) asm("__wrap__ZN6xatlas19SetProgressCallbackEPNS_"
                                   "5AtlasEPFbNS_16ProgressCategoryEiPvES3_");
void wrap_progress(xatlas::Atlas *atlas, xatlas::ProgressFunc fn, void *data) {
  if (!fn) {
    real_progress(atlas, fn, data);
    return;
  }
  auto old = forward.exchange(fn);
  if (old && old != fn)
    throw std::runtime_error("Unexpected atlas progress function");
  real_progress(atlas, trampoline, data);
}
int main() try {
  auto original = source(), legacy = original, empty = original,
       observed = original;
  auto old_info = unwrap(legacy, 512, 2);
  auto new_info = unwrap(empty, 512, 2, {});
  check(same(legacy, empty) && old_info.width == new_info.width &&
            old_info.height == new_info.height &&
            old_info.charts == new_info.charts &&
            old_info.utilization == new_info.utilization,
        "Empty cancellation retains the exact three-argument atlas "
        "geometry/UV/default numeric result");
  std::atomic<size_t> visits{0};
  std::atomic_bool chart{false}, pack{false};
  unwrap(observed, 512, 2, [&] {
    ++visits;
    if (category == int(xatlas::ProgressCategory::ComputeCharts))
      chart = true;
    if (category == int(xatlas::ProgressCategory::PackCharts))
      pack = true;
    return false;
  });
  check(visits > 100 && chart && pack && same(observed, legacy),
        "A live noncanceling callback observes real chart/pack work without "
        "changing any atlas output");
  auto before = original;
  check(cancelled(before, [] { return true; }) && same(before, original),
        "Immediate cancellation leaves original geometry, UV, pins, seams and "
        "ordering exact");
  for (auto stage : {xatlas::ProgressCategory::ComputeCharts,
                     xatlas::ProgressCategory::PackCharts}) {
    auto candidate = original;
    std::atomic_bool entered{false};
    bool stopped = cancelled(candidate, [&] {
      if (category == int(stage) && percentage > 0 && percentage < 100) {
        entered = true;
        return true;
      }
      return false;
    });
    check(stopped && entered && same(candidate, original),
          stage == xatlas::ProgressCategory::ComputeCharts
              ? "Actual mid-chart cancellation unwinds without changing the "
                "input mesh or authoring flags"
              : "Actual mid-pack cancellation unwinds without changing the "
                "input mesh or authoring flags");
  }
  auto late = original;
  std::atomic_bool complete{false};
  check(cancelled(late,
                  [&] {
                    if (category ==
                            int(xatlas::ProgressCategory::BuildOutputMeshes) &&
                        percentage == 100) {
                      complete = true;
                      return true;
                    }
                    return false;
                  }) &&
            complete && same(late, original),
        "Cancellation at real completed output still refuses before mesh "
        "publication");
  auto after = original;
  std::atomic_bool output{false};
  size_t post = 0;
  check(cancelled(after,
                  [&] {
                    if (category ==
                            int(xatlas::ProgressCategory::BuildOutputMeshes) &&
                        percentage == 100)
                      output = true;
                    return output && category < 0 && ++post == 10;
                  }) &&
            post == 10 && same(after, original),
        "Cancellation during host output reconstruction retains exact old UV "
        "and geometry authority");
  auto throwing = original;
  std::atomic_bool thrown{false};
  bool caught = false;
  try {
    unwrap(throwing, 512, 2, [&]() -> bool {
      if (category == int(xatlas::ProgressCategory::ComputeCharts) &&
          percentage > 0 && percentage < 100) {
        thrown = true;
        throw std::logic_error("chart callback sentinel");
      }
      return false;
    });
  } catch (const std::logic_error &e) {
    caught = std::string_view(e.what()) == "chart callback sentinel";
  }
  check(caught && thrown && same(throwing, original),
        "Chart callback exceptions cross no vendor worker boundary and rethrow "
        "with their original type/message");
  auto nonstandard = original;
  caught = false;
  try {
    unwrap(nonstandard, 512, 2, [&]() -> bool {
      if (category == int(xatlas::ProgressCategory::PackCharts) &&
          percentage > 0 && percentage < 100)
        throw 37;
      return false;
    });
  } catch (int value) {
    caught = value == 37;
  }
  check(caught && same(nonstandard, original),
        "Nonstandard callback exceptions also unwind on the caller with exact "
        "original mesh");
  auto cold = original;
  std::atomic_bool add{false};
  check(cancelled(cold,
                  [&] {
                    if (category == int(xatlas::ProgressCategory::AddMesh)) {
                      add = true;
                      return true;
                    }
                    return false;
                  }) &&
            add && same(cold, original),
        "Cancellation during native AddMesh joins its pending tasks before "
        "returning");
  auto retry = original;
  unwrap(retry, 512, 2, [] { return false; });
  check(same(retry, legacy), "Canceled and throwing atlas jobs retry to the "
                             "exact original successful UV result");
  struct Rendezvous {
    std::mutex mutex;
    std::condition_variable changed;
    int ready = 0;
    void arrive() {
      std::unique_lock lock(mutex);
      ++ready;
      changed.notify_all();
      if (!changed.wait_for(lock, std::chrono::seconds(15),
                            [&] { return ready >= 2; }))
        throw std::runtime_error("Atlas concurrency rendezvous timed out");
    }
  } rendezvous;
  auto run = [&](bool stop) {
    auto value = original;
    bool first = true, hit = false;
    try {
      unwrap(value, 512, 2, [&] {
        if (category == int(xatlas::ProgressCategory::ComputeCharts) && first) {
          first = false;
          rendezvous.arrive();
        }
        if (stop && category == int(xatlas::ProgressCategory::PackCharts) &&
            percentage > 0 && percentage < 100) {
          hit = true;
          return true;
        }
        return false;
      });
      return !stop && same(value, legacy);
    } catch (const std::runtime_error &e) {
      return stop && hit &&
             std::string_view(e.what()).find("cancel") !=
                 std::string_view::npos &&
             same(value, original);
    }
  };
  auto a = std::async(std::launch::async, run, true),
       b = std::async(std::launch::async, run, false);
  check(a.get() && b.get(),
        "Concurrent atlas contexts keep cancellation, callback lifetime and "
        "successful output independent");
  auto fixture=import_fixture::Fixture{};
  auto &primitive=fixture.j["meshes"][0]["primitives"][0];
  primitive["attributes"].erase("NORMAL");primitive["attributes"].erase("TEXCOORD_0");
  primitive["targets"][0].erase("NORMAL");
  fixture.j["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
  const auto path=std::filesystem::temp_directory_path()/("dome-flat-atlas-cancel-"+std::to_string(getpid())+".glb");
  struct Cleanup {std::filesystem::path file;~Cleanup(){std::error_code e;std::filesystem::remove(file,e);}} cleanup{path};
  fixture.save(path);Scene caller;const auto before_import=caller.encode();
  ImportAuthoringOptions options;options.paint_resolution=64;bool visited=false,refused=false;
  options.limits.cancelled=[&]{if(category==int(xatlas::ProgressCategory::ComputeCharts)){visited=true;return true;}return false;};
  try {prepare_authoring_import(caller,path,options);}
  catch(const std::runtime_error &e){refused=std::string_view(e.what()).find("cancel")!=std::string_view::npos;}
  check(visited && refused && caller.encode()==before_import,
      "Actual missing-normal/missing-UV import propagates cancellation from real xatlas chart work and preserves its complete caller scene");
  std::cout << checks << " cancellable atlas checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
