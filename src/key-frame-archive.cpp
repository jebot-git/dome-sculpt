// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-frame-archive.hpp"
#include "dome/material-bindings.hpp"
#include <bit>
#include <cstring>
namespace dome::detail {
namespace {
constexpr size_t cap=128*1024*1024;
template<class T>bool own(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
void stop(const std::function<bool()> &c){if(c&&c())throw std::runtime_error("Native key frame archive cancelled");}
size_t index(const nlohmann::json &v,size_t n){if(!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))throw std::invalid_argument("Invalid native key archive reference");const auto i=v.get<uint64_t>();if(i>=n)throw std::out_of_range("Native key archive reference is outside its owning pool");return size_t(i);}
void charge(size_t &total,size_t n){if(n>cap-total)throw std::length_error("Native key frame archive exceeds 128 MiB before allocation");total+=n;}
template<class T>nlohmann::json binary(const std::vector<T> &v){std::vector<uint8_t>b(v.size()*sizeof(T));if(!b.empty())std::memcpy(b.data(),v.data(),b.size());if constexpr(std::endian::native==std::endian::big){if constexpr(sizeof(T)>1)for(size_t at=0;at<b.size();at+=4)std::reverse(b.begin()+at,b.begin()+at+4);}return nlohmann::json::binary(std::move(b));}
template<class T>void shape(const nlohmann::json &v,size_t max,size_t &bytes){if(!v.is_binary()||v.get_binary().size()%sizeof(T)||v.get_binary().size()/sizeof(T)>max)throw std::invalid_argument("Invalid bounded native key domain buffer");charge(bytes,v.get_binary().size());}
template<class T>bool equal(const nlohmann::json &j,const std::vector<T> &v){const auto &b=j.get_binary();if(b.size()!=v.size()*sizeof(T))return false;if constexpr(std::endian::native==std::endian::little||sizeof(T)==1)return b.empty()||std::memcmp(b.data(),v.data(),b.size())==0;else{const auto *p=reinterpret_cast<const uint8_t*>(v.data());for(size_t at=0;at<b.size();++at)if(b[at]!=p[(at&~size_t(3))+(3-(at&3))])return false;return true;}}
nlohmann::json mesh_record(const Mesh &m){return{{"positions",binary(m.positions)},{"normals",binary(m.normals)},{"uv",binary(m.uv)},{"indices",binary(m.indices)},{"pins",binary(m.uv_pins)},{"seams",binary(m.uv_seams)}};}
void mesh_shape(const nlohmann::json &j,size_t &bytes){if(!j.is_object()||j.size()!=6)throw std::invalid_argument("Invalid native key basis fields");shape<Vec3>(j.at("positions"),2'000'000,bytes);shape<Vec3>(j.at("normals"),2'000'000,bytes);shape<std::array<float,2>>(j.at("uv"),2'000'000,bytes);shape<uint32_t>(j.at("indices"),6'000'000,bytes);shape<uint8_t>(j.at("pins"),2'000'000,bytes);shape<uint8_t>(j.at("seams"),6'000'000,bytes);}
bool mesh_equal(const nlohmann::json &j,const Mesh &m){return equal(j.at("positions"),m.positions)&&equal(j.at("normals"),m.normals)&&equal(j.at("uv"),m.uv)&&equal(j.at("indices"),m.indices)&&equal(j.at("pins"),m.uv_pins)&&equal(j.at("seams"),m.uv_seams);}
}
nlohmann::json KeyFrameArchive::encode(const NormalFrameArchive &frames){
  if(std::none_of(frames.sets_.begin(),frames.sets_.end(),[](const auto&s){return bool(s->position_domain);}))return frames.encode();
  frames.storage_bytes();std::vector<std::shared_ptr<const FramePositionDomain>> domains;
  std::function<uint32_t(std::shared_ptr<const FramePositionDomain>)> add=[&](auto d)->uint32_t{
    const auto found=std::find(domains.begin(),domains.end(),d);if(found!=domains.end())return uint32_t(found-domains.begin());
    d->validate();if(d->previous())add(d->previous());if(domains.size()>=256)throw std::length_error("Native key position domain count exceeds archive budget");domains.push_back(std::move(d));return uint32_t(domains.size()-1);
  };
  auto output=frames.encode_impl(add);output["version"]=2;output["position_domains"]=nlohmann::json::array();
  for(const auto&d:domains){const auto previous=std::find(domains.begin(),domains.end(),d->previous());output["position_domains"].push_back({{"version",1},{"basis",mesh_record(*d->basis())},{"current_vertices",binary(d->current_vertices())},{"origin_vertices",binary(d->origin_vertices())},{"source_frame_vertices",binary(d->source_frame_vertices())},{"previous",d->previous()?nlohmann::json(size_t(previous-domains.begin())):nlohmann::json(nullptr)}});}
  return output;
}
KeyFrameArchive::KeyFrameArchive(const nlohmann::json &input):input_(input){
  const bool derived=input.is_object()&&input.contains("version")&&input.at("version").is_number_integer()&&input.at("version")==2;
  if(!derived){frames_=NormalFrameArchive::decode(input);references_.resize(frames_.sets_.size());return;}
  // Parse every dimension and sum all serialized buffers before allocating a
  // single frame/domain. Source Paint/mesh allocations belong to their own
  // owning material quota, never an unbounded secondary decode here.
  if(input.size()!=4||!input.at("position_domains").is_array()||input.at("position_domains").empty()||input.at("position_domains").size()>256||!input.at("frames").is_array()||input.at("frames").size()>768||!input.at("sets").is_array()||input.at("sets").size()>256)throw std::invalid_argument("Invalid owning frame2 archive shape");
  size_t bytes=0;std::vector<bool> reachable(input.at("position_domains").size());
  for(size_t i=0;i<reachable.size();++i){const auto &d=input.at("position_domains")[i];if(!d.is_object()||d.size()!=6||!d.at("version").is_number_integer()||d.at("version")!=1)throw std::invalid_argument("Unsupported native key domain record");mesh_shape(d.at("basis"),bytes);for(const char *k:{"current_vertices","origin_vertices","source_frame_vertices"})shape<uint32_t>(d.at(k),2'000'000,bytes);if(!d.at("previous").is_null())index(d.at("previous"),i);}
  for(const auto &f:input.at("frames")){for(const char*k:{"origin_vertices","source_vertices","source_corners","normals","tangents"}){const auto &b=f.at(k);if(!b.is_binary())throw std::invalid_argument("Invalid frame2 role buffer");charge(bytes,b.get_binary().size());}if(!f.at("targets").is_array()||f.at("targets").size()>128)throw std::invalid_argument("Invalid frame2 role targets");for(const auto&t:f.at("targets"))for(const char*k:{"normals","tangents"}){const auto &b=t.at(k);if(!b.is_binary())throw std::invalid_argument("Invalid frame2 target buffer");charge(bytes,b.get_binary().size());}}
  for(const auto&s:input.at("sets")){if(!s.is_object()||s.size()!=9||!s.at("targets").is_array()||s.at("targets").size()>128)throw std::invalid_argument("Invalid frame2 set shape");for(const auto&t:s.at("targets")){const auto &b=t.at("positions");if(!b.is_binary())throw std::invalid_argument("Invalid frame2 POSITION buffer");charge(bytes,b.get_binary().size());}if(s.at("position_domain").is_null())references_.push_back({});else{auto id=index(s.at("position_domain"),reachable.size());references_.push_back(uint32_t(id));while(!reachable[id]){reachable[id]=true;const auto&p=input.at("position_domains")[id].at("previous");if(p.is_null())break;id=index(p,id);}}}
  if(std::find(reachable.begin(),reachable.end(),false)!=reachable.end())throw std::invalid_argument("Orphan native key position-domain record");
  frames_=NormalFrameArchive::decode_impl(input,true);domains_.resize(reachable.size());used_.resize(frames_.sets_.size());
}
const std::shared_ptr<const NormalFrameSet>&KeyFrameArchive::at(uint32_t id)const{if(references_.at(id)&&!used_.at(id))throw std::invalid_argument("Unbound frame2 requires its actual owning key context");return frames_.at(id);}
std::shared_ptr<const NormalFrameSet> KeyFrameArchive::bind(uint32_t id,std::shared_ptr<const Mesh> current,const SourceMaterialBindings &before,const std::function<bool()> &cancelled){
  stop(cancelled);if(!references_.at(id))throw std::invalid_argument("An owning native key node requires an actual frame2 position domain");const auto domain_id=*references_[id];const auto &record=input_.at("position_domains").at(domain_id);
  auto expected=derive_frame_position_domain(std::move(current),before,cancelled);const auto &prior=record.at("previous");
  if(prior.is_null()){if(expected->previous())throw std::invalid_argument("Native key domain omitted its real prior authority");}
  else{const auto p=index(prior,domain_id);if(!domains_[p]||!own(expected->previous(),domains_[p]))throw std::invalid_argument("Native key domain has an unbound or borrowed prior authority");}
  if(!mesh_equal(record.at("basis"),*expected->basis())||!equal(record.at("current_vertices"),expected->current_vertices())||!equal(record.at("origin_vertices"),expected->origin_vertices())||!equal(record.at("source_frame_vertices"),expected->source_frame_vertices()))throw std::invalid_argument("Native key domain record differs from actual ordered-corner replay");
  if(domains_[domain_id]){if(!own(domains_[domain_id]->current_geometry(),expected->current_geometry())||!own(domains_[domain_id]->before_basis(),expected->before_basis())||!own(domains_[domain_id]->source_frame_basis(),expected->source_frame_basis()))throw std::invalid_argument("One native key domain cannot acquire conflicting geometry/source owners");expected=domains_[domain_id];}
  if(used_[id])return frames_.sets_.at(id);
  auto set=std::make_shared<NormalFrameSet>(*frames_.sets_.at(id));set->position_domain=expected;set->validate();stop(cancelled);
  // Prove complete aggregate storage before replacing the local staged entry.
  auto archive=frames_;archive.sets_[id]=set;archive.storage_bytes();domains_[domain_id]=std::move(expected);frames_=std::move(archive);used_[id]=true;return set;
}
void KeyFrameArchive::require_complete()const{for(size_t i=0;i<references_.size();++i)if(references_[i]&&!used_[i])throw std::invalid_argument("Unowned native key frame/domain record");for(const auto&d:domains_)if(!d)throw std::invalid_argument("Unbound native key domain history");}
} // namespace dome::detail
