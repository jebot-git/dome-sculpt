// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include "dome/emission-native.hpp"
#include "dome/native-authoring.hpp"
#include "native-read-scope.hpp"
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>
namespace dome {
LoadedSceneFile load_scene_file(const std::filesystem::path &path,
                               const std::function<bool()> &cancelled) {
  detail::NativeReadScope native_read(cancelled);
  constexpr size_t limit = 256 * 1024 * 1024;
  auto check = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Project loading cancelled");
  };
  check();
  const auto size = std::filesystem::file_size(path);
  if (size > limit)
    throw std::length_error("Scene exceeds file size limit");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open scene");
  std::vector<uint8_t> bytes;
  bytes.reserve(size_t(size));
  std::array<char, 65536> chunk;
  while (input) {
    check();
    input.read(chunk.data(), chunk.size());
    const auto count = size_t(input.gcount());
    if (count > limit - bytes.size())
      throw std::length_error("Scene exceeds file size limit");
    // File growth cannot make vector's geometric capacity exceed the file cap.
    if (count > bytes.capacity() - bytes.size())
      bytes.reserve(bytes.size() + count);
    bytes.insert(bytes.end(), chunk.data(), chunk.data() + count);
  }
  if (!input.eof()) throw std::runtime_error("Cannot read scene");
  check();
  if (bytes.size() >= 4 && bytes[0] == 'g' && bytes[1] == 'l' &&
      bytes[2] == 'T' && bytes[3] == 'F') {
    NativeEmissionReadLimits limits;
    limits.cancelled = cancelled;
    auto native = extract_native_emission(bytes, limits);
    check();
    if (!native) {
      NativeAuthoringReadLimits authoring;
      authoring.cancelled = cancelled;
      native = extract_native_authoring(bytes, authoring);
      check();
    }
    if (!native)
      throw std::invalid_argument(
          "This model has no editable Dome Sculpt project; use Import mesh");
    return {std::move(*native), SceneFileKind::AuthoringPackage};
  }
  auto json = nlohmann::json::from_cbor(bytes);
  check();
  auto scene = Scene::decode(json);
  check();
  return {std::move(scene), SceneFileKind::Project};
}
} // namespace dome
