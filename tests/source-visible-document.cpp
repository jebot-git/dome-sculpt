// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/polygon-scene.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
using namespace dome;
namespace dome {
struct DocumentTransactionTestAccess {
  static auto history(const Document &doc) {
    std::lock_guard lock(doc.model_mutex_);
    return std::tuple{doc.undo_.size(),      doc.redo_.size(),
                      doc.content_,          doc.next_content_,
                      doc.saved_content_,    doc.state_.revision,
                      doc.state_.generation, doc.state_.edit_authority,
                      doc.state_.held_object};
  }
};
} // namespace dome
namespace {
size_t checks = 0;
void check(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
  std::cout << "PASS " << why << std::endl;
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
struct Driver {
  Document &doc;
  SpatialFrame frame;
  explicit Driver(Document &d) : doc(d) {
    frame.focused = frame.head_valid = true;
    frame.head.position = {0, 1.6f, 0};
    frame.hands[1].tracked.valid = true;
    send();
  }
  void send(float x = 0, float y = 0, bool click = false, float force = 0) {
    ++frame.sequence;
    frame.time += .03;
    auto &h = frame.hands[1];
    h.x = x;
    h.y = y;
    h.joystick = click;
    h.tracked.trigger = force;
    doc.submit(frame);
    if (!doc.wait_input())
      throw std::runtime_error("Source rebind input timeout");
  }
  void sector(int n) {
    float a = n * std::numbers::pi_v<float> / 6;
    send(std::cos(a), std::sin(a));
    send(std::cos(a), std::sin(a), true);
    send();
  }
  void open() {
    send();
    send(0, 0, true);
    send();
    sector(8);
    sector(10);
    sector(5);
    sector(10);
    sector(11);
  }
  void value(float t) {
    auto state = doc.frame().state;
    auto &h = frame.hands[1];
    h.tracked.aimValid = true;
    h.tracked.aim.position =
        ob::transform(state->panel, {((100 + 420 * t) / 640.f - .5f) * .6f,
                                     (.5f - 300 / 640.f) * .6f, .25f});
    h.tracked.aim.orientation = state->panel.orientation;
    send(0, 0, false, .8f);
    send();
  }
  void confirm() {
    send(0, 0, true);
    send();
  }
};
Scene fixture() {
  Scene s;
  append_humanoid(s, HumanoidControls{}, "Source-assisted native", {}, 64);
  return s;
}
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--ui-fixture") {
    fixture().save(argv[2]);
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--inspect") {
    auto s = Scene::load(argv[2]);
    std::cout << nlohmann::json{{"scene", s.encode()}}.dump();
    return 0;
  }
  check(argc == 2, "Choose native workflow output directory");
  std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  auto source = fixture();
  std::vector<uint64_t> targets;
  for (const auto &o : source.objects())
    if (o.painted)
      targets.push_back(o.id);
  check(targets.size() == 19,
        "Genuine imported source group contains19 editable targets");
  Document doc;
  doc.replace(source);
  doc.select_objects(targets);
  check(doc.wait_mesh(),
        "Initial real immutable mesh fence ready before controller");
  Driver hand(doc);
  hand.open();
  check(doc.frame().state->menu_page == 14,
        "Unchanged joystick route reaches legacy skin page14");
  hand.sector(4);
  hand.value(1.f / 31);
  hand.confirm();
  check(doc.frame().state->skin_options.limits.max_influences == 2,
        "Legacy influence preference staged before explicit fixed4 method");
  const auto before = doc.copy_scene().encode();
  hand.sector(1);
  hand.value(2.f / 3);
  hand.confirm();
  check(doc.frame().state->skin_options.mode ==
                SkinAuthoringMode::SourceVisible &&
            doc.frame().state->menu_page == 29 &&
            doc.frame().state->menu_enabled[3],
        "Actual ray selects source-assisted method and explicit selected "
        "page29 action");
  check(doc.copy_scene().encode() == before &&
            doc.frame().state->skin_options.limits.max_influences == 2,
        "New method does not mutate Scene or reset independent legacy "
        "preference");
  check(!doc.frame().state->menu_enabled[9] &&
            !doc.frame().state->menu_enabled[10] &&
            !doc.frame().state->menu_enabled[11],
        "Source membership/visibility/fixed4 policy cells are informational");
  auto path = out / "accepted.dsculpt";
  doc.copy_scene().save(path);
  hand.sector(3);
  check(doc.frame().state->asset_busy,
        "Actual selected controller action admits real source helper");
  auto saved = doc.save_async(path);
  saved.get();
  check(doc.wait_asset() && doc.wait_mesh() && doc.frame().state->error.empty(),
        "Real selected helper and dependent Save finish without self-wait");
  const auto accepted = doc.copy_scene();
  check(accepted.encode() != before &&
            Scene::load(path).encode() == accepted.encode(),
        "Queued Save writes accepted weights and full source closure");
  size_t changed = 0;
  bool original_owners = true, preset = true;
  for (auto id : targets) {
    const auto &o = accepted.object(id), &prior = source.object(id);
    if (o.deformation != prior.deformation) {
      ++changed;
      original_owners &=
          o.rig_source && o.rig_source->deformation == prior.deformation;
    } else
      original_owners &= o.rig_source == prior.rig_source;
    for (const auto &row : o.deformation->skin->vertices)
      preset &= row.size() <= 4;
  }
  check(changed > 0 && original_owners,
        "Only changed targets capture their exact first source rig");
  check(preset,
        "New method uses fixed4 while retaining legacy cap2 preference");
  check(doc.undo() && doc.copy_scene().encode() == before,
        "Complete selected bind is exactly one global Undo");
  check(!doc.undo(),
        "Method/reference/selection settings added no phantom history");
  check(doc.redo() && doc.copy_scene().encode() == accepted.encode(),
        "Redo restores complete accepted field and first sources");
  auto stale = doc.frame().state->selected_objects;
  doc.select_object(targets.front());
  const auto unchanged = doc.copy_scene().encode();
  auto opts = doc.frame().state->skin_options;
  check(refuses([&] { doc.bind_skin_selected(opts, {}, stale); }) &&
            doc.copy_scene().encode() == unchanged &&
            !doc.frame().state->asset_busy,
        "Stale explicit selected scope refuses rather than substituting new "
        "targets");
  auto active = doc.copy_scene().active();
  opts.reference = active.id;
  opts.locks.assign(active.deformation->skeleton->bones.size(), 1);
  doc.bind_skin(opts);
  check(doc.wait_asset() && doc.wait_mesh() &&
            doc.frame().state->error.empty() &&
            doc.copy_scene().encode() == unchanged,
        "Real native all-locked rebind is exact no-op");
  check(doc.frame().state->skin_use_locks &&
            doc.frame().state->skin_options.locks.empty() &&
            doc.frame().state->weights.locks == opts.locks,
        "Typed admission publishes matching checked locks and options "
        "atomically");
  doc.configure_object_grab(true, true);
  doc.hold_object(active.id);
  check(doc.wait_mesh(), "Pinned held authority ready");
  const auto pinned = doc.copy_scene().encode();
  const auto pinned_history = DocumentTransactionTestAccess::history(doc);
  doc.bind_skin(opts);
  const bool finished = doc.wait_asset();
  std::cerr << "PINNED diagnostic finished=" << finished
            << " error=" << doc.frame().state->error
            << " held=" << doc.frame().state->held_object
            << " expected=" << active.id
            << " scene_equal=" << (doc.copy_scene().encode() == pinned)
            << std::endl;
  check(finished && doc.frame().state->error.empty() &&
            doc.copy_scene().encode() == pinned &&
            doc.frame().state->held_object == active.id &&
            DocumentTransactionTestAccess::history(doc) == pinned_history,
        "Held active target keeps permission and readonly support on no-op "
        "rebind");
  std::cout << "SUMMARY checks=" << checks
            << " selected19=1 queued_save=1 oneUndo=1 stale_selection=1 "
               "all_locked=1 pinned=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
