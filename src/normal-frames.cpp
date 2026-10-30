// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-frames.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <cstring>
#include <set>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
constexpr size_t max_bytes=128*1024*1024,max_corners=6'000'000,max_vertices=2'000'000,max_targets=128;
template<class T> nlohmann::json binary(const std::vector<T> &v) {
  static_assert(std::is_trivially_copyable_v<T> && sizeof(T)%4==0);
  static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
  std::vector<uint8_t> bytes(v.size()*sizeof(T));
  if(!bytes.empty())std::memcpy(bytes.data(),v.data(),bytes.size());
  if constexpr(std::endian::native==std::endian::big)for(size_t at=0;at<bytes.size();at+=4)std::reverse(bytes.begin()+at,bytes.begin()+at+4);
  return nlohmann::json::binary(std::move(bytes));
}
template<class T> std::vector<T> unbinary(const nlohmann::json &j,size_t cap) {
  if(!j.is_binary() || j.get_binary().size()%sizeof(T) || j.get_binary().size()/sizeof(T)>cap)
    throw std::runtime_error("Invalid native frame binary data");
  std::vector<T> result(j.get_binary().size()/sizeof(T));if(!result.empty())std::memcpy(result.data(),j.get_binary().data(),j.get_binary().size());
  if constexpr(std::endian::native==std::endian::big){auto *bytes=reinterpret_cast<uint8_t*>(result.data());for(size_t at=0;at<result.size()*sizeof(T);at+=4)std::reverse(bytes+at,bytes+at+4);}
  return result;
}
uint64_t identity(const nlohmann::json &j,bool zero=false) {
  if(!j.is_number_unsigned() && (!j.is_number_integer() || j.get<int64_t>()<0))throw std::runtime_error("Invalid frame identity");
  auto value=j.get<uint64_t>();if(!zero && !value)throw std::runtime_error("Missing frame identity");return value;
}
uint32_t u32(const nlohmann::json &j,uint32_t cap) {auto value=identity(j,true);if(value>cap)throw std::runtime_error("Native frame integer is out of range");return uint32_t(value);}
uint32_t index(const nlohmann::json &j,size_t size) {auto value=identity(j,true);if(value>=size)throw std::runtime_error("Frame archive identity is out of range");return uint32_t(value);}
void vectors(const std::vector<Vec3> &values,size_t count,bool optional=false) {
  if(values.empty() && optional)return;
  if(values.size()!=count)throw std::invalid_argument("Normal frame direction count differs from corners");
  for(auto v:values)if(!finite(v) || std::max({std::abs(v.x),std::abs(v.y),std::abs(v.z)})>1e6f)throw std::invalid_argument("Normal frame direction is nonfinite or outside native limits");
}
void checked_add(size_t &sum,size_t bytes) {if(bytes>max_bytes-sum)throw std::length_error("Native normal frame payload exceeds 128 MiB");sum+=bytes;}
}
size_t NormalFrames::storage_bytes() const {
  size_t sum=0;for(const auto *v:{&origin_vertices,&source_vertices,&source_corners})checked_add(sum,v->size()*sizeof(uint32_t));
  checked_add(sum,normals.size()*sizeof(Vec3));checked_add(sum,tangents.size()*sizeof(tangents[0]));
  for(const auto &t:targets){checked_add(sum,t.normals.size()*sizeof(Vec3));checked_add(sum,t.tangents.size()*sizeof(Vec3));}return sum;
}
void NormalFrames::validate(bool deep) const {
  static_assert(sizeof(Vec3)==12 && sizeof(std::array<float,4>)==16);
  const size_t corners=normals.size();
  if(size_t(transport)>1 || !corners || corners%3 || corners>max_corners || !origin_vertex_count || origin_vertex_count>max_vertices ||
     source_vertex_count>max_vertices || source_corner_count>max_corners || origin_vertices.size()!=corners || targets.size()>max_targets ||
     (!tangents.empty() && tangents.size()!=corners) || (generated_uv && (!generated_tangents || *generated_uv>1)) ||
     (generated_tangents && (tangents.empty() || !generated_uv)))throw std::invalid_argument("Invalid normal frame shape or provenance");
  storage_bytes();
  for(const auto &target:targets)if((!target.normals.empty() && target.normals.size()!=corners)||(!target.tangents.empty() && target.tangents.size()!=corners))throw std::invalid_argument("Normal frame target size differs");
  if(source_vertices.empty() && source_corners.empty()) {
    if(source_vertex_count || source_corner_count)throw std::invalid_argument("Normal frame has missing source identities");
  } else if(source_vertices.size()!=corners || source_corners.size()!=corners || !source_vertex_count || !source_corner_count || source_corner_count%3)
    throw std::invalid_argument("Normal frame source corner table is incomplete");
  for(const auto &target:targets)if(tangents.empty() && !target.tangents.empty())throw std::invalid_argument("N-only frame cannot invent target tangents");
  if(!deep)return;
  vectors(normals,corners);
  for(auto n:normals){float l=length(n);if(!std::isfinite(l)||std::abs(l-1)>.001f)throw std::invalid_argument("Normal frame base normals must be unit vectors");}
  for(auto id:origin_vertices)if(id>=origin_vertex_count)throw std::invalid_argument("Normal frame origin vertex is out of range");
  if(source_vertices.empty() && source_corners.empty()) {
    if(source_vertex_count || source_corner_count)throw std::invalid_argument("Normal frame has missing source identities");
  } else {
    if(source_vertices.size()!=corners || source_corners.size()!=corners || !source_vertex_count || !source_corner_count)throw std::invalid_argument("Normal frame source corner table is incomplete");
    for(size_t c=0;c<corners;++c)if(source_vertices[c]>=source_vertex_count || source_corners[c]>=source_corner_count)throw std::invalid_argument("Normal frame source identity is out of range");
  }
  for(size_t c=0;c<tangents.size();++c){auto t=tangents[c];float l=length(Vec3{t[0],t[1],t[2]});if(!finite(Vec3{t[0],t[1],t[2]})||!std::isfinite(l)||std::abs(l-1)>.001f||(t[3]!=-1&&t[3]!=1)||t[3]!=tangents[c/3*3][3])throw std::invalid_argument("Invalid normal frame tangent or triangle handedness");}
  std::set<uint64_t> ids;
  for(const auto &target:targets){if(!target.id || !ids.insert(target.id).second)throw std::invalid_argument("Duplicate or missing normal frame target identity");vectors(target.normals,corners,true);vectors(target.tangents,corners,true);if(tangents.empty() && !target.tangents.empty())throw std::invalid_argument("N-only frame cannot invent target tangents");}
}
nlohmann::json NormalFrames::encode() const {
  validate();nlohmann::json j={{"version",1},{"transport",int(transport)},{"generated",generated_tangents},{"generated_uv",generated_uv?nlohmann::json(*generated_uv):nlohmann::json(nullptr)},
    {"origin_count",origin_vertex_count},{"source_count",source_vertex_count},{"source_corner_count",source_corner_count},
    {"origin_vertices",binary(origin_vertices)},{"source_vertices",binary(source_vertices)},{"source_corners",binary(source_corners)},
    {"normals",binary(normals)},{"tangents",binary(tangents)},{"targets",nlohmann::json::array()}};
  for(const auto &t:targets)j["targets"].push_back({{"id",t.id},{"normals",binary(t.normals)},{"tangents",binary(t.tangents)}});
  return j;
}
NormalFrames NormalFrames::decode(const nlohmann::json &j) {
  if(!j.is_object() || j.size()!=13 || !j.at("version").is_number_integer() || j.at("version")!=1 || !j.at("targets").is_array() || j.at("targets").size()>max_targets)throw std::runtime_error("Unsupported normal frame payload");
  size_t bytes=0;for(auto key:{"origin_vertices","source_vertices","source_corners","normals","tangents"}){const auto &v=j.at(key);if(!v.is_binary())throw std::runtime_error("Invalid frame buffer");checked_add(bytes,v.get_binary().size());}
  for(const auto &t:j.at("targets"))if(!t.is_object() || t.size()!=3)throw std::runtime_error("Invalid normal target fields");
  for(const auto &t:j.at("targets"))for(auto key:{"normals","tangents"}){const auto &v=t.at(key);if(!v.is_binary())throw std::runtime_error("Invalid frame target buffer");checked_add(bytes,v.get_binary().size());}
  NormalFrames f;f.transport=NormalFrameTransport(u32(j.at("transport"),1));f.generated_tangents=j.at("generated").get<bool>();if(!j.at("generated_uv").is_null())f.generated_uv=u32(j.at("generated_uv"),1);
  f.origin_vertex_count=u32(j.at("origin_count"),max_vertices);f.source_vertex_count=u32(j.at("source_count"),max_vertices);f.source_corner_count=u32(j.at("source_corner_count"),max_corners);
  f.origin_vertices=unbinary<uint32_t>(j.at("origin_vertices"),max_corners);f.source_vertices=unbinary<uint32_t>(j.at("source_vertices"),max_corners);f.source_corners=unbinary<uint32_t>(j.at("source_corners"),max_corners);
  f.normals=unbinary<Vec3>(j.at("normals"),max_corners);f.tangents=unbinary<std::array<float,4>>(j.at("tangents"),max_corners);
  for(const auto &t:j.at("targets"))f.targets.push_back({identity(t.at("id")),unbinary<Vec3>(t.at("normals"),max_corners),unbinary<Vec3>(t.at("tangents"),max_corners)});
  f.validate();return f;
}
void NormalFrameSet::validate(bool deep) const {
  if(skin_policy!=NormalFrameSkinPolicy::WeightedJointInverseTranspose)throw std::invalid_argument("Unknown native normal frame skin policy");
  if(canonical_evaluation!=NativeCanonicalNormalEvaluation::RawInterpolatedVertexTbn)throw std::invalid_argument("Unknown native canonical normal evaluation");
  if(!pbr && !mtoon && !canonical)throw std::invalid_argument("Empty native normal frame set");
  if(!next_target || targets.size()>max_targets || current_targets.size()>max_targets)throw std::invalid_argument("Invalid frame target registry bounds");
  std::set<uint64_t> ids,active;std::optional<uint32_t> count;std::optional<size_t> corners;const NormalFrames *first=nullptr;
  for(const auto &frame:{pbr,mtoon,canonical})if(frame){frame->validate(deep);if(deep && first && frame->origin_vertices!=first->origin_vertices)throw std::invalid_argument("Normal frame origin corner identities differ");if(!first)first=frame.get();if((count && *count!=frame->origin_vertex_count)||(corners && *corners!=frame->normals.size()))throw std::invalid_argument("Normal frame domains differ");count=frame->origin_vertex_count;corners=frame->normals.size();if(frame->targets.size()!=targets.size())throw std::invalid_argument("Normal frame target registry is incomplete");for(size_t i=0;i<targets.size();++i)if(frame->targets[i].id!=targets[i].id)throw std::invalid_argument("Normal frame target order differs from registry");}
  for(const auto &target:targets){if(!target.id || target.id>=next_target || !ids.insert(target.id).second || (!target.origin_slot && target.positions.empty()))throw std::invalid_argument("Invalid frame target authority");if(target.origin_slot && *target.origin_slot>=max_targets)throw std::invalid_argument("Frame origin target slot is out of range");if(!target.positions.empty() && target.positions.size()!=*count)throw std::invalid_argument("Frame POSITION authority size differs");if(deep)vectors(target.positions,*count,true);}
  for(auto id:current_targets)if(!ids.contains(id)||!active.insert(id).second)throw std::invalid_argument("Current native morph slots have ambiguous or unknown frame IDs");
  storage_bytes();
}
size_t NormalFrameSet::storage_bytes() const {
  size_t bytes=0;std::set<const NormalFrames*> seen;for(const auto &f:{pbr,mtoon,canonical})if(f && seen.insert(f.get()).second)checked_add(bytes,f->storage_bytes());
  for(const auto &t:targets)checked_add(bytes,t.positions.size()*sizeof(Vec3));
  return bytes;
}
uint32_t NormalFrameArchive::add(std::shared_ptr<const NormalFrameSet> set,bool deep) {
  if(!set)throw std::invalid_argument("Missing normal frame set");
  set->validate(deep);auto it=std::find(sets_.begin(),sets_.end(),set);if(it!=sets_.end())return uint32_t(it-sets_.begin());if(sets_.size()>=256)throw std::length_error("Too many native normal frame sets");sets_.push_back(std::move(set));return uint32_t(sets_.size()-1);
}
const std::shared_ptr<const NormalFrameSet> &NormalFrameArchive::at(uint32_t id) const {return sets_.at(id);}
size_t NormalFrameArchive::storage_bytes() const {
  size_t bytes=0;std::set<const NormalFrames*> frames;
  for(const auto &s:sets_){for(const auto &f:{s->pbr,s->mtoon,s->canonical})if(f && frames.insert(f.get()).second)checked_add(bytes,f->storage_bytes());for(const auto &t:s->targets)checked_add(bytes,t.positions.size()*sizeof(Vec3));}return bytes;
}
nlohmann::json NormalFrameArchive::encode() const {
  nlohmann::json j={{"version",1},{"frames",nlohmann::json::array()},{"sets",nlohmann::json::array()}};std::vector<std::shared_ptr<const NormalFrames>> frames;
  auto add=[&](auto frame)->nlohmann::json{if(!frame)return nullptr;auto it=std::find(frames.begin(),frames.end(),frame);if(it==frames.end()){frames.push_back(frame);return frames.size()-1;}return size_t(it-frames.begin());};
  size_t bytes=0;
  for(const auto &set:sets_){set->validate();nlohmann::json value={{"canonical_evaluation",int(set->canonical_evaluation)},{"skin_policy",int(set->skin_policy)},{"pbr",add(set->pbr)},{"mtoon",add(set->mtoon)},{"canonical",add(set->canonical)},{"next_target",set->next_target},{"current_targets",set->current_targets},{"targets",nlohmann::json::array()}};for(const auto &t:set->targets){checked_add(bytes,t.positions.size()*sizeof(Vec3));value["targets"].push_back({{"id",t.id},{"origin_slot",t.origin_slot?nlohmann::json(*t.origin_slot):nlohmann::json(nullptr)},{"source_target",t.source_target?nlohmann::json(*t.source_target):nlohmann::json(nullptr)},{"positions",binary(t.positions)}});}j["sets"].push_back(std::move(value));}
  for(const auto &f:frames){checked_add(bytes,f->storage_bytes());j["frames"].push_back(f->encode());}return j;
}
NormalFrameArchive NormalFrameArchive::decode(const nlohmann::json &j) {
  if(!j.is_object() || j.size()!=3 || !j.at("version").is_number_integer() || j.at("version")!=1 || !j.at("frames").is_array() || j.at("frames").size()>768 || !j.at("sets").is_array() || j.at("sets").size()>256)throw std::runtime_error("Invalid native frame archive");
  size_t bytes=0;for(const auto &f:j.at("frames")){for(auto key:{"origin_vertices","source_vertices","source_corners","normals","tangents"}){const auto &b=f.at(key);if(!b.is_binary())throw std::runtime_error("Invalid frame archive buffer");checked_add(bytes,b.get_binary().size());}if(!f.at("targets").is_array() || f.at("targets").size()>max_targets)throw std::runtime_error("Invalid frame target count");for(const auto &t:f.at("targets"))for(auto key:{"normals","tangents"}){const auto &b=t.at(key);if(!b.is_binary())throw std::runtime_error("Invalid frame target buffer");checked_add(bytes,b.get_binary().size());}}
  for(const auto &s:j.at("sets")){if(!s.is_object() || s.size()!=8 || !s.at("canonical_evaluation").is_number_integer() || s.at("canonical_evaluation")!=1 || !s.at("skin_policy").is_number_integer() || s.at("skin_policy")!=1 || !s.at("current_targets").is_array() || s.at("current_targets").size()>max_targets)throw std::runtime_error("Invalid frame set fields or transport policy");identity(s.at("next_target"));for(auto key:{"pbr","mtoon","canonical"})if(!s.at(key).is_null())index(s.at(key),j.at("frames").size());if(!s.at("targets").is_array()||s.at("targets").size()>max_targets)throw std::runtime_error("Invalid frame registry count");for(const auto &t:s.at("targets")){if(!t.is_object() || t.size()!=4)throw std::runtime_error("Invalid frame target authority fields");const auto &b=t.at("positions");if(!b.is_binary())throw std::runtime_error("Invalid frame position authority");checked_add(bytes,b.get_binary().size());}}
  std::vector<std::shared_ptr<const NormalFrames>> frames;for(const auto &f:j.at("frames"))frames.push_back(std::make_shared<const NormalFrames>(NormalFrames::decode(f)));
  NormalFrameArchive archive;for(const auto &value:j.at("sets")){auto s=std::make_shared<NormalFrameSet>();if(!value.at("skin_policy").is_number_integer() || value.at("skin_policy")!=1)throw std::runtime_error("Unsupported native normal skin policy");s->skin_policy=NormalFrameSkinPolicy::WeightedJointInverseTranspose;if(!value.at("canonical_evaluation").is_number_integer() || value.at("canonical_evaluation")!=1)throw std::runtime_error("Unsupported native canonical normal evaluation");s->canonical_evaluation=NativeCanonicalNormalEvaluation::RawInterpolatedVertexTbn;auto get=[&](const char *key){return value.at(key).is_null()?std::shared_ptr<const NormalFrames>{}:frames.at(index(value.at(key),frames.size()));};s->pbr=get("pbr");s->mtoon=get("mtoon");s->canonical=get("canonical");s->next_target=identity(value.at("next_target"));if(!value.at("current_targets").is_array()||value.at("current_targets").size()>max_targets)throw std::runtime_error("Invalid current frame target map");for(const auto &v:value.at("current_targets"))s->current_targets.push_back(identity(v));for(const auto &t:value.at("targets")){FrameTargetIdentity record;record.id=identity(t.at("id"));if(!t.at("origin_slot").is_null())record.origin_slot=u32(t.at("origin_slot"),max_targets-1);if(!t.at("source_target").is_null())record.source_target=u32(t.at("source_target"),4095);record.positions=unbinary<Vec3>(t.at("positions"),max_vertices);s->targets.push_back(std::move(record));}s->validate();archive.sets_.push_back(std::move(s));}return archive;
}
} // namespace dome
