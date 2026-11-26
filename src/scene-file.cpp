// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include "dome/emission-native.hpp"
#include "dome/native-authoring.hpp"
#include "native-read-scope.hpp"
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <streambuf>
#include <utility>
namespace dome {
namespace {
constexpr size_t project_byte_limit = 256 * 1024 * 1024;

void check_read(const std::ifstream &input) {
  if (input.bad() || (input.fail() && !input.eof()))
    throw std::runtime_error("Cannot read scene");
}

class ProjectInputBuffer final : public std::streambuf {
public:
  ProjectInputBuffer(std::ifstream &input, const std::function<bool()> &cancelled)
      : input_(input), cancelled_(cancelled) {}

private:
  int_type underflow() override {
    if (cancelled_ && cancelled_())
      throw std::runtime_error("Project loading cancelled");
    input_.read(buffer_.data(), buffer_.size());
    check_read(input_);
    const auto count = size_t(input_.gcount());
    if (count > project_byte_limit - read_bytes_)
      throw std::length_error("Scene exceeds file size limit");
    if (!count)
      return traits_type::eof();
    read_bytes_ += count;
    setg(buffer_.data(), buffer_.data(), buffer_.data() + count);
    return traits_type::to_int_type(*gptr());
  }

  std::ifstream &input_;
  const std::function<bool()> &cancelled_;
  size_t read_bytes_ = 0;
  std::array<char, 65536> buffer_;
};
} // namespace

LoadedSceneFile load_scene_file(const std::filesystem::path &path,
                               const std::function<bool()> &cancelled) {
  detail::NativeReadScope native_read(cancelled);
  auto check = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Project loading cancelled");
  };
  check();
  const auto size = std::filesystem::file_size(path);
  if (size > project_byte_limit)
    throw std::length_error("Scene exceeds file size limit");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open scene");
  std::array<char, 4> prefix{};
  input.read(prefix.data(), prefix.size());
  check_read(input);
  input.clear();
  input.seekg(0);
  if (!input) throw std::runtime_error("Cannot seek scene");
  check();
  if (prefix == std::array<char, 4>{'g', 'l', 'T', 'F'}) {
    // Authenticated GLB extraction uses random access to the entire container.
    std::vector<uint8_t> bytes;
    bytes.reserve(size_t(size));
    std::array<char, 65536> chunk;
    while (input) {
      check();
      input.read(chunk.data(), chunk.size());
      check_read(input);
      const auto count = size_t(input.gcount());
      if (count > project_byte_limit - bytes.size())
        throw std::length_error("Scene exceeds file size limit");
      // File growth cannot make vector's geometric capacity exceed the cap.
      if (count > bytes.capacity() - bytes.size())
        bytes.reserve(bytes.size() + count);
      bytes.insert(bytes.end(), chunk.data(), chunk.data() + count);
    }
    check();
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
  ProjectInputBuffer buffer(input, cancelled);
  std::istream project(&buffer);
  auto json = nlohmann::json::from_cbor(project);
  check();
  auto scene = Scene::decode(json);
  check();
  return {std::move(scene), SceneFileKind::Project};
}
} // namespace dome
