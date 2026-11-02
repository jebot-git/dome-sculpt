// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-sampling.hpp"
#include <bit>
#include <nlohmann/json.hpp>

namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Normal sampling archive cancelled");
}
uint32_t integer(const nlohmann::json &j, uint32_t maximum) {
  if ((!j.is_number_unsigned() && (!j.is_number_integer() || j.get<int64_t>() < 0)) ||
      j.get<uint64_t>() > maximum)
    throw std::invalid_argument("Normal sampling integer exceeds its declared range");
  return j.get<uint32_t>();
}
void images(const std::shared_ptr<const Image> &a,
            const std::shared_ptr<const Image> &b, uint32_t resolution) {
  for (const auto &image : {a,b}) {
    if (!image) throw std::invalid_argument("Missing retained normal field image");
    image->validate();
    if (image->width != resolution || image->height != resolution)
      throw std::invalid_argument("Retained normal field dimensions differ");
  }
}
uint32_t word(const nlohmann::json::binary_t &b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at+1]) << 8 |
         uint32_t(b[at+2]) << 16 | uint32_t(b[at+3]) << 24;
}
nlohmann::json words(std::span<const uint32_t> values,
                     const std::function<bool()> &cancelled) {
  std::vector<uint8_t> bytes;
  bytes.reserve(values.size()*4);
  for (size_t i=0;i<values.size();++i) {
    if ((i&1023)==0) stop(cancelled);
    const auto value=values[i];
    for (unsigned k=0;k<4;++k) bytes.push_back(uint8_t(value>>(8*k)));
  }
  return nlohmann::json::binary(std::move(bytes));
}
size_t detail_bytes(const nlohmann::json &detail,const PaintImageArchive &image_pool) {
  if (!detail.is_object() || detail.size()!=5 || integer(detail.at("version"),1)!=1 ||
      !detail.at("interpretation").is_string() ||
      detail.at("interpretation").get_ref<const std::string &>()!="prepared-attributes" ||
      !detail.at("coverage").is_binary())
    throw std::invalid_argument("Invalid retained normal detail authority");
  const auto &pbr=image_pool.at(integer(detail.at("pbr"),UINT32_MAX));
  const auto &toon=image_pool.at(integer(detail.at("mtoon"),UINT32_MAX));
  const auto r=pbr->width;
  if(r<64||r>2048||(r&(r-1))||detail.at("coverage").get_binary().size()!=size_t(r)*r)
    throw std::invalid_argument("Invalid retained normal detail dimensions");
  images(pbr,toon,r);return detail.at("coverage").get_binary().size();
}
std::shared_ptr<const NormalDetailAuthority> decode_detail(const nlohmann::json &data,
    const PaintImageArchive &image_pool) {
  detail_bytes(data,image_pool);auto detail=std::make_shared<NormalDetailAuthority>();
  detail->pbr=image_pool.at(integer(data.at("pbr"),UINT32_MAX));
  detail->mtoon=image_pool.at(integer(data.at("mtoon"),UINT32_MAX));
  detail->coverage=data.at("coverage").get_binary();return detail;
}
nlohmann::json encode_detail(const NormalDetailAuthority &detail,PaintImageArchive &images) {
  detail.validate(detail.pbr?detail.pbr->width:0);
  return {{"version",1},{"interpretation","prepared-attributes"},
      {"pbr",images.add(detail.pbr)},{"mtoon",images.add(detail.mtoon)},
      {"coverage",nlohmann::json::binary(detail.coverage)}};
}
size_t encoded_bytes(const nlohmann::json &j, const PaintImageArchive &image_pool,
                     const std::function<bool()> &cancelled,const nlohmann::json *details=nullptr) {
  if (!j.is_object() || j.size()!=7 || integer(j.at("version"),1)!=1)
    throw std::invalid_argument("Unsupported normal sampling domain");
  const auto &uv=j.at("coordinates"), &toon=j.at("mtoon_coordinates"), &indices=j.at("indices");
  if (!uv.is_binary() || !toon.is_binary() || !indices.is_binary() || uv.get_binary().empty() ||
      uv.get_binary().size()%8 || uv.get_binary().size()/8>2'000'000 ||
      (!toon.get_binary().empty() && toon.get_binary().size()!=uv.get_binary().size()) ||
      indices.get_binary().empty() || indices.get_binary().size()%12 ||
      indices.get_binary().size()/4>6'000'000)
    throw std::invalid_argument("Invalid normal sampling binary geometry dimensions");
  size_t bytes=uv.get_binary().size()+toon.get_binary().size()+indices.get_binary().size();
  const auto r=integer(j.at("resolution"),2048);
  if (r<64 || (r&(r-1))) throw std::invalid_argument("Invalid normal sampling resolution");
  const auto &resolved=j.at("resolved");
  if (!resolved.is_array() || resolved.size()!=2)
    throw std::invalid_argument("Retained resolved normal role table is incomplete");
  images(image_pool.at(integer(resolved[0],UINT32_MAX)),
         image_pool.at(integer(resolved[1],UINT32_MAX)),r);
  const auto &coordinates=uv.get_binary(), &corners=indices.get_binary();
  for (const auto *field : {&coordinates,&toon.get_binary()})
    for (size_t at=0;at<field->size();at+=4) {
      if ((at&4095)==0) stop(cancelled);
      const auto q=std::bit_cast<float>(word(*field,at));
      if (!std::isfinite(q) || std::abs(q)>10000)
        throw std::invalid_argument("Encoded normal sampling coordinate exceeds its finite range");
    }
  for (size_t at=0;at<corners.size();at+=4) {
    if ((at&4095)==0) stop(cancelled);
    if (word(corners,at)>=coordinates.size()/8)
      throw std::invalid_argument("Encoded normal corner references unavailable coordinates");
  }
  const auto &reference=j.at("detail");
  if (!reference.is_null()) {
    const nlohmann::json *detail=&reference;
    if(details){const auto id=integer(reference,UINT32_MAX);
      if(id>=details->size())throw std::out_of_range("Invalid retained normal detail reference");
      detail=&details->at(id);
    }
    const auto count=detail_bytes(*detail,image_pool);if(!details)bytes+=count;
    images(image_pool.at(integer(detail->at("pbr"),UINT32_MAX)),
           image_pool.at(integer(detail->at("mtoon"),UINT32_MAX)),r);
  }
  return bytes;
}
}
void NormalDetailAuthority::validate(uint32_t r) const {
  if (version!=1 || r<64||r>2048||(r&(r-1))||interpretation!=BakedNormalInterpretation::PreparedAttributes ||
      coverage.size()!=size_t(r)*r)
    throw std::invalid_argument("Invalid retained prepared-detail normal authority");
  images(pbr,mtoon,r);
}
size_t NormalSamplingDomain::storage_bytes() const {
  if (coordinates.empty() || coordinates.size()>2'000'000 ||
      (!mtoon_coordinates.empty() && mtoon_coordinates.size()!=coordinates.size()) ||
      indices.empty() || indices.size()>6'000'000 || indices.size()%3)
    throw std::invalid_argument("Invalid retained normal coordinate domain dimensions");
  if (detail && detail->coverage.size()>4'194'304)
    throw std::length_error("Retained normal detail coverage exceeds budget");
  return (coordinates.size()+mtoon_coordinates.size())*8+indices.size()*4+(detail?detail->coverage.size():0);
}
void NormalSamplingDomain::validate(const std::function<bool()> &cancelled) const {
  validate(true,cancelled);
}
void NormalSamplingDomain::validate(bool geometry,const std::function<bool()> &cancelled) const {
  storage_bytes();
  if (version!=1 || resolution<64 || resolution>2048 || (resolution&(resolution-1)))
    throw std::invalid_argument("Invalid retained normal sampling interpretation");
  if(geometry)for (const auto *field : {&coordinates,&mtoon_coordinates})
    for (size_t v=0;v<field->size();++v) {
      if ((v&1023)==0) stop(cancelled);
      for (const float q : (*field)[v]) if (!std::isfinite(q) || std::abs(q)>10000)
        throw std::invalid_argument("Retained normal source coordinate exceeds its finite range");
    }
  if(geometry)for (size_t c=0;c<indices.size();++c) {
    if ((c&1023)==0) stop(cancelled);
    if (indices[c]>=coordinates.size())
      throw std::invalid_argument("Retained normal field corner references unavailable coordinates");
  }
  images(resolved_pbr,resolved_mtoon,resolution);
  if (detail) detail->validate(resolution);
}
NormalSamplingArchive::NormalSamplingArchive(NormalSamplingArchiveLimits limits):limits_(limits) {
  if (!limits.max_domains || limits.max_domains>512 || !limits.max_bytes ||
      limits.max_bytes>256*1024*1024||!limits.max_details||limits.max_details>1024)
    throw std::invalid_argument("Invalid retained normal sampling archive bounds");
}
uint32_t NormalSamplingArchive::add(std::shared_ptr<const NormalSamplingDomain> domain) {
  stop(limits_.cancelled);
  if (!domain) throw std::invalid_argument("Missing retained normal sampling domain");
  for (size_t i=0;i<domains_.size();++i) if (domains_[i]==domain) {
    referenced_[i]=true;return uint32_t(i);
  }
  domain->validate(limits_.cancelled);
  NormalSamplingArchive staged=*this;
  if(domain->detail)staged.insert_detail(domain->detail);
  const auto bytes=domain->storage_bytes()-(domain->detail?domain->detail->coverage.size():0);
  if (staged.domains_.size()>=limits_.max_domains || bytes>limits_.max_bytes-staged.bytes_)
    throw std::length_error("Retained normal sampling archive exceeds aggregate budget");
  // Stage both allocations before publishing either owner or byte accounting.
  staged.domains_.reserve(staged.domains_.size()+1);staged.referenced_.reserve(staged.referenced_.size()+1);
  staged.domains_.push_back(std::move(domain));staged.referenced_.push_back(true);staged.bytes_+=bytes;
  *this=std::move(staged);return uint32_t(domains_.size()-1);
}
uint32_t NormalSamplingArchive::insert_detail(std::shared_ptr<const NormalDetailAuthority> detail){
  stop(limits_.cancelled);
  if(!detail||!detail->pbr)throw std::invalid_argument("Missing retained normal detail identity");
  for(size_t i=0;i<details_.size();++i)if(details_[i]==detail){detail_referenced_[i]=true;return uint32_t(i);}
  detail->validate(detail->pbr->width);
  if(details_.size()>=limits_.max_details||detail->coverage.size()>limits_.max_bytes-bytes_)
    throw std::length_error("Retained normal detail archive exceeds aggregate budget");
  details_.reserve(details_.size()+1);detail_referenced_.reserve(detail_referenced_.size()+1);
  bytes_+=detail->coverage.size();details_.push_back(std::move(detail));detail_referenced_.push_back(true);
  return uint32_t(details_.size()-1);
}
uint32_t NormalSamplingArchive::add_detail(std::shared_ptr<const NormalDetailAuthority> detail){
  NormalSamplingArchive staged=*this;const auto id=staged.insert_detail(std::move(detail));
  *this=std::move(staged);return id;
}
const std::shared_ptr<const NormalSamplingDomain> &NormalSamplingArchive::at(uint32_t id) const {
  if (id>=domains_.size()) throw std::out_of_range("Invalid normal sampling domain reference");
  referenced_[id]=true;return domains_[id];
}
const std::shared_ptr<const NormalDetailAuthority> &NormalSamplingArchive::detail_at(uint32_t id)const{
  if(id>=details_.size())throw std::out_of_range("Invalid retained normal detail reference");
  detail_referenced_[id]=true;return details_[id];
}
void NormalSamplingArchive::require_reachable() const {
  if (std::find(referenced_.begin(),referenced_.end(),false)!=referenced_.end())
    throw std::invalid_argument("Unreferenced retained normal sampling authority");
  if(std::find(detail_referenced_.begin(),detail_referenced_.end(),false)!=detail_referenced_.end())
    throw std::invalid_argument("Unreferenced retained normal detail identity");
}
nlohmann::json NormalSamplingArchive::encode(PaintImageArchive &images) const {
  auto entries=nlohmann::json::array(),details=nlohmann::json::array();
  for(const auto &detail:details_){stop(limits_.cancelled);details.push_back(encode_detail(*detail,images));}
  for (const auto &domain : domains_) {
    domain->validate(limits_.cancelled);
    std::vector<uint32_t> uv;uv.reserve(domain->coordinates.size()*2);
    for (size_t v=0;v<domain->coordinates.size();++v) {
      if ((v&1023)==0) stop(limits_.cancelled);
      for (const auto f : domain->coordinates[v]) uv.push_back(std::bit_cast<uint32_t>(f));
    }
    std::vector<uint32_t> toon;toon.reserve(domain->mtoon_coordinates.size()*2);
    for(size_t v=0;v<domain->mtoon_coordinates.size();++v) {
      if ((v&1023)==0) stop(limits_.cancelled);
      for (const auto f : domain->mtoon_coordinates[v]) toon.push_back(std::bit_cast<uint32_t>(f));
    }
    nlohmann::json detail=nullptr;
    if(domain->detail){const auto found=std::find(details_.begin(),details_.end(),domain->detail);
      if(found==details_.end())throw std::logic_error("Missing retained normal detail table owner");
      detail=size_t(found-details_.begin());
    }
    entries.push_back({{"version",1},{"resolution",domain->resolution},
        {"coordinates",words(uv,limits_.cancelled)},
        {"mtoon_coordinates",words(toon,limits_.cancelled)},
        {"indices",words(domain->indices,limits_.cancelled)},
        {"resolved",nlohmann::json::array({images.add(domain->resolved_pbr),images.add(domain->resolved_mtoon)})},
        {"detail",std::move(detail)}});
  }
  return {{"version",2},{"details",std::move(details)},{"domains",std::move(entries)}};
}
NormalSamplingArchive NormalSamplingArchive::decode(const nlohmann::json &j,
    const PaintImageArchive &images, NormalSamplingArchiveLimits limits) {
  NormalSamplingArchive result(limits);
  if(!j.is_object()||!j.contains("version"))throw std::invalid_argument("Unsupported normal sampling archive");
  const auto version=integer(j.at("version"),2);
  if ((version!=1&&version!=2)||j.size()!=(version==2?3:2)||
      !j.at("domains").is_array() || j.at("domains").size()>limits.max_domains)
    throw std::invalid_argument("Unsupported normal sampling archive");
  const nlohmann::json *details=nullptr;
  if(version==2){details=&j.at("details");
    if(!details->is_array()||details->size()>limits.max_details)
      throw std::invalid_argument("Invalid retained normal detail table");
  }
  size_t bytes=0;
  if(details)for(const auto &entry:*details){
    stop(limits.cancelled);const auto size=detail_bytes(entry,images);
    if(size>limits.max_bytes-bytes)throw std::length_error("Encoded normal detail table exceeds aggregate budget");
    bytes+=size;
  }
  for (const auto &entry : j.at("domains")) {
    stop(limits.cancelled);
    const auto size=encoded_bytes(entry,images,limits.cancelled,details);
    if (size>limits.max_bytes-bytes)
      throw std::length_error("Encoded normal sampling archive exceeds aggregate budget");
    bytes+=size;
  }
  if(details)for(const auto &entry:*details){
    stop(limits.cancelled);result.insert_detail(decode_detail(entry,images));result.detail_referenced_.back()=false;
  }
  for (const auto &entry : j.at("domains")) {
    stop(limits.cancelled);
    auto domain=std::make_shared<NormalSamplingDomain>();
    domain->resolution=integer(entry.at("resolution"),2048);
    const auto &uv=entry.at("coordinates").get_binary(), &toon=entry.at("mtoon_coordinates").get_binary(), &indices=entry.at("indices").get_binary();
    domain->coordinates.reserve(uv.size()/8);domain->indices.reserve(indices.size()/4);
    for(size_t at=0;at<uv.size();at+=8) {
      if ((at&8191)==0) stop(limits.cancelled);
      domain->coordinates.push_back({std::bit_cast<float>(word(uv,at)),std::bit_cast<float>(word(uv,at+4))});
    }
    domain->mtoon_coordinates.reserve(toon.size()/8);
    for(size_t at=0;at<toon.size();at+=8) {
      if ((at&8191)==0) stop(limits.cancelled);
      domain->mtoon_coordinates.push_back({std::bit_cast<float>(word(toon,at)),std::bit_cast<float>(word(toon,at+4))});
    }
    for(size_t at=0;at<indices.size();at+=4) {
      if ((at&4095)==0) stop(limits.cancelled);
      domain->indices.push_back(word(indices,at));
    }
    const auto &resolved=entry.at("resolved");
    if(!resolved.is_array() || resolved.size()!=2)
      throw std::invalid_argument("Retained resolved normal role table is incomplete");
    domain->resolved_pbr=images.at(integer(resolved[0],UINT32_MAX));
    domain->resolved_mtoon=images.at(integer(resolved[1],UINT32_MAX));
    if (!entry.at("detail").is_null()) {
      if(details)domain->detail=result.detail_at(integer(entry.at("detail"),UINT32_MAX));
      else domain->detail=decode_detail(entry.at("detail"),images);
    }
    result.add(std::move(domain));result.referenced_.back()=false;
  }
  if(result.bytes_!=bytes)throw std::logic_error("Normal sampling archive aggregate accounting differs");
  return result;
}
} // namespace dome
