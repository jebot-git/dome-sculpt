// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-authoring.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/source-placement.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
namespace allocation {
thread_local bool armed = false, hit = false;
thread_local size_t calls = 0, boundary = 0;
void *allocate(size_t bytes, size_t alignment = 0) {
  if (armed && ++calls >= boundary && boundary) {
    hit = true;
    throw std::bad_alloc();
  }
  void *p = nullptr;
  if (alignment) {
    if (posix_memalign(&p, alignment, bytes ? bytes : 1)) p = nullptr;
  } else p = std::malloc(bytes ? bytes : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
struct Scope {
  explicit Scope(size_t at) { calls = 0; hit = false; boundary = at; armed = true; }
  ~Scope() { armed = false; }
};
}
void *operator new(size_t n) { return allocation::allocate(n); }
void *operator new[](size_t n) { return allocation::allocate(n); }
void *operator new(size_t n, std::align_val_t a) { return allocation::allocate(n, size_t(a)); }
void *operator new[](size_t n, std::align_val_t a) { return allocation::allocate(n, size_t(a)); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { std::free(p); }
using namespace dome;
namespace {
void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
Scene fixture() {
  Mesh m;
  constexpr float r = .1f;
  m.positions = {{-r,-r,-r},{r,-r,-r},{r,r,-r},{-r,r,-r},
                 {-r,-r,r},{r,-r,r},{r,r,r},{-r,r,r}};
  for (auto p : m.positions) m.normals.push_back(normalized(p));
  m.indices = {0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,
               1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
  unwrap(m, 128);
  Scene scene;
  auto &o = scene.active(); o.name = "Painted posed affine source";
  o.transform.position = {1,2,3}; o.transform.scale = 2;
  o.affine_edit = std::make_shared<const NativeAffineEdit>(AffineTransform(
      std::array<double,16>{1.15,.1,.05,0,.2,.8,.12,0,0,.15,1.1,0,0,0,0,1}));
  o.mesh_active = true; o.painted = std::make_shared<PaintedMesh>(m, 128);
  PaintSettings ink; ink.front_only = false; ink.color = {.1f,.2f,.8f,.7f};
  Brush b; b.from = b.to = {}; b.radius = .5f;
  o.painted->begin_stroke(); o.painted->stamp(b, ink); o.painted->end_stroke();
  o.painted->add_layer("Hidden source layer");
  o.painted->layer_properties("Hidden source layer", false, .35f);
  DeformationState d(m);
  d.skeleton = Skeleton{{{"root",-1,{{0,0,0},{},{1,1,1}},.2f}}};
  d.skin = SkinWeights{}; d.skin->vertices.assign(m.positions.size(), {{0,1}});
  d.pose = {d.skeleton->bones[0].rest}; d.pose[0].translation.x = .15f;
  d.morphs = MorphSet(m); auto target = m.positions;
  for (auto &p : target) p.z += .35f*p.x+.035f;
  d.morphs->set_target(m, "slant", target); d.morph_weights = {.4f};
  scene.set_deformation(o.id, std::move(d)); scene.validate(); return scene;
}
}
int main(int argc, char **argv) try {
  const auto source = fixture(); const auto before = source.encode();
  auto warm = source.snapshot(); create_triangle_remesh(warm, 140);
  const auto endpoint = warm.encode(); size_t measured = 0;
  for (int repeat = 0; repeat < 3; ++repeat) {
    auto candidate = source.snapshot();
    { allocation::Scope scope(0); create_triangle_remesh(candidate, 140); }
    if (repeat) require(measured == allocation::calls, "Remesh allocation count is unstable");
    measured = allocation::calls;
    require(candidate.encode() == endpoint, "Measured remesh endpoint is not deterministic");
  }
  require(measured > 0, "No actual remesh allocations measured");
  std::cout << "BASELINE " << measured << std::endl;
  if (argc == 2 && std::string_view(argv[1]) == "--measure") return 0;
  const size_t first = argc > 1 ? std::stoull(argv[1]) : 1;
  const size_t end = argc > 2 ? std::stoull(argv[2]) : measured + 2;
  require(first >= 1 && first < end && end <= measured + 2, "Invalid half-open fault range");
  size_t refused = 0, optional = 0, successes = 0;
  for (size_t at = first; at < end; ++at) {
    auto candidate = source.snapshot(); const auto old = candidate.active(); bool failed = false;
    { allocation::Scope scope(at);
      try { create_triangle_remesh(candidate, 140); }
      catch (const std::exception &) { failed = true; }
    }
    const bool hit = allocation::hit; const auto calls = allocation::calls;
    require(hit == (at <= measured), "Persistent allocation boundary was not actually reached");
    if (failed) {
      ++refused;
      require(hit && calls >= at && candidate.encode() == before,
              "Failed remesh changed the complete original scene");
      const auto &o = candidate.active();
      require(o.painted == old.painted && o.volume == old.volume &&
                  o.deformation == old.deformation && o.affine_edit == old.affine_edit &&
                  o.polygon == old.polygon,
              "Failed remesh changed original immutable owner identities");
    } else {
      require(candidate.encode() == endpoint, "Allocation failure allowed a partial remesh endpoint");
      if (hit) ++optional; else ++successes;
    }
    auto retry = source.snapshot(); create_triangle_remesh(retry, 140);
    require(retry.encode() == endpoint && source.encode() == before,
            "Real retry after denial did not retain exact source/endpoint");
    if ((at & 127) == 0) std::cout << "REACHED " << at << std::endl;
  }
  std::cout << "PASS range [" << first << ',' << end << ") refused=" << refused
            << " optional=" << optional << " endpoint=" << successes << std::endl;
  return 0;
} catch (const std::exception &e) {
  allocation::armed = false; std::cerr << "FAIL " << e.what() << std::endl; return 1;
}
