// SPDX-License-Identifier: GPL-3.0-or-later
#include "skin-editor.hpp"
#include "appearance.h"
#include "dome/humanoid-creator.hpp"
#include "native-weight-reference.hpp"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
void edit_skin_dialog(Document &doc) {
  auto draft = doc.draft_scene();
  const auto &target = draft.scene.active();
  if (!target.visible || target.locked || !target.mesh_active ||
      !target.painted)
    throw std::invalid_argument("Select a visible, unlocked prepared mesh");
  auto state = doc.frame().state;
  auto options = state->skin_options;
  int row = std::max(32, dx::size + 14), sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int width = std::min(std::max(720, dx::size * 45), sw - 40),
      height = std::min(row * 21, sh - 40), label = width / 2;
  DomeDialog window(width, height, "Bind or transfer skin");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0, 0, width - 24, row * 22);
  content.resizable(nullptr);
  Fl_Box hint(16, 4, width - 48, row * 3,
              "Bind in the reference rest frame.\n"
              "Keep target paint, morphs and pose.\n"
              "Different rigs need joint mapping.");
  hint.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  int y = row * 3;
  Fl_Choice mode(label, y, width - label - 40, row - 4, "Method");
  y += row;
  mode.add("Automatic bind");
  mode.add("Transfer skin weights");
  mode.add("Source-assisted (fixed4)");
  mode.add("Native handles");
  mode.value(int(options.mode));
  Fl_Choice source(label, y, width - label - 40, row - 4, "Reference mesh");
  y += row;
  Fl_Choice scope(label, y, width - label - 40, row - 4, "Bind targets");
  y += row;
  scope.add("Active part");
  scope.add("Selected editable parts");
  scope.value(0);
  Fl_Value_Input seed(label, y, width - label - 40, row - 4,
                      "Seed radius (mm)");
  y += row;
  Fl_Value_Input margin(label, y, width - label - 40, row - 4,
                        "Seed margin (mm)");
  y += row;
  Fl_Value_Input distance(label, y, width - label - 40, row - 4,
                          "Transfer (mm)");
  y += row;
  Fl_Value_Input falloff(label, y, width - label - 40, row - 4,
                         "Distance falloff");
  y += row;
  Fl_Value_Input cap(label, y, width - label - 40, row - 4, "Max influences");
  y += row;
  Fl_Value_Input prune(label, y, width - label - 40, row - 4,
                       "Prune threshold");
  y += row;
  seed.bounds(.001, 10000000);
  seed.step(.1);
  seed.value(options.seed_distance * 1000);
  margin.bounds(0, 10000000);
  margin.step(.1);
  margin.value(options.seed_margin * 1000);
  distance.bounds(.001, 10000000);
  distance.step(.1);
  distance.value(options.transfer_distance * 1000);
  falloff.bounds(1, 8);
  falloff.step(.1);
  falloff.value(options.falloff);
  cap.bounds(1, 32);
  cap.step(1);
  cap.value(options.limits.max_influences);
  prune.bounds(0, .99);
  prune.step(.0001);
  prune.value(options.limits.prune);
  Fl_Choice contacts(label, y, width - label - 40, row - 4, "Surface contacts");
  y += row;
  contacts.add("Strict rays");
  contacts.add("Line integral");
  contacts.value(int(options.native_visible.contact_handling));
  contacts.tooltip(
      "Finite-line integral handles isolated exact surface contacts.\n"
      "Fully coplanar handles and unresolved support still refuse.\n"
      "No fallback or topology repair is applied.");
  Fl_Check_Button keep(20, y, width - 50, row, "Preserve checked influences");
  y += row;
  Fl_Check_Browser locks(20, y, width - 64, row * 6);
  y += row * 6;
  if (target.deformation && target.deformation->skeleton &&
      target.deformation->skin) {
    const auto &bones = target.deformation->skeleton->bones;
    keep.value(state->skin_use_locks);
    for (size_t i = 0; i < bones.size(); ++i)
      locks.add(bones[i].name.c_str(),
                state->weights.locks.size() == bones.size() &&
                    state->weights.locks[i]);
  } else {
    keep.value(0);
    keep.deactivate();
    locks.deactivate();
  }
  content.size(width - 24, y + row);
  content.end();
  scroll.end();
  int button_width = std::min(width / 2 - 24, std::max(140, dx::size * 8));
  Fl_Button cancel(width / 2 - button_width - 10, height - row * 2 + 8,
                   button_width, row, "Cancel"),
      apply(width / 2 + 10, height - row * 2 + 8, button_width, row,
            "Bind / transfer");
  struct Context {
    Fl_Window *window;
    Fl_Choice *source, *mode, *scope, *contacts;
    Fl_Box *hint;
    Fl_Button *apply;
    const Scene *scene;
    uint64_t target;
    SkinAuthoringOptions *options;
    std::vector<uint64_t> ids;
    std::vector<std::string> names;
    std::array<Fl_Value_Input *, 6> fields;
    Fl_Check_Button *keep;
    Fl_Check_Browser *locks;
    bool accepted = false, selection_ok = false, source_selection_ok = false,
         selected_scope = false, source_ready = true;
    void refresh() {
      uint64_t previous =
          source->value() >= 0 && size_t(source->value()) < ids.size()
              ? ids[source->value()]
              : options->reference;
      ids.clear();
      names.clear();
      std::vector<Fl_Menu_Item> items;
      names.reserve(scene->objects().size());
      items.reserve(scene->objects().size() + 1);
      const auto &active = scene->object(target);
      for (const auto &o : scene->objects())
        if (o.mesh_active && o.painted && o.deformation &&
            o.deformation->skeleton &&
            (mode->value() != 1 || (o.deformation->skin && o.id != target)) &&
            (mode->value() != 3 || detail::native_weight_reference(o)) &&
            (mode->value() != 2 ||
             (o.group == active.group && o.appearance && active.appearance &&
              o.appearance->asset &&
              o.appearance->asset == active.appearance->asset &&
              !o.appearance->asset.owner_before(active.appearance->asset) &&
              !active.appearance->asset.owner_before(o.appearance->asset) &&
              o.deformation->skin && o.deformation->humanoid))) {
          ids.push_back(o.id);
          names.push_back(o.name + (o.visible ? "" : " [hidden]"));
        }
      for (const auto &name : names) {
        Fl_Menu_Item item{};
        item.label(name.c_str());
        items.push_back(item);
      }
      items.emplace_back();
      source->copy(items.data());
      auto found = std::find(ids.begin(), ids.end(), previous);
      source->value(found == ids.end() ? 0 : int(found - ids.begin()));
      const bool source_assisted = mode->value() == 2;
      const bool native = mode->value() == 3;
      const bool explicit_parts = source_assisted || native;
      if (native)
        contacts->activate();
      else
        contacts->deactivate();
      if (source_assisted) {
        for (auto *field : fields)
          field->deactivate();
        scope->activate();
        hint->label(
            "Imported-rig joint membership; connected-surface visibility.\n"
            "The source group supplies read-only support.\n"
            "Fixed4 influences; checked weights and originals are retained.");
        apply->label(scope->value() == 1 ? "Bind selected" : "Bind active");
      } else if (native) {
        for (auto *field : fields)
          field->deactivate();
        fields[4]->activate();
        fields[5]->activate();
        scope->activate();
        hint->label("Selected parts supply support.\n"
                    "Keep the full rig and source.\n"
                    "Keep paint, morphs and pose.");
        apply->label(scope->value() == 1 ? "Bind selected" : "Bind active");
      } else {
        scope->deactivate();
        fields[4]->activate();
        fields[5]->activate();
        hint->label("Bind in the reference rest frame.\nKeep target paint, "
                    "morphs and pose.\nDifferent rigs need joint mapping.");
        apply->label("Bind / transfer");
        if (mode->value() == 0) {
          fields[0]->activate();
          fields[1]->activate();
          fields[3]->activate();
          fields[2]->deactivate();
        } else {
          fields[0]->deactivate();
          fields[1]->deactivate();
          fields[3]->deactivate();
          fields[2]->activate();
        }
      }
      if (explicit_parts && !source_ready)
        hint->label("Finish shape editing or object motion before binding REST "
                    "weights.");
      const bool targets_ready =
          source_assisted ? source_selection_ok : selection_ok;
      if (ids.empty() || (explicit_parts && !source_ready) ||
          (explicit_parts && scope->value() == 1 && !targets_ready))
        apply->deactivate();
      else
        apply->activate();
    }
  } context{&window,
            &source,
            &mode,
            &scope,
            &contacts,
            &hint,
            &apply,
            &draft.scene,
            target.id,
            &options,
            {},
            {},
            {&seed, &margin, &distance, &falloff, &cap, &prune},
            &keep,
            &locks};
  context.source_ready = !state->shape_sculpt && !state->object_grabbing;
  context.selection_ok =
      !state->selected_objects.empty() && state->selected_objects.size() <= 64;
  context.source_selection_ok = context.selection_ok;
  for (auto id : state->selected_objects) {
    auto info = std::find_if(state->objects.begin(), state->objects.end(),
                             [&](const auto &o) { return o.id == id; });
    const auto &o = draft.scene.object(id);
    const bool editable = info != state->objects.end() && info->editable &&
                          o.mesh_active && o.painted;
    context.selection_ok &= editable;
    context.source_selection_ok &= editable && o.group == target.group &&
                                   o.deformation && o.deformation->skin;
  }
  context.refresh();
  mode.callback(
      [](Fl_Widget *, void *p) { static_cast<Context *>(p)->refresh(); },
      &context);
  scope.callback(
      [](Fl_Widget *, void *p) { static_cast<Context *>(p)->refresh(); },
      &context);
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  apply.callback(
      [](Fl_Widget *, void *p) {
        auto &c = *static_cast<Context *>(p);
        try {
          if (c.source->value() < 0 ||
              size_t(c.source->value()) >= c.ids.size())
            throw std::invalid_argument(
                "Choose an eligible skinned reference mesh");
          auto next = *c.options;
          next.reference = c.ids[c.source->value()];
          next.mode = SkinAuthoringMode(c.mode->value());
          next.native_visible.contact_handling =
              NativeContactHandling(c.contacts->value());
          next.seed_distance = float(c.fields[0]->value() / 1000);
          next.seed_margin = float(c.fields[1]->value() / 1000);
          next.transfer_distance = float(c.fields[2]->value() / 1000);
          next.falloff = float(c.fields[3]->value());
          double influences = c.fields[4]->value();
          if (!std::isfinite(influences) || influences < 1 || influences > 32 ||
              std::floor(influences) != influences)
            throw std::invalid_argument(
                "Max influences must be an integer from1 to32");
          next.limits.max_influences = uint32_t(influences);
          next.limits.prune = float(c.fields[5]->value());
          next.locks.clear();
          if (c.keep->value())
            for (int i = 1; i <= c.locks->nitems(); ++i)
              next.locks.push_back(c.locks->checked(i));
          c.selected_scope = (next.mode == SkinAuthoringMode::SourceVisible ||
                              next.mode == SkinAuthoringMode::NativeVisible) &&
                             c.scope->value() == 1;
          if (c.selected_scope &&
              !(next.mode == SkinAuthoringMode::SourceVisible
                    ? c.source_selection_ok
                    : c.selection_ok))
            throw std::invalid_argument("Select editable native parts or parts "
                                        "of the same source group");
          next.validate();
          *c.options = std::move(next);
          c.accepted = true;
          c.window->hide();
        } catch (const std::exception &e) {
          fl_alert("%s", e.what());
        }
      },
      &context);
  window.end();
  window.set_modal();
  dx::style(&window);
  window.show();
  cancel.take_focus();
  while (window.shown())
    Fl::wait();
  if (context.accepted) {
    if (context.selected_scope)
      doc.bind_skin_selected(std::move(options), draft.source,
                             state->selected_objects);
    else
      doc.bind_skin(std::move(options), draft.source);
  }
}
} // namespace dome
