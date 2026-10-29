// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui-conventions.hpp"
#include "appearance.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
namespace dome {
bool confirm_discard(Fl_Window &parent) {
  int row = std::max(30, dx::size + 12), width = std::max(420, dx::size * 28),
      height = row * 5;
  DomeDialog dialog(parent.x() + (parent.w() - width) / 2,
                    parent.y() + (parent.h() - height) / 2, width, height,
                    "Unsaved changes");
  Fl_Box text(20, 10, width - 40, row * 2, "Discard unsaved sculpt changes?");
  text.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  Fl_Return_Button cancel(20, height - row - 20, (width - 60) / 2, row,
                          "Cancel");
  Fl_Button discard(40 + (width - 60) / 2, height - row - 20, (width - 60) / 2,
                    row, "Discard");
  bool accepted = false;
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  discard.callback(
      [](Fl_Widget *w, void *v) {
        *static_cast<bool *>(v) = true;
        w->window()->hide();
      },
      &accepted);
  dialog.end();
  dialog.set_modal();
  dx::style(&dialog);
  dialog.show();
  cancel.take_focus();
  while (dialog.shown())
    Fl::wait();
  return accepted;
}
} // namespace dome
