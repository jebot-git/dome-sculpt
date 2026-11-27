// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include "dome/emission-native.hpp"
#include "dome/native-authoring.hpp"
#include "io.hpp"
#include "native-read-scope.hpp"
#include "native-scene-write.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <streambuf>
#include <utility>
namespace dome {
namespace {
template<class Write>
class SceneOutputBuffer final : public std::streambuf {
  const Write &write_;
  size_t limit_;
  bool checksum_;
  std::array<char,65536> storage_;
  detail::NativeSceneWriteResult result_;

  size_t buffered() const { return size_t(pptr()-pbase()); }
  void reset() {
    setp(storage_.data(),storage_.data()+std::min(storage_.size(),limit_-result_.bytes));
  }
  void emit(const char *data,size_t count) {
    const std::span bytes(reinterpret_cast<const uint8_t *>(data),count);
    write_(bytes);
    result_.bytes+=count;
    if(checksum_)result_.checksum=detail::extend_scene_checksum(result_.checksum,bytes);
  }
  void flush_pending() {
    if(const auto count=buffered())emit(pbase(),count);
    reset();
  }

  int_type overflow(int_type next) override {
    flush_pending();
    if(!traits_type::eq_int_type(next,traits_type::eof())) {
      if(pptr()==epptr())throw std::length_error("Scene exceeds file size limit");
      *pptr()=traits_type::to_char_type(next);
      pbump(1);
    }
    return traits_type::not_eof(next);
  }
  std::streamsize xsputn(const char *data,std::streamsize length) override {
    if(length<=0)return 0;
    const auto count=size_t(length);
    if(count>limit_-result_.bytes-buffered())
      throw std::length_error("Scene exceeds file size limit");
    if(count>size_t(epptr()-pptr()))flush_pending();
    if(count>=storage_.size()) {
      emit(data,count);
      reset();
    } else {
      std::copy_n(data,count,pptr());
      pbump(int(count));
    }
    return length;
  }
  int sync() override {
    flush_pending();
    return 0;
  }

public:
  SceneOutputBuffer(const Write &write,size_t limit,bool checksum)
      :write_(write),limit_(limit),checksum_(checksum) { reset(); }
  detail::NativeSceneWriteResult result() const { return result_; }
};
} // namespace

detail::NativeSceneWriteResult detail::write_native_scene(
    const Scene &scene,const std::filesystem::path &path,size_t limit,bool checksum) {
  const auto encoded=scene.encode();
  NativeSceneWriteResult result;
  atomic_write_generated(path,{},[&](const auto &write) {
    SceneOutputBuffer buffer(write,limit,checksum);
    std::ostream output(&buffer);
    output.exceptions(std::ios::badbit|std::ios::failbit);
    nlohmann::json::to_cbor(encoded,output);
    output.flush();
    result=buffer.result();
  });
  return result;
}

void Scene::save(const std::filesystem::path &path) const {
  (void)detail::write_native_scene(*this,path,detail::maximum_native_scene_bytes,false);
}

LoadedSceneFile load_scene_file(const std::filesystem::path &path,
                               const std::function<bool()> &cancelled) {
  detail::NativeReadScope native_read(cancelled);
  constexpr size_t limit=detail::maximum_native_scene_bytes;
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
