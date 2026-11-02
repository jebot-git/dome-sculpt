// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparse-resample-grid.hpp"
#include "dome/math.hpp"
#include <chrono>
#include <limits>
#include <string_view>
#include <utility>
#include <iostream>
#include <unordered_map>
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
using namespace dome::sparse_resample;
namespace {
using Clock = std::chrono::steady_clock;
using Reference = std::unordered_map<dome::Int3, double, dome::IntHash>;
double ms(Clock::time_point since) { return std::chrono::duration<double, std::milli>(Clock::now() - since).count(); }
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
std::vector<uint64_t> reference(const Reference &map, std::span<const Point> queries, float source, float target, double background) {
  std::vector<uint64_t> result;
  result.reserve(queries.size());
  for (auto q : queries) {
    const double p[]{double(q.x) * target / source, double(q.y) * target / source, double(q.z) * target / source};
    const dome::Int3 lo{int(std::floor(p[0])), int(std::floor(p[1])), int(std::floor(p[2]))};
    const double f[]{p[0] - lo.x, p[1] - lo.y, p[2] - lo.z};
    double value = background;
    for (int z = 0; z < 2; ++z) for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
      const auto found = map.find(lo + dome::Int3{x, y, z});
      if (found != map.end()) value += (found->second - background) * (x ? f[0] : 1 - f[0]) *
                                      (y ? f[1] : 1 - f[1]) * (z ? f[2] : 1 - f[2]);
    }
    result.push_back(std::bit_cast<uint64_t>(value));
  }
  return result;
}
void compare(std::span<const uint64_t> actual, std::span<const uint64_t> expected) {
  require(actual.size() == expected.size(), "Sparse result size changed");
  for (size_t i = 0; i < actual.size(); ++i) if (actual[i] != expected[i]) {
    std::cerr << "Sparse mismatch at " << i << std::hex << " expected=" << expected[i] << " actual=" << actual[i] << '\n';
    throw std::runtime_error("Sparse interpolation differs from current native CPU formula");
  }
}
#ifdef DOME_TEST_SYCL
std::vector<uint64_t> device_sample(const Grid &grid, std::span<const Point> queries, Parameters p, bool arc,
                                   std::vector<double> &timings, std::span<const uint64_t> expected) {
  validate_queries(queries, p);
  if (queries.empty()) return {};
  const auto begin = Clock::now();
  sycl::queue queue{sycl::gpu_selector_v, [](sycl::exception_list errors) {
    for (auto error : errors) std::rethrow_exception(error);
  }, sycl::property_list{sycl::property::queue::in_order{}}};
  const auto queue_ms = ms(begin);
  const auto name = queue.get_device().get_info<sycl::info::device::name>();
  require(!arc || (name.find("Intel") != std::string::npos && name.find("Arc") != std::string::npos && name.find("A770") != std::string::npos),
          "Requested strict Arc A770 is absent");
  std::cout << "DEVICE " << name << " fp64=" << queue.get_device().has(sycl::aspect::fp64)
            << " queue_init_ms=" << queue_ms << '\n';
  constexpr size_t chunk = 65536;
  struct Memory {
    sycl::queue &queue;
    Cell *cells = nullptr;
    Point *points = nullptr;
    uint64_t *values = nullptr;
    ~Memory() {
      try { queue.wait_and_throw(); } catch (...) {}
      if (cells) try { sycl::free(cells, queue); } catch (...) {}
      if (points) try { sycl::free(points, queue); } catch (...) {}
      if (values) try { sycl::free(values, queue); } catch (...) {}
    }
  } memory{queue};
  const auto allocation_begin = Clock::now();
  memory.cells = sycl::malloc_device<Cell>(grid.cells().size(), queue);
  memory.points = sycl::malloc_device<Point>(std::min(chunk, queries.size()), queue);
  memory.values = sycl::malloc_device<uint64_t>(std::min(chunk, queries.size()), queue);
  if (!memory.cells || !memory.points || !memory.values) throw std::bad_alloc();
  const auto allocation_ms = ms(allocation_begin);
  std::cout << "GPU_ALLOCATION_MS " << allocation_ms << std::endl;
  const auto *cells = memory.cells;
  auto *points = memory.points;
  auto *values = memory.values;
  const auto mask = grid.mask();
  std::vector<uint64_t> result(queries.size());
  for (int iteration = 0; iteration < 4; ++iteration) {
    const auto started = Clock::now();
    // Every measured operation includes the full immutable field upload. This
    // prototype does not claim a persistent cross-operation device cache.
    queue.memcpy(memory.cells, grid.cells().data(), grid.cells().size() * sizeof(Cell));
    for (size_t first = 0; first < queries.size(); first += chunk) {
      const auto count = std::min(chunk, queries.size() - first);
      queue.memcpy(points, queries.data() + first, count * sizeof(Point));
      queue.parallel_for(sycl::range<1>{count}, [=](sycl::id<1> id) {
        values[id] = sample(cells, mask, points[id], p);
      });
      queue.memcpy(result.data() + first, values, count * sizeof(uint64_t));
      queue.wait_and_throw();
    }
    timings.push_back(ms(started));
    compare(result, expected);
  }
  std::cout << "GPU_NONRETAINED_QUEUE_ALLOCATION_MS " << queue_ms + allocation_ms << std::endl;
  return result;
}
#endif
size_t exercise(std::vector<Entry> entries, std::vector<Point> queries, float source, float target,
                double background, bool device, bool arc, bool encoded_host) {
  Reference map;
  for (const auto &entry : entries) map.emplace(dome::Int3{entry.point.x, entry.point.y, entry.point.z}, entry.value);
  const auto ref_begin = Clock::now();
  const auto expected = reference(map, queries, source, target, background);
  const auto reference_ms = ms(ref_begin);
  const auto packed = Clock::now();
  const Grid grid(entries);
  const auto packing_ms = ms(packed);
  const auto p = parameters(source, target, background);
  const auto admitted = Clock::now();
  validate_queries(queries, p);
  const auto validation_ms = ms(admitted);
  if (encoded_host) {
    std::vector<uint64_t> actual;
    for (auto q : queries) actual.push_back(sample(grid.cells().data(), grid.mask(), q, p));
    compare(actual, expected);
  }
#ifdef DOME_TEST_SYCL
  if (device) {
    std::vector<double> times;
    const auto actual = device_sample(grid, queries, p, arc, times, expected);
    compare(actual, expected);
    std::cout << "SPARSE_JOB entries=" << entries.size() << " queries=" << queries.size()
              << " field_bytes=" << grid.cells().size() * sizeof(Cell)
              << " query_bytes=" << queries.size() * sizeof(Point) << " readback_bytes=" << actual.size() * 8
              << " native_cpu_ms=" << reference_ms << " host_pack_ms=" << packing_ms
              << " query_validation_ms=" << validation_ms << '\n';
    for (size_t i = 0; i < times.size(); ++i)
      std::cout << "SPARSE_GPU iteration=" << i << " upload_kernel_readback_ms=" << times[i]
                << " full_host_pack_inclusive_ms=" << packing_ms + validation_ms + times[i] << '\n';
  }
#else
  require(!device, "This build has no GPU test entry point");
#endif
  return expected.size();
}
}
int main(int argc, char **argv) try {
  const bool device = argc > 1 && std::string_view(argv[1]) == "--device";
  const bool arc = argc > 2 && std::string_view(argv[2]) == "--arc";
  size_t checked = 0;
  for (double background : {0., 1.}) for (auto spacing : {std::pair{.03f, .02f}, std::pair{.02f, .03f}, std::pair{.00001f, .00001f}}) {
    std::vector<Entry> entries{{{-1, 0, 0}, .25}, {{0, 0, 0}, -.375}, {{1, -1, 0}, 1e-6},
                               {{-1, -1, -1}, std::numeric_limits<double>::denorm_min()},
                               {{1000000, -1000000, 3}, 999999.}, {{-1000000, 1000000, -3}, -999999.}};
    std::vector<Point> queries;
    for (int z = -4; z <= 4; ++z) for (int y = -4; y <= 4; ++y) for (int x = -4; x <= 4; ++x) queries.push_back({x, y, z});
    if (spacing.first == spacing.second) {
      queries.push_back({1000000, -1000000, 3});
      queries.push_back({-1000000, 1000000, -3});
    }
    checked += exercise(entries, queries, spacing.first, spacing.second, background, device, arc, true);
  }
  checked += exercise({}, {{0, 0, 0}, {-1, -1, -1}}, .03f, .02f, 1., device, arc, true);
  auto refuses = [](auto operation) { try { operation(); } catch (const std::exception &) { return; } throw std::runtime_error("Invalid sparse input was admitted"); };
  refuses([] { const std::vector<Entry> entries{{{0, 0, 0}, 1}, {{0, 0, 0}, 2}}; Grid grid(entries); });
  refuses([] { const std::vector<Entry> entries{{{0, 0, 0}, std::numeric_limits<double>::infinity()}}; Grid grid(entries); });
  refuses([] { parameters(0, .01f, 0); });
  refuses([] { const std::array<Point, 1> points{{{INT32_MAX, 0, 0}}}; validate_queries(points, parameters(.00001f, 1, 0)); });
  if (device) {
    std::vector<Entry> entries;
    for (int z = -32; z < 32; ++z) for (int y = -32; y < 32; ++y) for (int x = -32; x < 32; ++x)
      entries.push_back({{x, y, z}, .05 * std::sin(x * .1) + .02 * std::cos(y * .04) + z * .0002});
    std::vector<Point> queries;
    for (int z = -48; z < 48; ++z) for (int y = -48; y < 48; ++y) for (int x = -48; x < 48; ++x) queries.push_back({x, y, z});
    checked += exercise(entries, queries, .03f, .02f, 0, true, arc, false);
  }
  std::cout << "PASS " << checked << " exact sparse interpolation words plus malformed input guards\n";
  return 0;
} catch (const std::exception &error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
