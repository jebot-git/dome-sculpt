// SPDX-License-Identifier: GPL-3.0-or-later
#include "heat-seams.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << '\n';
}
Mesh split() {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0},
                 {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  m.normals.assign(6, {0, 0, 1});
  m.uv = {{0, 0}, {.4f, 0}, {0, .4f}, {.6f, .6f}, {1, .6f}, {.6f, 1}};
  m.indices = {0, 1, 2, 3, 4, 5};
  return m;
}
SculptObject owner(Mesh m) {
  SculptObject o;
  o.id = 7;
  o.mesh_active = true;
  o.painted = std::make_shared<PaintedMesh>(std::move(m), 64);
  DeformationState d(o.painted->mesh());
  Bone a;
  a.name = "Left";
  a.length = .8f;
  a.rest.translation = {0, 0, -.1f};
  auto b = a;
  b.name = "Right";
  b.rest.translation.x = 1;
  d.skeleton = Skeleton{};
  d.skeleton->bones = {a, b};
  d.skin = SkinWeights{};
  d.skin->vertices.assign(o.painted->mesh().positions.size(), {{0, 1}});
  d.morphs = MorphSet(o.painted->mesh());
  d.morphs->targets.push_back(
      {"Exact morph", std::vector<Vec3>(o.painted->mesh().positions.size())});
  o.deformation = std::make_shared<const DeformationState>(std::move(d));
  return o;
}
template <class F> void reject(F f, const char *m) {
  bool yes = false;
  try {
    f();
  } catch (const std::exception &) {
    yes = true;
  }
  check(yes, m);
}
} // namespace
int main() try {
  HeatBindingOptions options;
  options.operation = HeatOperator::PositiveGraph;
  options.maximum_seed_distance = 2;
  auto o = owner(split());
  auto before = o.deformation->encode(o.painted->mesh());
  auto geometry = o.painted->mesh_owner();
  auto graph = capture_heat_seams(o, options);
  auto groups = graph.groups(o);
  check(graph.joined_edges() == 1 && groups[1] == groups[3] &&
            groups[2] == groups[5],
        "Exact raw UV seam with identical all-pose skin/morph witnesses joins "
        "only virtual rows");
  HeatBindingReport report;
  auto skin = heat_bind_surface(o.painted->mesh(), *o.deformation->skeleton,
                                options, {}, &report, {}, {}, groups);
  check(report.matrix_vertices == 4 && skin.vertices.size() == 6,
        "Production heat matrix contracts certified seam and scatters to every "
        "original raw vertex");
  check(skin.vertices[1] == skin.vertices[3] &&
            skin.vertices[2] == skin.vertices[5],
        "Scattered seam weights preserve exact all-pose coincidence");
  bool solved = false;
  for (auto &row : skin.vertices)
    for (auto i : row)
      solved |= i.joint == 1 && i.weight > .1f;
  check(solved, "Artist skin rows authenticate continuity but are not copied "
                "as solved weights");
  check(o.deformation->encode(o.painted->mesh()) == before &&
            o.painted->mesh_owner() == geometry,
        "Virtual seam construction leaves original rig/morph/skin/topology "
        "owner exact");
  auto foreign = o;
  foreign.id = 8;
  reject([&] { graph.groups(foreign); },
         "Foreign object cannot borrow an equal captured geometry certificate");
  foreign = o;
  foreign.deformation =
      std::make_shared<const DeformationState>(*o.deformation);
  reject(
      [&] { graph.groups(foreign); },
      "Copied equal skin/bind owner cannot borrow captured seam certificate");
  auto bind = *o.deformation;
  bind.skeleton->inverse_bind_override = bind.skeleton->inverse_bind();
  bind.skeleton->inverse_bind_override->at(0)[12] += .1f;
  foreign = o;
  foreign.deformation = std::make_shared<const DeformationState>(bind);
  reject([&] { graph.groups(foreign); },
         "Changed effective bind authority invalidates captured seam graph");
  auto different = *o.deformation;
  different.skin->vertices[5] = {{1, 1}};
  foreign = o;
  foreign.deformation = std::make_shared<const DeformationState>(different);
  check(capture_heat_seams(foreign, options).joined_edges() == 0,
        "Different skin semantics do not authorize geometric seam welding");
  different = *o.deformation;
  different.morphs->targets[0].delta[5].z = .01f;
  foreign = o;
  foreign.deformation = std::make_shared<const DeformationState>(different);
  check(capture_heat_seams(foreign, options).joined_edges() == 0,
        "Different exact morph POSITION deltas do not authorize seam welding");
  different = *o.deformation;
  different.skin->vertices[1] = {{0, .3f}, {1, .7f}};
  different.skin->vertices[3] = {{1, .7f}, {0, .3f}};
  foreign = o;
  foreign.deformation = std::make_shared<const DeformationState>(different);
  check(capture_heat_seams(foreign, options).joined_edges() == 1,
        "Skin equivalence compares exact joint/weight semantics rather than "
        "row order");
  different.skin->vertices[3] = {{1, std::nextafter(.7f, 1.f)},
                                 {0, 1 - std::nextafter(.7f, 1.f)}};
  foreign.deformation = std::make_shared<const DeformationState>(different);
  check(capture_heat_seams(foreign, options).joined_edges() == 0,
        "Value-near skin weights never grant exact seam authority");
  auto near = split();
  near.positions[5].x = std::nextafter(0.f, 1.f);
  check(capture_heat_seams(owner(near), options).joined_edges() == 0,
        "One-subnormal position gap is not welded by proximity");
  auto overlap = split();
  overlap.positions[3] = {1, 0, 0};
  overlap.positions[4] = {0, 0, 0};
  overlap.positions[5] = {.2f, .2f, 0};
  check(capture_heat_seams(owner(overlap), options).joined_edges() == 0,
        "Overlapping coplanar incident faces cannot grant seam authority");
  auto touch = split();
  touch.positions[3] = {0, 0, 0};
  touch.positions[4] = {-1, 0, 0};
  touch.positions[5] = {0, -1, 0};
  check(capture_heat_seams(owner(touch), options).joined_edges() == 0,
        "Point-only touching surfaces remain independent");
  Mesh ambiguous;
  for (int t = 0; t < 3; ++t) {
    for (int k = 0; k < 3; ++k) {
      ambiguous.positions.push_back(split().positions[size_t(k)]);
      ambiguous.normals.push_back({0, 0, 1});
      auto uv = std::array<float, 2>{float(t % 2) * .5f, float(t / 2) * .5f};
      if (k == 1)
        uv[0] += .3f;
      if (k == 2)
        uv[1] += .3f;
      ambiguous.uv.push_back(uv);
      ambiguous.indices.push_back(uint32_t(t * 3 + k));
    }
  }
  auto ambiguous_graph = capture_heat_seams(owner(ambiguous), options);
  check(ambiguous_graph.joined_edges() == 0 &&
            ambiguous_graph.ambiguous_edges() == 3,
        "Three coincident boundary candidates cannot be arbitrarily paired");
  foreign = o;
  foreign.painted =
      std::shared_ptr<PaintedMesh>(o.painted.get(), [](PaintedMesh *) {});
  reject([&] { graph.groups(foreign); },
         "Equal paint address with foreign shared ownership cannot borrow seam "
         "authority");
  auto cancel = options;
  unsigned polls = 0;
  reject([&] { capture_heat_seams(o, cancel, [&] { return ++polls >= 1; }); },
         "Seam capture cancellation returns no partially accepted map");
  cancel.maximum_work = 1;
  reject([&] { capture_heat_seams(o, cancel); },
         "Seam capture enforces work before construction");
  cancel = options;
  cancel.maximum_bytes = 4096;
  reject([&] { capture_heat_seams(o, cancel); },
         "Seam capture enforces scratch bytes before construction");
  std::cout << "PASS " << checks
            << " owner/skin/morph/bind/gap/contact/matrix seam checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
