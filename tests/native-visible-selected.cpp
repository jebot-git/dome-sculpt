// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
using namespace dome;
#include "native-weight-controller.hpp"
#define main base_geometry_reference_main
#include "geometry-weight-reference.cpp"
#undef main
int main(int argc, char **argv) try {
  size_t checks = 0;
  auto check = [&](bool v, const char *s) {
    ++checks;
    if (!v)
      throw std::runtime_error(s);
    std::cout << "PASS " << s << std::endl;
  };
  if (argc != 2)
    return 2;
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out / "state");
  std::filesystem::create_directories(out / "config");
  setenv("XDG_STATE_HOME", (out / "state").c_str(), 1);
  setenv("XDG_CONFIG_HOME", (out / "config").c_str(), 1);
  auto source = fixture();
  const auto id = source.add("Second original unskinned part", Volume{});
  auto &left = source.object(id);
  left.painted = source.object(1).painted->snapshot();
  left.mesh_active = true;
  left.affine_edit = source.object(1).affine_edit;
  DeformationState original(left.painted->mesh());
  original.morphs = MorphSet(left.painted->mesh());
  auto key = left.painted->mesh().positions;
  key[0].z += .01f;
  original.morphs->set_target(left.painted->mesh(), "Original left detail",
                              key);
  original.morph_weights = {.25f};
  source.set_deformation(id, original);
  source.select(2);
  source.validate();
  const std::vector<uint64_t> targets{2, id};
  const auto original_before = source.encode();
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::NativeVisible;
  options.reference = 1;
  options.limits.max_influences = 2;
  options.limits.prune = .01f;
  auto expected = source.snapshot();
  NativeVisibleSkinReport report;
  check(author_native_visible_skin(expected, targets, options, {}, &report) &&
            report.parts == 2 && report.changed_parts == 2,
        "Actual native selected factory binds two unskinned parts with "
        "readonly full rig");
  const auto accepted = expected.encode();
  Document doc;
  doc.replace(source);
  doc.select_objects(targets);
  check(doc.wait_mesh(), "Initial immutable selected mesh ready");
  auto selection_expected = source.snapshot();
  selection_expected.select(id);
  check(doc.copy_scene().encode() == selection_expected.encode(),
        "Multi-selection changes only its documented active member");
  auto selected_input = doc.copy_scene();
  const auto before = selected_input.encode();
  std::cout << "SELECTION_DIFF "
            << nlohmann::json::diff(original_before, before).dump()
            << std::endl;

  Driver hand(doc);
  hand.open();
  check(doc.frame().state->menu_page == 14,
        "Actual joystick reaches unchanged skin14");
  hand.sector(1);
  hand.value(1);
  hand.confirm();
  check(doc.frame().state->skin_options.mode ==
                SkinAuthoringMode::NativeVisible &&
            doc.frame().state->menu_page == 31,
        "Actual Method ray selects native finite handles and page31");
  check(doc.frame().state->menu_enabled[3] &&
            doc.frame().state->menu_enabled[9] &&
            doc.frame().state->menu_enabled[10] &&
            doc.frame().state->menu_enabled[11],
        "Native selected/influence/prune/contact controls enabled");
  hand.sector(11);
  hand.value(1);
  hand.confirm();
  check(doc.frame().state->skin_options.native_visible.contact_handling ==
            NativeContactHandling::FiniteLineIntegral,
        "Actual page31 surface-contact control explicitly selects finite-line "
        "integral");
  hand.sector(9);
  hand.value(1.f / 31);
  hand.confirm();
  hand.sector(10);
  hand.value(.01f / .99f);
  hand.confirm();
  check(doc.frame().state->skin_options.limits.max_influences == 2 &&
            std::abs(doc.frame().state->skin_options.limits.prune - .01f) <
                1e-5,
        "Actual native influence/prune sliders stage user values");
  options = doc.frame().state->skin_options;
  auto staged = selected_input.snapshot();
  author_native_visible_skin(staged, targets, options);
  check(doc.copy_scene().encode() == before && !doc.frame().state->can_undo,
        "Method and sliders do not mutate Scene or history");
  const auto file = out / "selected.dsculpt";
  doc.copy_scene().save(file);
  hand.sector(3);
  auto saving = doc.save_async(file);
  saving.get();
  check(doc.wait_asset() && doc.wait_mesh() && doc.frame().state->error.empty(),
        "Real selected controller and dependent Save finish");
  check(doc.copy_scene().encode() == staged.encode() &&
            Scene::load(file).encode() == staged.encode(),
        "Actual selected output and saved accepted endpoint exact");
  check(doc.undo() && doc.copy_scene().encode() == before && doc.redo() &&
            doc.copy_scene().encode() == staged.encode(),
        "Selected native operation is exactly one Undo/Redo");
  auto saved_selection = doc.frame().state->selected_objects;
  doc.select_object(2);
  const auto stable = doc.copy_scene().encode();
  bool refused = false;
  try {
    doc.bind_skin_selected(options, {}, saved_selection);
  } catch (const std::logic_error &) {
    refused = true;
  }
  check(refused && doc.copy_scene().encode() == stable &&
            !doc.frame().state->asset_busy,
        "Stale selected target IDs refuse atomically");
  auto restored = Scene::load(file);
  restore_rig_source(restored, 2, false);
  restore_rig_source(restored, id, false);
  check(restored.encode() == before,
        "Both full original unskinned sources restore after native restart");
  check(spatial_page_count == 32 &&
            spatial_action(28, 1) == SpatialCommand::ExportEditable256 &&
            spatial_action(29, 3) == SpatialCommand::BindSkinSelected &&
            spatial_action(30, 4) == SpatialCommand::EmissionRelative,
        "Native31 preserves export28 source29 HDR30 and all existing IDs");
  std::cout << "PASS native_selected_checks=" << checks
            << " work=" << report.work
            << " workspace=" << report.planned_workspace << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
