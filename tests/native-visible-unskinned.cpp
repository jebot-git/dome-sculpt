// SPDX-License-Identifier: GPL-3.0-or-later
// Genuine unskinned CC0 import. Original GLB JSON supplies explicit landmark
// coordinates only: this reader never reads the original BIN/artist values.
#include "dome/import-authoring.hpp"
#include "dome/rig-fit.hpp"
#include "dome/skin-authoring.hpp"
#include "geometry-weight-authoring.hpp"
#include "native-weight-reference.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
nlohmann::json original_metadata(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  uint32_t head[5]{};
  file.read(reinterpret_cast<char *>(head), sizeof(head));
  if (!file || head[0] != 0x46546c67 || head[1] != 2 || head[4] != 0x4e4f534a ||
      head[3] > 4 * 1024 * 1024)
    throw std::runtime_error("Bounded original JSON chunk required");
  std::string json(head[3], '\0');
  file.read(json.data(), json.size());
  if (!file)
    throw std::runtime_error("Original JSON read failed");
  return nlohmann::json::parse(json);
}
Vec3 point(const AffineTransform &m, Vec3 p) {
  const auto q = m.point({p.x, p.y, p.z});
  return {float(q[0]), float(q[1]), float(q[2])};
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 5 &&
      (argc != 6 || std::string_view(argv[5]) != "finite-line-integral"))
    throw std::invalid_argument(
        "Choose original JSON-source GLB, genuinely static GLB, output "
        "directory, target substring or all");
  const std::filesystem::path original = argv[1], input = argv[2],
                              out = argv[3];
  const std::string selection = argv[4];
  const bool integral = argc == 6;
  std::filesystem::create_directories(out);
  ImportLimits limits;
  limits.mode = ImportMode::PreserveGltf;
  auto raw = import_mesh(input, limits);
  if (!raw.skins.empty() || raw.primitives.size() != 19)
    throw std::runtime_error(
        "Expected complete genuinely unskinned19-part input");
  for (const auto &p : raw.primitives)
    if (!p.influences.empty())
      throw std::runtime_error("Artist weight values are not a solver input");
  std::cout << "STAGE static input verified19parts no skins or influences\n"
            << std::flush;
  ImportAuthoringOptions choices;
  choices.paint_resolution = 64;
  auto imported = prepare_authoring_import(Scene{}, input, choices);
  auto &scene = imported.scene;
  if (imported.objects.size() != 19)
    throw std::runtime_error("Actual native import must retain all19 parts");
  uint64_t reference = 0;
  size_t biggest = 0;
  for (auto id : imported.objects) {
    const auto &o = scene.object(id);
    std::cout << "PART " << id << ' ' << o.name
              << " vertices=" << o.painted->mesh().positions.size() << '\n';
    if (o.name.find("Body") != std::string::npos &&
        o.painted->mesh().positions.size() > biggest) {
      reference = id;
      biggest = o.painted->mesh().positions.size();
    }
  }
  if (!reference)
    throw std::runtime_error("No genuine body surface reference");
  const auto metadata = original_metadata(original);
  HumanLandmarks landmarks;
  landmarks.left = {-1, 0, 0};
  landmarks.front = {0, 0, -1};
  const auto inverse = scene.world_matrix(reference).inverse();
  for (const auto &entry :
       metadata.at("extensions").at("VRM").at("humanoid").at("humanBones")) {
    auto role = entry.at("bone").get<std::string>();
    // Explicit VRM0 -> native/VRM1 thumb role semantics, as in vroid-creator.
    if (role.ends_with("ThumbProximal"))
      role.replace(role.size() - 8, 8, "Metacarpal");
    else if (role.ends_with("ThumbIntermediate"))
      role.replace(role.size() - 12, 12, "Proximal");
    const auto node = entry.at("node").get<size_t>();
    if (node >= raw.nodes.size())
      throw std::runtime_error(
          "Landmark source-node identity outside retained static model");
    const auto &m = raw.nodes[node].world;
    landmarks.points.emplace(role, point(inverse, {m[12], m[13], m[14]}));
  }
  std::ofstream(out / "landmarks.json") << landmarks.encode().dump(2);
  const auto before_fit = scene.encode();
  scene.save(out / "original-unskinned.dsculpt");
  RigFitOptions fit;
  fit.bind = false;
  std::cout << "STAGE actual explicit landmark fit reference=" << reference
            << " landmarks=" << landmarks.points.size() << std::endl;
  const auto fitted = author_humanoid_fit(scene, reference, landmarks, fit);
  std::cout << "FIT bones=" << fitted.bones << " bound=" << fitted.bound
            << std::endl;
  if (fitted.bound || !scene.object(reference).deformation->skeleton ||
      scene.object(reference).deformation->skin)
    throw std::runtime_error(
        "Explicit native intent must remain unweighted before solver");
  auto before = scene.encode();
  scene.save(out / "fitted-unskinned.dsculpt");
  std::vector<uint64_t> targets;
  for (auto id : imported.objects)
    if (selection == "all" ||
        scene.object(id).name.find(selection) != std::string::npos)
      targets.push_back(id);
  if (targets.empty())
    throw std::runtime_error("No explicit selected target");
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::NativeVisible;
  options.reference = reference;
  if (integral)
    options.native_visible.contact_handling =
        NativeContactHandling::FiniteLineIntegral;
  NativeVisibleSkinReport report;
  const auto start = std::chrono::steady_clock::now();
  std::cout << "STAGE genuine native solve targets=" << targets.size()
            << " construction="
            << (integral ? "native-finite-line-integral"
                         : "public-pointwise-native")
            << " work_bound=" << options.native_visible.maximum_work
            << std::endl;
  try {
    const bool changed =
        author_native_visible_skin(scene, targets, options, {}, &report);
    if (!changed)
      throw std::runtime_error("No native field produced");
  } catch (const std::exception &e) {
    const bool exact = scene.encode() == before;
    std::ofstream(out / "failure.json") << nlohmann::json{
        {"error", e.what()},
        {"exact_rollback", exact},
        {"elapsed",
         std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
             .count()},
        {"targets", targets},
        {"work_bound",
         options.native_visible.maximum_work}}.dump(2);
    if (!exact)
      throw std::runtime_error("Genuine native failure changed owning Scene");
    throw;
  }
  scene.save(out / "bound.dsculpt");
  std::cout << "RESULT changed=" << report.changed_parts
            << " rows=" << report.rows << " work=" << report.work
            << " bytes=" << report.planned_workspace << " seconds="
            << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             start)
                   .count()
            << " measure_zero_origins=" << report.measure_zero_origins
            << " quality_unmeasured=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
