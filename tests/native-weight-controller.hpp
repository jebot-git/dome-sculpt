// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <numbers>
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
