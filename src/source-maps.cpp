// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-maps.hpp"
#include "dome/paint.hpp"
#include "source-uv-archive.hpp"
#include <nlohmann/json.hpp>
#include <set>
#include <bit>
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
void PaintSourceTexture::validate(unsigned coordinate_count) const {
  if(coordinate_count<1||coordinate_count>8)throw std::invalid_argument("Invalid original texture coordinate authority");
  if(!image)return;
  image->validate();
  if(!wrap_valid(wrap_s)||!wrap_valid(wrap_t)||
     (mag_filter!=0&&mag_filter!=9728&&mag_filter!=9729)||
     (min_filter!=0&&min_filter!=9728&&min_filter!=9729&&(min_filter<9984||min_filter>9987)))
    throw std::invalid_argument("Unsupported original texture sampler");
  if(texcoord<0||unsigned(texcoord)>=coordinate_count)throw std::invalid_argument("Original texture coordinate set requires explicit surface authority");
  for(float v:uv_transform)if(!std::isfinite(v)||std::abs(v)>10000)
    throw std::invalid_argument("Invalid original texture UV transform");
}
bool PaintSourceMaps::uses_uv1() const noexcept {
  for(auto slot:slots(*this))if(slot->image&&slot->texcoord==1)return true;
  return false;
}
bool PaintSourceMaps::uses_alternate_uv() const noexcept {
  for(auto slot:slots(*this))if(slot->image&&slot->texcoord!=0)return true;
  return false;
}
unsigned PaintSourceMaps::coordinate_count() const noexcept {return extra_uv_sets.empty()?(uv1?2u:1u):8u;}
void PaintSourceMaps::validate_surface(const Mesh &mesh) const {
  validate();
  if(uv1&&uv1->size()!=mesh.positions.size())throw std::invalid_argument("Source UV1 vertex domain differs from the mesh");
  for(const auto &[set,coordinates]:extra_uv_sets)if(coordinates->size()!=mesh.positions.size())throw std::invalid_argument("Extra source UV vertex domain differs from the mesh");
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
  if(uses_uv1()!=bool(uv1))throw std::invalid_argument("Source UV1 views require a paired coordinate owner");
  if(uv1){
    if(uv1->empty()||uv1->size()>2'000'000)throw std::length_error("Source UV1 coordinate budget exceeded");
    for(const auto &q:*uv1)for(float v:q)if(!std::isfinite(v)||std::abs(v)>10000)throw std::invalid_argument("Invalid original UV1 coordinate");
  }
  if(extra_uv_sets.size()>6)throw std::length_error("Extra source UV set count exceeded");
  for(const auto &[set,coordinates]:extra_uv_sets){
    if(set<2||set>7||!coordinates||coordinates->empty()||coordinates->size()>2'000'000)throw std::invalid_argument("Invalid extra source UV coordinate owner");
    bool used=false;for(auto slot:slots(*this))used|=slot->image&&slot->texcoord==int(set);
    if(!used)throw std::invalid_argument("Unreferenced extra source UV coordinate owner");
    for(const auto &q:*coordinates)for(float value:q)if(!std::isfinite(value)||std::abs(value)>10000)throw std::invalid_argument("Invalid extra source UV coordinate");
  }
  for(auto s:slots(*this)){
    s->validate(coordinate_count());
    if(s->image){if(s->texcoord>=2&&!extra_uv_sets.contains(uint32_t(s->texcoord)))throw std::invalid_argument("Extra source UV sampler lacks its exact coordinate owner");images.add(s->image);}
  }
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
  if(m.uv1){
    std::vector<uint8_t> bytes;bytes.reserve(m.uv1->size()*8);
    for(const auto &q:*m.uv1)for(float value:q){auto bits=std::bit_cast<uint32_t>(value);for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(uint8_t(bits>>shift));}
    j["source_uv1"]={{"version",1},{"vertices",m.uv1->size()},{"data",nlohmann::json::binary(std::move(bytes))}};
  }
  j["normal_scale"]=m.normal_scale;j["occlusion_strength"]=m.occlusion_strength;
  if(m.extra_uv_sets.empty())return j;
  auto sets=nlohmann::json::array();
  for(const auto &[set,coordinates]:m.extra_uv_sets){
    std::vector<uint8_t> bytes;bytes.reserve(coordinates->size()*8);
    for(const auto &q:*coordinates)for(float value:q){auto bits=std::bit_cast<uint32_t>(value);for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(uint8_t(bits>>shift));}
    sets.push_back({{"set",set},{"vertices",coordinates->size()},{"data",nlohmann::json::binary(std::move(bytes))}});
  }
  // An envelope makes legacy standalone decoders reject before adopting any
  // extra-coordinate source payload; ordinary UV0/1 output remains byte-exact.
  return {{"version",2},{"payload",std::move(j)},{"extra_uv_sets",std::move(sets)}};
}
std::shared_ptr<const PaintSourceMaps> decode_source_maps(const nlohmann::json &encoded,const PaintImageArchive&a){
  detail::source_uv_archive_measure(encoded);
  const bool extended=encoded.contains("version");
  if(extended&&(!encoded.is_object()||encoded.size()!=3||!encoded.at("version").is_number_integer()||encoded.at("version").get<int64_t>()!=2||!encoded.at("payload").is_object()||!encoded.at("extra_uv_sets").is_array()||encoded.at("extra_uv_sets").empty()||encoded.at("extra_uv_sets").size()>6))throw std::invalid_argument("Invalid extra source UV envelope");
  const auto &j=extended?encoded.at("payload"):encoded;
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
  }
  if(j.contains("source_uv1")){
    const auto &v=j.at("source_uv1");const auto &count=v.at("vertices"),&data=v.at("data");
    if(!v.is_object()||v.size()!=3||!v.at("version").is_number_integer()||v.at("version")!=1||!count.is_number_integer()||count.get<int64_t>()<1||count.get<uint64_t>()>2'000'000||!data.is_binary()||data.get_binary().size()!=count.get<uint64_t>()*8)throw std::invalid_argument("Invalid persisted source UV1 domain");
    auto coordinates=std::make_shared<std::vector<std::array<float,2>>>();coordinates->resize(count.get<size_t>());const auto &bytes=data.get_binary();size_t at=0;
    for(auto &q:*coordinates)for(float &value:q){uint32_t word=0;for(unsigned shift=0;shift<32;shift+=8)word|=uint32_t(bytes[at++])<<shift;value=std::bit_cast<float>(word);}
    m->uv1=std::move(coordinates);
  }
  if(extended)for(const auto &v:encoded.at("extra_uv_sets")){
    if(!v.is_object()||v.size()!=3||!v.at("set").is_number_integer()||v.at("set").get<int64_t>()<2||v.at("set").get<uint64_t>()>7||!v.at("vertices").is_number_integer()||v.at("vertices").get<int64_t>()<1||v.at("vertices").get<uint64_t>()>2'000'000||!v.at("data").is_binary()||v.at("data").get_binary().size()!=v.at("vertices").get<uint64_t>()*8)throw std::invalid_argument("Invalid persisted extra source UV coordinates");
    const auto set=v.at("set").get<uint32_t>();if(m->extra_uv_sets.contains(set))throw std::invalid_argument("Duplicate persisted extra source UV set");
    auto values=std::make_shared<std::vector<std::array<float,2>>>(v.at("vertices").get<size_t>());size_t at=0;const auto &bytes=v.at("data").get_binary();
    for(auto &q:*values)for(auto &value:q){uint32_t word=0;for(unsigned shift=0;shift<32;shift+=8)word|=uint32_t(bytes[at++])<<shift;value=std::bit_cast<float>(word);}m->extra_uv_sets.emplace(set,std::move(values));
  }
  m->normal_scale=j.at("normal_scale").get<float>();m->occlusion_strength=j.at("occlusion_strength").get<float>();m->validate();return m;
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
std::span<const std::array<float,2>> source_texture_coordinates(const Mesh &mesh,const PaintSourceMaps &maps,int coordinate){
  if(coordinate==0){if(mesh.uv.size()!=mesh.positions.size())throw std::invalid_argument("Source UV0 vertex domain is unavailable");return mesh.uv;}
  if(coordinate==1){if(!maps.uv1||maps.uv1->size()!=mesh.positions.size())throw std::invalid_argument("Source UV1 requires its exact mesh vertex domain");return *maps.uv1;}
  auto found=maps.extra_uv_sets.find(uint32_t(coordinate));
  if(coordinate<2||coordinate>7||found==maps.extra_uv_sets.end()||!found->second||found->second->size()!=mesh.positions.size())throw std::invalid_argument("Extra source UV requires its exact mesh vertex domain");
  return *found->second;
}
std::array<float,2> source_texture_uv(const Mesh &mesh,const PaintSourceMaps &maps,int coordinate,size_t triangle,std::array<float,3> bary){
  auto uv=source_texture_coordinates(mesh,maps,coordinate);std::array<float,2> result{};
  if(triangle>=mesh.indices.size()/3)throw std::out_of_range("Source UV triangle is invalid");
  float sum=0;for(float v:bary){if(!std::isfinite(v))throw std::invalid_argument("Nonfinite source UV barycentric coordinate");sum+=v;}
  if(std::abs(sum-1)>1e-4f)throw std::invalid_argument("Source UV barycentric coordinates are not normalized");
  for(size_t c=0;c<3;++c){auto id=mesh.indices[triangle*3+c];if(id>=uv.size())throw std::out_of_range("Source UV vertex identity is invalid");for(size_t k=0;k<2;++k)result[k]+=uv[id][k]*bary[c];}
  return result;
}
std::array<float,4> sample_surface_texture(const Mesh &mesh,const PaintSourceMaps &maps,const PaintSourceTexture &view,size_t triangle,std::array<float,3> bary,bool srgb){
  if(!view.image)return {1,1,1,1};
  view.validate(maps.coordinate_count());auto uv=source_texture_uv(mesh,maps,view.texcoord,triangle,bary);
  // Coordinates are now explicitly resolved; the image/filter sampler receives
  // that chosen pair, not a relabeled geometry stream.
  auto resolved=view;resolved.texcoord=0;return sample_source_texture(resolved,uv[0],uv[1],srgb);
}
Vec3 source_texture_normal(const Mesh &mesh,const PaintSourceMaps &maps,size_t triangle,
                           std::array<float,3> bary,float strength) {
  if(!std::isfinite(strength)||strength<0||strength>4)throw std::invalid_argument("Invalid source normal strength");
  std::array<uint32_t,3> ids{};Vec3 n{};
  for(size_t c=0;c<3;++c){
    ids[c]=mesh.indices.at(triangle*3+c);
    n+=normalized(mesh.normals.at(ids[c]))*bary[c];

  }
  n=normalized(n);
  if(!maps.normal.image||maps.normal_scale==0||strength==0)return n;
  auto coordinates=source_texture_coordinates(mesh,maps,maps.normal.texcoord);
  auto uv0=coordinates[ids[0]],uv1=coordinates[ids[1]],uv2=coordinates[ids[2]];
  const auto &a=maps.normal.uv_transform;
  double u1=double(uv1[0])-uv0[0],v1=double(uv1[1])-uv0[1],
         u2=double(uv2[0])-uv0[0],v2=double(uv2[1])-uv0[1];
  double x1=a[0]*u1+a[1]*v1,y1=a[2]*u1+a[3]*v1,
         x2=a[0]*u2+a[1]*v2,y2=a[2]*u2+a[3]*v2;
  double determinant=x1*y2-x2*y1,scale=std::hypot(x1,y1)*std::hypot(x2,y2);
  if(!scale||std::abs(determinant)<=scale*1e-8)return n;
  auto e1=mesh.positions[ids[1]]-mesh.positions[ids[0]],e2=mesh.positions[ids[2]]-mesh.positions[ids[0]];
  auto direction=[&](double x,double y,double z){
    double size=std::hypot(x,y,z);if(size<1e-100)return Vec3{};
    double sign=determinant<0?-1:1;return Vec3{float(x/size*sign),float(y/size*sign),float(z/size*sign)};
  };
  auto t=direction(e1.x*y2-e2.x*y1,e1.y*y2-e2.y*y1,e1.z*y2-e2.z*y1),
       raw_b=direction(e2.x*x1-e1.x*x2,e2.y*x1-e1.y*x2,e2.z*x1-e1.z*x2);
  t=t-n*dot(n,t);if(length(t)<1e-8f)return n;t=normalized(t);
  auto b=cross(n,t)*(dot(cross(n,t),raw_b)<0?-1.f:1.f);
  auto encoded=sample_surface_texture(mesh,maps,maps.normal,triangle,bary,false);
  double factor=double(maps.normal_scale)*strength;
  double dx=(encoded[0]*2-1)*factor,dy=(encoded[1]*2-1)*factor,dz=encoded[2]*2-1;
  double magnitude=std::hypot(dx,dy,dz);if(magnitude<1e-20)return n;
  Vec3 detail{float(dx/magnitude),float(dy/magnitude),float(dz/magnitude)};
  auto result=t*detail.x+b*detail.y+n*detail.z;
  return length(result)>1e-8f?normalized(result):n;
}
} // namespace dome
