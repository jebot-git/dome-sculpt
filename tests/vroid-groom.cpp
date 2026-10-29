// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/creation.hpp"
#include "dome/source-maps.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
using namespace dome;
void require(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
float low(const Mesh &m) {
  float y = 1000;
  for (auto p : m.positions)
    y = std::min(y, p.y);
  return y;
}
float width(const Mesh &m) {
  float x = -1000, lo = 1000;
  for (auto p : m.positions) {
    x = std::max(x, p.x);
    lo = std::min(lo, p.x);
  }
  return x - lo;
}
float bangs(const Mesh &m) {
  float y = 1000;
  for (int lock = 0; lock < 24; ++lock) {
    float theta = 2 * std::numbers::pi_v<float> * (lock + .5f) / 24;
    if (std::cos(theta) > .7f)
      y = std::min(y, m.positions[331 + lock * 72 + 70].y);
  }
  return y;
}
size_t inside_count(const Mesh &hair, const std::vector<HumanoidPart> &body,
                    Vec3 direction = {.031f, .017f, 1}) {
  struct Tri {
    Vec3 a, b, c;
    float ymin, ymax;
  };
  std::vector<Tri> triangles;
  for (const auto &p : body)
    for (size_t i = 0; i < p.mesh.indices.size(); i += 3) {
      auto a = p.mesh.positions[p.mesh.indices[i]],
           b = p.mesh.positions[p.mesh.indices[i + 1]],
           c = p.mesh.positions[p.mesh.indices[i + 2]];
      triangles.push_back(
          {a, b, c, std::min({a.y, b.y, c.y}), std::max({a.y, b.y, c.y})});
    }
  direction = normalized(direction);
  size_t count = 0;
  for (auto p : hair.positions) {
    auto exits_outward = [&](Vec3 ray) {
      float nearest = 10;
      Vec3 hit_normal;
      for (const auto &t : triangles) {
        Vec3 e1 = t.b - t.a, e2 = t.c - t.a, h = cross(ray, e2);
        float det = dot(e1, h);
        if (std::abs(det) < 1e-10f)
          continue;
        float inv = 1 / det;
        Vec3 s = p - t.a;
        float u = dot(s, h) * inv;
        if (u < 0 || u > 1)
          continue;
        Vec3 q = cross(s, e1);
        float v = dot(ray, q) * inv;
        if (v < 0 || u + v > 1)
          continue;
        float distance = dot(e2, q) * inv;
        if (distance > 1e-5f && distance < nearest) {
          nearest = distance;
          hit_normal = cross(e1, e2);
        }
      }
      return nearest < 10 && dot(normalized(hit_normal), ray) > .001f;
    };
    // Source garments have open hems: a single outward hit through an opening
    // cannot establish containment. Require an outward exit in both directions.
    if (exits_outward(direction) && exits_outward(direction * -1))
      ++count;
  }
  return count;
}
int main() try {
  auto defaults = humanoid_baseline_defaults("vroid-beta-female");
  auto base = create_humanoid(defaults);
  require(base.parts.size() == 19,
          "Neutral hair controls retain exact19 source parts");
  auto c = defaults;
  c.hair_length = .4f;
  c.hair_volume = .35f;
  c.fringe = .3f;
  auto hair = create_humanoid(c);
  require(hair.parts.size() == 20, "Nonzero hair controls add original fitted "
                                   "groom rather than stretching source scalp");
  for (size_t i = 0; i < 19; ++i) {
    const auto &a = base.parts[i], &b = hair.parts[i];
    require(a.deformation.encode(a.mesh) == b.deformation.encode(b.mesh) &&
                a.mesh.uv == b.mesh.uv && a.mesh.indices == b.mesh.indices &&
                a.color == b.color &&
                a.appearance->original_bytes == b.appearance->original_bytes,
            "Every original source part, UV/index, rig/morph, palette and "
            "archive reference remains unchanged");
  }
  const auto &g = hair.parts.back();
  require(!g.appearance && g.unlit && g.source_maps &&
              g.mesh.positions.size() == 2059 && g.mesh.indices.size() == 10464,
          "Generated groom has original geometry/texture provenance and "
          "bounded dense topology");
  Mesh probes;
  auto source_world = g.deformation.skeleton->world();
  for (auto role : {"chest", "hips", "leftUpperArm"}) {
    auto joint = g.deformation.humanoid->joints.at(role);
    probes.positions.push_back({source_world[joint][12],
                                source_world[joint][13],
                                source_world[joint][14]});
  }
  auto interior_hits = inside_count(probes, base.parts);
  std::cout << "SEEDED_BODY_INTERIOR_HITS " << interior_hits << '\n';
  require(interior_hits >= 2,
          "Bidirectional ray test detects seeded real torso/shoulder interiors");
  Mesh near_inside, near_outside;
  auto chest = probes.positions.front();
  for (const auto &part : base.parts)
    for (size_t i = 0; i < part.mesh.indices.size(); i += 3) {
      auto a = part.mesh.positions[part.mesh.indices[i]],
           b = part.mesh.positions[part.mesh.indices[i + 1]],
           d = part.mesh.positions[part.mesh.indices[i + 2]];
      auto centroid = (a + b + d) / 3;
      auto n = normalized(cross(b - a, d - a));
      if (std::abs(centroid.y - chest.y) < .03f &&
          std::abs(centroid.x - chest.x) < .04f && std::abs(n.z) > .8f &&
          near_inside.positions.size() < 16) {
        near_inside.positions.push_back(centroid - n * .002f);
        near_outside.positions.push_back(centroid + n * .002f);
      }
    }
  auto clipped = inside_count(near_inside, base.parts),
       clear_surface = inside_count(near_outside, base.parts);
  std::cout << "SEEDED_NEAR_SURFACE " << clipped << " vs " << clear_surface << '\n';
  require(clipped > 0 && clipped > clear_surface,
          "Independent rays distinguish seeded 2mm torso-surface clipping");
  auto collisions = inside_count(g.mesh, base.parts);
  std::cout << "REST_INTERSECTION_VERTICES " << collisions << '\n';
  require(collisions == 0, "Fitted resting groom stays outside the independent "
                           "oriented body-ray volume test");
  auto paint = PaintedMesh::from_maps(g.mesh, g.source_maps, 256);
  require(
      paint->surface_diagnostics().policy == PaintUvPolicy::Strict &&
          paint->surface_diagnostics().unpaintable_uv_faces.empty() &&
          paint->surface_diagnostics().linked_texels == 0,
      "Original groom has strictly paintable nondegenerate separate UV charts");
  uint32_t head = g.deformation.humanoid->joints.at("head");
  for (const auto &v : g.deformation.skin->vertices)
    if (v.size() != 1 || v[0].joint != head || v[0].weight != 1)
      throw std::runtime_error("Groom weight was not an exact head binding");
  auto state = g.deformation;
  for (auto &b : state.skeleton->bones)
    state.pose.push_back(b.rest);
  float angle = .18f;
  Quat turn{std::cos(angle / 2), 0, std::sin(angle / 2), 0};
  state.pose[head].rotation = turn;
  auto world = state.skeleton->world();
  Vec3 pivot{world[head][12], world[head][13], world[head][14]};
  auto posed = state.preview(g.mesh);
  for (size_t i = 0; i < posed.positions.size(); ++i)
    if (length(posed.positions[i] -
               (pivot + turn.rotate(g.mesh.positions[i] - pivot))) > 1e-5f)
      throw std::runtime_error(
          "Groom head binding did not follow exact rigid head pose");
  require(true, "Original groom follows normalized source head pose exactly");
  auto posed_body = base.parts;
  for (auto &part : posed_body) {
    auto body_state = part.deformation;
    for (const auto &bone : body_state.skeleton->bones)
      body_state.pose.push_back(bone.rest);
    body_state.pose[head].rotation = turn;
    part.mesh = body_state.preview(part.mesh);
  }
  auto pose_collisions = inside_count(posed, posed_body);
  std::cout << "POSE_INTERSECTION_VERTICES " << pose_collisions << '\n';
  require(pose_collisions == 0, "Mild shared head pose preserves body/groom "
                                "clearance in independent ray test");
  auto short_c = c;
  short_c.hair_length = .1f;
  auto short_g = create_humanoid(short_c);
  auto long_c = c;
  long_c.hair_length = .9f;
  auto long_g = create_humanoid(long_c);
  require(low(long_g.parts.back().mesh) < low(short_g.parts.back().mesh) - .2f,
          "Hair length makes a measurable native strand-length change");
  auto rendered_long = defaults;
  rendered_long.hair_length = .8f;
  rendered_long.hair_volume = .6f;
  rendered_long.fringe = .4f;
  auto rendered_model = create_humanoid(rendered_long);
  auto rendered_body = rendered_model.parts;
  rendered_body.pop_back();
  auto vertical_hits = inside_count(rendered_model.parts.back().mesh,
                                   rendered_body, {.013f, 1, -.021f});
  std::cout << "RENDERED_LONG_VERTICAL_CONTAINMENT " << vertical_hits << '\n';
  require(vertical_hits == 0, "Exact rendered long control combination clears "
                              "independent vertical body rays");
  auto small_c = c;
  small_c.hair_volume = 0;
  auto small = create_humanoid(small_c);
  auto large_c = c;
  large_c.hair_volume = 1;
  auto large = create_humanoid(large_c);
  require(width(large.parts.back().mesh) >
              width(small.parts.back().mesh) + .01f,
          "Hair volume changes the fitted cap/lock silhouette");
  auto clear_c = c;
  clear_c.fringe = 0;
  auto clear = create_humanoid(clear_c);
  auto fringe_c = c;
  fringe_c.fringe = 1;
  auto fringe = create_humanoid(fringe_c);
  require(bangs(fringe.parts.back().mesh) <
              bangs(clear.parts.back().mesh) - .02f,
          "Fringe control changes front lock tips independently");
  auto male = humanoid_baseline_defaults("vroid-beta-male");
  male.hair_length = .35f;
  male.hair_volume = .25f;
  male.fringe = .3f;
  auto male_g = create_humanoid(male);
  PaintedMesh::from_maps(male_g.parts.back().mesh,
                         male_g.parts.back().source_maps, 256);
  require(
      male_g.parts.size() == 20,
      "Original groom fits and stays paintable on distinct male source rig");
  for (int variation = 0; variation < 4; ++variation) {
    auto extreme = defaults;
    extreme.height = variation & 1 ? 1.2f : 2.2f;
    extreme.hair_length = variation & 2 ? 1 : .01f;
    extreme.hair_volume = variation & 1 ? 1 : 0;
    extreme.fringe = variation & 2 ? 1 : 0;
    auto example = create_humanoid(extreme);
    const auto &part = example.parts.back();
    PaintedMesh::from_maps(part.mesh, part.source_maps, 256);
    part.deformation.validate(part.mesh);
    auto body = example.parts;
    body.pop_back();
    require(inside_count(part.mesh, body) == 0,
            "Extreme rest groom clears full body in independent ray test");
  }
  require(
      true,
      "Extreme bounded groom controls retain nondegenerate strict-UV geometry");
  auto mh = create_humanoid(humanoid_baseline_defaults("makehuman-hm08"));
  require(morph_topology_id(mh.parts.back().mesh) == 14369968804387486394ull,
          "Shared original groom extraction preserves historical MH geometry "
          "exactly");
  if (const char *directory = std::getenv("DOME_VROID_GROOM_FIXTURE")) {
    std::filesystem::create_directories(directory);
    const std::array names{"medium", "long", "fringe", "posed", "male"};
    for (size_t example = 0; example < names.size(); ++example) {
      auto controls = humanoid_baseline_defaults(
          example == 4 ? "vroid-beta-male" : "vroid-beta-female");
      controls.hair_length = example == 1 ? .8f : example == 2 ? .05f : .45f;
      controls.hair_volume = example == 1 ? .6f : .35f;
      controls.fringe = example == 2 ? 1.f : .4f;
      Scene scene;
      auto group = append_humanoid(scene, controls, "Fitted hair");
      if (example == 3)
        for (const auto &object : scene.objects())
          if (object.group == group) {
            auto deformation = *object.deformation;
            for (const auto &bone : deformation.skeleton->bones)
              deformation.pose.push_back(bone.rest);
            deformation.pose[deformation.humanoid->joints.at("head")].rotation =
                {std::cos(.12f), 0, std::sin(.12f), 0};
            scene.set_deformation(object.id, std::move(deformation));
          }
      auto path = std::filesystem::path(directory) /
                  ("vroid-groom-" + std::string(names[example]) + ".glb");
      export_glb(scene.surfaces(true), path);
      std::cout << names[example] << " bytes="
                << std::filesystem::file_size(path) << '\n';
    }
  }
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
