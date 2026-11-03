// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/heat-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
void check(float gap, int axis, bool entry) {
  Mesh m;
  for (int layer = 0; layer < 2; ++layer) {
    float e = layer ? 1 : .1f, z = layer ? gap : 0;
    uint32_t b = m.positions.size();
    for (auto p : std::array<Vec3, 4>{
             {{-e, -e, z}, {e, -e, z}, {e, e, z}, {-e, e, z}}}) {
      m.positions.push_back(p);
      m.normals.push_back({0, 0, 1});
      m.uv.push_back({0, 0});
    }
    if (layer && entry)
      m.indices.insert(m.indices.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
    else
      m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  }
  m.indices.insert(m.indices.end(), {1, 5, 6, 1, 6, 2});
  for (auto &p : m.positions) {
    if (axis == 1)
      p = {p.y, p.z, p.x};
    if (axis == 2)
      p = {p.z, p.x, p.y};
  }
  Skeleton rig;
  Bone b;
  b.name = "Point source";
  b.rest.translation = {0, 0, .2f};
  rig.bones.push_back(b);
  HeatSourceSegment source{{0, 0, .2}, {0, 0, .2}, 0};
  if (axis == 1)
    source.head = source.tail = {0, .2, 0};
  if (axis == 2)
    source.head = source.tail = {.2, 0, 0};
  HeatBindingOptions o;
  o.operation = HeatOperator::PositiveGraph;
  o.visibility = HeatVisibility::EntryFaces;
  o.occlusion = HeatOcclusion::ConnectedSurface;
  o.maximum_seed_distance = 2;
  std::function<bool()> cancel;
  Budget budget{0, 10'000'000, cancel};
  auto s = assemble(m, budget, o.operation, {});
  seed(s, m, rig, o, budget, {&source, 1}, {});
  std::cout << "NEAR_CONTACT gap=" << gap << " axis=" << axis
            << " entry=" << entry << " lower_heat=" << s.heat[0]
            << " components=" << s.components << '\n';
  if (entry ? s.heat[0] != 0 : !(s.heat[0] > 0))
    throw std::runtime_error("Visibility origin offset skipped a real "
                             "same-surface near-contact entry wall");
}
int main() try {
  for (float gap : {1e-8f, 1e-7f, 1e-6f, 1e-5f, .01f})
    for (int axis = 0; axis < 3; ++axis) {
      check(gap, axis, true);
      check(gap, axis, false);
    }
  std::cout << "PASS 30 genuine connected near-contact entry/exit cases across "
               "gaps and coordinate axes\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
