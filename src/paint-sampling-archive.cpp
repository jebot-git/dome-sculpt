// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/paint-sampling.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <nlohmann/json.hpp>

namespace dome { namespace {
void stop(const PaintSamplingArchiveLimits &l) {
  if(l.geometry.cancelled&&l.geometry.cancelled())throw std::runtime_error("Paint sampling archive cancelled");
}
uint32_t integer(const nlohmann::json &j,size_t maximum) {
  if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>maximum)
    throw std::invalid_argument("Invalid paint sampling archive integer");
  return j.get<uint32_t>();
}
uint32_t word(const nlohmann::json::binary_t &b,size_t at) {
  return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
void append(std::vector<uint8_t> &b,uint32_t value) {
  for(unsigned k=0;k<4;++k)b.push_back(uint8_t(value>>(k*8)));
}
template<class F> nlohmann::json words(size_t count,F value,const PaintSamplingArchiveLimits &l) {
  std::vector<uint8_t> bytes;bytes.reserve(count*4);
  for(size_t i=0;i<count;++i){if((i&1023)==0)stop(l);append(bytes,value(i));}
  return nlohmann::json::binary(std::move(bytes));
}
void validate_corners(const PaintSamplingDomain &d,const PaintSamplingArchiveLimits &l) {
  // A complete triangle permutation can be validated without fabricating a
  // current REST mesh or trusting positional/count-only matching.
  const auto &m=*d.basis();const auto corners=d.source_corners();
  if(corners.size()!=m.indices.size())throw std::invalid_argument("Incomplete paint sampling corner map");
  std::vector<uint8_t> seen(m.indices.size()/3);
  for(size_t c=0;c<corners.size();c+=3){if((c&1023)==0)stop(l);auto at=corners[c];
    if(at>=m.indices.size()||at%3||corners[c+1]!=at+1||corners[c+2]!=at+2||seen[at/3]++)
      throw std::invalid_argument("Paint sampling corners reverse, duplicate or lose a source triangle");
  }
}
size_t preflight_basis(const nlohmann::json &j,const PaintSamplingArchiveLimits &l) {
  if(!j.is_object()||j.size()!=6)throw std::invalid_argument("Invalid paint sampling basis fields");
  auto blob=[&](const char *key,size_t stride,size_t cap,bool empty=false)->const auto&{
    const auto &v=j.at(key);if(!v.is_binary()||(!empty&&v.get_binary().empty())||v.get_binary().size()%stride||v.get_binary().size()/stride>cap)
      throw std::length_error("Invalid paint sampling binary dimensions");
    return v.get_binary();};
  const auto &p=blob("positions",12,l.geometry.max_vertices),&n=blob("normals",12,l.geometry.max_vertices),
    &uv=blob("uv",8,l.geometry.max_vertices),&ix=blob("indices",12,l.geometry.max_triangles),
    &pins=blob("pins",1,l.geometry.max_vertices,true),&seams=blob("seams",1,l.geometry.max_triangles*3,true);
  const size_t vertices=p.size()/12;
  if(n.size()!=p.size()||uv.size()/8!=vertices||(!pins.empty()&&pins.size()!=vertices)||(!seams.empty()&&seams.size()!=ix.size()/4))
    throw std::invalid_argument("Paint sampling basis streams have different vertex/corner identities");
  size_t bytes=p.size()+n.size()+uv.size()+ix.size()+pins.size()+seams.size();
  if(bytes>l.geometry.max_basis_bytes||bytes>l.max_bytes)throw std::length_error("Paint sampling basis exceeds byte budget");
  if(bytes/4>l.geometry.max_work)throw std::length_error("Paint sampling preflight work budget exceeded");
  for(auto *b:{&p,&n,&uv})for(size_t at=0;at<b->size();at+=4){if((at&4095)==0)stop(l);auto f=std::bit_cast<float>(word(*b,at));
    if(!std::isfinite(f)||(b==&uv&&(f<0||f>1)))throw std::invalid_argument("Paint sampling basis has invalid finite/unit coordinates");}
  for(size_t at=0;at<ix.size();at+=4){if((at&4095)==0)stop(l);if(word(ix,at)>=vertices)throw std::out_of_range("Paint sampling basis index is out of range");}
  for(auto *b:{&pins,&seams})for(auto flag:*b)if(flag>1)throw std::invalid_argument("Invalid paint sampling edit flag");
  return bytes;
}
Mesh read_basis(const nlohmann::json &j) {
  Mesh m;auto vec3=[&](const char *key,auto &out){const auto &b=j.at(key).get_binary();out.reserve(b.size()/12);for(size_t at=0;at<b.size();at+=12)out.push_back({std::bit_cast<float>(word(b,at)),std::bit_cast<float>(word(b,at+4)),std::bit_cast<float>(word(b,at+8))});};
  vec3("positions",m.positions);vec3("normals",m.normals);
  const auto &uv=j.at("uv").get_binary(),&ix=j.at("indices").get_binary();m.uv.reserve(uv.size()/8);m.indices.reserve(ix.size()/4);
  for(size_t at=0;at<uv.size();at+=8)m.uv.push_back({std::bit_cast<float>(word(uv,at)),std::bit_cast<float>(word(uv,at+4))});
  for(size_t at=0;at<ix.size();at+=4)m.indices.push_back(word(ix,at));
  m.uv_pins=j.at("pins").get_binary();m.uv_seams=j.at("seams").get_binary();m.validate();return m;
}
} // namespace
PaintSamplingArchive::PaintSamplingArchive(PaintSamplingArchiveLimits l):limits_(std::move(l)) {
  if(!limits_.max_domains||limits_.max_domains>128||!limits_.max_bases||limits_.max_bases>128||!limits_.max_bytes||limits_.max_bytes>128*1024*1024)
    throw std::invalid_argument("Invalid paint sampling pool limits");
  stop(limits_);
  // Validate all nested limits even when the pool is empty, before arithmetic.
  auto probe=limits_.geometry;
  if(!probe.max_vertices||probe.max_vertices>2'000'000||!probe.max_triangles||probe.max_triangles>2'000'000||!probe.max_basis_bytes||probe.max_basis_bytes>128*1024*1024||!probe.max_working_bytes||probe.max_working_bytes>512*1024*1024||!probe.max_work||probe.max_work>256'000'000)
    throw std::invalid_argument("Invalid paint sampling geometry pool limits");
}
uint32_t PaintSamplingArchive::add(std::shared_ptr<const PaintSamplingDomain> d) {
  stop(limits_);if(!d)throw std::invalid_argument("Missing paint sampling domain");
  for(size_t i=0;i<domains_.size();++i)if(domains_[i]==d)return uint32_t(i);
  if(domains_.size()>=limits_.max_domains)throw std::length_error("Paint sampling domain count exceeded");
  auto count=d->storage_bytes(limits_.geometry);validate_corners(*d,limits_);
  size_t unique=0;bool shared=false;for(size_t i=0;i<domains_.size();++i){
    bool first=true;for(size_t k=0;k<i;++k)if(domains_[k]->basis()==domains_[i]->basis())first=false;
    unique+=first;shared|=domains_[i]->basis()==d->basis();}
  if(shared)count=d->source_corners().size()*sizeof(uint32_t);
  else if(unique>=limits_.max_bases)throw std::length_error("Paint sampling basis count exceeded");
  if(count>limits_.max_bytes-bytes_)throw std::length_error("Paint sampling pool byte budget exceeded");
  domains_.reserve(domains_.size()+1);referenced_.reserve(referenced_.size()+1);stop(limits_);
  domains_.push_back(std::move(d));referenced_.push_back(0);bytes_+=count;return uint32_t(domains_.size()-1);
}
const std::shared_ptr<const PaintSamplingDomain> &PaintSamplingArchive::at(uint32_t id) const {
  if(id>=domains_.size())throw std::out_of_range("Paint sampling reference is out of range");
  referenced_[id]=1;return domains_[id];
}
void PaintSamplingArchive::require_reachable() const {
  if(std::find(referenced_.begin(),referenced_.end(),uint8_t(0))!=referenced_.end())throw std::invalid_argument("Orphan paint sampling domain");
}
nlohmann::json PaintSamplingArchive::encode() const {
  nlohmann::json out={{"version",1},{"bases",nlohmann::json::array()},{"domains",nlohmann::json::array()}};
  std::vector<std::shared_ptr<const Mesh>> bases;
  for(const auto &d:domains_){stop(limits_);d->storage_bytes(limits_.geometry);validate_corners(*d,limits_);
    auto found=std::find(bases.begin(),bases.end(),d->basis());size_t id=size_t(found-bases.begin());
    if(found==bases.end()){const auto &m=*d->basis();bases.push_back(d->basis());
      auto vectors=[&](const auto &v){return words(v.size()*3,[&](size_t i){auto p=v[i/3];return std::bit_cast<uint32_t>(i%3==0?p.x:i%3==1?p.y:p.z);},limits_);};
      out["bases"].push_back({{"positions",vectors(m.positions)},{"normals",vectors(m.normals)},
        {"uv",words(m.uv.size()*2,[&](size_t i){return std::bit_cast<uint32_t>(m.uv[i/2][i%2]);},limits_)},
        {"indices",words(m.indices.size(),[&](size_t i){return m.indices[i];},limits_)},
        {"pins",nlohmann::json::binary(m.uv_pins)},{"seams",nlohmann::json::binary(m.uv_seams)}});}
    out["domains"].push_back({{"basis",id},{"corners",words(d->source_corners().size(),[&](size_t i){return d->source_corners()[i];},limits_)}});
  }return out;
}
PaintSamplingArchive PaintSamplingArchive::decode(const nlohmann::json &j,PaintSamplingArchiveLimits limits) {
  PaintSamplingArchive result(std::move(limits));const auto &l=result.limits_;
  if(!j.is_object()||j.size()!=3||integer(j.at("version"),1)!=1||!j.at("bases").is_array()||!j.at("domains").is_array()||j.at("bases").empty()||j.at("domains").empty()||j.at("bases").size()>l.max_bases||j.at("domains").size()>l.max_domains)
    throw std::invalid_argument("Invalid paint sampling pool shape/version");
  size_t total=0;std::array<size_t,128> basis_bytes{};size_t basis_at=0;
  for(const auto &base:j.at("bases")){stop(l);auto count=preflight_basis(base,l);
    if(count>l.max_bytes-total)throw std::length_error("Paint sampling pool basis bytes exceeded");
    total+=count;basis_bytes[basis_at++]=count;
    if(total/4>l.geometry.max_work)throw std::length_error("Paint sampling cumulative preflight work exceeded");
  }
  // Check all references and dimensions before allocating a single Mesh vector.
  std::vector<uint8_t> referenced(j.at("bases").size());
  for(const auto &d:j.at("domains")){stop(l);if(!d.is_object()||d.size()!=2)throw std::invalid_argument("Invalid paint sampling domain fields");auto id=integer(d.at("basis"),UINT32_MAX);if(id>=referenced.size())throw std::out_of_range("Paint sampling domain basis is missing");referenced[id]=1;
    const auto &b=d.at("corners");if(!b.is_binary()||b.get_binary().size()!=j.at("bases")[id].at("indices").get_binary().size())throw std::invalid_argument("Paint sampling corner dimensions differ");
    auto count=b.get_binary().size();if(count>l.geometry.max_basis_bytes-basis_bytes[id])throw std::length_error("Paint sampling basis and correspondence exceed retained bytes");
    if(count>l.max_bytes-total)throw std::length_error("Paint sampling pool corner bytes exceeded");total+=count;
    if(total/4>l.geometry.max_work)throw std::length_error("Paint sampling cumulative corner work exceeded");
    std::vector<uint8_t> seen(count/12);
    const auto &bytes=b.get_binary();for(size_t at=0;at<bytes.size();at+=12){if((at&4095)==0)stop(l);auto first=word(bytes,at);if(first>=count/4||first%3||word(bytes,at+4)!=first+1||word(bytes,at+8)!=first+2||seen[first/3]++)throw std::invalid_argument("Invalid paint sampling ordered corner triple");}}
  if(total>l.geometry.max_working_bytes)throw std::length_error("Paint sampling decoded geometry exceeds working bytes");
  if(std::find(referenced.begin(),referenced.end(),uint8_t(0))!=referenced.end())throw std::invalid_argument("Orphan paint sampling basis");
  std::vector<std::shared_ptr<const Mesh>> bases;bases.reserve(j.at("bases").size());for(const auto &base:j.at("bases")){stop(l);bases.push_back(std::make_shared<const Mesh>(read_basis(base)));}
  for(const auto &d:j.at("domains")){stop(l);auto id=integer(d.at("basis"),UINT32_MAX);const auto &bytes=d.at("corners").get_binary();std::vector<uint32_t> corners;corners.reserve(bytes.size()/4);for(size_t at=0;at<bytes.size();at+=4)corners.push_back(word(bytes,at));
    result.add(std::shared_ptr<const PaintSamplingDomain>(new const PaintSamplingDomain(bases[id],std::move(corners))));}
  return result;
}
} // namespace dome
