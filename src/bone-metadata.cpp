// SPDX-License-Identifier: GPL-3.0-or-later
#include "bone-metadata.hpp"
#include <cmath>
#include <stdexcept>
#include <type_traits>
namespace dome::detail {
namespace {
using Json = nlohmann::json;
void require(bool value, const char *why) {
  if (!value) throw std::invalid_argument(std::string("Bone authoring metadata: ") + why);
}
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Bone authoring metadata cancelled");
}
void valid_length(float length) {
  require(std::isfinite(length) && length > 1e-6f && length <= 1000,
          "localY length must satisfy native Skeleton bounds (1e-6,1000]");
}
} // namespace
std::optional<float> bone_length_record(const Json &extras) {
  if (!extras.is_object() || !extras.contains("domeSculptBone")) return {};
  const auto &record = extras.at("domeSculptBone");
  require(record.is_object() && record.size() == 3 && record.contains("version") &&
              record.contains("length") && record.contains("space"),
          "domeSculptBone requires exactly version, length and space");
  const auto &version = record.at("version");
  require(version.is_number_integer() &&
              (version.is_number_unsigned() ? version.get<uint64_t>() == 1
                                            : version.get<int64_t>() == 1),
          "unsupported domeSculptBone version (expected integer 1)");
  const auto &space = record.at("space");
  require(space.is_string() && space.get_ref<const std::string &>() == "localY",
          "unsupported domeSculptBone space (expected localY)");
  const auto &number = record.at("length");
  require(number.is_number() && std::isfinite(number.get<double>()) &&
              number.get<double>() > 1e-6 && number.get<double>() <= 1000,
          "domeSculptBone length is not a bounded finite native scalar");
  const float value = number.get<float>();
  valid_length(value);
  return value;
}
SourceBoneMetadata source_bone_metadata(const ImportedAsset &asset,
                                       const std::function<bool()> &cancelled) {
  stop(cancelled);
  require(asset.nodes.size() <= 100000, "source node count exceeds inherited metadata bound");
  SourceBoneMetadata result;
  result.extras.resize(asset.nodes.size());
  result.lengths.resize(asset.nodes.size());
  size_t bytes = 0, visits = 0;
  constexpr size_t max_bytes = 16 * 1024 * 1024;
  for (size_t n = 0; n < asset.nodes.size(); ++n) {
    stop(cancelled);
    const auto &text = asset.nodes[n].extras_json;
    if (text.empty()) continue;
    require(text.size() <= max_bytes - bytes, "source extras exceed inherited 16MiB JSON bound");
    bytes += text.size();
    auto value = Json::parse(text, [&](int depth, Json::parse_event_t, const Json &) {
      if (depth > 64 || ++visits > 50000000)
        throw std::length_error("Bone authoring metadata JSON work/depth bound exceeded");
      if ((visits & 1023) == 0) stop(cancelled);
      return true;
    });
    result.lengths[n] = bone_length_record(value);
    result.extras[n] = std::move(value);
  }
  stop(cancelled);
  return result;
}
bool write_bone_length(Json &node, float length) {
  require(node.is_object(), "target glTF node must be an object");
  valid_length(length);
  const auto existing = node.contains("extras") ? bone_length_record(node.at("extras"))
                                                 : std::nullopt;
  if ((existing && *existing == length) || (!existing && length == Bone{}.length))
    return false;
  require(!node.contains("extras") || node.at("extras").is_object(),
          "existing non-object node extras cannot hold domeSculptBone; original retained");
  auto extras = node.contains("extras") ? node.at("extras") : Json::object();
  extras["domeSculptBone"] = {{"version", 1}, {"length", length}, {"space", "localY"}};
  node["extras"] = std::move(extras);
  return true;
}
size_t apply_bone_lengths(Skeleton &skeleton, std::span<const uint32_t> sources,
                         const SourceBoneMetadata &metadata,
                         const std::function<bool()> &cancelled) {
  stop(cancelled);
  require(sources.size() == skeleton.bones.size() && sources.size() <= 256 &&
              metadata.extras.size() == metadata.lengths.size(),
          "actual joint/slot table mismatch");
  size_t count = 0;
  for (auto source : sources) {
    stop(cancelled);
    require(source < metadata.lengths.size(), "actual source joint is outside metadata table");
    if (metadata.lengths[source]) {
      valid_length(*metadata.lengths[source]);
      ++count;
    }
  }
  if (!count) return 0;
  auto next = skeleton;
  for (size_t slot = 0; slot < sources.size(); ++slot) {
    stop(cancelled);
    if (auto length = metadata.lengths[sources[slot]]) next.bones[slot].length = *length;
  }
  next.validate();
  stop(cancelled);
  static_assert(std::is_nothrow_move_assignable_v<Skeleton>);
  skeleton = std::move(next);
  return count;
}
} // namespace dome::detail
