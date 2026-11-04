// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include "native-weight-context.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
uint64_t reference(const Scene &s) {
  uint64_t id = 0;
  for (const auto &o : s.objects())
    if (o.deformation && o.deformation->skeleton) {
      if (id && id != o.id)
        throw std::runtime_error("Ambiguous native reference");
      id = o.id;
    }
  if (!id)
    throw std::runtime_error("No genuine fitted native reference");
  return id;
}
nlohmann::json inspect(const Scene &s) {
  auto id = reference(s);
  const auto &o = s.object(id);
  const auto context = detail::native_node_context(s, id, std::span(&id, 1));
  return {{"scene", s.encode()},
          {"name", o.name},
          {"rig_source", bool(o.rig_source)},
          {"bound", bool(o.deformation && o.deformation->skin)},
          {"target", id},
          {"context_parts", context.count}};
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::runtime_error(
        "Choose fixture/inspect/expected/restore and exact file");
  const std::string_view command = argv[1];
  if (command == "--ui-fixture") {
    const auto source = std::getenv("DOME_NATIVE_CONTEXT_INPUT");
    if (!source)
      throw std::runtime_error("Provide authentic unskinned fitted input");
    auto s = Scene::load(source);
    auto id = reference(s);
    const auto &a = s.object(id).appearance;
    if (!a || !a->asset || !a->asset->skins.empty())
      throw std::runtime_error("Actual input has retained artist skins");
    for (const auto &p : a->asset->primitives)
      if (!p.influences.empty())
        throw std::runtime_error("Artist influences in input");
    if (detail::native_node_context(s, id, std::span(&id, 1)).count != 9)
      throw std::runtime_error(
          "Actual fixture must retain complete nine-part original node");
    s.select(id);
    s.save(argv[2]);
    return 0;
  }
  auto s = Scene::load(argv[2]);
  auto id = reference(s);
  if (command == "--expected") {
    SkinAuthoringOptions o;
    o.mode = SkinAuthoringMode::NativeVisible;
    o.reference = id;
    o.native_visible.contact_handling =
        NativeContactHandling::FiniteLineIntegral;
    o.native_visible.support_context =
        NativeSupportContext::CompleteReferenceNode;
    author_native_visible_skin(s, std::span(&id, 1), o);
  } else if (command == "--restore")
    restore_rig_source(s, id, false);
  else if (command != "--inspect")
    throw std::runtime_error("Unknown inspection command");
  std::cout << inspect(s).dump() << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
