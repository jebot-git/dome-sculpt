// SPDX-License-Identifier: GPL-3.0-or-later
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include "dome/source-maps.hpp"
#include "normal-bake-encoding.hpp"
namespace dome::detail {
namespace {
// Include conservative Image/control-block/container bookkeeping in addition
// to vector payloads. The immutable conversion origin has its own producer
// budget and is not silently counted as new field-owned storage here.
constexpr size_t node_bytes=sizeof(BakedNormalField)+1024;
void policy(const BakedNormalFieldOptions &o){
  if(!o.max_working_bytes||o.max_working_bytes>512*1024*1024||
     !o.max_retained_bytes||o.max_retained_bytes>128*1024*1024||
     !o.max_replacements||o.max_replacements>128)
    throw std::invalid_argument("Invalid independent baked-normal field policy");
}
void stop(const BakedNormalFieldOptions &o){
  if(o.cancelled&&o.cancelled())throw std::runtime_error("Baked-normal field transition cancelled");
}
void budget(size_t prior,size_t pixels,size_t bytes,size_t cap){
  if(prior>cap||pixels>(cap-prior)/bytes)
    throw std::length_error("Independent baked-normal field exceeds its retained/working budget");
}
uint8_t encode(float x){return uint8_t(std::lround(std::clamp(x,0.f,1.f)*255));}
bool same_bytes(std::span<const uint8_t> a,std::span<const uint8_t> b,const BakedNormalFieldOptions &o){
  if(a.size()!=b.size())return false;
  if(a.data()==b.data())return true;
  constexpr size_t block=16384;
  for(size_t at=0;at<a.size();at+=block){
    stop(o);
    const auto count=std::min(block,a.size()-at);
    if(!std::equal(a.begin()+at,a.begin()+at+count,b.begin()+at))return false;
  }
  return true;
}
std::shared_ptr<const Image> write_image(
    const Image &write,const std::shared_ptr<const Image> &last,const BakedNormalFieldOptions &o){
  if(last&&same_bytes(write.rgba,last->rgba,o))return last;
  return std::make_shared<const Image>(write);
}
std::shared_ptr<const Image> compose(
    const std::shared_ptr<const Image> &previous,const Image &write,
    std::span<const uint8_t> coverage,const BakedNormalFieldOptions &o){
  auto result=std::make_shared<Image>(*previous);
  bool changed=false;
  for(size_t i=0;i<coverage.size();++i){
    if((i&1023)==0)stop(o);
    if(!coverage[i])continue;
    const float a=float(coverage[i])/255;
    const auto *before=previous->rgba.data()+i*4;
    const auto *after=write.rgba.data()+i*4;
    const Vec3 old_direction{float(before[0])/255*2-1,float(before[1])/255*2-1,float(before[2])/255*2-1};
    const Vec3 new_direction{float(after[0])/255*2-1,float(after[1])/255*2-1,float(after[2])/255*2-1};
    auto value=old_direction*(1-a)+new_direction*a;
    value=length(value)>1e-8f?normalized(value):Vec3{0,0,1};
    const std::array<uint8_t,4> bytes{encode(value.x*.5f+.5f),encode(value.y*.5f+.5f),
        encode(value.z*.5f+.5f),255};
    changed|=!std::equal(bytes.begin(),bytes.end(),before);
    std::copy(bytes.begin(),bytes.end(),result->rgba.data()+i*4);
  }
  return changed?std::shared_ptr<const Image>(std::move(result)):previous;
}
}
void BakedNormalField::account_retained_storage(){
  retained_bytes_=node_bytes+(prior_?prior_->retained_bytes_:0);
  const auto images=[](const BakedNormalField &field){
    return std::array{field.pbr_.get(),field.mtoon_.get(),field.write_pbr_.get(),field.write_mtoon_.get()};
  };
  const auto coverage=[](const BakedNormalField &field){
    return std::array{field.coverage_.get(),field.write_coverage_.get()};
  };
  const auto count_unique=[&](const auto &owners,auto payload_size){
    const auto current=owners(*this);
    for(auto it=current.begin();it!=current.end();++it){
      if(!*it||std::find(current.begin(),it,*it)!=it)continue;
      bool shared=false;
      for(auto node=prior_.get();node&&!shared;node=node->prior_.get()){
        const auto earlier=owners(*node);
        shared=std::find(earlier.begin(),earlier.end(),*it)!=earlier.end();
      }
      if(!shared)retained_bytes_+=payload_size(**it);
    }
  };
  count_unique(images,[](const Image &image){return image.rgba.size();});
  count_unique(coverage,[](const std::vector<uint8_t> &bytes){return bytes.size();});
}
std::shared_ptr<const BakedNormalField> BakedNormalField::from_canonical(
    std::shared_ptr<const CanonicalNormalConversion> origin,const BakedNormalFieldOptions &o){
  policy(o);stop(o);
  if(!origin||!origin->valid_owner(origin))throw std::invalid_argument("Baked normal field needs its actual canonical conversion owner");
  const auto projection=origin->projection();const auto r=projection->pbr->width;
  const auto pixels=size_t(r)*r;
  budget(node_bytes,pixels,projection->pbr==projection->mtoon?5:9,
      std::min(o.max_retained_bytes,o.max_working_bytes));
  origin->validate();
  auto result=std::shared_ptr<BakedNormalField>(new BakedNormalField);
  result->origin_=std::move(origin);result->pbr_=projection->pbr;result->mtoon_=projection->mtoon;
  result->coverage_=std::make_shared<const std::vector<uint8_t>>(pixels,255);
  result->account_retained_storage();
  result->self_=result;result->validate();stop(o);return result;
}
std::shared_ptr<const BakedNormalField> BakedNormalField::replace(
    const BakedTextures &incoming,const BakedNormalFieldOptions &o) const {
  policy(o);stop(o);validate();incoming.validate(resolution());
  if(incoming.normal_interpretation!=BakedNormalInterpretation::PreparedAttributes||
     incoming.maps.normal.rgba.empty()||incoming.maps.mtoon_normal.rgba.empty()||incoming.normal_authority)
    throw std::invalid_argument("Independent baked field requires complete newly encoded prepared normal roles");
  return replace_planes(incoming.maps.normal,incoming.maps.mtoon_normal,incoming.coverage,o);
}
std::array<Vec3,2> BakedNormalField::sample(const NormalBakeBasis &basis,std::array<float,2> uv) const {
  PaintSourceTexture view;
  view.wrap_s=view.wrap_t=33071;
  view.min_filter=view.mag_filter=9729;
  std::array<Vec3,2> directions;
  for (size_t role=0;role<directions.size();++role) {
    view.image=role?mtoon_:pbr_;
    const auto value=sample_source_texture(view,uv[0],uv[1],false);
    const auto n=mtoon_normal_map({value[0],value[1],value[2]},1);
    directions[role]=normalized(basis.t*n.x+basis.b*n.y+basis.n*n.z);
  }
  return directions;
}
std::shared_ptr<const BakedNormalField> BakedNormalField::replace_planes(
    const Image &pbr_write,const Image &toon_write,std::span<const uint8_t> coverage,
    const BakedNormalFieldOptions &o) const {
  policy(o);stop(o);validate();
  pbr_write.validate();toon_write.validate();
  if(pbr_write.width!=resolution()||pbr_write.height!=resolution()||toon_write.width!=resolution()||
      toon_write.height!=resolution()||coverage.size()!=size_t(resolution())*resolution())
    throw std::invalid_argument("Prepared normal write dimensions differ from their actual owning field");
  bool any=false;
  for(size_t i=0;i<coverage.size();++i){if((i&1023)==0)stop(o);any|=coverage[i]!=0;}
  if(!any){stop(o);return self_.lock();}
  if(replacements_>=o.max_replacements)throw std::length_error("Independent baked normal replacement history exceeds its bound");
  const auto pixels=size_t(resolution())*resolution();
  if(retained_bytes_>SIZE_MAX-node_bytes)throw std::length_error("Baked normal history size overflow");
  // Distinct detached input and resolved planes fit the working cap.
  // Charge retained storage after exact sharing and before publication.
  budget(retained_bytes_+node_bytes,pixels,17,o.max_working_bytes);
  auto result=std::shared_ptr<BakedNormalField>(new BakedNormalField);
  result->origin_=origin_;result->prior_=self_.lock();result->replacements_=replacements_+1;result->intrinsic_baseline_=intrinsic_baseline_;
  result->coverage_=coverage_;
  result->write_coverage_=write_coverage_&&same_bytes(coverage,*write_coverage_,o)?write_coverage_:
      std::make_shared<const std::vector<uint8_t>>(coverage.begin(),coverage.end());
  result->write_pbr_=write_image(pbr_write,write_pbr_,o);
  result->write_mtoon_=same_bytes(pbr_write.rgba,toon_write.rgba,o)?result->write_pbr_:
      write_image(toon_write,write_mtoon_,o);
  result->pbr_=compose(pbr_,*result->write_pbr_,coverage,o);
  const bool identical_roles=result->write_pbr_==result->write_mtoon_&&same_bytes(pbr_->rgba,mtoon_->rgba,o);
  result->mtoon_=identical_roles?result->pbr_:compose(mtoon_,*result->write_mtoon_,coverage,o);
  result->account_retained_storage();
  budget(result->retained_bytes_,0,1,o.max_retained_bytes);
  result->self_=result;result->validate();stop(o);return result;
}
void BakedNormalField::validate() const {
  const auto self=self_.lock();
  if(!self||self.get()!=this||!origin_||!origin_->valid_owner(origin_)||!pbr_||!mtoon_)
    throw std::invalid_argument("Incomplete owning baked-normal field");
  const auto r=origin_->before_paint()->resolution();
  for(const auto &image:{pbr_,mtoon_}){image->validate();if(image->width!=r||image->height!=r)throw std::invalid_argument("Baked normal field dimensions differ from conversion origin");}
  if(!coverage_||coverage_->size()!=size_t(r)*r||std::any_of(coverage_->begin(),coverage_->end(),[](auto x){return x!=255;})||
     replacements_>128||retained_bytes_>128*1024*1024)
    throw std::invalid_argument("Resolved baked normal field lost its independent full coverage");
  if(!prior_){
    if(replacements_||write_pbr_||write_mtoon_||write_coverage_||
       (!intrinsic_baseline_&&(pbr_!=origin_->projection()->pbr||mtoon_!=origin_->projection()->mtoon)))
      throw std::invalid_argument("Initial baked normal field differs from its actual conversion");
  }else{
    if(prior_->origin_!=origin_||replacements_!=prior_->replacements_+1||!write_pbr_||!write_mtoon_||
       !write_coverage_||write_coverage_->size()!=coverage_->size())
      throw std::invalid_argument("Baked normal replacement lost its exact input history");
  }
}
std::shared_ptr<const BakedNormalField> BakedNormalField::from_intrinsic(
    std::shared_ptr<const CanonicalNormalConversion> origin,const BakedNormalFieldOptions &o){
  policy(o);stop(o);
  if(!origin||!origin->valid_owner(origin))throw std::invalid_argument("Intrinsic baked baseline requires its genuine conversion origin");
  const auto pixels=size_t(origin->before_paint()->resolution())*origin->before_paint()->resolution();
  budget(node_bytes,pixels,9,std::min(o.max_retained_bytes,o.max_working_bytes));
  NormalPreparationOptions options;options.max_working_bytes=o.max_working_bytes;options.cancelled=o.cancelled;
  options.max_angular_error=origin->receipt().accepted_angular_error;
  auto candidate=convert_native_intrinsic_normals({origin->before_paint(),{},origin->before_material(),origin->before_deformation(),{}},options);
  auto result=std::shared_ptr<BakedNormalField>(new BakedNormalField);
  result->origin_=std::move(origin);result->pbr_=candidate.projection->pbr;result->mtoon_=candidate.projection->mtoon;
  result->coverage_=std::make_shared<const std::vector<uint8_t>>(pixels,255);
  result->account_retained_storage();result->intrinsic_baseline_=true;
  result->self_=result;result->validate();stop(o);return result;
}
} // namespace dome::detail

namespace dome::detail {
std::shared_ptr<const NormalSamplingDomain> BakedNormalField::sampling_domain(
    const std::shared_ptr<const CanonicalNormalUv> &uv,const std::function<bool()> &cancelled) const {
  if(cancelled&&cancelled())throw std::runtime_error("Converted normal sampling cancelled");
  validate();
  if(!uv||!uv->valid_owner(uv)||uv->origin()!=origin_)
    throw std::invalid_argument("Normal sampling requires the exact field conversion owner");
  uv->validate_current(*uv->mesh(),cancelled);
  auto domain=std::make_shared<NormalSamplingDomain>();
  domain->resolution=resolution();domain->coordinates=uv->coordinates();domain->indices=uv->mesh()->indices;
  domain->resolved_pbr=pbr_;domain->resolved_mtoon=mtoon_;
  domain->validate(cancelled);
  return domain;
}
} // namespace dome::detail
