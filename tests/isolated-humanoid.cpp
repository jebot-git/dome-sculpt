// SPDX-License-Identifier: GPL-3.0-or-later
#include "document-authority.hpp"
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include <bit>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
void guards() {
  Scene s;
  auto group = s.add_group("Metadata", {}, 0,
                           std::make_shared<const HumanoidControls>());
  s.active().group = group;
  const auto before = s.snapshot();
  auto after = s.snapshot();
  check(before.group(group).humanoid != after.group(group).humanoid,
        "Independent snapshots genuinely detach creator controls");
  detail::require_isolated_scene(before, after, s.active().id);
  check(true, "Independent equal snapshots preserve full creator metadata");
  auto encoded = before.group(group).humanoid->encode();
  size_t fields = 0, components = 0;
  for (auto at = encoded.begin(); at != encoded.end(); ++at) {
    if (at.key() == "version")
      continue;
    const size_t count = at->is_array() ? at->size() : 1;
    for (size_t c = 0; c < count; ++c) {
      auto changed = encoded;
      if (at->is_array())
        changed[at.key()][c] = at->at(c).get<float>() == .25f ? .5f : .25f;
      else if (at->is_string())
        changed[at.key()] = "vroid-beta-male";
      else
        changed[at.key()] = at.key() == "height" ? 1.7f : .25f;
      auto next = after.snapshot();
      next.group(group).humanoid = std::make_shared<const HumanoidControls>(
          HumanoidControls::decode(changed));
      check(refuses([&] {
              detail::require_isolated_scene(before, next, s.active().id);
            }),
            "Every encoded metadata member/component remains protected");
      ++components;
    }
    ++fields;
  }
  auto zero = after.snapshot();
  auto controls =
      std::make_shared<HumanoidControls>(*zero.group(group).humanoid);
  controls->anime = -0.0f;
  zero.group(group).humanoid = controls;
  check(std::bit_cast<uint32_t>(controls->anime) !=
                std::bit_cast<uint32_t>(before.group(group).humanoid->anime) &&
            refuses([&] {
              detail::require_isolated_scene(before, zero, s.active().id);
            }),
        "Signed-zero bit mutation is protected without padding memcmp");
  auto absent = after.snapshot();
  absent.group(group).humanoid.reset();
  check(refuses([&] {
          detail::require_isolated_scene(before, absent, s.active().id);
        }),
        "Creator metadata presence is protected");
  std::cout << "COVERAGE fields=" << fields << " components=" << components
            << " signed_zero=1\n";
}
void native(const std::filesystem::path &out) {
  Scene scene;
  append_humanoid(scene, HumanoidControls{}, "Pinned source", {}, 64);
  auto other = scene.add_group("Unrelated metadata", {}, 0,
                               std::make_shared<const HumanoidControls>());
  const auto target = scene.active().id, creator = scene.active().group;
  auto ghost = std::find_if(scene.objects().begin(), scene.objects().end(),
                            [&](const auto &o) {
                              return o.id != target && o.painted &&
                                     o.deformation && o.deformation->humanoid;
                            });
  check(ghost != scene.objects().end(),
        "Actual source contains an independent skinned ghost");
  const auto ghost_id = ghost->id;
  Document doc;
  doc.replace(scene);
  check(doc.wait_mesh(), "Pinned native initial mesh ready");
  doc.configure_object_grab(true, true);
  doc.hold_object(target);
  check(doc.wait_mesh(), "Pinned native immutable mesh ready");
  const auto before = doc.copy_scene().encode();
  auto path = out / "original.dsculpt";
  doc.copy_scene().save(path);
  auto state = doc.frame().state;
  const auto revision = state->revision, generation = state->generation,
             authority = state->edit_authority;
  for (int kind = 0; kind < 3; ++kind) {
    std::promise<void> started, release;
    auto ready = started.get_future();
    auto go = release.get_future().share();
    struct ReleaseGate {
      std::promise<void> &promise; bool released=false;
      void now(){if(!released){released=true;promise.set_value();}}
      ~ReleaseGate(){if(!released)try{promise.set_value();}catch(...){}}
    } unlock{release};
    doc.edit_scene("Forbidden creator metadata", [&, kind](Scene &s,
                                                           const auto &) {
      started.set_value();
      go.wait();
      if (kind < 2) {
        auto &g = s.group(kind == 0 ? creator : other);
        auto value = std::make_shared<HumanoidControls>(*g.humanoid);
        value->shoulder_width = .5f;
        g.humanoid = value;
      } else {
        auto d =
            std::make_shared<DeformationState>(*s.object(ghost_id).deformation);
        std::swap(d->humanoid->joints.at("leftUpperArm"),
                  d->humanoid->joints.at("rightUpperArm"));
        s.object(ghost_id).deformation = d;
      }
    });
    if (ready.wait_for(std::chrono::seconds(60)) != std::future_status::ready) {
      unlock.now();
      throw std::runtime_error(
          "Actual helper failed to reach native metadata gate");
    }
    auto save = doc.save_async(path);
    unlock.now();
    bool failed = false;
    try {
      save.get();
    } catch (const std::exception &) {
      failed = true;
    }
    check(doc.wait_asset() && failed && !doc.frame().state->error.empty(),
          "Forbidden actual helper and its dependent Save refuse");
    auto current = doc.frame().state;
    check(
        doc.copy_scene().encode() == before &&
            Scene::load(path).encode() == before &&
            current->held_object == target && current->revision == revision &&
            current->generation == generation &&
            current->edit_authority == authority && !current->can_undo,
        "All Scene/source/history/held authority and prior file remain exact");
  }
}
} // namespace
int main(int argc, char **argv) try {
  guards();
  if (argc == 2) {
    std::filesystem::create_directories(argv[1]);
    native(argv[1]);
  }
  std::cout << "PASS " << checks << " isolated creator metadata checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
