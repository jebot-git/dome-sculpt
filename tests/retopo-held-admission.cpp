// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_RETOPO_HANDOFF_EMBEDDED
#include "retopo-save-handoff.cpp"
namespace {
void hold_point(Document &d) {
  check(d.begin_desktop(), "Actual desktop owns the held retopology gesture");
  Brush b; b.radius = .01f;
  b.from = b.to = point(handoff_matrix, evaluated({.22f,.17f,.061f}, true));
  d.stamp_desktop(b); ready(d);
}
}
int main(int argc, char **argv) try {
  if (argc != 2) return 2;
  const std::filesystem::path out = argv[1]; std::filesystem::create_directories(out);
  const auto s = source(handoff_matrix, true); const auto exact = s.encode();
  size_t measured = 0;
  {
    Document d; draft(d, s); hold_point(d);
    handoff_fault::calls = handoff_fault::fail = 0; handoff_fault::armed = true;
    try { d.end_retopology(true); } catch (...) { handoff_fault::armed = false; throw; }
    handoff_fault::armed = false; measured = handoff_fault::calls;
    check(measured > 0 && measured < 4096, "Actual held-Apply caller boundaries measured");
    check(d.wait_asset() && d.frame().state->error.empty(), "Uninjected held Apply completes");
  }
  std::cout << "HELD_ADMISSION_PREFIXES " << measured << std::endl;
  for (size_t at = 1; at <= measured; ++at) {
    Document d; draft(d, s);
    const auto before = d.frame().state->retopology->topology->encode();
    hold_point(d);
    check(d.frame().state->retopology->topology->encode() != before,
          "The held point visibly changes actual draft geometry");
    handoff_fault::calls = 0; handoff_fault::fail = at; handoff_fault::hit = false;
    bool refused = false; handoff_fault::armed = true;
    try { d.end_retopology(true); } catch (const std::bad_alloc &) { refused = true; }
    catch (...) { handoff_fault::armed = false; throw; }
    handoff_fault::armed = false;
    check(handoff_fault::hit && refused, "Every held admission prefix is persistently denied");
    check(!d.frame().state->asset_busy && d.copy_scene().encode() == exact,
          "Rejected held admission retains original scene and no accepted asset owner");
    // This must still cancel the original live gesture. A hidden queued
    // EndGesture or prematurely released desktop commits the point instead.
    d.cancel_desktop(); ready(d);
    if (d.frame().state->retopology->topology->encode() != before)
      throw std::runtime_error("Rejected held Apply committed the input gesture at prefix " + std::to_string(at));
    check(d.copy_scene().encode() == exact, "Held-gesture cancellation preserves source authority");
    hold_point(d); d.end_retopology(true);
    const auto path = out / "retry.dsculpt"; auto saved = d.save_async(path); saved.get();
    check(d.wait_asset() && Scene::load(path).encode() == d.copy_scene().encode(),
          "A real held gesture retries to an immediate exact accepted Save");
    std::cout << "PASS held prefix " << at << std::endl;
  }
  {
    Document d; draft(d, s); check(d.begin_desktop(), "Sample admission owns an actual gesture"); ready(d);
    const auto view = d.frame().state->retopology;
    Brush b; b.radius = .01f;
    b.from = b.to = point(handoff_matrix, evaluated({.22f,.17f,.061f}, true));
    handoff_fault::calls = 0; handoff_fault::fail = 1; handoff_fault::hit = false;
    bool refused = false; handoff_fault::armed = true;
    try { d.stamp_desktop(b); } catch (const std::bad_alloc &) { refused = true; }
    catch (...) { handoff_fault::armed = false; throw; }
    handoff_fault::armed = false;
    check(refused && handoff_fault::hit, "Input-view preparation allocation is actually denied");
    ready(d);
    check(d.frame().state->retopology == view,
          "A rejected input-view allocation cannot enqueue a hidden sample or publication");
    d.stamp_desktop(b); ready(d);
    check(d.frame().state->retopology->topology->encode() != view->topology->encode(),
          "The same actual input sample succeeds after the rejected preparation");
    d.cancel_desktop(); ready(d);
    check(d.frame().state->retopology->topology->encode() == view->topology->encode(),
          "Cancelling the retried gesture restores exact topology");
  }
  std::cout << "PASS " << checks << " held retopology admission checks\n";
  return 0;
} catch (const std::exception &e) {
  handoff_fault::armed = false; input_gate::release(); atlas_gate::release();
  std::cerr << "FAIL " << e.what() << '\n'; return 1;
}
