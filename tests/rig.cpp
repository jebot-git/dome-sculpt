// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
static void require(bool value, const char *what) {
  if (!value)
    throw std::runtime_error(what);
  std::cout << "PASS " << what << '\n';
}
template <class F> void rejects(F fn, const char *what) {
  bool failed = false;
  try {
    fn();
  } catch (const std::exception &) {
    failed = true;
  }
  require(failed, what);
}
static Mesh plane() {
  Mesh m;
  m.positions = {{-1, 0, 0}, {1, 0, 0}, {-1, 1, 0}, {1, 1, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.indices = {0, 1, 2, 1, 3, 2};
  m.uv = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  return m;
}
static float weight(const VertexWeights &w, uint32_t joint) {
  for (auto i : w)
    if (i.joint == joint)
      return i.weight;
  return 0;
}
static Mesh hairpin() {
  Mesh m;
  m.positions = {{0, 0, 0},         {.006f, 0, 0}, {0, .006f, 0},
                 {.006f, .006f, 0}, {0, 1, 0},     {.006f, 1, 0},
                 {.025f, 0, 0},     {.031f, 0, 0}, {.025f, .006f, 0},
                 {.031f, .006f, 0}, {.025f, 1, 0}, {.031f, 1, 0}};
  m.normals.assign(m.positions.size(), {0, 0, 1});
  m.indices = {0, 1, 2, 1, 3, 2,  2, 3,  4,  3, 5, 4, 6, 7, 8,
               7, 9, 8, 8, 9, 10, 9, 11, 10, 1, 6, 3, 6, 8, 3};
  return m;
}
int main() try {
  Skeleton rig;
  Bone child;
  child.name = "Child";
  child.parent = 1;
  child.rest.translation = {0, 1, 0};
  child.length = 1;
  Bone root;
  root.name = "Root";
  root.length = 1;
  rig.bones = {child, root};
  auto rest = rig.skin_matrices({});
  for (auto &m : rest)
    for (int c = 0; c < 4; c++)
      for (int r = 0; r < 4; r++)
        require(std::abs(m[c * 4 + r] - (c == r ? 1.f : 0.f)) < 1e-5f,
                "Rest pose has identity skin transforms");
  auto segments = rig.segments();
  require(std::abs(segments[0][0].y - 1) < 1e-5f &&
              std::abs(segments[0][1].y - 2) < 1e-5f,
          "Bone handles expose hierarchical world-space head/tail segments");
  auto reflected = rig;
  reflected.bones[1].rest.scale.x = -1;
  auto reflected_bind = reflected.skin_matrices({});
  require(std::abs(reflected_bind[0][0] - 1) < 1e-5f,
          "Nonsingular reflected bone scales retain valid inverse binds");
  auto centimetres = rig;
  centimetres.bones[0].rest.scale = {.01f, .01f, .01f};
  centimetres.bones[1].rest.scale = {.0001f, .0001f, .0001f};
  auto centimetre_bind = centimetres.skin_matrices({});
  require(std::abs(centimetre_bind[0][0] - 1) < 1e-5f &&
              std::abs(centimetre_bind[0][13]) < 1e-5f,
          "Inverse binds remain scale-relative for nested centimetre joints");
  auto persisted = Skeleton::decode(rig.encode());
  require(persisted.bones[0].parent == 1,
          "Unordered bone hierarchy survives persistence");
  auto invalid = rig;
  invalid.bones[1].parent = 0;
  rejects([&] { invalid.validate(); }, "Skeleton cycles are rejected");
  invalid = rig;
  invalid.bones[0].rest.rotation.w = 2;
  rejects([&] { invalid.validate(); }, "Nonunit bone rotations are rejected");
  std::array<uint8_t, 3> locked{1, 0, 0};
  auto normalized =
      normalize_weights({{0, .3f}, {1, .8f}, {2, .1f}}, 3, locked, {2, 0});
  require(weight(normalized, 0) == .3f &&
              std::abs(weight(normalized, 1) - .7f) < 1e-6f &&
              weight(normalized, 2) == 0,
          "Pruning/caps reserve hard-locked mass exactly");
  rejects(
      [] {
        normalize_weights({{0, .8f}, {1, .7f}}, 2,
                          std::array<uint8_t, 2>{1, 1});
      },
      "Impossible hard-lock sums fail visibly");
  rejects([] { normalize_weights({{0, 0}}, 1); },
          "Zero weights do not invent arbitrary fallback bones");
  rejects([] { normalize_weights({{0, .5f}, {0, .5f}}, 1); },
          "Duplicate weight joints are rejected");
  auto painted = paint_weight({{0, .3f}, {1, .4f}, {2, .3f}}, 1, .5f, 3,
                              std::array<uint8_t, 3>{1, 0, 0});
  require(weight(painted, 0) == .3f && weight(painted, 1) == .5f &&
              std::abs(weight(painted, 2) - .2f) < 1e-6f,
          "Weight painting preserves selected value and locked mass");
  rejects(
      [] {
        paint_weight({{0, .3f}, {1, .7f}}, 0, .5f, 2,
                     std::array<uint8_t, 2>{1, 0});
      },
      "Painting hard-locked groups is rejected");
  auto parent_overflow = rig.encode();
  parent_overflow["bones"][0]["parent"] = UINT64_MAX;
  rejects([&] { Skeleton::decode(parent_overflow); },
          "Unsigned parent overflow cannot become the root sentinel");
  Mesh mesh = plane();
  SkinWeights weights;
  weights.vertices.assign(4, {{1, 1}});
  auto pose = std::vector<JointTransform>{child.rest, root.rest};
  pose[1].translation = {2, 0, 0};
  auto moved = deform_mesh(mesh, weights, rig, pose);
  require(std::abs(moved.positions[0].x - mesh.positions[0].x - 2) < 1e-5f,
          "Hierarchical skinning applies world transforms once");
  auto hp = hairpin();
  Skeleton arms;
  Bone left;
  left.name = "Long.L";
  left.rest.translation = {.003f, 0, 0};
  left.length = 1;
  Bone right;
  right.name = "Short.R";
  right.rest.translation = {.028f, 0, 0};
  right.length = .1f;
  arms.bones = {left, right};
  BindOptions opts;
  opts.maximum_seed_distance = .05f;
  opts.seed_margin = .0001f;
  auto bound = automatic_bind(hp, arms, opts);
  require(weight(bound.vertices[10], 1) > weight(bound.vertices[10], 0),
          "Surface graph avoids jumping to the nearby opposite thin limb");
  auto floating = hp;
  uint32_t begin = floating.positions.size();
  floating.positions.insert(floating.positions.end(),
                            {{10, 0, 0}, {11, 0, 0}, {10, 1, 0}});
  floating.normals.resize(floating.positions.size(), {0, 0, 1});
  floating.indices.insert(floating.indices.end(),
                          {begin, begin + 1, begin + 2});
  rejects([&] { automatic_bind(floating, arms, opts); },
          "Disconnected far-away parts require a plausible bone seed");
  rejects([&] { automatic_bind(hp, arms, opts, [] { return true; }); },
          "Automatic binding honors worker cancellation");
  opts.maximum_work = 1;
  rejects([&] { automatic_bind(hp, arms, opts); },
          "Automatic binding enforces its work budget");
  SkinWeights asym;
  asym.vertices = {
      {{0, 1}}, {{1, 1}}, {{0, .8f}, {1, .2f}}, {{0, .3f}, {1, .7f}}};
  auto mirrored = mirror_weights(mesh, asym, std::array<uint32_t, 2>{1, 0});
  require(weight(mirrored.vertices[2], 0) == .7f &&
              weight(mirrored.vertices[2], 1) == .3f,
          "Weight symmetry mirrors vertices and bone identities");
  auto twice = mirror_weights(mesh, mirrored, std::array<uint32_t, 2>{1, 0});
  require(twice.vertices == asym.vertices,
          "Double weight mirror restores exact influences");
  auto badmap = std::array<uint32_t, 2>{0, 0};
  rejects([&] { mirror_weights(mesh, asym, badmap); },
          "Non-bijective bone symmetry is rejected");
  Mesh seam = mesh;
  seam.positions.insert(seam.positions.end(), mesh.positions.begin(),
                        mesh.positions.end());
  seam.normals.insert(seam.normals.end(), mesh.normals.begin(),
                      mesh.normals.end());
  seam.uv.insert(seam.uv.end(), mesh.uv.begin(), mesh.uv.end());
  for (auto i : mesh.indices)
    seam.indices.push_back(i + 4);
  auto seammap = mirror_vertices(seam);
  for (size_t v = 0; v < seammap.size(); v++)
    require(seammap[seammap[v]] == v,
            "Symmetry retains bijective seam duplicates");
  Mesh source;
  source.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  source.normals.assign(3, {0, 0, 1});
  source.indices = {0, 1, 2};
  SkinWeights sw;
  sw.vertices = {{{0, 1}}, {{1, 1}}, {{0, 1}}};
  Mesh target = source;
  target.positions = {{.25f, .25f, .01f}, {.5f, .25f, .01f}, {.25f, .5f, .01f}};
  auto transfer = transfer_weights(source, sw, target, 2, .1f);
  require(std::abs(weight(transfer.vertices[0], 1) - .25f) < 1e-5f,
          "Nearest-surface transfer interpolates triangle barycentric weights");
  rejects([&] { transfer_weights(source, sw, target, 2, .001f); },
          "Transfer rejects vertices outside the explicit surface radius");
  auto decoded = SkinWeights::decode(transfer.encode(), 3, 2);
  require(decoded.vertices == transfer.vertices,
          "Skin weights round trip independently of a scene frontend");
  MorphSet morph(mesh);
  auto edited = mesh.positions;
  edited[0].z = .2f;
  morph.set_target(mesh, "Raise.L", edited);
  morph.mirror_target(mesh, 0, "Raise.R");
  auto shaped = morph.evaluate(mesh, std::array<float, 2>{.5f, 1});
  require(std::abs(shaped.positions[0].z - .1f) < 1e-6 &&
              std::abs(shaped.positions[1].z - .2f) < 1e-6 &&
              shaped.uv == mesh.uv && shaped.indices == mesh.indices,
          "Editable mirrored morphs preserve topology and UVs");
  auto loaded = MorphSet::decode(morph.encode(), mesh);
  require(loaded.targets.size() == 2,
          "Morph deltas and stable topology identity persist");
  auto reordered = mesh;
  std::swap(reordered.indices[0], reordered.indices[1]);
  rejects([&] { loaded.validate(reordered); },
          "Topology reorder cannot silently reuse old morph indices");
  auto bad = morph.encode();
  bad["targets"][0]["delta"][0][0] = std::numeric_limits<float>::infinity();
  rejects([&] { MorphSet::decode(bad, mesh); },
          "Nonfinite persisted morph deltas are rejected");
  auto old = morph.targets;
  rejects([&] { morph.set_target(mesh, "Broken", std::span<const Vec3>{}); },
          "Failed morph edits leave the previous target set intact");
  require(morph.targets.size() == old.size() &&
              morph.targets[0].delta[0].z == old[0].delta[0].z,
          "Morph edit publication is transactional");
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
