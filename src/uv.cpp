// SPDX-License-Identifier: GPL-3.0-or-later
// Chart traversal adapted from Wings 3D AutoUV uv_to_charts/collect_chart_fun.
// Copyright (c) 2001-2011 Dan Gudmundsson, Bjorn Gustavsson.
// Original permissive notice: third_party/wings/license.terms.
#include "dome/uv.hpp"
#include <bit>
#include <map>
#include <numeric>
#include <set>
namespace dome {
namespace {
using P = std::array<float, 2>;
P add(P a, P b) { return {a[0] + b[0], a[1] + b[1]}; }
P sub(P a, P b) { return {a[0] - b[0], a[1] - b[1]}; }
P mul(P a, float f) { return {a[0] * f, a[1] * f}; }
float area(P a, P b, P c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}
uint32_t next(uint32_t c) { return c / 3 * 3 + (c + 1) % 3; }
struct Dsu {
  std::vector<uint32_t> p;
  Dsu(size_t n) : p(n) { std::iota(p.begin(), p.end(), 0); }
  uint32_t root(uint32_t n) {
    auto r = n;
    while (p[r] != r)
      r = p[r];
    while (p[n] != n) {
      auto next = p[n];
      p[n] = r;
      n = next;
    }
    return r;
  }
  void join(uint32_t a, uint32_t b) {
    a = root(a);
    b = root(b);
    if (a != b)
      p[std::max(a, b)] = std::min(a, b);
  }
};
bool overlap(const Mesh &m, uint32_t f, uint32_t g) {
  using D = std::array<double, 2>;
  auto point = [&](uint32_t face, int k) {
    auto p = m.uv[m.indices[face * 3 + k]];
    return D{p[0], p[1]};
  };
  auto cross = [](D a, D b, D c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  };
  std::vector<D> poly{point(f, 0), point(f, 1), point(f, 2)};
  D b[] = {point(g, 0), point(g, 1), point(g, 2)};
  double tolerance =
      std::max(1e-16, std::min(std::abs(cross(poly[0], poly[1], poly[2])),
                               std::abs(cross(b[0], b[1], b[2]))) *
                          1e-9);
  double sign = cross(b[0], b[1], b[2]) > 0 ? 1 : -1;
  for (int k = 0; k < 3 && !poly.empty(); ++k) {
    std::vector<D> out;
    for (size_t i = 0; i < poly.size(); ++i) {
      auto a = poly[i], c = poly[(i + 1) % poly.size()];
      double da = sign * cross(b[k], b[(k + 1) % 3], a),
             dc = sign * cross(b[k], b[(k + 1) % 3], c);
      if (da >= 0)
        out.push_back(a);
      if ((da < 0) != (dc < 0)) {
        double t = da / (da - dc);
        out.push_back({a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t});
      }
    }
    poly = std::move(out);
  }
  double sum = 0;
  for (size_t i = 0; i < poly.size(); ++i)
    sum += poly[i][0] * poly[(i + 1) % poly.size()][1] -
           poly[i][1] * poly[(i + 1) % poly.size()][0];
  return std::abs(sum) > tolerance;
}
} // namespace
UvTopology::UvTopology(const Mesh &m) {
  m.validate();
  if (m.uv.size() != m.positions.size())
    throw std::invalid_argument("UV editing requires an unwrapped mesh");
  std::map<std::array<float, 3>, uint32_t> points;
  std::vector<uint32_t> canonical;
  for (auto p : m.positions) {
    auto [it, added] =
        points.emplace(std::array{p.x, p.y, p.z}, uint32_t(canonical.size()));
    canonical.push_back(it->second);
  }
  std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> adjacency;
  for (uint32_t c = 0; c < m.indices.size(); ++c) {
    auto a = canonical[m.indices[c]], b = canonical[m.indices[next(c)]];
    adjacency[std::minmax(a, b)].push_back(c);
  }
  Dsu regions(m.indices.size() / 3);
  for (auto &[key, corners] : adjacency) {
    UvEdge e{m.indices[corners[0]], m.indices[next(corners[0])], corners,
             corners.size() != 2};
    if (corners.size() == 2) {
      auto c = corners[0], d = corners[1];
      bool reverse = canonical[m.indices[c]] == canonical[m.indices[next(d)]];
      auto da = reverse ? next(d) : d, db = reverse ? d : next(d);
      e.seam = m.uv[m.indices[c]] != m.uv[m.indices[da]] ||
               m.uv[m.indices[next(c)]] != m.uv[m.indices[db]] ||
               (!m.uv_seams.empty() && (m.uv_seams[c] || m.uv_seams[d]));
      if (!e.seam)
        regions.join(c / 3, d / 3);
    }
    edges.push_back(std::move(e));
  }
  face_island.resize(m.indices.size() / 3);
  std::map<uint32_t, uint32_t> ids;
  for (uint32_t f = 0; f < face_island.size(); ++f) {
    auto [it, added] = ids.emplace(regions.root(f), uint32_t(islands.size()));
    if (added)
      islands.emplace_back();
    face_island[f] = it->second;
    islands[it->second].push_back(f);
  }
}
bool uv_changed(const Mesh &a, const Mesh &b) {
  if (a.indices.size() != b.indices.size())
    return true;
  for (size_t c = 0; c < a.indices.size(); ++c) {
    auto i = a.indices[c], j = b.indices[c];
    if (a.uv[i] != b.uv[j] ||
        (a.uv_pins.empty() ? 0 : a.uv_pins[i]) !=
            (b.uv_pins.empty() ? 0 : b.uv_pins[j]) ||
        (a.uv_seams.empty() ? 0 : a.uv_seams[c]) !=
            (b.uv_seams.empty() ? 0 : b.uv_seams[c]))
      return true;
  }
  return false;
}
std::string validate_uv(const Mesh &m) {
  try {
    m.validate();
  } catch (const std::exception &e) {
    return e.what();
  }
  if (m.uv.size() != m.positions.size())
    return "The mesh has no UV coordinates";
  for (auto p : m.uv)
    if (p[0] < 0 || p[0] > 1 || p[1] < 0 || p[1] > 1)
      return "UVs must stay inside the texture tile";
  constexpr int grid = 64;
  std::vector<std::vector<uint32_t>> buckets(grid * grid);
  for (uint32_t f = 0; f < m.indices.size() / 3; ++f) {
    P p[3];
    for (int k = 0; k < 3; ++k)
      p[k] = m.uv[m.indices[f * 3 + k]];
    if (std::abs(area(p[0], p[1], p[2])) < 1e-12)
      return "A UV triangle is collapsed";
    int x0 = std::clamp(int(std::min({p[0][0], p[1][0], p[2][0]}) * grid), 0,
                        grid - 1),
        x1 = std::clamp(int(std::max({p[0][0], p[1][0], p[2][0]}) * grid), 0,
                        grid - 1);
    int y0 = std::clamp(int(std::min({p[0][1], p[1][1], p[2][1]}) * grid), 0,
                        grid - 1),
        y1 = std::clamp(int(std::max({p[0][1], p[1][1], p[2][1]}) * grid), 0,
                        grid - 1);
    std::set<uint32_t> visited;
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) {
        auto &bucket = buckets[y * grid + x];
        for (auto g : bucket)
          if (visited.insert(g).second && overlap(m, f, g))
            return "UV triangles overlap; move or repack the islands";
        bucket.push_back(f);
      }
  }
  return {};
}
UvWorkspace::UvWorkspace(Mesh m, uint32_t resolution)
    : resolution_(resolution) {
  if (resolution < 64 || resolution > 2048)
    throw std::invalid_argument("Invalid UV resolution");
  if (m.uv_pins.empty())
    m.uv_pins.resize(m.positions.size());
  if (m.uv_seams.empty())
    m.uv_seams.resize(m.indices.size());
  publish(std::move(m), true);
}
void UvWorkspace::remember(UvView before) {
  auto &m = *before.mesh;
  size_t bytes = m.positions.size() * sizeof(Vec3) * 2 +
                 m.uv.size() * sizeof(P) + m.indices.size() * 4 +
                 m.uv_pins.size() + m.uv_seams.size();
  undo_.push_back(std::move(before));
  redo_.clear();
  while (undo_.size() > 1 &&
         (undo_.size() > 32 || undo_.size() * bytes > 32 * 1024 * 1024))
    undo_.erase(undo_.begin());
}
void UvWorkspace::finish_gesture(bool cancel) {
  if (!gesture_)
    return;
  auto before = std::move(*gesture_);
  gesture_.reset();
  if (cancel)
    view_ = std::move(before);
  else if (uv_changed(*view_.mesh, *before.mesh))
    remember(std::move(before));
  view_.can_undo = !undo_.empty();
  view_.can_redo = !redo_.empty();
  view_.revision = ++revision_;
}
void UvWorkspace::publish(Mesh m, bool topology) {
  m.validate();
  if (!gesture_ && view_.mesh && uv_changed(m, *view_.mesh)) {
    remember(view_);
  }
  if (topology || !view_.topology)
    view_.topology = std::make_shared<UvTopology>(m);
  bool pending = gesture_ && uv_changed(m, *gesture_->mesh);
  view_.can_undo = pending || !undo_.empty();
  view_.can_redo = !pending && !redo_.empty();
  view_.error = validate_uv(m);
  view_.valid = view_.error.empty();
  view_.mesh = std::make_shared<Mesh>(std::move(m));
  view_.revision = ++revision_;
}
std::vector<uint32_t> UvWorkspace::selected() const {
  if (view_.vertices)
    return view_.vertex == UINT32_MAX ? std::vector<uint32_t>{}
                                      : std::vector<uint32_t>{view_.vertex};
  std::set<uint32_t> vertices;
  for (auto f :
       view_.topology->islands.at(view_.topology->face_island.at(view_.face)))
    for (int k = 0; k < 3; ++k)
      vertices.insert(view_.mesh->indices[f * 3 + k]);
  return {vertices.begin(), vertices.end()};
}
void UvWorkspace::edit(UvAction action, float x, float y) {
  if (!std::isfinite(x) || !std::isfinite(y))
    throw std::invalid_argument("Non-finite UV control input");
  if (action == UvAction::BeginGesture) {
    if (!gesture_)
      gesture_ = view_;
    return;
  }
  if (action == UvAction::EndGesture || action == UvAction::CancelGesture) {
    finish_gesture(action == UvAction::CancelGesture);
    return;
  }
  // Avoid roundoff from center + (point - center) for an identity transform.
  // A neutral wheel sample must not create an edit or destroy redo history.
  if ((action == UvAction::Move && x == 0 && y == 0) ||
      (action == UvAction::Scale && x == 1) ||
      (action == UvAction::Rotate && x == 0))
    return;
  // Picks, mode changes, topology operations and history commands are separate
  // actions. Only continuous transforms belong to an open gesture.
  if (action != UvAction::Move && action != UvAction::Scale &&
      action != UvAction::Rotate)
    finish_gesture();
  if (action == UvAction::Tool) {
    view_.tool = int(std::clamp(x, 0.f, 2.f));
    view_.revision = ++revision_;
    return;
  }
  if (action == UvAction::Undo || action == UvAction::Redo) {
    auto &source = action == UvAction::Undo ? undo_ : redo_,
         &dest = action == UvAction::Undo ? redo_ : undo_;
    if (!source.empty()) {
      dest.push_back(view_);
      int tool = view_.tool;
      view_ = source.back();
      view_.tool = tool;
      source.pop_back();
      view_.revision = ++revision_;
      view_.can_undo = !undo_.empty();
      view_.can_redo = !redo_.empty();
    }
    return;
  }
  if (action == UvAction::VertexMode) {
    view_.vertices = !view_.vertices;
    view_.vertex = UINT32_MAX;
    view_.revision = ++revision_;
    return;
  }
  if (action == UvAction::Next) {
    auto &t = *view_.topology;
    auto i = (t.face_island[view_.face] + 1) % t.islands.size();
    view_.face = t.islands[i][0];
    view_.vertex = UINT32_MAX;
    view_.revision = ++revision_;
    return;
  }
  if (action == UvAction::Pick) {
    const auto &m = *view_.mesh;
    P p{x, y};
    view_.cursor = p;
    if (view_.vertices) {
      float best = .04f * .04f;
      view_.vertex = UINT32_MAX;
      for (uint32_t i = 0; i < m.uv.size(); ++i) {
        auto d = sub(m.uv[i], p);
        float dist = d[0] * d[0] + d[1] * d[1];
        if (dist < best) {
          best = dist;
          view_.vertex = i;
        }
      }
      if (view_.vertex != UINT32_MAX)
        for (uint32_t f = 0; f < m.indices.size() / 3; ++f)
          for (int k = 0; k < 3; ++k)
            if (m.indices[f * 3 + k] == view_.vertex) {
              view_.face = f;
              goto found;
            }
    } else
      for (uint32_t f = 0; f < m.indices.size() / 3; ++f) {
        auto a = m.uv[m.indices[f * 3]], b = m.uv[m.indices[f * 3 + 1]],
             c = m.uv[m.indices[f * 3 + 2]];
        float d = area(a, b, c);
        if (area(p, b, c) / d >= -1e-6 && area(a, p, c) / d >= -1e-6 &&
            area(a, b, p) / d >= -1e-6) {
          view_.face = f;
          break;
        }
      }
  found:
    view_.revision = ++revision_;
    return;
  }
  auto chosen = selected();
  if (chosen.empty() && action != UvAction::Seam && action != UvAction::Pack &&
      action != UvAction::Relax) {
    view_.error = "Select a UV vertex first";
    view_.revision = ++revision_;
    return;
  }
  auto m = *view_.mesh;
  if (action == UvAction::Pin) {
    bool pin = std::any_of(chosen.begin(), chosen.end(),
                           [&](auto i) { return !m.uv_pins[i]; });
    for (auto i : chosen)
      m.uv_pins[i] = pin;
  } else if (action == UvAction::Seam) {
    // Mark the nearest edge of the selected face. Face-corner flags survive
    // save independently of coincident UVs, as in AutoUV's explicit cut set.
    x = view_.cursor[0];
    y = view_.cursor[1];
    uint32_t corner = view_.face * 3;
    float best = INFINITY;
    for (uint32_t k = 0; k < 3; ++k) {
      auto a = m.uv[m.indices[view_.face * 3 + k]],
           b = m.uv[m.indices[next(view_.face * 3 + k)]];
      auto d = sub(b, a), p = sub(P{x, y}, a);
      float len = d[0] * d[0] + d[1] * d[1],
            t = len > 0
                    ? std::clamp((p[0] * d[0] + p[1] * d[1]) / len, 0.f, 1.f)
                    : 0;
      auto delta = sub(p, mul(d, t));
      float dist = delta[0] * delta[0] + delta[1] * delta[1];
      if (dist < best) {
        best = dist;
        corner = view_.face * 3 + k;
      }
    }
    for (auto &e : view_.topology->edges)
      if (std::find(e.corners.begin(), e.corners.end(), corner) !=
          e.corners.end()) {
        bool cut = !m.uv_seams[corner];
        for (auto c : e.corners)
          m.uv_seams[c] = cut;
        break;
      }
    UvTopology cut(m);
    Dsu fans(m.indices.size());
    auto same_point = [&](uint32_t a, uint32_t b) {
      auto p = m.positions[m.indices[a]], q = m.positions[m.indices[b]];
      return p.x == q.x && p.y == q.y && p.z == q.z;
    };
    for (auto &e : cut.edges)
      if (!e.seam && e.corners.size() == 2) {
        auto a = e.corners[0], b = e.corners[1];
        bool reverse = same_point(a, next(b));
        fans.join(a, reverse ? next(b) : b);
        fans.join(next(a), reverse ? b : next(b));
      }
    Mesh split;
    split.uv_seams = m.uv_seams;
    std::map<std::array<uint32_t, 4>, uint32_t> vertices;
    for (uint32_t c = 0; c < m.indices.size(); ++c) {
      auto i = m.indices[c];
      auto n = m.normals[i];
      auto key = std::array{fans.root(c), std::bit_cast<uint32_t>(n.x),
                            std::bit_cast<uint32_t>(n.y),
                            std::bit_cast<uint32_t>(n.z)};
      auto [it, added] =
          vertices.emplace(key, uint32_t(split.positions.size()));
      if (added) {
        split.positions.push_back(m.positions[i]);
        split.normals.push_back(n);
        split.uv.push_back(m.uv[i]);
        split.uv_pins.push_back(m.uv_pins[i]);
      } else
        split.uv_pins[it->second] |= m.uv_pins[i];
      split.indices.push_back(it->second);
    }
    view_.vertex = UINT32_MAX;
    publish(std::move(split), true);
    return;
  } else if (action == UvAction::Relax) {
    std::vector<std::set<uint32_t>> neighbors(m.positions.size());
    std::vector<bool> boundary(m.positions.size());
    for (auto &e : view_.topology->edges) {
      for (auto c : e.corners) {
        auto a = m.indices[c], b = m.indices[next(c)];
        neighbors[a].insert(b);
        neighbors[b].insert(a);
        if (e.seam)
          boundary[a] = boundary[b] = true;
      }
    }
    auto island =
        view_.topology->islands[view_.topology->face_island[view_.face]];
    std::set<uint32_t> movable;
    for (auto f : island)
      for (int k = 0; k < 3; ++k)
        movable.insert(m.indices[f * 3 + k]);
    for (int iteration = 0; iteration < 20; ++iteration) {
      auto uv = m.uv;
      for (auto i : movable)
        if (!boundary[i] && !m.uv_pins[i] && !neighbors[i].empty()) {
          P average{};
          float weight = 0;
          for (auto j : neighbors[i]) {
            float w =
                1 / std::max(1e-6f, length(m.positions[i] - m.positions[j]));
            average = add(average, mul(m.uv[j], w));
            weight += w;
          }
          uv[i] = add(mul(m.uv[i], .5f), mul(average, .5f / weight));
        }
      // Backtrack rather than fold a chart or move a pinned boundary.
      bool valid = false;
      for (int step = 0; step < 10 && !valid; ++step) {
        valid = true;
        for (auto f : island) {
          auto a = m.indices[f * 3], b = m.indices[f * 3 + 1],
               c = m.indices[f * 3 + 2];
          if (area(uv[a], uv[b], uv[c]) * area(m.uv[a], m.uv[b], m.uv[c]) <=
              1e-16)
            valid = false;
        }
        if (!valid)
          for (auto i : movable)
            uv[i] = mul(add(uv[i], m.uv[i]), .5f);
      }
      if (valid)
        m.uv = std::move(uv);
    }
  } else if (action == UvAction::Pack) {
    struct Rect {
      float x, y, w, h;
    };
    struct Island {
      std::vector<uint32_t> vs;
      P low{1, 1}, high{};
      bool pinned = false;
    };
    std::vector<Island> islands;
    for (auto &faces : view_.topology->islands) {
      Island i;
      std::set<uint32_t> vs;
      for (auto f : faces)
        for (int k = 0; k < 3; ++k)
          vs.insert(m.indices[f * 3 + k]);
      i.vs.assign(vs.begin(), vs.end());
      for (auto v : i.vs) {
        for (int k = 0; k < 2; ++k) {
          i.low[k] = std::min(i.low[k], m.uv[v][k]);
          i.high[k] = std::max(i.high[k], m.uv[v][k]);
        }
        i.pinned |= m.uv_pins[v];
      }
      islands.push_back(std::move(i));
    }
    std::sort(islands.begin(), islands.end(), [](auto &a, auto &b) {
      auto x = sub(a.high, a.low), y = sub(b.high, b.low);
      return x[0] * x[1] > y[0] * y[1];
    });
    float pad = 4.f / resolution_;
    auto attempt = [&](float scale, std::vector<P> &uv) {
      std::vector<Rect> free{{0, 0, 1, 1}};
      auto subtract = [&](Rect box) {
        std::vector<Rect> next;
        for (auto r : free) {
          if (box.x >= r.x + r.w || box.x + box.w <= r.x ||
              box.y >= r.y + r.h || box.y + box.h <= r.y) {
            next.push_back(r);
            continue;
          }
          if (box.x > r.x)
            next.push_back({r.x, r.y, box.x - r.x, r.h});
          if (box.x + box.w < r.x + r.w)
            next.push_back(
                {box.x + box.w, r.y, r.x + r.w - box.x - box.w, r.h});
          if (box.y > r.y)
            next.push_back({r.x, r.y, r.w, box.y - r.y});
          if (box.y + box.h < r.y + r.h)
            next.push_back(
                {r.x, box.y + box.h, r.w, r.y + r.h - box.y - box.h});
        }
        free = std::move(next);
        for (size_t a = 0; a < free.size(); ++a)
          for (size_t b = a + 1; b < free.size();) {
            auto u = free[a], v = free[b];
            auto contains = [](Rect a, Rect b) {
              return b.x >= a.x - 1e-7 && b.y >= a.y - 1e-7 &&
                     b.x + b.w <= a.x + a.w + 1e-7 &&
                     b.y + b.h <= a.y + a.h + 1e-7;
            };
            if (contains(u, v))
              free.erase(free.begin() + b);
            else if (contains(v, u)) {
              free.erase(free.begin() + a);
              --a;
              break;
            } else
              ++b;
          }
      };
      for (auto &i : islands)
        if (i.pinned)
          subtract({i.low[0] - pad, i.low[1] - pad,
                    i.high[0] - i.low[0] + 2 * pad,
                    i.high[1] - i.low[1] + 2 * pad});
      for (auto &i : islands)
        if (!i.pinned) {
          auto size = mul(sub(i.high, i.low), scale);
          int best = -1;
          bool rotate = false;
          float score = INFINITY;
          for (size_t k = 0; k < free.size(); ++k)
            for (int turn = 0; turn < 2; ++turn) {
              float w = size[turn] + 2 * pad, h = size[1 - turn] + 2 * pad;
              auto r = free[k];
              if (w <= r.w && h <= r.h && std::min(r.w - w, r.h - h) < score) {
                best = int(k);
                rotate = turn;
                score = std::min(r.w - w, r.h - h);
              }
            }
          if (best < 0)
            return false;
          auto r = free[best];
          for (auto v : i.vs) {
            auto p = mul(sub(m.uv[v], i.low), scale);
            if (rotate)
              p = {size[1] - p[1], p[0]};
            uv[v] = {r.x + pad + p[0], r.y + pad + p[1]};
          }
          subtract({r.x, r.y, size[rotate] + 2 * pad, size[!rotate] + 2 * pad});
        }
      return true;
    };
    float lo = 0, hi = 1;
    std::vector<P> best = m.uv;
    auto trial = m.uv;
    if (!attempt(.0001f, trial))
      throw std::runtime_error("Pinned islands leave no room for packing");
    for (int k = 0; k < 4; ++k) {
      trial = m.uv;
      if (!attempt(hi, trial))
        break;
      lo = hi;
      best = trial;
      hi *= 2;
    }
    for (int k = 0; k < 20; ++k) {
      float mid = (lo + hi) / 2;
      trial = m.uv;
      if (attempt(mid, trial)) {
        lo = mid;
        best = trial;
      } else
        hi = mid;
    }
    if (lo == 0)
      throw std::runtime_error("UV islands do not fit with four-pixel padding");
    m.uv = std::move(best);
  } else {
    P center{};
    for (auto i : chosen)
      center = add(center, m.uv[i]);
    center = mul(center, 1.f / chosen.size());
    for (auto i : chosen)
      if (!m.uv_pins[i]) {
        if (action == UvAction::Move)
          m.uv[i] = add(m.uv[i], P{x, y});
        else if (action == UvAction::Scale)
          m.uv[i] = add(center,
                        mul(sub(m.uv[i], center), std::clamp(x, .01f, 100.f)));
        else if (action == UvAction::Rotate) {
          auto p = sub(m.uv[i], center);
          m.uv[i] = add(center, P{p[0] * std::cos(x) - p[1] * std::sin(x),
                                  p[0] * std::sin(x) + p[1] * std::cos(x)});
        }
      }
  }
  publish(std::move(m));
}
} // namespace dome
