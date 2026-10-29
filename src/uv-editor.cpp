// SPDX-License-Identifier: GPL-3.0-or-later
#include "uv-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
namespace dome {
namespace {
class UvCanvas : public Fl_Widget {
  Document &doc_;
  bool dragging_ = false, wheeling_ = false;
  float last_u_ = 0, last_v_ = 0;
  int side() const { return std::max(1, std::min(w(), h()) - 40); }
  int left() const { return x() + (w() - side()) / 2; }
  int top() const { return y() + (h() - side()) / 2; }
  static void wheel_finished(void *data) {
    auto &self = *static_cast<UvCanvas *>(data);
    self.wheeling_ = false;
    auto view = self.doc_.frame().state->uv;
    if (view && !view->committing && !view->solving) {
      try {
        self.doc_.edit_uv(UvAction::EndGesture);
      } catch (const std::exception &e) {
        self.doc_.report_error(e.what());
      }
    }
  }
  void finish_gesture() {
    Fl::remove_timeout(wheel_finished, this);
    dragging_ = wheeling_ = false;
    auto view = doc_.frame().state->uv;
    if (view && !view->committing && !view->solving)
      doc_.edit_uv(UvAction::EndGesture);
  }

public:
  UvCanvas(int x, int y, int w, int h, Document &d)
      : Fl_Widget(x, y, w, h), doc_(d) {}
  ~UvCanvas() override { Fl::remove_timeout(wheel_finished, this); }
  int handle(int event) override try {
    auto view = doc_.frame().state->uv;
    if (view && (view->solving || view->committing) &&
        (event == FL_PUSH || event == FL_DRAG || event == FL_MOUSEWHEEL)) return 1;
    if (event == FL_FOCUS)
      return 1;
    if (event == FL_UNFOCUS) {
      finish_gesture();
      return 1;
    }
    if (event == FL_KEYDOWN && (Fl::event_state() & FL_CTRL) &&
        Fl::event_key() == 'z') {
      finish_gesture();
      doc_.edit_uv(Fl::event_state(FL_SHIFT) ? UvAction::Redo : UvAction::Undo);
      redraw();
      return 1;
    }
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
      float u = float(Fl::event_x() - left()) / side(),
            v = float(Fl::event_y() - top()) / side();
      if (u < 0 || u > 1 || v < 0 || v > 1)
        return 1;
      take_focus();
      finish_gesture();
      doc_.edit_uv(UvAction::Pick, u, v);
      doc_.edit_uv(UvAction::BeginGesture);
      dragging_ = true;
      last_u_ = u;
      last_v_ = v;
      redraw();
      return 1;
    }
    if (event == FL_DRAG && dragging_) {
      float u = float(Fl::event_x() - left()) / side(),
            v = float(Fl::event_y() - top()) / side();
      doc_.edit_uv(UvAction::BeginGesture);
      doc_.edit_uv(UvAction::Move, u - last_u_, v - last_v_);
      last_u_ = u;
      last_v_ = v;
      redraw();
      return 1;
    }
    if (event == FL_RELEASE) {
      finish_gesture();
      return 1;
    }
    if (event == FL_MOUSEWHEEL) {
      if (!wheeling_)
        finish_gesture();
      doc_.edit_uv(UvAction::BeginGesture);
      wheeling_ = true;
      Fl::remove_timeout(wheel_finished, this);
      Fl::add_timeout(.25, wheel_finished, this);
      doc_.edit_uv(
          Fl::event_state(FL_SHIFT) ? UvAction::Rotate : UvAction::Scale,
          Fl::event_state(FL_SHIFT) ? -.05f * Fl::event_dy()
                                    : std::exp(-.05f * Fl::event_dy()));
      redraw();
      return 1;
    }
    return Fl_Widget::handle(event);
  } catch (const std::exception &e) {
    doc_.report_error(e.what());
    return 1;
  }
  void draw() override {
    auto s = doc_.frame().state;
    if (!s->uv)
      return;
    auto &v = *s->uv;
    auto &m = *v.mesh;
    fl_push_clip(x(), y(), w(), h());
    fl_color(s->light ? fl_rgb_color(208, 208, 208) : fl_rgb_color(48, 48, 48));
    fl_rectf(x(), y(), w(), h());
    auto line = [&](std::array<float, 2> a, std::array<float, 2> b) {
      for (int k = 0; k < 2; ++k) {
        a[k] = std::clamp(a[k], -1.f, 2.f);
        b[k] = std::clamp(b[k], -1.f, 2.f);
      }
      fl_line(left() + int(a[0] * side()), top() + int(a[1] * side()),
              left() + int(b[0] * side()), top() + int(b[1] * side()));
    };
    fl_color(s->light ? fl_rgb_color(170, 170, 170) : fl_rgb_color(70, 70, 70));
    for (int i = 0; i <= 8; ++i) {
      float p = i / 8.f;
      line({p, 0}, {p, 1});
      line({0, p}, {1, p});
    }
    for (auto &e : v.topology->edges) {
      bool selected = false;
      for (auto c : e.corners)
        selected |=
            v.topology->face_island[c / 3] == v.topology->face_island[v.face];
      fl_color(selected ? FL_SELECTION_COLOR : FL_FOREGROUND_COLOR);
      if (std::any_of(e.corners.begin(), e.corners.end(),
                      [&](auto c) { return m.uv_seams[c]; }))
        fl_color(fl_rgb_color(255, 170, 80));
      for (auto c : e.corners)
        line(m.uv[m.indices[c]], m.uv[m.indices[c / 3 * 3 + (c + 1) % 3]]);
    }
    if (v.edge < m.indices.size()) {
      auto a = v.edge, b = a / 3 * 3 + (a + 1) % 3;
      fl_color(FL_SELECTION_COLOR);
      fl_line_style(FL_SOLID, 3);
      line(m.uv[m.indices[a]], m.uv[m.indices[b]]);
      fl_line_style(0);
    }
    if (v.vertices)
      for (uint32_t i = 0; i < m.uv.size(); ++i) {
        auto p = m.uv[i];
        if (p[0] < -1 || p[0] > 2 || p[1] < -1 || p[1] > 2)
          continue;
        fl_color(m.uv_pins[i]    ? fl_rgb_color(255, 170, 80)
                 : i == v.vertex ? FL_SELECTION_COLOR
                                 : FL_FOREGROUND_COLOR);
        int n = i == v.vertex ? 9 : 5;
        fl_rectf(left() + int(p[0] * side()) - n / 2,
                 top() + int(p[1] * side()) - n / 2, n, n);
      }
    fl_pop_clip();
  }
};
} // namespace
void edit_uv(Document &doc) {
  doc.begin_uv();
  try {
    int row = std::max(32, dx::size + 14), sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh);
    int width = std::min(std::max(840, dx::size * 46), sw - 40),
        height = std::min(900, sh - 40);
    DomeDialog window(width, height, "UV editor");
    window.begin();
    UvCanvas canvas(0, row * 3, width, height - row * 5, doc);
    Fl_Box status(12, height - row * 2, width - 24, row);
    status.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    struct Command {
      Document *doc;
      UvCanvas *canvas;
      Fl_Box *status;
      Fl_Button *apply;
      UvAction action;
      float value;
    };
    std::vector<std::unique_ptr<Command>> commands;
    std::vector<Fl_Button *> buttons;
    auto refresh = [&] {
      auto state = doc.frame().state;
      auto v = state->uv;
      if (!v) {
        window.hide();
        return;
      }
      status.copy_label(
          v->solving ? state->message.c_str()
          : !state->error.empty() ? state->error.c_str()
          : v->valid
              ? (v->vertices ? "Vertices: drag | Wheel: scale | Shift+wheel: "
                               "turn | Orange: pinned/cut"
                             : "Islands: drag | Wheel: scale | Shift+wheel: "
                               "turn | Pick seam edge for Seam/Stitch")
              : v->error.c_str());
      if (buttons.size() > 5) {
        for (auto button : buttons)
          if (v->committing || v->solving)
            button->deactivate();
          else
            button->activate();
        if (v->can_undo && !v->committing && !v->solving)
          buttons[4]->activate();
        else
          buttons[4]->deactivate();
        if (v->can_redo && !v->committing && !v->solving)
          buttons[5]->activate();
        else
          buttons[5]->deactivate();
      }
      canvas.redraw();
    };
    struct Labels {
      const char *label;
      UvAction action;
      float value;
    };
    const Labels items[] = {{"Scale -", UvAction::Scale, .9f},
                            {"Scale +", UvAction::Scale, 1.1f},
                            {"Turn -", UvAction::Rotate, -.2617994f},
                            {"Turn +", UvAction::Rotate, .2617994f},
                            {"Undo", UvAction::Undo, 0},
                            {"Redo", UvAction::Redo, 0},
                            {"Mode", UvAction::VertexMode, 0},
                            {"Pin/unpin", UvAction::Pin, 0},
                            {"Relax", UvAction::Relax, 0},
                            {"Pack", UvAction::Pack, 0},
                            {"Seam", UvAction::Seam, 0},
                            {"Next", UvAction::Next, 0},
                            {"Unwrap", UvAction::Conformal, 0},
                            {"Stitch", UvAction::Stitch, 0}};
    Fl_Button cancel(width / 2 - 180, height - row + 4, 160, row - 8, "Cancel"),
        apply(width / 2 + 20, height - row + 4, 160, row - 8, "Apply");
    for (size_t i = 0; i < std::size(items); ++i) {
      int col = i % 6, y = (i / 6) * row;
      auto b = new Fl_Button(col * width / 6 + 4, y + 4, width / 6 - 8, row - 8,
                             items[i].label);
      buttons.push_back(b);
      auto c = std::make_unique<Command>(Command{
          &doc, &canvas, &status, &apply, items[i].action, items[i].value});
      b->callback(
          [](Fl_Widget *, void *ptr) {
            auto &c = *static_cast<Command *>(ptr);
            try {
              c.doc->edit_uv(c.action, c.value);
              auto v = c.doc->frame().state->uv;
              c.status->copy_label(
                  v->valid ? (v->vertices ? "Vertex mode" : "Island mode")
                           : v->error.c_str());
              if (v->valid && !v->solving && !v->committing)
                c.apply->activate();
              else
                c.apply->deactivate();
              c.canvas->redraw();
            } catch (const std::exception &e) {
              c.status->copy_label(e.what());
            }
          },
          c.get());
      commands.push_back(std::move(c));
    }
    struct Result {
      Fl_Window *window;
      Document *doc;
    } result{&window, &doc};
    cancel.callback(
        [](Fl_Widget *, void *p) { static_cast<Result *>(p)->window->hide(); },
        &result);
    apply.callback(
        [](Fl_Widget *, void *p) {
          auto &r = *static_cast<Result *>(p);
          auto view = r.doc->frame().state->uv;
          if (view && view->valid && !view->solving && !view->committing) {
            try {
              r.doc->end_uv(true);
            } catch (const std::exception &e) {
              r.doc->report_error(e.what());
            }
          }
        },
        &result);
    window.end();
    window.resizable(canvas);
    window.size_range(640, 480);
    window.set_modal();
    dx::style(&window);
    refresh();
    window.show();
    while (window.shown()) {
      Fl::wait(.05);
      refresh();
      if (auto view = doc.frame().state->uv; view && view->valid && !view->solving && !view->committing)
        apply.activate();
      else
        apply.deactivate();
    }
    doc.end_uv(false);
  } catch (...) {
    doc.end_uv(false);
    throw;
  }
}
} // namespace dome
