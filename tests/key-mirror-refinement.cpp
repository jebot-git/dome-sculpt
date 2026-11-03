// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdlib>
#include <new>
namespace allocation {
thread_local long remaining = -1;
thread_local bool count = false;
thread_local size_t calls = 0;
} // namespace allocation
void *operator new(size_t n) {
  if (allocation::count)
    ++allocation::calls;
  if (allocation::remaining >= 0 && allocation::remaining-- == 0) {
    allocation::remaining = 0;
    throw std::bad_alloc();
  }
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
#include "key-mirror-refinement.hpp"
#include <cmath>
#include <iostream>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << std::endl;
}
bool same_positions(const std::vector<Vec3> &a, const std::vector<Vec3> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z)
      return false;
  return true;
}
bool refuses(auto f, std::string_view diagnostic) {
  try {
    f();
    return false;
  } catch (const std::exception &e) {
    std::cout << "REFUSAL " << e.what() << std::endl;
    return std::string_view(e.what()).find(diagnostic) != std::string_view::npos;
  }
}
std::shared_ptr<const Mesh> square() {
  Mesh m;
  m.positions = {{-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return std::make_shared<const Mesh>(std::move(m));
}
Vec3 reflect(Vec3 p) { return {-p.x, p.y, p.z}; }
Vec3 at(const Mesh &m, const MirrorSurfacePoint &s) {
  Vec3 p;
  for (size_t k = 0; k < 3; ++k)
    p += m.positions[m.indices[s.triangle * 3 + k]] * float(s.barycentric[k]);
  return p;
}
bool same(const KeyMirrorRefinement &a, const KeyMirrorRefinement &b) {
  if (a.source != b.source || a.opposite != b.opposite || a.corners.size() != b.corners.size() ||
      a.maximum_rest_error != b.maximum_rest_error || a.work_units != b.work_units)
    return false;
  for (size_t c = 0; c < a.corners.size(); ++c) {
    const auto &x = a.corners[c];
    const auto &y = b.corners[c];
    if (x.position.x != y.position.x || x.position.y != y.position.y ||
        x.position.z != y.position.z || x.current.triangle != y.current.triangle ||
        x.current.barycentric != y.current.barycentric ||
        x.reflected.triangle != y.reflected.triangle ||
        x.reflected.barycentric != y.reflected.barycentric)
      return false;
  }
  return true;
}
// Independent physical piecewise-linear field, not a runtime correspondence.
double field(double x, double y) {
  const double u = x + .5, v = y + .5;
  return v <= u ? .04 * (u - v) - .06 * v : -.06 * u + .09 * (v - u);
}
double sampled(const MirrorSurfacePoint &p) {
  constexpr double z[] = {0, .04, -.06, 0, -.06, .09};
  double sum = 0;
  for (size_t k = 0; k < 3; ++k)
    sum += z[p.triangle * 3 + k] * p.barycentric[k];
  return sum;
}
} // namespace
int main() try {
  const auto source = square();
  const auto positions = source->positions;
  const auto indices = source->indices;
  check(refuses([&] { reflected_key_correspondence(*source, 1e-6f); }, "triangle refinement"),
        "Genuine opposite diagonals cannot use the historical corner bijection");
  const auto result = refine_reflected_key(source, 1e-6f);
  check(result.source == source && source->positions.size() == 4 &&
            result.corners.size() > source->indices.size() && result.corners.size() % 3 == 0,
        "Detached refinement keeps the genuine raw4 source and owns newly derived corners");
  double area = 0, field_error = 0, normal_error = 0, tangent_error = 0;
  bool interior = false;
  for (size_t c = 0; c < result.corners.size(); ++c) {
    const auto &v = result.corners[c];
    const auto &o = result.corners[result.opposite[c]];
    check(length(at(*source, v.current) - v.position) < 1e-6f &&
              length(reflect(at(*source, v.reflected)) - v.position) < 1e-6f,
          "Every derived corner has actual source-triangle barycentrics on both sides");
    check(result.opposite[result.opposite[c]] == c &&
              length(v.position - reflect(o.position)) < 1e-6f,
          "Explicit opposite corners form a genuine reflection involution");
    field_error =
        std::max(field_error, std::abs(sampled(v.reflected) - field(-v.position.x, v.position.y)));
    for (auto w : v.current.barycentric)
      interior |= w > 1e-6 && w < 1 - 1e-6;
  }
  for (size_t c = 0; c < result.corners.size(); c += 3) {
    Vec3 p[3];
    std::array<float, 2> uv[3];
    double x = 0, y = 0;
    for (size_t k = 0; k < 3; ++k) {
      const auto &v = result.corners[c + k];
      p[k] = v.position;
      p[k].z += float(sampled(v.reflected));
      uv[k] = {v.position.x + .5f, v.position.y + .5f};
      x += v.position.x / 3.;
      y += v.position.y / 3.;
    }
    area += length(cross(result.corners[c + 1].position - result.corners[c].position,
                         result.corners[c + 2].position - result.corners[c].position)) *
            .5;
    const bool lower = y + .5 <= -x + .5;
    const double fx = lower ? -.04 : .15, fy = lower ? -.10 : .09;
    const auto expected_n = normalized(Vec3{float(-fx), float(-fy), 1});
    const auto actual_n = normalized(cross(p[1] - p[0], p[2] - p[0]));
    normal_error = std::max(normal_error, double(length(expected_n - actual_n)));
    const auto e1 = p[1] - p[0], e2 = p[2] - p[0];
    const float du1 = uv[1][0] - uv[0][0], dv1 = uv[1][1] - uv[0][1], du2 = uv[2][0] - uv[0][0],
                dv2 = uv[2][1] - uv[0][1];
    const auto actual_t = normalized((e1 * dv2 - e2 * dv1) / (du1 * dv2 - du2 * dv1));
    const auto expected_t = normalized(Vec3{1, 0, float(fx)});
    tangent_error = std::max(tangent_error, double(length(actual_t - expected_t)));
  }
  std::cout << "REFINEMENT triangles=" << result.corners.size() / 3 << " area=" << area
            << " field=" << field_error << " N=" << normal_error << " T=" << tangent_error
            << std::endl;
  check(interior && std::abs(area - 1) < 1e-6 && field_error < 1e-7,
        "Nonmatching diagonals retain full area and exact mirrored piecewise field, including real "
        "interior points");
  check(normal_error < 1e-6 && tangent_error < 1e-6,
        "Independent analytic slope and UV derivative verify geometry-provenance N/T construction");
  bool independent_seam = false;
  for (size_t i = 0; i < result.corners.size(); ++i)
    for (size_t j = i + 1; j < result.corners.size(); ++j) {
      const auto &a = result.corners[i];
      const auto &b = result.corners[j];
      if (length(a.position - b.position) < 1e-7f && a.reflected.triangle != b.reflected.triangle) {
        const double x = sampled(a.reflected) + (a.reflected.triangle ? .03 : -.02),
                     y = sampled(b.reflected) + (b.reflected.triangle ? .03 : -.02);
        independent_seam |= std::abs(x - y) > .049;
      }
    }
  check(independent_seam, "Coincident derived corners keep distinct original face fields without "
                          "choosing or averaging their seam");
  auto reversed_source = std::make_shared<Mesh>(*source);
  for (size_t c = 0; c < reversed_source->indices.size(); c += 3)
    std::swap(reversed_source->indices[c + 1], reversed_source->indices[c + 2]);
  reversed_source->normals.assign(4, {0, 0, -1});
  const auto reverse_result = refine_reflected_key(reversed_source, 1e-6f);
  bool reverse_winding = true;
  for (size_t c = 0; c < reverse_result.corners.size(); c += 3)
    reverse_winding &=
        cross(reverse_result.corners[c + 1].position - reverse_result.corners[c].position,
              reverse_result.corners[c + 2].position - reverse_result.corners[c].position)
            .z < 0;
  check(reverse_winding, "Refinement preserves actual original clockwise source winding rather "
                         "than imposing projection order");
  auto vertical = std::make_shared<Mesh>(*source);
  for (auto &p : vertical->positions)
    p = {0, p.x, p.y};
  vertical->normals.assign(4, {1, 0, 0});
  const auto vertical_result = refine_reflected_key(vertical, 1e-6f);
  bool vertical_winding = true;
  for (size_t c = 0; c < vertical_result.corners.size(); c += 3)
    vertical_winding &=
        cross(vertical_result.corners[c + 1].position - vertical_result.corners[c].position,
              vertical_result.corners[c + 2].position - vertical_result.corners[c].position)
            .x > 0;
  check(vertical_winding, "Geometry-only reflection on its fixed plane retains actual corner "
                          "winding; it mints no role-normal parity certificate");
  size_t low = 1, high = 1024 * 1024;
  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    KeyMirrorLimits bound;
    bound.max_bytes = mid;
    bool accepted = false;
    try {
      accepted = same(result, refine_reflected_key(source, 1e-6f, bound));
    } catch (const std::length_error &) {
    }
    if (accepted)
      high = mid;
    else
      low = mid + 1;
  }
  KeyMirrorLimits exact_cap;
  exact_cap.max_bytes = low;
  check(same(result, refine_reflected_key(source, 1e-6f, exact_cap)),
        "Exact conservative prospective byte cap accepts deterministic output");
  exact_cap.max_bytes = low - 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, exact_cap); }, "quota"),
        "One byte below the conservative prospective cap refuses before the prohibited allocation");
  std::cout << "EXACT_FORECAST_CAP " << low << std::endl;
  auto quantized = std::make_shared<Mesh>(*source);
  for (auto &p : quantized->positions)
    p.y += 8388600.f;
  check(
      refuses([&] { refine_reflected_key(quantized, 1e-6f); }, "represent"),
      "Derived float positions cannot hide barycentric quantization outside the actual tolerance");
  auto duplicate = std::make_shared<Mesh>(*source);
  duplicate->indices.insert(duplicate->indices.end(), source->indices.begin(),
                            source->indices.end());
  check(refuses([&] { refine_reflected_key(duplicate, 1e-6f); }, "ambiguous"),
        "Coincident distinct source faces cannot silently borrow a nearest seam owner");
  auto asymmetric = std::make_shared<Mesh>(*source);
  asymmetric->positions[1].x += .1f;
  check(refuses([&] { refine_reflected_key(asymmetric, 1e-6f); }, "hole"),
        "Unrepresented reflected geometry refuses without widening the declared tolerance");
  auto oversized = std::make_shared<Mesh>();
  oversized->positions.resize(2'000'001);
  check(refuses([&] { refine_reflected_key(oversized, 1e-6f); }, "source streams"),
        "Input stream cardinality is bounded before uninterruptible mesh validation");
  oversized.reset();
  KeyMirrorLimits limits;
  limits.max_bytes = 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "quota"),
        "Actual scratch/output allocation is preflighted against its unchanged explicit cap");
  limits = {};
  limits.max_work = 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "work"),
        "Actual pair work is bounded before broad-phase arrays");
  limits = {};
  size_t polls = 0;
  limits.cancelled = [&] {
    ++polls;
    return false;
  };
  refine_reflected_key(source, 1e-6f, limits);
  for (size_t cut : {size_t(1), polls / 2, polls}) {
    size_t at = 0;
    limits.cancelled = [&] { return ++at >= cut; };
    check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "cancelled") &&
              same_positions(source->positions, positions) && source->indices == indices,
          "Early middle and late cancellation return no partial derived domain or changed source");
  }
  limits = {};
  limits.max_work = result.work_units;
  check(same(result, refine_reflected_key(source, 1e-6f, limits)),
        "Exact measured work boundary accepts");
  limits.max_work = result.work_units - 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "work"),
        "One work unit below refuses without a partial returned domain");
  allocation::calls = 0;
  allocation::count = true;
  auto measured = refine_reflected_key(source, 1e-6f);
  allocation::count = false;
  const auto calls = allocation::calls;
  check(calls > 0 && calls < 1000,
        "Detached common refinement has a finite measured allocation frontier");
  for (size_t cut = 0; cut < calls; ++cut) {
    allocation::remaining = long(cut);
    bool failed = false;
    try {
      refine_reflected_key(source, 1e-6f);
    } catch (const std::bad_alloc &) {
      failed = true;
    } catch (...) {
      allocation::remaining = -1;
      throw;
    }
    allocation::remaining = -1;
    if (!failed || !same_positions(source->positions, positions) || source->indices != indices)
      throw std::runtime_error("Persistent common-refinement allocation rollback failed at " +
                               std::to_string(cut));
  }
  check(same(result, refine_reflected_key(source, 1e-6f)),
        "All persistent allocation denials leave an exact deterministic retry and original source");
  std::cout << "ALLOCATION_PREFIXES " << calls << " POLLS " << polls << " WORK_UNITS "
            << result.work_units << "\nPASS " << checks << " detached common-refinement checks\n";
  return 0;
} catch (const std::exception &e) {
  allocation::remaining = -1;
  allocation::count = false;
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
