// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::invalid_argument(
        "Choose fixture/inspect/expected/restore and file");
  const std::string mode = argv[1];
  if (mode == "--ui-fixture") {
    Scene s;
    append_humanoid(s, HumanoidControls{}, "Source-assisted native", {}, 64);
    for (const auto &o : s.objects())
      if (o.painted && o.name.find("Body") != std::string::npos) {
        s.select(o.id);
        break;
      }
    s.save(argv[2]);
    return 0;
  }
  auto s = Scene::load(argv[2]);
  if (mode == "--expected") {
    SkinAuthoringOptions o;
    o.mode = SkinAuthoringMode::SourceVisible;
    o.reference = s.active().id;
    std::array<uint64_t, 1> ids{s.active().id};
    if (!author_source_visible_skin(s, ids, o))
      throw std::runtime_error("Expected real changed bind");
  } else if (mode == "--restore") {
    const auto id = s.active().id;
    if (!s.active().rig_source)
      throw std::runtime_error("Missing original source");
    restore_rig_source(s, id, false);
  } else if (mode != "--inspect")
    throw std::invalid_argument("Unknown mode");
  std::cout << nlohmann::json{{"scene", s.encode()},
                              {"active", s.active().id},
                              {"name", s.active().name},
                              {"rig_source", bool(s.active().rig_source)}}
                   .dump();
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
