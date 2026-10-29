// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
namespace dome {
class SculptWindow : public Fl_Double_Window {
public:
  using Fl_Double_Window::Fl_Double_Window;
  int handle(int event) override {
    int mods = Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META);
    if ((event == FL_KEYDOWN || event == FL_SHORTCUT) && mods == FL_CTRL &&
        Fl::event_key() == 'q') {
      do_callback();
      return 1;
    }
    if (event == FL_SHORTCUT && Fl::event_key() == FL_Escape)
      return 1;
    return Fl_Double_Window::handle(event);
  }
};
class DomeDialog : public Fl_Double_Window {
public:
  using Fl_Double_Window::Fl_Double_Window;
  int handle(int event) override {
    int mods = Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META);
    if ((event == FL_KEYDOWN || event == FL_SHORTCUT) &&
        (Fl::event_key() == FL_Escape ||
         (mods == FL_CTRL && Fl::event_key() == 'w'))) {
      hide();
      return 1;
    }
    if (event == FL_KEYDOWN &&
        (Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter)) {
      auto button = dynamic_cast<Fl_Button *>(Fl::focus());
      if (button && button->window() == this && button->active_r()) {
        button->do_callback();
        return 1;
      }
    }
    return Fl_Double_Window::handle(event);
  }
};
bool confirm_discard(Fl_Window &parent);
} // namespace dome
