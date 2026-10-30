// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/import-authoring.hpp"
#include "dome/import-source.hpp"
#include "dome/polygon-source.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
int main() try {
  auto directory = std::filesystem::temp_directory_path() /
                   ("dome-polygon-import-" + std::to_string(getpid()));
  std::filesystem::create_directories(directory);
  struct Cleanup {
    std::filesystem::path directory;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(directory, error);
    }
  } cleanup{directory};
  const std::string bytes =
      "o folded_quads\nv -1000 0 0\nv 0 0 0\nv 0 1000 0\nv -1000 1000 0\nv 0 0 "
      "1000\nv 0 1000 1000\nvt 0 0\nvt 0.4 0\nvt 0.4 0.4\nvt 0 0.4\nvt 0.6 "
      "0\nvt 1 0\nvt 1 0.4\nvt 0.6 0.4\nvn 0 0 1\nvn -1 0 0\nf 1/1/1 2/2/1 "
      "3/3/1 4/4/1\nf 2/5/2 5/6/2 6/7/2 3/8/2\n";
  auto path = directory / "source.obj";
  {
    std::ofstream out(path, std::ios::binary);
    out << bytes;
    if (!out)
      throw std::runtime_error("Unable to write source fixture");
  }
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  options.choices.units = ImportUnits::Millimetres;
  auto imported = prepare_authoring_import(Scene{}, path, options);
  if (imported.objects.size() != 1)
    throw std::runtime_error(
        "Source fixture did not produce one native authoring object");
  auto &object = imported.scene.object(imported.objects.front());
  auto original = imported.scene.encode();
  auto result =
      prepare_polygon_topology(object.painted->mesh(), object.appearance.get());
  if (!result.authored_ngons || result.topology.faces().size() != 2 ||
      result.topology.vertices().size() != 6 ||
      result.topology.edges().size() != 7)
    throw std::runtime_error(
        "Actual OBJ adapter loops did not recover two adjacent native N-gons");
  std::cout << "PASS Actual ufbx→canonicalGLB→native authoring→polygon "
               "preparation restores original adjacent quads\n";
  if (std::count_if(result.topology.edges().begin(),
                    result.topology.edges().end(),
                    [](auto &edge) { return edge.hard; }) != 1)
    throw std::runtime_error(
        "Actual original normal seam did not preserve its hard shared edge");
  std::cout << "PASS Actual folded source normal seam remains an explicit hard "
               "polygon edge\n";
  for (auto &vertex : result.topology.vertices())
    if (std::max({std::abs(vertex.position.x), std::abs(vertex.position.y),
                  std::abs(vertex.position.z)}) > 1.00001f)
      throw std::runtime_error("Source millimetres were not preserved as "
                               "metre-local polygon coordinates");
  std::cout << "PASS Imported1000mm coordinates retain exact physical "
               "object-local scale\n";
  auto render = result.topology.triangulate();
  for (size_t i = 0; i < render.mesh.positions.size(); ++i) {
    auto n = result.source_vertices.at(i);
    if (render.mesh.uv[i] != object.painted->mesh().uv[n])
      throw std::runtime_error(
          "Actual source UV seam changed during polygon reconstruction");
  }
  std::cout << "PASS Actual authoring UV seams retain their exact old corner "
               "witnesses\n";
  if (!object.appearance->source_receipt ||
      !object.appearance->source_receipt->root.bytes ||
      std::string(object.appearance->source_receipt->root.bytes->begin(),
                  object.appearance->source_receipt->root.bytes->end()) !=
          bytes ||
      imported.scene.encode() != original)
    throw std::runtime_error("Polygon preparation changed original raw receipt "
                             "or committed native Scene");
  std::cout << "PASS Actual source receipt and full committed authoring Scene "
               "remain exact\n";
  auto restart = Scene::decode(original);
  auto &restored = restart.object(object.id);
  auto after = prepare_polygon_topology(restored.painted->mesh(),
                                        restored.appearance.get());
  if (after.topology.encode() != result.topology.encode() ||
      after.source_vertices != result.source_vertices)
    throw std::runtime_error("Scene11 source restart changed deterministic "
                             "native polygon authority");
  std::cout
      << "PASS Scene11 source restart retains deterministic original N-gon "
         "reconstruction\n6 polygon import integration checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
