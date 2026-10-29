// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
using namespace std::chrono_literals;
namespace {
void check(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
template<class F> void wait(F condition) {
  auto deadline = std::chrono::steady_clock::now() + 10s;
  while (!condition()) {
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("Recovery operation timed out");
    std::this_thread::sleep_for(10ms);
  }
}
struct Driver {
  Document &doc;
  SpatialFrame frame;
  Driver(Document &d) : doc(d) {
    frame.focused = frame.head_valid = true;
    frame.head.position = {0, 1.6f, 0};
    frame.hands[1].tracked.valid = true;
    frame.hands[1].tracked.grip.position = {0, 1.48f, -.5f};
    send();
  }
  void send(float x = 0, float y = 0, bool click = false) {
    ++frame.sequence; frame.time += .05;
    frame.hands[1].x = x; frame.hands[1].y = y;
    frame.hands[1].joystick = click;
    doc.submit(frame);
    if (!doc.wait_input()) throw std::runtime_error("Input worker timed out");
  }
  void sector(int i) {
    float a = i * 3.14159265358979323846f / 6;
    send(std::cos(a), std::sin(a));
    send(std::cos(a), std::sin(a), true); send();
  }
  void recovery() {
    send(); send(0, 0, true); send();
    sector(8); sector(11); sector(10);
    check(doc.frame().state->menu_page == 8, "Joystick reaches native recovery page");
  }
};
}
int main(int argc, char **argv) try {
  if (argc == 3 && std::string(argv[1]) == "--ui-fixtures") {
    RecoveryService service(argv[2]);
    for (const char *name : {"Project", "Project / two"}) {
      Scene scene;
      RecoverySource source{new_recovery_id(), 1, name};
      service.set_source(source);
      service.enqueue({source, 1, 1, scene.snapshot()});
      if (!service.flush() || !service.status().error.empty())
        throw std::runtime_error(service.status().error);
    }
    return 0;
  }
  if (argc != 1) throw std::invalid_argument("Unexpected test arguments");
  auto root = std::filesystem::temp_directory_path() /
              ("dome-document-recovery-" + std::to_string(getpid()));
  std::filesystem::remove_all(root);
  struct Cleanup { std::filesystem::path path; ~Cleanup() {
    std::error_code ec; std::filesystem::remove_all(path, ec);
  }} cleanup{root};
  Document doc;
  doc.seed();
  doc.enable_recovery(root);
  check(!std::filesystem::exists(root), "Clean documents do not create checkpoints");
  check(doc.begin_desktop(), "Acquire sculpt transaction");
  Brush brush; brush.tool = Tool::Carve; brush.from = brush.to = {0,0,.35f}; brush.radius = .1f;
  doc.stamp_desktop(brush);
  check(doc.recovery_status().accepted == 0, "Incomplete stroke is never checkpointed");
  doc.end_desktop();
  wait([&] { return doc.recovery_status().committed == 1; });
  auto found = RecoveryService::discover(root);
  check(found.candidates.size() == 1 && found.errors.empty(), "Completed edit becomes discoverable");
  auto edited = doc.copy_volume().voxels().size();
  bool rejected = false;
  try { doc.restore_recovery(); } catch (...) { rejected = true; }
  check(rejected, "Recovery cannot replace unsaved edits");
  rejected = false;
  try { doc.save(root / "missing" / "failed.dsculpt"); } catch (...) { rejected = true; }
  check(rejected && RecoveryService::discover(root).candidates.size() == 1,
        "Failed save retains recovery");
  doc.seed();
  Driver driver(doc);
  driver.recovery();
  driver.sector(0);
  wait([&] { return !doc.frame().state->recovery_busy; });
  check(doc.frame().state->recovery_candidates.size() == 1,
        "Replacement retains previous document checkpoint");
  driver.recovery();
  driver.sector(3);
  wait([&] { return !doc.frame().state->recovery_busy; });
  check(doc.frame().state->dirty && !doc.frame().state->asset_busy &&
        doc.copy_volume().voxels().size() == edited,
        "Restore produces editable unsaved project");
  wait([&] { return doc.recovery_status().committed >= 2; });
  doc.save(root / "recovered.dsculpt");
  wait([&] { auto s = doc.recovery_status(); return !s.busy && !s.pending; });
  check(!doc.frame().state->dirty && std::filesystem::exists(root / "recovered.dsculpt"),
        "Durable save marks restored project clean");
  found = RecoveryService::discover(root);
  check(found.candidates.size() == 1,
        "Save removes only current source checkpoints, retaining original recovery");
  return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
