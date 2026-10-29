// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-maps.hpp"
#include "dome/paint.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
namespace {
auto slots(const PaintSourceMaps &m) { return std::array{&m.color,&m.metal_rough,&m.emission,&m.normal,&m.occlusion}; }
void limits_valid(const PaintImageLimits &l) {
  if (!l.max_images || l.max_images>4096 || !l.max_image_pixels || l.max_image_pixels>64*1024*1024 ||
      !l.max_unique_pixels || l.max_unique_pixels>128*1024*1024)
    throw std::invalid_argument("Invalid original texture resource limits");
}
bool wrap_valid(int w){return w==10497||w==33648||w==33071;}
float wrap(double v,int mode) {
  if(mode==33071)return float(std::clamp(v,0.,1.));
  if(mode==33648){v=std::fmod(v,2.);if(v<0)v+=2.;return float(v<=1?v:2-v);}
  return float(v-std::floor(v));
}
int address(int p,int n,int mode){
  if(mode==33071)return std::clamp(p,0,n-1);
  if(mode==33648){int period=2*n;p%=period;if(p<0)p+=period;return p<n?p:period-1-p;}
  p%=n;return p<0?p+n:p;
}
}
void PaintSourceTexture::validate() const {
  if(!image)return;
  image->validate();
  if(!wrap_valid(wrap_s)||!wrap_valid(wrap_t)||
     (mag_filter!=0&&mag_filter!=9728&&mag_filter!=9729)||
     (min_filter!=0&&min_filter!=9728&&min_filter!=9729&&(min_filter<9984||min_filter>9987)))
    throw std::invalid_argument("Unsupported original texture sampler");
  if(texcoord!=0)throw std::invalid_argument("Original paint maps currently require TEXCOORD_0");
  for(float v:uv_transform)if(!std::isfinite(v)||std::abs(v)>10000)
    throw std::invalid_argument("Invalid original texture UV transform");
}
bool PaintSourceMaps::empty() const noexcept {
  for(auto s:slots(*this))if(s->image)return false;
  return true;
}
size_t PaintSourceMaps::unique_pixels() const {
  size_t total=0;std::set<const Image*> seen;
  for(auto s:slots(*this))if(s->image&&seen.insert(s->image.get()).second)
    total+=size_t(s->image->width)*s->image->height;
  return total;
}
void PaintSourceMaps::validate(const PaintImageLimits &l) const {
  limits_valid(l);PaintImageArchive images(l);
  for(auto s:slots(*this)){s->validate();if(s->image)images.add(s->image);}
  if(!std::isfinite(normal_scale)||normal_scale<0||
     !std::isfinite(occlusion_strength)||occlusion_strength<0||occlusion_strength>1)
    throw std::invalid_argument("Invalid original normal or occlusion strength");
}
PaintImageArchive::PaintImageArchive(PaintImageLimits l):limits_(l){limits_valid(l);}
uint32_t PaintImageArchive::add(std::shared_ptr<const Image> image){
  if(!image)throw std::invalid_argument("Missing original texture image");
  for(size_t i=0;i<images_.size();++i)if(images_[i].get()==image.get())return uint32_t(i);
  image->validate();size_t pixels=size_t(image->width)*image->height;
  if(pixels>limits_.max_image_pixels||pixels>limits_.max_unique_pixels-pixels_||images_.size()>=limits_.max_images)
    throw std::length_error("Original texture image budget exceeded");
  images_.push_back(std::move(image));pixels_+=pixels;return uint32_t(images_.size()-1);
}
const std::shared_ptr<const Image>& PaintImageArchive::at(uint32_t i)const{
  if(i>=images_.size())throw std::out_of_range("Invalid original texture image reference");
  return images_[i];
}
nlohmann::json PaintImageArchive::encode()const{
  auto result=nlohmann::json::array();
  for(auto &i:images_)result.push_back({{"width",i->width},{"height",i->height},{"rgba",nlohmann::json::binary(i->rgba)}});
  return result;
}
PaintImageArchive PaintImageArchive::decode(const nlohmann::json &j,PaintImageLimits l){
  PaintImageArchive result(l);
  if(!j.is_array()||j.size()>l.max_images)throw std::length_error("Invalid original texture archive size");
  for(auto &v:j){
    auto dimension=[&](const char *k){
      auto &d=v.at(k);
      if(!d.is_number_integer()||d.get<int64_t>()<1||d.get<uint64_t>()>8192) {
        throw std::runtime_error("Invalid original texture dimensions");
      }
      return d.get<uint32_t>();
    };
    uint32_t w=dimension("width"),h=dimension("height");size_t pixels=size_t(w)*h;
    auto &data=v.at("rgba");
    if(pixels>l.max_image_pixels||pixels>l.max_unique_pixels-result.pixels_||!data.is_binary()||data.get_binary().size()!=pixels*4)
      throw std::length_error("Invalid original texture archive payload");
    auto image=std::make_shared<Image>();image->width=w;image->height=h;
    image->rgba.assign(data.get_binary().begin(),data.get_binary().end());result.add(std::move(image));
  }return result;
}
nlohmann::json encode_source_maps(const PaintSourceMaps&m,PaintImageArchive&a){
  m.validate();auto j=nlohmann::json::object();const char *names[]={"color","metal_rough","emission","normal","occlusion"};size_t k=0;
  for(auto s:slots(m)){auto name=names[k++];if(s->image)j[name]={{"image",a.add(s->image)},{"wrap_s",s->wrap_s},{"wrap_t",s->wrap_t},
    {"min_filter",s->min_filter},{"mag_filter",s->mag_filter},{"texcoord",s->texcoord},{"uv_transform",s->uv_transform}};}
  j["normal_scale"]=m.normal_scale;j["occlusion_strength"]=m.occlusion_strength;return j;
}
std::shared_ptr<const PaintSourceMaps> decode_source_maps(const nlohmann::json &j,const PaintImageArchive&a){
  auto m=std::make_shared<PaintSourceMaps>();const char *names[]={"color","metal_rough","emission","normal","occlusion"};size_t k=0;
  for(auto s:{&m->color,&m->metal_rough,&m->emission,&m->normal,&m->occlusion}){auto name=names[k++];if(!j.contains(name))continue;
    auto &v=j.at(name),&id=v.at("image");if(!id.is_number_integer()||id.get<int64_t>()<0||id.get<uint64_t>()>UINT32_MAX)
      throw std::runtime_error("Invalid original texture archive reference");
    auto integer=[&](const char *key){
      auto &value=v.at(key);
      if(!value.is_number_integer() || (value.is_number_unsigned()?value.get<uint64_t>()>INT32_MAX:
          value.get<int64_t>()<0||value.get<int64_t>()>INT32_MAX))
        throw std::runtime_error("Invalid original texture integer metadata");
      return value.get<int>();
    };
    s->image=a.at(id.get<uint32_t>());s->wrap_s=integer("wrap_s");s->wrap_t=integer("wrap_t");
    s->min_filter=integer("min_filter");s->mag_filter=integer("mag_filter");s->texcoord=integer("texcoord");
    s->uv_transform=v.at("uv_transform").get<std::array<float,6>>();
  }m->normal_scale=j.at("normal_scale").get<float>();m->occlusion_strength=j.at("occlusion_strength").get<float>();m->validate();return m;
}
std::array<float,4> sample_source_texture(const PaintSourceTexture&t,float u,float v,bool srgb){
  if(!t.image)return {1,1,1,1};
  t.validate();
  if(!std::isfinite(u)||!std::isfinite(v))throw std::invalid_argument("Invalid original texture sample coordinate");
  auto &m=t.uv_transform;double tu=double(m[0])*u+double(m[1])*v+m[4],tv=double(m[2])*u+double(m[3])*v+m[5];
  auto &im=*t.image;float x=wrap(tu,t.wrap_s)*im.width-.5f,y=wrap(tv,t.wrap_t)*im.height-.5f;
  bool nearest=t.mag_filter==9728;int bx=int(std::floor(x)),by=int(std::floor(y));float fx=x-bx,fy=y-by;
  if(nearest){bx=int(std::floor(x+.5f));by=int(std::floor(y+.5f));fx=fy=0;}
  std::array<float,4> result{};
  for(int k=0;k<(nearest?1:4);++k){int sx=address(bx+(k&1),int(im.width),t.wrap_s),sy=address(by+(k>>1),int(im.height),t.wrap_t);
    float weight=nearest?1:((k&1)?fx:1-fx)*((k>>1)?fy:1-fy);size_t at=(size_t(sy)*im.width+sx)*4;
    for(int c=0;c<4;++c){float value=im.rgba[at+c]/255.f;if(srgb&&c<3)value=srgb_to_linear(value);result[c]+=weight*value;}
  }return result;
}
} // namespace dome
