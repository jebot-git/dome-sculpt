// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/baked-emission.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace dome {
namespace {
constexpr size_t retained_limit = 128 * 1024 * 1024;
constexpr std::array<uint8_t, 8> magic{'D','O','M','E','E','M','I','T'};
constexpr uint32_t packed_version=2;
constexpr uint32_t all_components=127, binary_flag=128, coverage_component=64;
constexpr size_t packed_metadata=sizeof(uint32_t)+7*sizeof(float);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(BakedEmissionSample) == 7 * sizeof(float));
void stop(const BakedEmissionLimits &limits) {
  if (limits.cancelled && limits.cancelled())
    throw std::runtime_error("Baked emission cancelled");
}
// Round the mathematical bound to the stored FLOAT domain. This permits only
// its unavoidable final half-ULP rounding, not an arbitrary epsilon/clamp.
float absolute_bound(float coverage) {
  return float(double(std::numeric_limits<float>::max()) * coverage);
}
void put(std::vector<uint8_t> &out, size_t &offset, uint64_t value, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) out[offset++] = uint8_t(value >> (8 * i));
}
uint64_t get(std::span<const uint8_t> in, size_t &offset, size_t bytes) {
  uint64_t value = 0;
  for (size_t i = 0; i < bytes; ++i) value |= uint64_t(in[offset++]) << (8 * i);
  return value;
}
template<class F> void components(const BakedEmissionSample &v, F f) {
  for (float x : v.legacy) f(x);
  for (float x : v.absolute) f(x);
  f(v.coverage);
}
std::array<float,7> coefficients(const BakedEmissionSample &v){
  return {v.legacy[0],v.legacy[1],v.legacy[2],v.absolute[0],v.absolute[1],v.absolute[2],v.coverage};
}
BakedEmissionSample sample_value(const std::array<float,7> &v){
  return {{v[0],v[1],v[2]},{v[3],v[4],v[5]},v[6]};
}
uint32_t bits(float value){return std::bit_cast<uint32_t>(value);}
bool binary_coverage(float value){return bits(value)==bits(0.f)||bits(value)==bits(1.f);}
size_t checked_count(uint32_t resolution,const BakedEmissionLimits &limits){
  if(!limits.max_bytes||limits.max_bytes>retained_limit)
    throw std::invalid_argument("Invalid baked emission byte budget");
  stop(limits);
  if(resolution<8||resolution>4096)throw std::invalid_argument("Invalid baked emission grid");
  return size_t(resolution)*resolution;
}
size_t coverage_words(size_t count){return (count+31)/32;}
struct PackedLayout {
  uint32_t mask=0;
  bool binary=false;
  std::array<float,7> constants{};
  size_t stride() const {return size_t(std::popcount(mask));}
  size_t coefficient_words(size_t count) const {return stride()*count;}
  size_t retained(size_t count) const {
    return packed_metadata+(coefficient_words(count)+(binary?coverage_words(count):0))*sizeof(uint32_t);
  }
};
float read_float(std::span<const uint8_t> bytes,size_t &offset){
  return std::bit_cast<float>(uint32_t(get(bytes,offset,4)));
}
struct WireLayout {
  uint32_t resolution;
  size_t count;
  size_t retained;
  bool packed;
  PackedLayout layout;
};
WireLayout wire_layout(std::span<const uint8_t> bytes,const BakedEmissionLimits &limits);
BakedEmissionSample read(std::span<const uint8_t> bytes, size_t &offset) {
  BakedEmissionSample result;
  for (auto &v : result.legacy) v = std::bit_cast<float>(uint32_t(get(bytes, offset, 4)));
  for (auto &v : result.absolute) v = std::bit_cast<float>(uint32_t(get(bytes, offset, 4)));
  result.coverage = std::bit_cast<float>(uint32_t(get(bytes, offset, 4)));
  return result;
}
BakedEmissionSample weighted(std::span<const BakedEmissionSample> values,
                             std::span<const double> weights) {
  double coverage = 0;
  std::array<double, 3> legacy{}, absolute{};
  for (size_t i = 0; i < values.size(); ++i) {
    coverage += double(values[i].coverage) * weights[i];
    for (size_t k = 0; k < 3; ++k) {
      legacy[k] += double(values[i].legacy[k]) * weights[i];
      absolute[k] += double(values[i].absolute[k]) * weights[i];
    }
  }
  BakedEmissionSample result;
  result.coverage = float(coverage);
  for (size_t k = 0; k < 3; ++k) {
    result.legacy[k] = float(legacy[k]);
    result.absolute[k] = float(absolute[k]);
  }
  // At the subnormal boundary FLOAT coverage can underflow even though an HDR
  // coefficient remains representable. Refuse that unrepresentable result;
  // never quietly replace it by zero or manufacture coverage.
  result.validate();
  return result;
}
} // namespace

void BakedEmissionSample::validate() const {
  if (!std::isfinite(coverage) || coverage < 0 || coverage > 1)
    throw std::invalid_argument("Invalid baked emission coverage");
  const float bound = absolute_bound(coverage);
  for (size_t k = 0; k < 3; ++k) {
    if (!std::isfinite(legacy[k]) || legacy[k] < 0 || legacy[k] > coverage ||
        !std::isfinite(absolute[k]) || absolute[k] < 0 || absolute[k] > bound)
      throw std::invalid_argument("Invalid premultiplied baked emission coefficient");
  }
}
std::array<float, 3> BakedEmissionSample::resolve(std::array<float, 3> background,
                                                float strength) const {
  validate();
  if (!std::isfinite(strength) || strength < 0)
    throw std::invalid_argument("Invalid baked emission legacy strength");
  std::array<float, 3> result;
  for (size_t k = 0; k < 3; ++k) {
    if (!std::isfinite(background[k]) || background[k] < 0)
      throw std::invalid_argument("Invalid baked emission background");
    const double value = double(background[k]) * (1 - double(coverage)) +
                         double(legacy[k]) * strength + absolute[k];
    if (value > std::numeric_limits<float>::max())
      throw std::overflow_error("Baked emission radiance exceeds FLOAT range");
    result[k] = float(value);
  }
  return result;
}
size_t BakedEmissionField::required_bytes(uint32_t resolution,
                                         const BakedEmissionLimits &limits) {
  const uint64_t count = checked_count(resolution,limits);
  const uint64_t bytes = count * 7 * sizeof(float);
  if (bytes > limits.max_bytes || bytes > std::numeric_limits<size_t>::max())
    throw std::length_error("Baked emission exceeds retained byte budget");
  return size_t(bytes);
}
std::shared_ptr<const BakedEmissionField>
BakedEmissionField::create(uint32_t r, std::span<const BakedEmissionSample> values,
                            const BakedEmissionLimits &limits) {
  const size_t bytes = required_bytes(r, limits);
  if (values.size() != bytes / (7 * sizeof(float)))
    throw std::invalid_argument("Baked emission grid/sample count mismatch");
  for (size_t i = 0; i < values.size(); ++i) {
    if ((i & 4095) == 0) stop(limits);
    values[i].validate();
  }
  stop(limits);
  auto result = std::shared_ptr<const BakedEmissionField>(new BakedEmissionField(r, values));
  stop(limits);
  return result;
}
std::shared_ptr<const BakedEmissionField> BakedEmissionField::generate(uint32_t r,
    const std::function<BakedEmissionSample(size_t)> &source,const BakedEmissionLimits &limits){
  return generate_storage(r,source,limits,false);
}
std::shared_ptr<const BakedEmissionField> BakedEmissionField::generate_compact(uint32_t r,
    const std::function<BakedEmissionSample(size_t)> &source,const BakedEmissionLimits &limits){
  return generate_storage(r,source,limits,true);
}
std::shared_ptr<const BakedEmissionField> BakedEmissionField::generate_storage(uint32_t r,
    const std::function<BakedEmissionSample(size_t)> &source,const BakedEmissionLimits &limits,
    bool prefer_compact){
  const size_t count=checked_count(r,limits);
  if(!source)throw std::invalid_argument("Missing baked emission sample generator");
  const auto dense_bytes=count*sizeof(BakedEmissionSample);
  const auto dense=[&]{
    std::vector<BakedEmissionSample> values(count);
    for(size_t i=0;i<count;++i){
      if((i&4095)==0)stop(limits);
      values[i]=source(i);
      values[i].validate();
    }
    stop(limits);
    return std::shared_ptr<const BakedEmissionField>(new BakedEmissionField(r,std::move(values)));
  };
  if(!prefer_compact&&dense_bytes<=limits.max_bytes)return dense();
  PackedLayout layout;
  bool binary=true;
  for(size_t i=0;i<count;++i){
    if((i&4095)==0)stop(limits);
    const auto value=source(i);
    value.validate();
    const auto fields=coefficients(value);
    if(i==0)layout.constants=fields;
    for(size_t c=0;c<fields.size();++c)
      if(bits(fields[c])!=bits(layout.constants[c]))layout.mask|=1u<<c;
    binary&=binary_coverage(value.coverage);
  }
  layout.binary=binary&&(layout.mask&coverage_component);
  if(layout.binary)layout.mask&=~coverage_component;
  const auto compact_bytes=layout.retained(count);
  if(dense_bytes<=limits.max_bytes&&dense_bytes<=compact_bytes)return dense();
  if(compact_bytes>limits.max_bytes)
    throw std::length_error("Exact FLOAT baked emission exceeds the available paint budget");
  auto result=std::unique_ptr<BakedEmissionField>(new BakedEmissionField(r));
  result->varying_mask_=layout.mask;
  result->constants_=layout.constants;
  for(size_t c=0;c<layout.constants.size();++c)
    if((layout.mask&(1u<<c))||(layout.binary&&c==6))result->constants_[c]=0;
  result->varying_.resize(layout.coefficient_words(count));
  if(layout.binary)result->coverage_words_.resize(coverage_words(count));
  if(layout.mask||layout.binary){
    size_t next=0;
    for(size_t i=0;i<count;++i){
      if((i&4095)==0)stop(limits);
      const auto value=source(i);
      value.validate();
      const auto fields=coefficients(value);
      for(size_t c=0;c<fields.size();++c){
        if(layout.mask&(1u<<c))result->varying_[next++]=fields[c];
        else if(!(layout.binary&&c==6)&&bits(fields[c])!=bits(layout.constants[c]))
          throw std::invalid_argument("Baked emission generator changed its constant coefficient");
      }
      if(layout.binary){
        if(!binary_coverage(value.coverage))
          throw std::invalid_argument("Baked emission generator changed its binary coverage");
        if(value.coverage==1)result->coverage_words_[i/32]|=1u<<(i%32);
      }
    }
  }
  stop(limits);
  return std::shared_ptr<const BakedEmissionField>(std::move(result));
}
size_t BakedEmissionField::storage_bytes() const noexcept {
  if(!values_.empty())return values_.size()*sizeof(BakedEmissionSample);
  return packed_metadata+(varying_.size()+coverage_words_.size())*sizeof(uint32_t);
}
BakedEmissionField::StorageView BakedEmissionField::storage() const noexcept {
  return {varying_mask_,constants_,values_,varying_,coverage_words_};
}
BakedEmissionSample BakedEmissionField::texel(size_t at) const {
  if(at>=sample_count())throw std::out_of_range("Baked emission texel outside grid");
  if(!values_.empty())return values_[at];
  auto fields=constants_;
  size_t next=at*size_t(std::popcount(varying_mask_));
  for(size_t c=0;c<fields.size();++c)
    if(varying_mask_&(1u<<c))fields[c]=varying_[next++];
  if(!coverage_words_.empty())fields[6]=float((coverage_words_[at/32]>>(at%32))&1u);
  return sample_value(fields);
}
BakedEmissionSample BakedEmissionField::sample(float u, float v) const {
  if (!std::isfinite(u) || !std::isfinite(v))
    throw std::invalid_argument("Invalid baked emission UV");
  const double x = std::clamp(double(u) * resolution_ - .5, 0., double(resolution_ - 1));
  const double y = std::clamp(double(v) * resolution_ - .5, 0., double(resolution_ - 1));
  const uint32_t x0 = uint32_t(x), y0 = uint32_t(y);
  const uint32_t x1 = std::min(x0 + 1, resolution_ - 1), y1 = std::min(y0 + 1, resolution_ - 1);
  const double a = x - x0, b = y - y0;
  const std::array<BakedEmissionSample, 4> values{
      texel(size_t(y0)*resolution_+x0), texel(size_t(y0)*resolution_+x1),
      texel(size_t(y1)*resolution_+x0), texel(size_t(y1)*resolution_+x1)};
  const std::array<double, 4> weights{(1-a)*(1-b), a*(1-b), (1-a)*b, a*b};
  return weighted(values, weights);
}
BakedEmissionSample BakedEmissionField::over(const BakedEmissionSample &below,
                                             const BakedEmissionSample &above) {
  below.validate(); above.validate();
  const std::array<BakedEmissionSample, 2> values{above, below};
  const std::array<double, 2> weights{1., 1. - double(above.coverage)};
  return weighted(values, weights);
}
std::vector<uint8_t> BakedEmissionField::encode(const BakedEmissionLimits &limits) const {
  checked_count(resolution_,limits);
  const size_t bytes=storage_bytes();
  if(bytes>limits.max_bytes)throw std::length_error("Baked emission exceeds retained byte budget");
  const bool packed=values_.empty();
  std::vector<uint8_t> out(encoded_header_bytes + bytes);
  std::copy(magic.begin(), magic.end(), out.begin());
  size_t offset = magic.size();
  put(out, offset, packed?packed_version:version, 4); put(out, offset, resolution_, 4);
  put(out, offset, sample_count(), 8); put(out, offset, bytes, 8);
  if(packed){
    put(out,offset,varying_mask_|(coverage_words_.empty()?0:binary_flag),4);
    for(auto value:constants_)put(out,offset,bits(value),4);
    for(size_t i=0;i<varying_.size();++i){
      if((i&4095)==0)stop(limits);
      put(out,offset,bits(varying_[i]),4);
    }
    for(size_t i=0;i<coverage_words_.size();++i){
      if((i&4095)==0)stop(limits);
      put(out,offset,coverage_words_[i],4);
    }
  }
  for (size_t i = 0; i < values_.size(); ++i) {
    if ((i & 4095) == 0) stop(limits);
    components(values_[i], [&](float v) { put(out, offset, std::bit_cast<uint32_t>(v), 4); });
  }
  stop(limits);
  return out;
}
namespace {
WireLayout wire_layout(std::span<const uint8_t> bytes,const BakedEmissionLimits &limits){
  stop(limits);
  const auto header=BakedEmissionField::encoded_header_bytes;
  if(bytes.size()<header||!std::equal(magic.begin(),magic.end(),bytes.begin()))
    throw std::invalid_argument("Invalid baked emission field header");
  size_t offset=magic.size();
  const auto version=get(bytes,offset,4);
  if(version!=BakedEmissionField::version&&version!=packed_version)
    throw std::invalid_argument("Unsupported baked emission field version");
  const auto r=uint32_t(get(bytes,offset,4));
  const auto count=checked_count(r,limits);
  const auto encoded_count=get(bytes,offset,8);
  const auto payload=get(bytes,offset,8);
  if(encoded_count!=count||payload!=bytes.size()-header)
    throw std::invalid_argument("Baked emission encoded size mismatch");
  if(version==BakedEmissionField::version){
    const auto retained=BakedEmissionField::required_bytes(r,limits);
    if(payload!=retained)throw std::invalid_argument("Baked emission encoded size mismatch");
    return {r,count,retained,false,{}};
  }
  if(payload<packed_metadata)throw std::invalid_argument("Missing packed baked emission layout");
  PackedLayout layout;
  const auto code=uint32_t(get(bytes,offset,4));
  layout.mask=code&all_components;
  layout.binary=bool(code&binary_flag);
  if(code>255||(layout.binary&&(layout.mask&coverage_component)))
    throw std::invalid_argument("Invalid packed baked emission channel flags");
  for(size_t c=0;c<layout.constants.size();++c){
    layout.constants[c]=read_float(bytes,offset);
    if(((layout.mask&(1u<<c))||(layout.binary&&c==6))&&bits(layout.constants[c])!=0)
      throw std::invalid_argument("Unexpected packed baked emission constant");
  }
  const auto retained=layout.retained(count);
  if(payload!=retained||retained>=count*sizeof(BakedEmissionSample))
    throw std::invalid_argument("Invalid packed baked emission length");
  if(retained>limits.max_bytes)throw std::length_error("Baked emission exceeds retained byte budget");
  return {r,count,retained,true,layout};
}
void validate_payload(std::span<const uint8_t> bytes,const WireLayout &wire,const BakedEmissionLimits &limits){
  auto offset=BakedEmissionField::encoded_header_bytes;
  if(!wire.packed){
    for(size_t i=0;i<wire.count;++i){
      if((i&4095)==0)stop(limits);
      read(bytes,offset).validate();
    }
  }else{
    offset+=packed_metadata;
    auto coverage=offset+wire.layout.coefficient_words(wire.count)*sizeof(float);
    uint32_t word=0;
    for(size_t i=0;i<wire.count;++i){
      if((i&4095)==0)stop(limits);
      auto values=wire.layout.constants;
      for(size_t c=0;c<values.size();++c)
        if(wire.layout.mask&(1u<<c))values[c]=read_float(bytes,offset);
      if(wire.layout.binary){
        if(i%32==0)word=uint32_t(get(bytes,coverage,4));
        values[6]=float(word&1u);
        word>>=1;
      }
      sample_value(values).validate();
    }
    if(word)throw std::invalid_argument("Packed baked emission has bits outside its coverage grid");
  }
  stop(limits);
}
}
BakedEmissionField::EncodedSize BakedEmissionField::inspect_encoded(std::span<const uint8_t> bytes,
    const BakedEmissionLimits &limits){
  const auto wire=wire_layout(bytes,limits);
  validate_payload(bytes,wire,limits);
  return {wire.resolution,wire.retained};
}
uint32_t BakedEmissionField::encoded_resolution(std::span<const uint8_t> bytes,
    const BakedEmissionLimits &limits){return inspect_encoded(bytes,limits).resolution;}
std::shared_ptr<const BakedEmissionField>
BakedEmissionField::decode(std::span<const uint8_t> bytes, const BakedEmissionLimits &limits) {
  const auto wire=wire_layout(bytes,limits);
  validate_payload(bytes,wire,limits);
  size_t offset=encoded_header_bytes;
  auto result=std::unique_ptr<BakedEmissionField>(new BakedEmissionField(wire.resolution));
  if(!wire.packed){
    result->values_.resize(wire.count);
    for(size_t i=0;i<wire.count;++i){
      if((i&4095)==0)stop(limits);
      result->values_[i]=read(bytes,offset);
    }
  }else{
    offset+=packed_metadata;
    result->varying_mask_=wire.layout.mask;
    result->constants_=wire.layout.constants;
    result->varying_.resize(wire.layout.coefficient_words(wire.count));
    for(size_t i=0;i<result->varying_.size();++i){
      if((i&4095)==0)stop(limits);
      result->varying_[i]=read_float(bytes,offset);
    }
    if(wire.layout.binary)result->coverage_words_.resize(coverage_words(wire.count));
    for(size_t i=0;i<result->coverage_words_.size();++i){
      if((i&4095)==0)stop(limits);
      result->coverage_words_[i]=uint32_t(get(bytes,offset,4));
    }
  }
  stop(limits);
  return std::shared_ptr<const BakedEmissionField>(std::move(result));
}
} // namespace dome
