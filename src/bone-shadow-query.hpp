// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-line-integral.hpp"
#include "bone-shadow-filter.hpp"
#include "bone-source-shadow.hpp"
#include <bit>
#include <memory>
#include <numeric>
#include <span>
#include <vector>
namespace dome::detail {
enum class BoneWallDomain { WholeGroup, ConnectedSourceSurface };
// Strict source ownership remains the default. Native open-segment semantics
// exclude only exact origin-plane contacts whose ENTIRE finite source is
// independently proved strictly on one side. No vertex/incidence union follows.
enum class BoneOriginPolicy { StrictOwnership, NativeOpenSegment };
struct BoneQueryReport {
  size_t nodes = 0, candidates = 0, intervals = 0, unions = 0,
         exact_predicates = 0, filtered_misses = 0, filtered_full = 0,
         row_hits = 0, row_misses = 0, source_hits = 0, source_misses = 0,
         search_work = 0, union_work = 0, integration_work = 0,
         predicate_work = 0;
};
struct BoneQueryAmbiguity : std::runtime_error {
  bone_shadow::Kind kind;
  uint32_t face;
  BoneQueryAmbiguity(bone_shadow::Kind k, uint32_t f)
      : std::runtime_error(
            "Bone query has an unowned origin/coplanar/grazing contact"),
        kind(k), face(f) {}
};
// Owning geometry-only BVH: source/rig/intent authority remains in the caller's
// captured immutable group. No geometry, bounds or scratch aliases caller data.
class BoneShadowQuery {
  using P = bone_shadow::Point;
  struct Box {
    P lo{}, hi{};
  };
  struct Range {
    double lo = 0, hi = 0;
  };
  static Range difference(double a, double b) {
    double x = a - b;
    return {std::nextafter(x, -INFINITY), std::nextafter(x, INFINITY)};
  }
  static Range sum(Range a, Range b) {
    return {std::nextafter(a.lo + b.lo, -INFINITY),
            std::nextafter(a.hi + b.hi, INFINITY)};
  }
  static Range negative(Range a) { return {-a.hi, -a.lo}; }
  static Range product(Range a, Range b) {
    const std::array<double, 4> values{a.lo * b.lo, a.lo * b.hi, a.hi * b.lo,
                                       a.hi * b.hi};
    return {std::nextafter(*std::min_element(values.begin(), values.end()),
                           -INFINITY),
            std::nextafter(*std::max_element(values.begin(), values.end()),
                           INFINITY)};
  }
  struct Fan {
    P row{}, head{}, tail{};
    std::array<Range, 3> normal;
    bool point = false;
    std::array<Range, 3> from_head, from_tail;
  };
  template <class Budget>
  static Fan prepare_fan(P row, P head, P tail, Budget &w) {
    w.step(96);
    Fan fan;
    fan.row = row;
    fan.head = head;
    fan.tail = tail;
    fan.point = head == tail;
    std::array<Range, 3> a, b;
    for (size_t k = 0; k < 3; ++k) {
      a[k] = difference(head[k], row[k]);
      b[k] = difference(tail[k], row[k]);
      fan.from_head[k] = negative(a[k]);
      fan.from_tail[k] = negative(b[k]);
    }
    for (size_t k = 0; k < 3; ++k)
      fan.normal[k] = sum(product(a[(k + 1) % 3], b[(k + 2) % 3]),
                          negative(product(a[(k + 2) % 3], b[(k + 1) % 3])));
    return fan;
  }
  // Interval arithmetic only culls a node after proving strict separation.
  // No endpoint/midpoint visibility or approximate plane contact is admitted.
  template <class Budget>
  static bool possible(const Fan &fan, Box box, Budget &w) {
#if defined(__FAST_MATH__)
    (void)fan;
    (void)box;
    (void)w;
    return true;
#else
    w.step(9);
    // Exact binary32 halfspace certificate, independent of mesh/source type:
    // all node triangles lie in this coordinate plane, the row lies there,
    // and every point on the finite source lies strictly on one side. Every
    // open row-to-source segment therefore misses the plane. Origin ownership
    // has already been validated over the complete wall domain, so a foreign
    // coincident triangle cannot be hidden by this cull. A coplanar endpoint,
    // crossing source or nonzero-thickness box deliberately gets no shortcut.
    for (size_t k = 0; k < 3; ++k)
      if (box.lo[k] == box.hi[k] && fan.row[k] == box.lo[k] &&
          ((fan.head[k] > box.lo[k] && fan.tail[k] > box.lo[k]) ||
           (fan.head[k] < box.lo[k] && fan.tail[k] < box.lo[k])))
        return false;
    w.step(64);
    if (fan.point) {
      double begin = 0, end = 1;
      for (size_t k = 0; k < 3; ++k) {
        if (fan.head[k] == fan.row[k]) {
          if (fan.row[k] < box.lo[k] || fan.row[k] > box.hi[k])
            return false;
          continue;
        }
        const auto d = difference(fan.head[k], fan.row[k]);
        auto quotient = [&](float coordinate) {
          const auto n = difference(coordinate, fan.row[k]);
          const std::array<double, 4> q{n.lo / d.lo, n.lo / d.hi, n.hi / d.lo,
                                        n.hi / d.hi};
          return Range{
              std::nextafter(*std::min_element(q.begin(), q.end()), -INFINITY),
              std::nextafter(*std::max_element(q.begin(), q.end()), INFINITY)};
        };
        auto a = quotient(box.lo[k]), b = quotient(box.hi[k]);
        if (fan.head[k] < fan.row[k])
          std::swap(a, b);
        begin = std::max(begin, a.lo);
        end = std::min(end, b.hi);
        if (begin > end)
          return false;
      }
      return true;
    }
    Range value;
    for (size_t k = 0; k < 3; ++k) {
      const Range axis{difference(box.lo[k], fan.row[k]).lo,
                       difference(box.hi[k], fan.row[k]).hi};
      value = sum(value, product(axis, fan.normal[k]));
    }
    return !(value.lo > 0 || value.hi < 0);
#endif
  }
  struct Node {
    Box box;
    uint32_t first = 0, count = 0, left = UINT32_MAX, right = UINT32_MAX;
  };
  struct RowPlane {
    uint64_t key = UINT64_MAX;
    int8_t side = 0;
    bool inside = false;
  };
  struct SourcePlane {
    uint32_t epoch = 0;
    int8_t head = 0, tail = 0;
    std::optional<std::array<volume_exact::Integer, 2>> values;
    uint8_t edge_mask = 0;
    std::array<std::array<Range, 3>, 6> edge_normals;
  };
  std::shared_ptr<const uint8_t> identity_;
  const BoneOriginPolicy origin_admission_;
  static size_t cache_slots(size_t points) {
    return points ? std::bit_ceil(std::min(size_t(1) << 19, points * 64)) : 0;
  }
  std::vector<P> points_;
  std::vector<uint32_t> incidence_, components_;
  std::vector<std::array<uint32_t, 3>> faces_;
  std::vector<Box> boxes_;
  std::vector<uint32_t> order_, component_roots_, forest_;
  std::vector<Node> nodes_;
  static Box combine(Box a, Box b) {
    for (size_t k = 0; k < 3; ++k) {
      a.lo[k] = std::min(a.lo[k], b.lo[k]);
      a.hi[k] = std::max(a.hi[k], b.hi[k]);
    }
    return a;
  }
  template <class Budget> uint32_t build(size_t begin, size_t end, Budget &w) {
    w.stop();
    Box box = boxes_[order_[begin]];
    for (size_t at = begin + 1; at < end; ++at) {
      w.step(6);
      box = combine(box, boxes_[order_[at]]);
    }
    const uint32_t index = nodes_.size();
    nodes_.push_back({box, uint32_t(begin), uint32_t(end - begin)});
    if (end - begin <= 8)
      return index;
    size_t axis = 0;
    for (size_t k = 1; k < 3; ++k)
      if (double(box.hi[k]) - box.lo[k] > double(box.hi[axis]) - box.lo[axis])
        axis = k;
    std::sort(order_.begin() + begin, order_.begin() + end,
              [&](uint32_t a, uint32_t b) {
                w.step();
                double x = double(boxes_[a].lo[axis]) + boxes_[a].hi[axis],
                       y = double(boxes_[b].lo[axis]) + boxes_[b].hi[axis];
                return x != y ? x < y : a < b;
              });
    const size_t middle = (begin + end) / 2;
    auto left = build(begin, middle, w), right = build(middle, end, w);
    nodes_[index].left = left;
    nodes_[index].right = right;
    nodes_[index].count = 0;
    return index;
  }
  template <class Budget> static bool overlaps(Box a, Box b, Budget &w) {
    w.step(6);
    for (size_t k = 0; k < 3; ++k)
      if (a.hi[k] < b.lo[k] || b.hi[k] < a.lo[k])
        return false;
    return true;
  }
  static long double integer_value(const volume_exact::Integer &i) {
    long double value = 0;
    for (size_t at = i.used; at-- > 0;)
      value = std::ldexp(value, 32) + i.limb[at];
    return std::ldexp(value, i.shift) * i.sign;
  }
  template <class Budget>
  static double outward(const volume_exact::Fraction &f, bool upper,
                        Budget &w) {
    volume_exact::Arithmetic exact(w);
    if (exact.compare(f, exact.zero()) < 0 || exact.compare(f, exact.one()) > 0)
      throw std::logic_error("Shadow interval outside finite source");
    double value =
        double(integer_value(f.numerator) / integer_value(f.denominator));
    if (!std::isfinite(value))
      throw std::runtime_error("Unrepresentable shadow endpoint");
    value = std::clamp(value, 0., 1.);
    for (size_t attempt = 0; attempt < 8; ++attempt) {
      w.step(32);
      auto current =
          bone_shadow::ratio(exact.scalar(value), exact.scalar(1.), exact);
      int comparison = exact.compare(current, f);
      if (upper ? comparison >= 0 : comparison <= 0)
        return value;
      value = std::nextafter(value, upper ? INFINITY : -INFINITY);
    }
    throw std::runtime_error(
        "Shadow endpoint failed exact outward certificate");
  }

public:
  BoneShadowQuery(const BoneShadowQuery &) = delete;
  BoneShadowQuery &operator=(const BoneShadowQuery &) = delete;
  BoneShadowQuery(BoneShadowQuery &&) noexcept = default;
  BoneShadowQuery &operator=(BoneShadowQuery &&) = delete;
  struct Scratch {
    std::vector<volume_exact::Interval> shadows;
    std::vector<uint32_t> stack;
    std::vector<RowPlane> rows;
    std::vector<SourcePlane> sources;
    std::vector<uint8_t> origin_checked;
    std::shared_ptr<const uint8_t> geometry;
    uint32_t source_epoch = 0;
    P head{}, tail{};
    explicit Scratch(const BoneShadowQuery &q) {
      shadows.reserve(q.faces_.size());
      stack.reserve(q.nodes_.size());
      rows.resize(cache_slots(q.points_.size()));
      sources.resize(q.faces_.size());
      origin_checked.resize(q.points_.size());
    }
  };

private:
  template <class Budget>
  void prepare(Scratch &s, P head, P tail, Budget &w) const {
    if (s.geometry.get() != identity_.get() ||
        s.geometry.owner_before(identity_) ||
        identity_.owner_before(s.geometry)) {
      w.step(s.rows.size() + s.sources.size() + s.origin_checked.size());
      for (auto &row : s.rows)
        row.key = UINT64_MAX;
      for (auto &source : s.sources)
        source.epoch = 0;
      std::fill(s.origin_checked.begin(), s.origin_checked.end(), uint8_t(0));
      s.geometry = identity_;
      s.source_epoch = 0;
    }
    w.step(8);
    if (!s.source_epoch || head != s.head || tail != s.tail) {
      if (++s.source_epoch == 0) {
        w.step(s.sources.size());
        for (auto &source : s.sources)
          source.epoch = 0;
        s.source_epoch = 1;
      }
      s.head = head;
      s.tail = tail;
    }
  }
  template <class Budget>
  int plane_sign(uint32_t face, P point, Budget &w) const {
    const auto &f = faces_[face];
    int sign = bone_shadow::filtered_orientation(points_[f[0]], points_[f[1]],
                                                 points_[f[2]], point, w);
    if (sign)
      return sign;
    volume_exact::Arithmetic exact(w);
    return bone_shadow::determinant(points_[f[0]], points_[f[1]], points_[f[2]],
                                    point, exact)
        .sign;
  }
  template <class Budget>
  RowPlane row_plane(uint32_t row, uint32_t face, Scratch &s, Budget &w,
                     BoneQueryReport &r) const {
    const uint64_t key = (uint64_t(row) << 32) | face;
    uint64_t hash = key ^ (key >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    auto &cached = s.rows[hash & (s.rows.size() - 1)];
    w.step(8);
    if (cached.key == key) {
      ++r.row_hits;
      return cached;
    }
    ++r.row_misses;
    RowPlane result;
    result.key = key;
    result.side = plane_sign(face, points_[row], w);
    if (!result.side) {
      const auto &f = faces_[face];
      volume_exact::Arithmetic exact(w);
      int drop = -1;
      std::array<std::array<float, 2>, 3> flat;
      auto project = [](P p, size_t k) {
        return std::array<float, 2>{p[(k + 1) % 3], p[(k + 2) % 3]};
      };
      for (size_t k = 0; k < 3 && drop < 0; ++k) {
        for (size_t c = 0; c < 3; ++c)
          flat[c] = project(points_[f[c]], k);
        if (exact.orient2(flat[0], flat[1], flat[2]).sign)
          drop = int(k);
      }
      if (drop < 0)
        throw std::logic_error("Owning row-plane has degenerate wall");
      const int orientation = exact.orient2(flat[0], flat[1], flat[2]).sign;
      result.inside = true;
      for (size_t c = 0; c < 3; ++c) {
        int sign = exact
                       .orient2(flat[c], flat[(c + 1) % 3],
                                project(points_[row], drop))
                       .sign;
        result.inside &= !sign || sign == orientation;
      }
    }
    cached = result;
    return result;
  }
  // Source-independent origin ownership is proved over the COMPLETE selected
  // wall domain before any opaque-ray early exit. An opaque wall cannot hide a
  // foreign coincident origin. Only a fully completed pass becomes reusable.
  template <class Budget>
  bool open_origin_clear(uint32_t row, uint32_t face, P head, P tail,
                         Scratch &s, Budget &w, BoneQueryReport &r) const {
    const auto plane = row_plane(row, face, s, w, r);
    if (plane.side || !plane.inside)
      return false; // Actual exact p-on-closed-triangle proof is mandatory.
    auto &source = source_plane(face, head, tail, s, w, r);
    if (source.head && source.head == source.tail)
      return true;
    // At |coordinate|<=10000, each exact binary32 difference has <=225bits.
    // This degree-four supporting-plane predicate needs <908bits, within the
    // checked1024-bit arithmetic. Its fixed temporaries are admitted separately
    // before query geometry/scratch copies; no heap or epsilon is involved.
    static_assert(4 * 225 + 8 <= sizeof(volume_exact::Integer{}.limb) * 8);
    w.step(32);
    volume_exact::Arithmetic exact(w);
    const auto &ids = faces_[face];
    using V = std::array<volume_exact::Integer, 3>;
    auto difference = [&](P a, P b) {
      V out;
      for (size_t k = 0; k < 3; ++k)
        out[k] = exact.subtract(exact.scalar(a[k]), exact.scalar(b[k]));
      return out;
    };
    auto cross = [&](const V &a, const V &b) {
      V out;
      for (size_t k = 0; k < 3; ++k)
        out[k] = exact.subtract(exact.multiply(a[(k + 1) % 3], b[(k + 2) % 3]),
                                exact.multiply(a[(k + 2) % 3], b[(k + 1) % 3]));
      return out;
    };
    auto dot = [&](const V &a, const V &b) {
      auto value = exact.multiply(a[0], b[0]);
      for (size_t k = 1; k < 3; ++k)
        value = exact.add(value, exact.multiply(a[k], b[k]));
      return value.sign;
    };
    for (size_t k = 0; k < 3; ++k) {
      const auto a = points_[ids[k]], b = points_[ids[(k + 1) % 3]],
                 c = points_[ids[(k + 2) % 3]];
      const auto edge = difference(b, a),
                 inward = cross(edge, difference(c, a));
      const int p = dot(cross(edge, difference(points_[row], a)), inward);
      if (p < 0)
        throw std::logic_error(
            "Exact closed-triangle supporting edge mismatch");
      if (p)
        continue;
      // p lies on this actual closed triangle edge. The triangle is in its
      // closed inward halfspace. Strictly outward BOTH finite endpoints prove
      // the entire source and every t>0 point of every ray stays outside it.
      const int h = dot(cross(edge, difference(head, a)), inward);
      if (h < 0 &&
          (head == tail || dot(cross(edge, difference(tail, a)), inward) < 0))
        return true;
    }
    return false;
  }
  template <class Budget>
  void validate_origin(uint32_t row, BoneWallDomain domain, P head, P tail,
                       BoneOriginPolicy origin_policy, Scratch &s, Budget &w,
                       BoneQueryReport &r) const {
    const uint8_t bit = domain == BoneWallDomain::WholeGroup ? 2 : 1;
    w.step();
    if (origin_policy == BoneOriginPolicy::StrictOwnership &&
        (s.origin_checked[row] & bit))
      return;
    const Box point{points_[row], points_[row]};
    s.stack.clear();
    if (domain == BoneWallDomain::ConnectedSourceSurface) {
      auto root = component_roots_[components_[row]];
      if (root != UINT32_MAX)
        s.stack.push_back(root);
    } else
      for (auto root : forest_)
        s.stack.push_back(root);
    while (!s.stack.empty()) {
      w.stop();
      auto node = s.stack.back();
      s.stack.pop_back();
      const auto &n = nodes_[node];
      if (!overlaps(point, n.box, w))
        continue;
      if (!n.count) {
        s.stack.push_back(n.right);
        s.stack.push_back(n.left);
        continue;
      }
      for (size_t at = n.first; at < n.first + n.count; ++at) {
        w.step(4);
        const auto face = order_[at];
        const auto &ids = faces_[face];
        if (std::any_of(ids.begin(), ids.end(),
                        [&](uint32_t id) {
                          return incidence_[id] == incidence_[row];
                        }) ||
            !overlaps(point, boxes_[face], w))
          continue;
        auto plane = row_plane(row, face, s, w, r);
        if (!plane.side && plane.inside) {
          if (origin_policy == BoneOriginPolicy::NativeOpenSegment) {
            // Affine plane distance is strictly nonzero throughout a segment
            // when both exact endpoint signs agree. Each open ray therefore
            // meets this plane only at excluded t=0. This proof belongs to
            // this source epoch, never to source-independent origin_checked.
            if (open_origin_clear(row, face, head, tail, s, w, r))
              continue;
          }
          throw BoneQueryAmbiguity(bone_shadow::Kind::OriginContact, face);
        }
      }
    }
    if (origin_policy == BoneOriginPolicy::StrictOwnership)
      s.origin_checked[row] |= bit == 2 ? 3 : 1;
  }
  template <class Budget>
  static bool add_coverage(std::vector<volume_exact::Interval> &coverage,
                           volume_exact::Interval piece,
                           const volume_exact::Fraction &lo,
                           const volume_exact::Fraction &hi, Budget &w) {
    volume_exact::Arithmetic exact(w);
    auto first = std::lower_bound(coverage.begin(), coverage.end(), piece.enter,
                                  [&](const auto &a, const auto &b) {
                                    return exact.compare(a.exit, b) < 0;
                                  });
    auto end = first;
    while (end != coverage.end() &&
           exact.compare(end->enter, piece.exit) <= 0) {
      if (exact.compare(end->enter, piece.enter) < 0)
        piece.enter = end->enter;
      if (exact.compare(end->exit, piece.exit) > 0)
        piece.exit = end->exit;
      ++end;
    }
    // Charge bounded exact-interval movement, not merely one vector call.
    constexpr size_t words = (sizeof(volume_exact::Interval) + 7) / 8;
    const size_t moved = first == end       ? size_t(coverage.end() - first)
                         : end == first + 1 ? 0
                                            : size_t(coverage.end() - end);
    w.step(words * (moved + 1));
    if (first == end)
      first = coverage.insert(first, piece);
    else {
      *first = piece;
      coverage.erase(first + 1, end);
    }
    return exact.compare(first->enter, lo) <= 0 &&
           exact.compare(first->exit, hi) >= 0;
  }
  template <class Budget>
  SourcePlane &source_plane(uint32_t face, P head, P tail, Scratch &s,
                            Budget &w, BoneQueryReport &r) const {
    w.step(2);
    auto &cached = s.sources[face];
    if (cached.epoch == s.source_epoch) {
      ++r.source_hits;
      return cached;
    }
    ++r.source_misses;
    SourcePlane result;
    result.epoch = s.source_epoch;
    result.head = plane_sign(face, head, w);
    result.tail = head == tail ? result.head : plane_sign(face, tail, w);
    cached = result;
    return cached;
  }
  template <class Budget>
  static Range cheap_product(Range a, Range b, Budget &w) {
    double lo, hi;
    w.step(2);
    if (a.lo >= 0) {
      w.step(2);
      if (b.lo >= 0) {
        lo = a.lo * b.lo;
        hi = a.hi * b.hi;
      } else if (b.hi <= 0) {
        lo = a.hi * b.lo;
        hi = a.lo * b.hi;
      } else {
        lo = a.hi * b.lo;
        hi = a.hi * b.hi;
      }
    } else if (a.hi <= 0) {
      w.step(2);
      if (b.lo >= 0) {
        lo = a.lo * b.hi;
        hi = a.hi * b.lo;
      } else if (b.hi <= 0) {
        lo = a.hi * b.hi;
        hi = a.lo * b.lo;
      } else {
        lo = a.lo * b.hi;
        hi = a.lo * b.lo;
      }
    } else {
      w.step(2);
      if (b.lo >= 0) {
        lo = a.lo * b.hi;
        hi = a.hi * b.hi;
      } else if (b.hi <= 0) {
        lo = a.hi * b.lo;
        hi = a.lo * b.lo;
      } else {
        w.step(4);
        lo = std::min(a.lo * b.hi, a.hi * b.lo);
        hi = std::max(a.lo * b.lo, a.hi * b.hi);
      }
    }
    w.step(4);
    return {std::nextafter(lo, -INFINITY), std::nextafter(hi, INFINITY)};
  }
  template <class Budget>
  int edge_sign(uint32_t face, size_t edge, bool tail, P origin,
                const std::array<Range, 3> &row_delta, SourcePlane &source,
                Budget &w) const {
#if defined(__FAST_MATH__)
    (void)face;
    (void)edge;
    (void)tail;
    (void)origin;
    (void)row_delta;
    (void)source;
    (void)w;
    return 0;
#else
    const size_t index = edge * 2 + size_t(tail);
    if (!(source.edge_mask & (1u << index))) {
      w.step(64);
      const auto &f = faces_[face];
      std::array<Range, 3> a, b, normal;
      for (size_t k = 0; k < 3; ++k) {
        a[k] = difference(points_[f[edge]][k], origin[k]);
        b[k] = difference(points_[f[(edge + 1) % 3]][k], origin[k]);
      }
      for (size_t k = 0; k < 3; ++k)
        normal[k] = sum(product(a[(k + 1) % 3], b[(k + 2) % 3]),
                        negative(product(a[(k + 2) % 3], b[(k + 1) % 3])));
      source.edge_normals[index] = normal;
      source.edge_mask |= uint8_t(1u << index);
    }
    Range value;
    for (size_t k = 0; k < 3; ++k) {
      auto term = cheap_product(source.edge_normals[index][k], row_delta[k], w);
      w.step(4);
      value = sum(value, term);
    }
    w.step(2);
    // orient(row,source,b,c) = -orient(source,b,c,row).
    if (value.lo > 0)
      return -1;
    if (value.hi < 0)
      return 1;
    return 0;
#endif
  }
  template <class Budget>
  bone_shadow::Result cached_shadow(uint32_t row, uint32_t face, P head, P tail,
                                    Scratch &s, Budget &w, BoneQueryReport &r,
                                    bone_shadow::FilterReport &report,
                                    const Fan &fan,
                                    BoneOriginPolicy origin_policy) const {
    using K = bone_shadow::Kind;
    const auto &f = faces_[face];
    const P p = points_[row];
    const auto row_side = row_plane(row, face, s, w, r);
    if (row_side.side < 0) {
      ++report.rejected;
      return {};
    }
    if (!row_side.side && row_side.inside) {
      if (origin_policy == BoneOriginPolicy::NativeOpenSegment) {
        if (open_origin_clear(row, face, head, tail, s, w, r)) {
          ++report.rejected;
          return {};
        }
      }
      return {K::OriginContact, {}};
    }
    auto &source = source_plane(face, head, tail, s, w, r);
    if ((row_side.side > 0 && source.head > 0 && source.tail > 0) ||
        (!row_side.side && source.head && source.head == source.tail)) {
      ++report.rejected;
      return {};
    }
    if (row_side.side > 0) {
      volume_exact::Arithmetic exact(w);
      volume_exact::Interval clipped{exact.zero(), exact.one()};
      bool used_exact = false;
      auto mark = [&] {
        if (!used_exact) {
          ++report.exact;
          used_exact = true;
        }
      };
      auto update = [&](volume_exact::Integer a, volume_exact::Integer b) {
        auto at = bone_shadow::ratio(a, exact.subtract(a, b), exact);
        if (a.sign > 0) {
          if (exact.compare(at, clipped.enter) > 0)
            clipped.enter = at;
        } else if (exact.compare(at, clipped.exit) < 0)
          clipped.exit = at;
        return exact.compare(clipped.enter, clipped.exit) <= 0;
      };
      // A coefficient pair that is already proved nonpositive needs no exact
      // magnitude. Only actual crossing constraints require rational roots.
      if (source.head > 0 || source.tail > 0) {
        mark();
        if (!source.values) {
          std::array<volume_exact::Integer, 2> values{
              bone_shadow::determinant(points_[f[0]], points_[f[1]],
                                       points_[f[2]], head, exact),
              bone_shadow::determinant(points_[f[0]], points_[f[1]],
                                       points_[f[2]], tail, exact)};
          source.values = std::move(values);
        }
        if (!update((*source.values)[0], (*source.values)[1]))
          return {};
      }
      for (size_t c = 0; c < 3; ++c) {
        int a = edge_sign(face, c, false, head, fan.from_head, source, w);
        int b = head == tail
                    ? a
                    : edge_sign(face, c, true, tail, fan.from_tail, source, w);
        std::optional<volume_exact::Integer> av, bv;
        if (!a) {
          mark();
          av = bone_shadow::determinant(p, head, points_[f[c]],
                                        points_[f[(c + 1) % 3]], exact);
          a = av->sign;
        }
        if (!b) {
          mark();
          bv = head == tail && av
                   ? *av
                   : bone_shadow::determinant(p, tail, points_[f[c]],
                                              points_[f[(c + 1) % 3]], exact);
          b = bv->sign;
        }
        if (a > 0 && b > 0) {
          ++report.rejected;
          return {};
        }
        if (a <= 0 && b <= 0)
          continue;
        mark();
        if (!av)
          av = bone_shadow::determinant(p, head, points_[f[c]],
                                        points_[f[(c + 1) % 3]], exact);
        if (!bv)
          bv = bone_shadow::determinant(p, tail, points_[f[c]],
                                        points_[f[(c + 1) % 3]], exact);
        if (!update(*av, *bv))
          return {};
      }
      if (!used_exact)
        ++report.full;
      return {K::Interval, clipped};
    }
    ++report.exact;
    return bone_shadow::triangle_shadow(
        p, head, tail, {points_[f[0]], points_[f[1]], points_[f[2]]}, w);
  }

public:
  static constexpr size_t native_origin_workspace = 16u * 1024u;
  size_t planned_bytes = 0, construction_forecast = 0;
  template <class Budget>
  BoneShadowQuery(
      std::span<const P> points, std::span<const uint32_t> indices,
      size_t maximum_bytes, Budget &w, std::span<const uint32_t> incidence = {},
      BoneOriginPolicy origin_admission = BoneOriginPolicy::StrictOwnership)
      : origin_admission_(origin_admission) {
    w.stop();
    const size_t count = indices.size() / 3;
    if ((origin_admission != BoneOriginPolicy::StrictOwnership &&
         origin_admission != BoneOriginPolicy::NativeOpenSegment) ||
        points.empty() || points.size() > 2'000'000 || !count ||
        count > 4'000'000 || indices.size() % 3)
      throw std::invalid_argument("Invalid owning bone query dimensions");
    const size_t depth = std::bit_width(count);
    construction_forecast =
        count * (64 + 16 * depth * depth) + points.size() * 8;
    planned_bytes =
        points.size() * (sizeof(P) + 3 * sizeof(uint32_t) + sizeof(uint8_t)) +
        count *
            (sizeof(std::array<uint32_t, 3>) + sizeof(Box) + sizeof(uint32_t) +
             2 * sizeof(Node) + sizeof(volume_exact::Interval) +
             2 * sizeof(uint32_t) + sizeof(SourcePlane) + sizeof(uint32_t)) +
        cache_slots(points.size()) * sizeof(RowPlane) + 65536;
    if (origin_admission == BoneOriginPolicy::NativeOpenSegment)
      planned_bytes += native_origin_workspace;
    if (planned_bytes > maximum_bytes ||
        construction_forecast > w.maximum - w.used)
      throw std::length_error(
          "Owning bone query construction exceeds forecast before copies");
    for (auto p : points)
      for (float value : p) {
        w.step();
        if (!std::isfinite(value) || std::abs(value) > 10000)
          throw std::invalid_argument(
              "Bone query geometry outside exact workspace");
      }
    for (auto i : indices) {
      w.step();
      if (i >= points.size())
        throw std::invalid_argument(
            "Bone query corner outside owning geometry");
    }
    if (!incidence.empty() && incidence.size() != points.size())
      throw std::invalid_argument("Visibility incidence dimensions differ");
    incidence_.resize(points.size());
    for (uint32_t i = 0; i < points.size(); ++i) {
      w.step(4);
      auto root = incidence.empty() ? i : incidence[i];
      if (root >= points.size() ||
          (!incidence.empty() && incidence[root] != root) ||
          points[root] != points[i])
        throw std::invalid_argument(
            "Visibility incidence lacks exact owning geometry");
      incidence_[i] = root;
    }
    identity_ = std::make_shared<const uint8_t>(0);
    points_.assign(points.begin(), points.end());
    faces_.reserve(count);
    boxes_.reserve(count);
    order_.resize(count);
    nodes_.reserve(count * 2);
    std::iota(order_.begin(), order_.end(), 0);
    volume_exact::Arithmetic exact(w);
    for (size_t c = 0; c < indices.size(); c += 3) {
      w.step(16);
      std::array<uint32_t, 3> f{indices[c], indices[c + 1], indices[c + 2]};
      const auto a = points_[f[0]], b = points_[f[1]], d = points_[f[2]];
      bool area = false;
      for (size_t drop = 0; drop < 3 && !area; ++drop) {
        auto project = [&](P p) {
          return std::array<float, 2>{p[(drop + 1) % 3], p[(drop + 2) % 3]};
        };
        area = exact.orient2(project(a), project(b), project(d)).sign != 0;
      }
      if (!area)
        throw std::invalid_argument("Bone query degenerate wall");
      Box box{a, a};
      for (size_t k = 0; k < 3; ++k) {
        box.lo[k] = std::min({a[k], b[k], d[k]});
        box.hi[k] = std::max({a[k], b[k], d[k]});
      }
      faces_.push_back(f);
      boxes_.push_back(box);
    }
    components_.resize(points.size());
    std::iota(components_.begin(), components_.end(), 0);
    auto root = [&](uint32_t i) {
      while (components_[i] != i) {
        w.step();
        components_[i] = components_[components_[i]];
        i = components_[i];
      }
      return i;
    };
    for (const auto &face : faces_) {
      w.step(3);
      components_[root(face[0])] = root(face[1]);
      components_[root(face[1])] = root(face[2]);
    }
    for (uint32_t i = 0; i < points.size(); ++i) {
      w.step();
      components_[i] = root(i);
    }
    // Each surface component owns a BVH. The local domain therefore never
    // traverses other materials/parts only to discard them at the leaves.
    // Whole-group control traverses the complete same immutable forest.
    component_roots_.assign(points.size(), UINT32_MAX);
    forest_.reserve(count);
    std::sort(order_.begin(), order_.end(), [&](uint32_t a, uint32_t b) {
      w.step(2);
      auto x = components_[faces_[a][0]], y = components_[faces_[b][0]];
      return x != y ? x < y : a < b;
    });
    for (size_t begin = 0; begin < count;) {
      const auto component = components_[faces_[order_[begin]][0]];
      size_t end = begin + 1;
      while (end < count && components_[faces_[order_[end]][0]] == component) {
        w.step();
        ++end;
      }
      const auto root = build(begin, end, w);
      component_roots_[component] = root;
      forest_.push_back(root);
      begin = end;
    }
  }
  size_t triangles() const { return faces_.size(); }
  size_t nodes() const { return nodes_.size(); }
  template <class Budget>
  double integrate(uint32_t row, P head, P tail, double mass, double lo,
                   double hi, Scratch &scratch, Budget &w,
                   BoneQueryReport &report,
                   BoneWallDomain domain = BoneWallDomain::WholeGroup,
                   bool use_filter = true, bool incremental_coverage = true,
                   BoneOriginPolicy origin_policy =
                       BoneOriginPolicy::StrictOwnership) const {
    w.stop();
    BoneQueryReport staged;
    const size_t query_start = w.used;
    bone_shadow::FilterReport filter_report;
    if (row >= points_.size() || !(mass > 0) || !std::isfinite(mass) ||
        lo < 0 || hi > 1 || lo > hi || !std::isfinite(lo) || !std::isfinite(hi))
      throw std::invalid_argument("Invalid owning source query");
    if ((origin_policy != BoneOriginPolicy::StrictOwnership &&
         origin_policy != BoneOriginPolicy::NativeOpenSegment) ||
        (origin_policy == BoneOriginPolicy::NativeOpenSegment &&
         (domain != BoneWallDomain::ConnectedSourceSurface ||
          origin_admission_ != BoneOriginPolicy::NativeOpenSegment)))
      throw std::invalid_argument("Native open-segment origins require their "
                                  "own connected wall component");
    if (scratch.shadows.capacity() < faces_.size() ||
        scratch.stack.capacity() < nodes_.size() ||
        scratch.rows.size() != cache_slots(points_.size()) ||
        scratch.sources.size() != faces_.size() ||
        scratch.origin_checked.size() != points_.size())
      throw std::invalid_argument("Unadmitted bone query scratch");
    if (lo == hi)
      return 0;
    const P p = points_[row];
    Box fan{p, p};
    for (size_t k = 0; k < 3; ++k) {
      if (!std::isfinite(head[k]) || !std::isfinite(tail[k]) ||
          std::abs(head[k]) > 10000 || std::abs(tail[k]) > 10000)
        throw std::invalid_argument("Source outside exact query workspace");
      fan.lo[k] = std::min({p[k], head[k], tail[k]});
      fan.hi[k] = std::max({p[k], head[k], tail[k]});
    }
    prepare(scratch, head, tail, w);
    validate_origin(row, domain, head, tail, origin_policy, scratch, w, staged);
    volume_exact::Arithmetic exact(w);
    const auto exact_lo =
        bone_shadow::ratio(exact.scalar(lo), exact.scalar(1.), exact);
    const auto exact_hi =
        bone_shadow::ratio(exact.scalar(hi), exact.scalar(1.), exact);
    bool fully_covered = false;
    const auto bounded_fan = prepare_fan(p, head, tail, w);
    scratch.shadows.clear();
    scratch.stack.clear();
    if (domain == BoneWallDomain::ConnectedSourceSurface) {
      auto root = component_roots_[components_[row]];
      if (root != UINT32_MAX)
        scratch.stack.push_back(root);
    } else
      for (auto root : forest_)
        scratch.stack.push_back(root);
    while (!scratch.stack.empty() && !fully_covered) {
      w.stop();
      auto at = scratch.stack.back();
      scratch.stack.pop_back();
      ++staged.nodes;
      const auto &node = nodes_[at];
      if (!overlaps(fan, node.box, w) ||
          (use_filter && !possible(bounded_fan, node.box, w)))
        continue;
      if (!node.count) {
        scratch.stack.push_back(node.right);
        scratch.stack.push_back(node.left);
        continue;
      }
      for (size_t at = node.first; at < node.first + node.count; ++at) {
        w.step(4);
        const auto face = order_[at];
        const auto &ids = faces_[face];
        if ((domain == BoneWallDomain::ConnectedSourceSurface &&
             components_[ids[0]] != components_[row]) ||
            std::any_of(ids.begin(), ids.end(),
                        [&](uint32_t id) {
                          return incidence_[id] == incidence_[row];
                        }) ||
            !overlaps(fan, boxes_[face], w) ||
            (use_filter && !possible(bounded_fan, boxes_[face], w)))
          continue;
        ++staged.candidates;
        const bone_shadow::Triangle wall{points_[ids[0]], points_[ids[1]],
                                         points_[ids[2]]};
        const auto predicate_start = w.used;
        auto shadow =
            use_filter
                ? cached_shadow(row, face, head, tail, scratch, w, staged,
                                filter_report, bounded_fan, origin_policy)
                : bone_shadow::triangle_shadow(p, head, tail, wall, w);
        if (!use_filter && shadow.kind == bone_shadow::Kind::OriginContact &&
            origin_policy == BoneOriginPolicy::NativeOpenSegment) {
          if (open_origin_clear(row, face, head, tail, scratch, w, staged))
            shadow = {};
        }
        staged.predicate_work += w.used - predicate_start;
        if (shadow.kind == bone_shadow::Kind::None)
          continue;
        if (shadow.kind != bone_shadow::Kind::Interval)
          throw BoneQueryAmbiguity(shadow.kind, face);
        if (incremental_coverage) {
          const auto union_start = w.used;
          fully_covered =
              add_coverage(scratch.shadows, std::move(shadow.interval),
                           exact_lo, exact_hi, w);
          staged.union_work += w.used - union_start;
        } else
          scratch.shadows.push_back(std::move(shadow.interval));
        ++staged.intervals;
        if (fully_covered)
          break;
      }
    }
    staged.search_work = w.used - query_start;
    const auto union_start = w.used;
    if (!incremental_coverage) {
      std::sort(scratch.shadows.begin(), scratch.shadows.end(),
                [&](const auto &a, const auto &b) {
                  w.step();
                  int c = exact.compare(a.enter, b.enter);
                  return c ? c < 0 : exact.compare(a.exit, b.exit) < 0;
                });
      size_t count = 0;
      for (const auto &piece : scratch.shadows) {
        w.step();
        if (count &&
            exact.compare(piece.enter, scratch.shadows[count - 1].exit) <= 0) {
          if (exact.compare(piece.exit, scratch.shadows[count - 1].exit) > 0)
            scratch.shadows[count - 1].exit = piece.exit;
        } else
          scratch.shadows[count++] = piece;
      }
      scratch.shadows.resize(count);
    }
    const size_t count = scratch.shadows.size();
    staged.unions += count;
    staged.union_work += w.used - union_start;
    const auto integration_start = w.used;
    double begin = lo, value = 0;
    auto exact_begin = exact_lo;
    const std::array<double, 3> row_d{p[0], p[1], p[2]},
        head_d{head[0], head[1], head[2]}, tail_d{tail[0], tail[1], tail[2]};
    for (size_t i = 0; i < count; ++i) {
      const auto &piece = scratch.shadows[i];
      if (exact.compare(piece.exit, exact_begin) <= 0)
        continue;
      const auto gap_end =
          exact.compare(piece.enter, exact_hi) < 0 ? piece.enter : exact_hi;
      if (exact.compare(exact_begin, gap_end) < 0) {
        const double enter = std::clamp(outward(gap_end, false, w), lo, hi);
        if (!(enter > begin))
          throw std::runtime_error(
              "Unresolved positive source visibility interval");
        value +=
            bone_line_integral(row_d, head_d, tail_d, mass, begin, enter, w);
      }
      if (exact.compare(piece.exit, exact_hi) >= 0) {
        exact_begin = exact_hi;
        begin = hi;
        break;
      }
      if (exact.compare(piece.exit, exact_begin) > 0) {
        exact_begin = piece.exit;
        begin =
            std::max(begin, std::clamp(outward(piece.exit, true, w), lo, hi));
      }
    }
    if (exact.compare(exact_begin, exact_hi) < 0) {
      if (!(begin < hi))
        throw std::runtime_error("Unresolved final source visibility interval");
      value += bone_line_integral(row_d, head_d, tail_d, mass, begin, hi, w);
    }
    w.stop();
    staged.integration_work = w.used - integration_start;
    staged.exact_predicates = filter_report.exact;
    staged.filtered_misses = filter_report.rejected;
    staged.filtered_full = filter_report.full;
    const std::array<size_t, 15> values{
        staged.nodes,         staged.candidates,       staged.intervals,
        staged.unions,        staged.exact_predicates, staged.filtered_misses,
        staged.filtered_full, staged.row_hits,         staged.row_misses,
        staged.source_hits,   staged.source_misses,    staged.search_work,
        staged.union_work,    staged.integration_work, staged.predicate_work};
    const std::array<size_t *, 15> destinations{&report.nodes,
                                                &report.candidates,
                                                &report.intervals,
                                                &report.unions,
                                                &report.exact_predicates,
                                                &report.filtered_misses,
                                                &report.filtered_full,
                                                &report.row_hits,
                                                &report.row_misses,
                                                &report.source_hits,
                                                &report.source_misses,
                                                &report.search_work,
                                                &report.union_work,
                                                &report.integration_work,
                                                &report.predicate_work};
    for (size_t i = 0; i < values.size(); ++i)
      if (values[i] > SIZE_MAX - *destinations[i])
        throw std::length_error("Bone query report overflow");
    for (size_t i = 0; i < values.size(); ++i)
      *destinations[i] += values[i];
    return value;
  }
};
} // namespace dome::detail
