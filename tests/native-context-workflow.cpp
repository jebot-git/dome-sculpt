// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
using namespace dome;
#include "native-weight-controller.hpp"
namespace {
std::atomic_bool armed{false}, entered{false};
std::atomic_size_t callbacks{0};
using Cancel = std::function<bool()>;
void require(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << std::endl;
}
std::string bytes(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
struct Release {
  ~Release() { armed = false; }
};
} // namespace
extern "C" bool
real_native(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const Cancel &,
            NativeVisibleSkinReport
                *) asm("__real__ZN4dome26author_native_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23NativeVisibleSkinReportE");
extern "C" bool
wrapped_native(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
               const Cancel &,
               NativeVisibleSkinReport
                   *) asm("__wrap__ZN4dome26author_native_visible_skinERNS_"
                          "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                          "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                          "23NativeVisibleSkinReportE");
extern "C" bool wrapped_native(Scene &s, std::span<const uint64_t> ids,
                               const SkinAuthoringOptions &o,
                               const Cancel &cancel,
                               NativeVisibleSkinReport *r) {
  size_t calls = 0;
  auto check = [&] {
    if (armed && ++calls == 64) {
      callbacks = calls;
      entered = true;
      while (armed && !(cancel && cancel()))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return cancel && cancel();
  };
  return real_native(s, ids, o, check, r);
}
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::invalid_argument(
        "Choose actual completed full-field directory and output directory");
  Release release;
  const std::filesystem::path field = argv[1], out = argv[2];
  std::filesystem::create_directories(out / "state");
  std::filesystem::create_directories(out / "config");
  setenv("XDG_STATE_HOME", (out / "state").c_str(), 1);
  setenv("XDG_CONFIG_HOME", (out / "config").c_str(), 1);
  auto input = Scene::load(field / "fitted-unskinned.dsculpt"),
       expected = Scene::load(field / "bound.dsculpt");
  std::vector<uint64_t> ids;
  uint64_t reference = 0;
  for (const auto &o : input.objects())
    if (o.deformation && o.deformation->skeleton)
      reference = o.id;
  ids.push_back(reference);
  require(ids.size() == 1 && reference,
          "Genuine one-target unskinned fitted native input and authentic "
          "reference");
  SkinAuthoringOptions prior;
  prior.mode = SkinAuthoringMode::NativeVisible;
  prior.reference = reference;
  require(prior.native_visible.contact_handling ==
              NativeContactHandling::PointwiseOpen,
          "Prior pointwise contact default is preserved");
  auto old = input.snapshot();
  bool refused = false;
  try {
    author_native_visible_skin(old, ids, prior);
  } catch (const std::runtime_error &e) {
    refused = std::string(e.what()).find("origin/coplanar/grazing") !=
              std::string::npos;
  }
  require(refused && old.encode() == input.encode(),
          "Original actual pointwise CC0 negative remains exact and is never a "
          "fallback");
  auto invalid = prior;
  invalid.native_visible.contact_handling = NativeContactHandling(2);
  bool invalid_refused = false;
  try {
    invalid.validate();
  } catch (const std::invalid_argument &) {
    invalid_refused = true;
  }
  require(invalid_refused,
          "Unknown finite-contact policy refuses before construction");
  expected = input.snapshot();
  auto expected_options = prior;
  expected_options.native_visible.contact_handling =
      NativeContactHandling::FiniteLineIntegral;
  expected_options.native_visible.support_context =
      NativeSupportContext::CompleteReferenceNode;
  require(author_native_visible_skin(expected, ids, expected_options),
          "Independent actual public partial-context factory creates expected "
          "field");
  Document doc;
  doc.replace(input);
  doc.select_objects(ids);
  require(doc.wait_mesh(std::chrono::seconds(120)),
          "Initial actual immutable mesh fence ready before controller input");
  const auto before = doc.copy_scene().encode();
  expected.select(ids.back());
  Driver hand(doc);
  hand.open();
  hand.sector(1);
  hand.value(1);
  hand.confirm();
  require(doc.frame().state->menu_page == 31,
          "Actual Method controller retains native31");
  require(doc.frame().state->menu_enabled[11] &&
              spatial_parameter(31, 11) == VrParameter::SkinContacts,
          "Actual contact control is enabled only in Native mode");
  hand.sector(11);
  auto diag = [&](const char *phase) {
    const auto s = doc.frame().state;
    std::cerr << "DIAG " << phase << " parameter=" << int(s->controls.parameter)
              << " components=" << s->controls.components
              << " value=" << s->controls.value[0] << ","
              << s->controls.value[1] << " contact="
              << int(s->skin_options.native_visible.contact_handling)
              << " support="
              << int(s->skin_options.native_visible.support_context)
              << " undo=" << s->can_undo << " error=" << s->error << std::endl;
  };
  diag("opened");
  hand.value(1);
  diag("contact");
  hand.value(1, 1);
  diag("support");
  hand.confirm();
  diag("confirmed");
  require(doc.frame().state->skin_options.native_visible.support_context ==
                  NativeSupportContext::CompleteReferenceNode &&
              doc.frame().state->skin_options.native_visible.contact_handling ==
                  NativeContactHandling::FiniteLineIntegral &&
              doc.copy_scene().encode() == before &&
              !doc.frame().state->can_undo,
          "Explicit contact choice is staged without Scene or history changes");
  const auto file = out / "accepted.dsculpt";
  doc.copy_scene().save(file);
  const auto prior_file = bytes(file);
  armed = true;
  entered = false;
  hand.sector(3);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (!entered && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  require(entered && callbacks == 64, "Actual real finite-line preparation "
                                      "reached deterministic callback64 gate");
  auto cancelled_save = doc.save_async(file);
  require(doc.frame().state->asset_busy && bytes(file) == prior_file,
          "Accepted dependent Save waits without replacing prior file");
  doc.cancel_background_work();
  armed = false;
  bool save_refused = false;
  try {
    cancelled_save.get();
  } catch (const std::exception &) {
    save_refused = true;
  }
  require(save_refused && doc.wait_asset(std::chrono::seconds(120)) &&
              doc.copy_scene().encode() == before &&
              bytes(file) == prior_file && !doc.frame().state->can_undo,
          "Real mid-helper Cancel fails exact dependent Save and retains every "
          "source, paint, pose, option owner and history");
  hand.sector(3);
  auto saving = doc.save_async(file);
  saving.get();
  require(doc.wait_asset(std::chrono::seconds(120)) &&
              doc.wait_mesh(std::chrono::seconds(120)) &&
              doc.frame().state->error.empty(),
          "Same actual controller retries complete finite-line selected worker "
          "and queued Save");
  require(doc.copy_scene().encode() == expected.encode() &&
              Scene::load(file).encode() == expected.encode(),
          "Actual one-target context native worker and accepted endpoint match "
          "independently qualified factory field exactly");
  require(doc.frame().state->skin_options.native_visible.contact_handling ==
              NativeContactHandling::FiniteLineIntegral,
          "Typed SkinAdmission retains exact chosen contact policy");
  require(doc.undo() && doc.copy_scene().encode() == before && doc.redo() &&
              doc.copy_scene().encode() == expected.encode(),
          "Actual full selected field publishes exactly one Undo/Redo");
  auto locked = doc.frame().state->skin_options;
  locked.locks.assign(
      input.object(reference).deformation->skeleton->bones.size(), 1);
  doc.bind_skin_selected(locked, {}, ids);
  auto no_op = doc.save_async(out / "locked.dsculpt");
  no_op.get();
  require(doc.wait_asset(std::chrono::seconds(120)) &&
              doc.copy_scene().encode() == expected.encode(),
          "All-locked actual finite-line bind retains every produced influence "
          "and exact saved endpoint");
  require(doc.undo() && doc.copy_scene().encode() == before && !doc.undo(),
          "All-locked no-op adds no extra history entry");
  require(doc.redo(), "Original changed field remains redoable");
  auto restored = Scene::load(file);
  for (auto id : ids)
    restore_rig_source(restored, id, false);
  require(restored.encode() == before,
          "Native accepted output restores full original "
          "paint/morph/material/placement/rig/source capsules");
  doc.finish_spatial();
  doc.save_async(out / "final.dsculpt").get();
  require(Scene::load(out / "final.dsculpt").encode() == expected.encode(),
          "Tracking teardown leaves accepted result and final Save exact");
  std::ofstream(out / "report.json") << nlohmann::json{
      {"parts", ids.size()},
      {"real_mid_helper_callback", callbacks.load()},
      {"contact_policy", "explicit finite-line integral"},
      {"save_reopen", true},
      {"cancel_prior_exact", true},
      {"one_undo", true},
      {"full_restore", true},
      {"all_locked", true}}.dump(2);
  std::cout << "PASS native_finite_line_workflow parts=" << ids.size()
            << std::endl;
  return 0;
} catch (const std::exception &e) {
  armed = false;
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
