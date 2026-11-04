// SPDX-License-Identifier: GPL-3.0-or-later
#include "baked-normal-field.hpp"
namespace dome::detail {
namespace {
void policy(const BakedNormalFieldOptions &o){
  if(!o.max_working_bytes||o.max_working_bytes>512*1024*1024||
     !o.max_retained_bytes||o.max_retained_bytes>128*1024*1024||
     !o.max_replacements||o.max_replacements>128)
    throw std::invalid_argument("Invalid independent baked-normal field policy");
}
void stop(const BakedNormalFieldOptions &o){
  if(o.cancelled&&o.cancelled())throw std::runtime_error("Baked-normal field transition cancelled");
}
void budget(size_t prior,size_t pixels,size_t bytes,const BakedNormalFieldOptions &o){
  if(prior>o.max_retained_bytes||pixels>(o.max_retained_bytes-prior)/bytes||
     prior>o.max_working_bytes||pixels>(o.max_working_bytes-prior)/bytes)
    throw std::length_error("Independent baked-normal field exceeds its retained/working budget");
}
uint8_t encode(float x){return uint8_t(std::lround(std::clamp(x,0.f,1.f)*255));}
}
std::shared_ptr<const BakedNormalField> BakedNormalField::from_canonical(
    std::shared_ptr<const CanonicalNormalConversion> origin,const BakedNormalFieldOptions &o){
  policy(o);stop(o);
  if(!origin||!origin->valid_owner(origin))throw std::invalid_argument("Baked normal field needs its actual canonical conversion owner");
  origin->validate();const auto projection=origin->projection();const auto r=projection->pbr->width;
  const auto pixels=size_t(r)*r;
  budget(sizeof(BakedNormalField),pixels,9,o);
  auto result=std::shared_ptr<BakedNormalField>(new BakedNormalField);
  result->origin_=std::move(origin);result->pbr_=projection->pbr;result->mtoon_=projection->mtoon;
  result->coverage_.assign(pixels,255);
  // Conservative resident accounting counts both role images even if aliased.
  result->retained_bytes_=sizeof(BakedNormalField)+pixels*9;
  result->self_=result;result->validate();stop(o);return result;
}
std::shared_ptr<const BakedNormalField> BakedNormalField::replace(
    const BakedTextures &incoming,const BakedNormalFieldOptions &o) const {
  policy(o);stop(o);validate();incoming.validate(resolution());
  if(incoming.normal_interpretation!=BakedNormalInterpretation::PreparedAttributes||
     incoming.maps.normal.rgba.empty()||incoming.maps.mtoon_normal.rgba.empty()||incoming.normal_authority)
    throw std::invalid_argument("Independent baked field requires complete newly encoded prepared normal roles");
  bool any=false;
  for(size_t i=0;i<incoming.coverage.size();++i){if((i&1023)==0)stop(o);any|=incoming.coverage[i]!=0;}
  if(!any){stop(o);return self_.lock();}
  if(replacements_>=o.max_replacements)throw std::length_error("Independent baked normal replacement history exceeds its bound");
  const auto pixels=size_t(resolution())*resolution();
  if(retained_bytes_>SIZE_MAX-sizeof(BakedNormalField))throw std::length_error("Baked normal history size overflow");
  const auto retained=retained_bytes_+sizeof(BakedNormalField);
  budget(retained,pixels,18,o);
  auto result=std::shared_ptr<BakedNormalField>(new BakedNormalField);
  result->origin_=origin_;result->prior_=self_.lock();result->replacements_=replacements_+1;
  result->write_pbr_=std::make_shared<const Image>(incoming.maps.normal);
  result->write_mtoon_=std::make_shared<const Image>(incoming.maps.mtoon_normal);
  result->write_coverage_=incoming.coverage;
  auto pbr=std::make_shared<Image>(*pbr_),toon=std::make_shared<Image>(*mtoon_);
  result->coverage_=coverage_;
  for(size_t i=0;i<pixels;++i){
    if((i&1023)==0)stop(o);
    if(!result->write_coverage_[i])continue; // Preserve missed texels byte-exactly.
    const float a=float(result->write_coverage_[i])/255;
    for(auto pair:{std::pair{pbr.get(),result->write_pbr_.get()},std::pair{toon.get(),result->write_mtoon_.get()}}){
      Vec3 before,after;
      before={float(pair.first->rgba[i*4])/255*2-1,float(pair.first->rgba[i*4+1])/255*2-1,float(pair.first->rgba[i*4+2])/255*2-1};
      after={float(pair.second->rgba[i*4])/255*2-1,float(pair.second->rgba[i*4+1])/255*2-1,float(pair.second->rgba[i*4+2])/255*2-1};
      auto value=before*(1-a)+after*a;
      value=length(value)>1e-8f?normalized(value):Vec3{0,0,1};
      pair.first->rgba[i*4]=encode(value.x*.5f+.5f);
      pair.first->rgba[i*4+1]=encode(value.y*.5f+.5f);
      pair.first->rgba[i*4+2]=encode(value.z*.5f+.5f);pair.first->rgba[i*4+3]=255;
    }
  }
  result->pbr_=std::move(pbr);result->mtoon_=std::move(toon);
  result->retained_bytes_=retained+pixels*18;
  result->self_=result;result->validate();stop(o);return result;
}
void BakedNormalField::validate() const {
  const auto self=self_.lock();
  if(!self||self.get()!=this||!origin_||!origin_->valid_owner(origin_)||!pbr_||!mtoon_)
    throw std::invalid_argument("Incomplete owning baked-normal field");
  const auto r=origin_->before_paint()->resolution();
  for(const auto &image:{pbr_,mtoon_}){image->validate();if(image->width!=r||image->height!=r)throw std::invalid_argument("Baked normal field dimensions differ from conversion origin");}
  if(coverage_.size()!=size_t(r)*r||std::any_of(coverage_.begin(),coverage_.end(),[](auto x){return x!=255;})||
     replacements_>128||retained_bytes_>128*1024*1024)
    throw std::invalid_argument("Resolved baked normal field lost its independent full coverage");
  if(!prior_){
    if(replacements_||write_pbr_||write_mtoon_||!write_coverage_.empty()||
       pbr_!=origin_->projection()->pbr||mtoon_!=origin_->projection()->mtoon)
      throw std::invalid_argument("Initial baked normal field differs from its actual conversion");
  }else{
    if(prior_->origin_!=origin_||replacements_!=prior_->replacements_+1||!write_pbr_||!write_mtoon_||write_coverage_.size()!=coverage_.size())
      throw std::invalid_argument("Baked normal replacement lost its exact input history");
  }
}
} // namespace dome::detail
