// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/scene.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << '\n';
}
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Missing genuine malformed source fixture");
  return {std::istreambuf_iterator<char>(f), {}};
}
ImportedAvatarVersion kind(const Scene &scene) {
  for (auto &o : scene.objects())
    if (o.appearance)
      return o.appearance->asset->avatar_version;
  throw std::runtime_error("Scene has no immutable imported model");
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  std::filesystem::path directory = argv[1];
  auto valid = Scene::load(directory / "worker.dsculpt");
  auto exact = valid.encode();
  check(kind(valid) == ImportedAvatarVersion::Vrm1,
        "Real native restart derives V1 from its validated immutable source");
  check(exact.at("source_models").size() == 1,
        "Replay negatives target the actual single deduplicated model owner");
  for (int invalid = 0; invalid < 6; ++invalid) {
    auto malformed = exact;
    malformed["source_models"][0]["original_glb"] = nlohmann::json::binary(
        bytes(directory / ("invalid" + std::to_string(invalid) + ".vrm")));
    std::string diagnostic;
    try {
      (void)Scene::decode(
          nlohmann::json::from_cbor(nlohmann::json::to_cbor(malformed)));
    } catch (const std::exception &e) {
      diagnostic = e.what();
    }
    check(!diagnostic.empty() &&
              (diagnostic.find("VRM1 metadata") != std::string::npos ||
               diagnostic.find("exact VRM1.0") != std::string::npos),
          "Actual retained-source CBOR replay refuses malformed or unsupported "
          "metadata through the version-authenticated parser");
    check(valid.encode() == exact,
          "Malformed retained-source replay leaves the original native model, "
          "paint, skin, metadata and history input exact");
  }
  auto ordinary = Scene::load(directory / "ordinary.dsculpt");
  check(
      kind(ordinary) == ImportedAvatarVersion::None,
      "ExtensionsUsed-only VRM declaration remains None after actual restart");
  auto forged = ordinary.encode();
  forged["source_models"][0]["avatar_version"] = 2;
  auto replay =
      Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(forged)));
  check(kind(replay) == ImportedAvatarVersion::None &&
            replay.encode() == ordinary.encode(),
        "A fabricated serialized advisory version cannot create absent raw "
        "avatar authority");
  std::cout << checks << " actual VRM1 retained-source replay checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
