// SPDX-License-Identifier: GPL-3.0-or-later
#include "baked-normal-archive.hpp"
#include "dome/source-maps.hpp"
#include "paint-archive-shape.hpp"
#include "material-archive-version.hpp"
namespace dome::detail {
thread_local BakedNormalArchive *BakedNormalArchive::current_=nullptr;
namespace {
size_t ref(const nlohmann::json &v,size_t count){
  if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>=count)
    throw std::invalid_argument("Invalid layered normal archive identity");
  return size_t(v.get<uint64_t>());
}
bool same(const Image &a,const Image &b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;}
}
BakedNormalArchive::BakedNormalArchive(PaintImageArchive &images)
    :previous_(current_),write_images_(&images),images_(images){current_=this;}
BakedNormalArchive::BakedNormalArchive(const nlohmann::json *input,const PaintImageArchive &images)
    :previous_(current_),images_(images),input_(input){
  if(input_){
    origins_.resize(input_->at("origins").size());fields_.resize(input_->at("fields").size());
    origin_busy_.resize(origins_.size());field_busy_.resize(fields_.size());
    origin_used_.resize(origins_.size());field_used_.resize(fields_.size());
    if(input_->at("version")==2){uv_.resize(input_->at("uv").size());uv_used_.resize(uv_.size());}
  }
  current_=this;
}
BakedNormalArchive::~BakedNormalArchive(){current_=previous_;}
BakedNormalArchive &BakedNormalArchive::writer(){
  if(!current_||!current_->write_images_)throw std::invalid_argument("Independent normal persistence requires its owning Scene33 writer");
  return *current_;
}
BakedNormalArchive &BakedNormalArchive::reader(){
  if(!current_||!current_->input_)throw std::invalid_argument("Independent normals require their actual Scene33 archive");
  return *current_;
}
size_t BakedNormalArchive::preflight(const nlohmann::json &j,const nlohmann::json &material){
  if(!j.is_object()||!j.at("version").is_number_integer()||
     !((j.at("version")==1&&j.size()==3)||(j.at("version")==2&&j.size()==4))||!j.at("origins").is_array()||j.at("origins").empty()||
     j.at("origins").size()>256||!j.at("fields").is_array()||j.at("fields").size()>256||!material.at("paints").is_array())
    throw std::invalid_argument("Invalid Scene33 layered normal graph");
  size_t retained=0;
  const SourcePaintDecodeContext context{false,physical_material_archive(material)};
  auto raw=[&](const nlohmann::json &c)->const nlohmann::json &{return paint_archive_payload(material.at("paints").at(ref(c.at("paint"),material.at("paints").size())),false,context);};
  for(const auto &c:j.at("origins")){
    if(!c.is_object()||c.size()!=7||!c.at("version").is_number_integer()||c.at("version")!=1||!c.at("receipt").is_object()||!c.at("frames").is_object())
      throw std::invalid_argument("Unsupported canonical attribute origin record");
    const auto &paint=raw(c);
    if(!paint.is_object()||paint.contains("version")||paint.contains("normal_projection")||!paint.contains("baked")||
       paint.at("baked").at("version")!=1||!paint.at("baked").at("normal").is_binary()||paint.at("baked").at("normal").get_binary().empty())
      throw std::invalid_argument("Layered normal origin must reference its actual raw native canonical paint");
  }
  std::vector<uint32_t> depth;depth.reserve(j.at("fields").size());
  for(size_t i=0;i<j.at("fields").size();++i){const auto &f=j.at("fields")[i];
    if(!f.is_object()||f.size()!=9||!f.at("version").is_number_integer()||f.at("version")!=1||!f.at("baseline").is_boolean()||!f.at("write_coverage").is_binary())
      throw std::invalid_argument("Invalid layered normal field record");
    const auto &c=j.at("origins").at(ref(f.at("origin"),j.at("origins").size()));
    const auto &paint=raw(c);
    const auto &r=paint.at("resolution");
    if(!r.is_number_integer()||r.get<int64_t>()<64||r.get<int64_t>()>2048)throw std::invalid_argument("Invalid layered normal grid");
    const size_t pixels=r.get<size_t>()*r.get<size_t>();
    const size_t bytes_per_node=sizeof(BakedNormalField)+1024+pixels*18;
    if(bytes_per_node>256*1024*1024-retained)throw std::length_error("Scene33 normal history exceeds aggregate retained budget");
    retained+=bytes_per_node;
    const auto bytes=f.at("write_coverage").get_binary().size();
    if(f.at("prior").is_null()){
      if(!f.at("write_pbr").is_null()||!f.at("write_mtoon").is_null()||bytes)throw std::invalid_argument("Initial field cannot carry fabricated normal-write history");
      depth.push_back(0);
    }else{
      const auto prior=ref(f.at("prior"),i);const auto &p=j.at("fields")[prior];
      if(p.at("origin")!=f.at("origin")||p.at("baseline")!=f.at("baseline")||bytes!=pixels||depth[prior]>=128)
        throw std::invalid_argument("Layered normal replacement lost its earlier source/grid authority");
      depth.push_back(depth[prior]+1);
    }
  }
  if(j.at("version")==2){
    if(!canonical_uv_material_archive(material)||!j.at("uv").is_array()||j.at("uv").empty()||j.at("uv").size()>256)
      throw std::invalid_argument("Normal UV pool requires actual bounded material16 correspondence");
    for(const auto &u:j.at("uv")){
      if(!u.is_object()||u.size()!=3||u.at("version")!=1)
        throw std::invalid_argument("Invalid converted normal UV record");
      ref(u.at("origin"),j.at("origins").size());
      const auto bytes=SourceNormalUvLayout::encoded_storage_bytes(u.at("layout"));
      // Include retained current mesh, root-coordinate stream and decode/compose scratch.
      if(retained>256*1024*1024-4096||bytes>(256*1024*1024-4096-retained)/8)
        throw std::length_error("Converted normal UV pool exceeds prospective aggregate budget");
      retained+=bytes*8+4096;
    }
  }else if(canonical_uv_material_archive(material))throw std::invalid_argument("Material16 requires actual normal UV pool version2");
  return retained;
}
uint32_t BakedNormalArchive::add_origin(std::shared_ptr<const CanonicalNormalConversion> c){
  if(!write_images_||!c||!c->valid_owner(c))throw std::invalid_argument("Missing actual canonical attribute owner");
  auto at=std::find(origins_.begin(),origins_.end(),c);if(at!=origins_.end())return uint32_t(at-origins_.begin());
  if(origins_.size()>=256)throw std::length_error("Canonical attribute pool exceeds node budget");
  c->validate();origins_.push_back(std::move(c));return uint32_t(origins_.size()-1);
}
uint32_t BakedNormalArchive::add_field(std::shared_ptr<const BakedNormalField> f){
  if(!write_images_||!f)throw std::invalid_argument("Missing actual baked normal field");
  auto at=std::find(fields_.begin(),fields_.end(),f);if(at!=fields_.end())return uint32_t(at-fields_.begin());
  f->validate();add_origin(f->origin());if(f->prior())add_field(f->prior());
  if(fields_.size()>=256)throw std::length_error("Baked normal pool exceeds node budget");
  fields_.push_back(std::move(f));return uint32_t(fields_.size()-1);
}
uint32_t BakedNormalArchive::add_uv(std::shared_ptr<const CanonicalNormalUv> uv){
  if(!write_images_||!uv||!uv->valid_owner(uv))throw std::invalid_argument("Missing actual converted normal UV owner");
  auto at=std::find(uv_.begin(),uv_.end(),uv);if(at!=uv_.end())return uint32_t(at-uv_.begin());
  if(uv_.size()>=256)throw std::length_error("Converted normal UV pool exceeds node budget");
  uv->validate_current(*uv->mesh());add_origin(uv->origin());uv_.push_back(std::move(uv));return uint32_t(uv_.size()-1);
}
std::shared_ptr<const CanonicalNormalUv> BakedNormalArchive::uv_at(uint32_t id){
  if(!input_||id>=uv_.size())throw std::invalid_argument("Invalid converted normal UV pool identity");
  uv_used_[id]=1;if(uv_[id])return uv_[id];const auto &j=input_->at("uv")[id];
  const auto origin=origin_at(uint32_t(ref(j.at("origin"),origins_.size())));
  const auto layout=SourceNormalUvLayout::decode(j.at("layout"));
  auto mesh=layout->target_mesh(origin->before_paint()->mesh());
  auto replay=CanonicalNormalUv::create(origin,mesh,layout->source_vertices);
  if(replay->layout()->encode()!=j.at("layout"))throw std::invalid_argument("Converted normal UV differs from actual owning source replay");
  uv_[id]=std::move(replay);return uv_[id];
}
void BakedNormalArchive::register_paint(const std::shared_ptr<const PaintedMesh> &paint,uint32_t id){
  const auto [it,inserted]=paint_ids_.emplace(paint.get(),id);
  if(!inserted&&it->second!=id)throw std::logic_error("One original paint acquired conflicting archive identities");
}
void BakedNormalArchive::paint_resolver(std::function<std::shared_ptr<const PaintedMesh>(uint32_t)> resolve){paint_resolver_=std::move(resolve);}
nlohmann::json BakedNormalArchive::encode(){
  if(!write_images_)throw std::logic_error("Cannot write a decoded normal pool");
  nlohmann::json j={{"version",1},{"origins",nlohmann::json::array()},{"fields",nlohmann::json::array()}};
  for(const auto &c:origins_){
    if(c->before_material())throw std::invalid_argument("Original typed canonical material requires its complete owning pool adapter");
    const auto p=paint_ids_.find(c->before_paint().get());if(p==paint_ids_.end())throw std::logic_error("Canonical origin lacks its original material Paint identity");
    NormalFrameArchive frames;frames.add(c->frames());
    j["origins"].push_back({{"version",1},{"paint",p->second},{"receipt",c->encode()},
      {"deformation",c->before_deformation()?c->before_deformation()->encode(c->before_paint()->mesh()):nlohmann::json(nullptr)},
      {"frames",frames.encode()},{"pbr",write_images_->add(c->projection()->pbr)},{"mtoon",write_images_->add(c->projection()->mtoon)}});
  }
  for(const auto &f:fields_){
    const auto origin=std::find(origins_.begin(),origins_.end(),f->origin());
    const auto prior=std::find(fields_.begin(),fields_.end(),f->prior());
    j["fields"].push_back({{"version",1},{"origin",size_t(origin-origins_.begin())},
      {"prior",f->prior()?nlohmann::json(size_t(prior-fields_.begin())):nlohmann::json(nullptr)},
      {"baseline",f->intrinsic_baseline_},{"pbr",write_images_->add(f->pbr_)},{"mtoon",write_images_->add(f->mtoon_)},
      {"write_pbr",f->write_pbr_?nlohmann::json(write_images_->add(f->write_pbr_)):nlohmann::json(nullptr)},
      {"write_mtoon",f->write_mtoon_?nlohmann::json(write_images_->add(f->write_mtoon_)):nlohmann::json(nullptr)},
      {"write_coverage",nlohmann::json::binary(f->write_coverage_)}});
  }
  if(!uv_.empty()){
    j["version"]=2;j["uv"]=nlohmann::json::array();
    for(const auto &uv:uv_){auto origin=std::find(origins_.begin(),origins_.end(),uv->origin());
      if(origin==origins_.end())throw std::logic_error("Normal UV owner lacks its actual root pool identity");
      j["uv"].push_back({{"version",1},{"origin",size_t(origin-origins_.begin())},{"layout",uv->layout()->encode()}});
    }
  }
  return j;
}
std::shared_ptr<const CanonicalNormalConversion> BakedNormalArchive::origin_at(uint32_t id){
  if(!input_||id>=origins_.size())throw std::invalid_argument("Invalid Scene33 attribute origin identity");
  origin_used_[id]=1;if(origins_[id])return origins_[id];
  if(origin_busy_[id]||!paint_resolver_)throw std::invalid_argument("Canonical origin has cyclic or missing original paint authority");
  origin_busy_[id]=1;const auto &j=input_->at("origins")[id];const auto paint=paint_resolver_(uint32_t(j.at("paint").get<uint32_t>()));
  std::shared_ptr<const DeformationState> deformation;
  if(!j.at("deformation").is_null())deformation=std::make_shared<const DeformationState>(DeformationState::decode(j.at("deformation"),paint->mesh()));
  auto frames=NormalFrameArchive::decode(j.at("frames"));if(frames.sets().size()!=1)throw std::invalid_argument("Canonical origin requires one actual complete frame set");
  auto projection=std::make_shared<PaintNormalProjection>();projection->pbr=images_.at(uint32_t(ref(j.at("pbr"),images_.size())));projection->mtoon=images_.at(uint32_t(ref(j.at("mtoon"),images_.size())));
  origins_[id]=CanonicalNormalConversion::restore(j.at("receipt"),{paint,{},{},deformation,{},{}},frames.at(0),std::move(projection));
  origin_busy_[id]=0;return origins_[id];
}
std::shared_ptr<const BakedNormalField> BakedNormalArchive::field_at(uint32_t id){
  if(!input_||id>=fields_.size())throw std::invalid_argument("Invalid Scene33 normal field identity");
  field_used_[id]=1;if(fields_[id])return fields_[id];
  if(field_busy_[id])throw std::invalid_argument("Cyclic normal field replay");
  field_busy_[id]=1;
  const auto &j=input_->at("fields")[id];const auto origin=origin_at(uint32_t(ref(j.at("origin"),origins_.size())));
  std::shared_ptr<const BakedNormalField> replay;
  if(j.at("prior").is_null())replay=j.at("baseline").get<bool>()?BakedNormalField::from_intrinsic(origin):BakedNormalField::from_canonical(origin);
  else{
    const auto prior=field_at(uint32_t(ref(j.at("prior"),id)));
    const auto pbr=images_.at(uint32_t(ref(j.at("write_pbr"),images_.size()))),toon=images_.at(uint32_t(ref(j.at("write_mtoon"),images_.size())));
    replay=prior->replace_planes(*pbr,*toon,j.at("write_coverage").get_binary(),{});
    if(replay==prior)throw std::invalid_argument("Redundant normal write cannot mint a new field identity");
  }
  const auto pbr=images_.at(uint32_t(ref(j.at("pbr"),images_.size()))),toon=images_.at(uint32_t(ref(j.at("mtoon"),images_.size())));
  if(!same(*replay->pbr(),*pbr)||!same(*replay->mtoon(),*toon)||replay->origin()!=origin||replay->intrinsic_baseline()!=j.at("baseline").get<bool>())
    throw std::invalid_argument("Persisted baked normals differ from actual owning field replay");
  // The private factories allocated a mutable object; publish archive image
  // owners only after complete replay equality, before any caller can see it.
  auto adopted=std::const_pointer_cast<BakedNormalField>(replay);adopted->pbr_=pbr;adopted->mtoon_=toon;
  fields_[id]=std::move(adopted);field_busy_[id]=0;return fields_[id];
}
void BakedNormalArchive::require_reachable() const {
  if(input_&&(std::find(uv_used_.begin(),uv_used_.end(),0)!=uv_used_.end()||std::find(origin_used_.begin(),origin_used_.end(),0)!=origin_used_.end()||std::find(field_used_.begin(),field_used_.end(),0)!=field_used_.end()))
    throw std::invalid_argument("Scene33 contains unreachable normal ownership records");
}
} // namespace dome::detail
