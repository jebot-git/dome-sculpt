// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-archive.hpp"
#include "texture-set-paint-access.hpp"
#include "dome/source-paint.hpp"
#include <algorithm>
#include <bit>
#include <map>
namespace dome::detail {
namespace {
using Json=nlohmann::json;
uint64_t integer(const Json &v,uint64_t maximum){
 if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>maximum)
  throw std::invalid_argument("Invalid texture-set archive identity or size");
 return v.get<uint64_t>();
}
void word(std::vector<uint8_t>&v,uint64_t x,size_t bytes){for(size_t k=0;k<bytes;++k)v.push_back(uint8_t(x>>(8*k)));}
uint64_t word(const std::vector<uint8_t>&v,size_t at,size_t bytes){uint64_t x=0;for(size_t k=0;k<bytes;++k)x|=uint64_t(v.at(at+k))<<(8*k);return x;}
const std::vector<uint8_t>&blob(const Json&j,size_t stride,size_t maximum,bool empty=false){
 if(!j.is_binary()||(!empty&&j.get_binary().empty())||j.get_binary().size()%stride||j.get_binary().size()/stride>maximum)
  throw std::invalid_argument("Invalid texture-set geometry or identity buffer");
 return j.get_binary();
}
Json material(const Material&m){return {{"name",m.name},{"color",m.color},{"metallic",m.metallic},{"roughness",m.roughness},
 {"emissive",m.emissive},{"toon",m.toon},{"double_sided",m.double_sided},{"alpha",int(m.alpha)},{"alpha_cutoff",m.alpha_cutoff},{"unlit",m.unlit}};}
Material material(const Json&j){
 if(!j.is_object()||j.size()!=10)throw std::invalid_argument("Incomplete texture-set material");
 Material m{j.at("name").get<std::string>(),j.at("color").get<std::array<float,4>>(),j.at("metallic").get<float>(),
 j.at("roughness").get<float>(),j.at("emissive").get<std::array<float,3>>(),j.at("toon").get<bool>(),j.at("double_sided").get<bool>()};
 m.alpha=Material::AlphaMode(integer(j.at("alpha"),3));m.alpha_cutoff=j.at("alpha_cutoff").get<float>();m.unlit=j.at("unlit").get<bool>();m.validate();return m;
}
bool same(const std::shared_ptr<const Mesh>&a,const std::shared_ptr<const Mesh>&b){return a&&a==b&&!a.owner_before(b)&&!b.owner_before(a);}
// Initial connected codec slice. Unsupported history/normal/source owners are
// explicit development guards, never omitted from the wire. Their actual
// archive/context adapters remain required before general texture-set admission.
void plain(const PaintedMesh&p,const SourceMaterialBindings*b,const CreatorAppearance*a,const ObjectBakeRecipe*r){
 if(b||a||r||p.paint_sampling_domain()||p.source_sampling_prepared()||p.source_maps()||p.color_projection()||p.normal_projection()||
    (p.baked()&&p.baked()->normal_field))
  throw std::invalid_argument("Texture-set source/normal/recipe graph needs its owning Scene40 adapter");
}
}
TextureSetArchive::TextureSetArchive(TextureSetLimits l):limits_(std::move(l)){limits_.validate();}
void TextureSetArchive::charge(size_t count,size_t unit){if(unit&&count>(limits_.maximum_bytes-bytes_)/unit)throw std::length_error("Texture-set archive exceeds its owning byte budget");bytes_+=count*unit;}
void TextureSetArchive::step(size_t n){if(n>limits_.maximum_work-work_)throw std::length_error("Texture-set archive exceeds its work budget");work_+=n;if(limits_.cancelled&&limits_.cancelled())throw std::runtime_error("Texture-set archive cancelled");}
uint32_t TextureSetArchive::add(const std::shared_ptr<const TextureSetTopology>&t){
 if(!t||!t->geometry())throw std::invalid_argument("Texture-set archive lost its geometry owner");
 for(size_t i=0;i<topology_.size();++i){step();if(topology_[i]==t)return uint32_t(i);}
 if(topology_.size()>=limits_.maximum_sets_per_scene)throw std::length_error("Too many retained texture-set topologies");
 const auto&m=*t->geometry();step(m.positions.size()+m.indices.size());charge(m.positions.size()+m.normals.size(),sizeof(Vec3));charge(m.uv.size(),sizeof(std::array<float,2>));charge(m.indices.size(),sizeof(uint32_t));charge(m.uv_pins.size()+m.uv_seams.size(),1);charge(t->faces_.size()+t->corners_.size(),sizeof(uint64_t));
 topology_.push_back(t);return uint32_t(topology_.size()-1);
}
Json TextureSetArchive::encode(const SculptObject&o,PaintImageArchive&images,NormalSamplingArchive*normals,PaintSamplingArchive*sampling){
 if(!o.texture_sets)throw std::invalid_argument("Texture-set archive requires actual collection membership");
 const auto&c=*o.texture_sets;c.validate(o,true,limits_);
 if(c.additional_.size()+1>limits_.maximum_sets_per_scene-sets_)throw std::length_error("Texture-set archive exceeds aggregate Scene set count");sets_+=c.additional_.size()+1;
 plain(*o.painted,o.material_bindings.get(),o.appearance.get(),o.bake_recipe.get());for(const auto&r:c.additional_)plain(*r.painted,r.material_bindings.get(),r.appearance.get(),r.bake_recipe.get());
 const auto id=add(c.topology_);std::vector<uint8_t>slots;slots.reserve(c.slots_->size()*4);for(auto x:*c.slots_){step();word(slots,x,4);}
 Json j={{"version",1},{"topology",id},{"active",c.active_},{"next_id",c.next_},{"slots",Json::binary(std::move(slots))},
 {"primary",TextureSetPaintAccess::encode(*o.painted,&images,normals,sampling)},{"additional",Json::array()}};
 for(const auto&r:c.additional_){step();j["additional"].push_back({{"id",r.id},{"name",r.name},{"material",material(r.material)},
 {"painted",TextureSetPaintAccess::encode(*r.painted,&images,normals,sampling)}});}
 return j;
}
Json TextureSetArchive::geometry_records()const{
 Json out={{"version",1},{"topologies",Json::array()}};
 for(const auto&t:topology_){
  const auto&m=*t->geometry();std::vector<uint8_t>p,n,uv,indices,faces,corners;
  p.reserve(m.positions.size()*12);n.reserve(m.normals.size()*12);uv.reserve(m.uv.size()*8);indices.reserve(m.indices.size()*4);faces.reserve(t->faces_.size()*8);corners.reserve(t->corners_.size()*8);
  for(auto v:m.positions)for(float x:{v.x,v.y,v.z})word(p,std::bit_cast<uint32_t>(x),4);
  for(auto v:m.normals)for(float x:{v.x,v.y,v.z})word(n,std::bit_cast<uint32_t>(x),4);
  for(auto v:m.uv)for(float x:v)word(uv,std::bit_cast<uint32_t>(x),4);
  for(auto x:m.indices)word(indices,x,4);for(auto x:t->faces_)word(faces,x,8);for(auto x:t->corners_)word(corners,x,8);
  out["topologies"].push_back({{"positions",Json::binary(std::move(p))},{"normals",Json::binary(std::move(n))},{"uv",Json::binary(std::move(uv))},
   {"indices",Json::binary(std::move(indices))},{"uv_pins",Json::binary(m.uv_pins)},{"uv_seams",Json::binary(m.uv_seams)},
   {"faces",Json::binary(std::move(faces))},{"corners",Json::binary(std::move(corners))},{"polygon_ids",t->polygon_ids_}});
 }
 return out;
}
TextureSetArchive TextureSetArchive::decode_geometry(const Json&j,TextureSetLimits limits){
 TextureSetArchive out(std::move(limits));
 if(!j.is_object()||j.size()!=2||integer(j.at("version"),1)!=1||!j.at("topologies").is_array()||j.at("topologies").empty()||j.at("topologies").size()>out.limits_.maximum_sets_per_scene)
  throw std::invalid_argument("Scene40 needs its actual bounded texture-set geometry table");
 // First measure every encoded buffer before any Mesh/vector reconstruction.
 for(const auto&v:j.at("topologies")){
  if(!v.is_object()||v.size()!=9)throw std::invalid_argument("Invalid texture-set topology record");
  const auto&p=blob(v.at("positions"),12,2'000'000),&n=blob(v.at("normals"),12,2'000'000),&uv=blob(v.at("uv"),8,2'000'000),&i=blob(v.at("indices"),12,out.limits_.maximum_triangles);
  if(p.size()!=n.size()||p.size()/12!=uv.size()/8)throw std::invalid_argument("Texture-set vertex arrays disagree");
  const auto&f=blob(v.at("faces"),8,out.limits_.maximum_triangles),&c=blob(v.at("corners"),8,out.limits_.maximum_corners);
  if(f.size()/8!=i.size()/12||c.size()/8!=i.size()/4)throw std::invalid_argument("Texture-set topology identities differ from actual corners");
  const auto&pins=blob(v.at("uv_pins"),1,p.size()/12,true),&seams=blob(v.at("uv_seams"),1,i.size()/4,true);
  if((!pins.empty()&&pins.size()!=p.size()/12)||(!seams.empty()&&seams.size()!=i.size()/4))throw std::invalid_argument("Invalid texture-set UV edit flags");
  out.charge(p.size()+n.size()+uv.size()+i.size()+f.size()+c.size()+pins.size()+seams.size(),1);out.step(p.size()/12+i.size()/4);
 }
 for(const auto&v:j.at("topologies")){
  auto t=std::shared_ptr<TextureSetTopology>(new TextureSetTopology);auto m=std::make_shared<Mesh>();
  auto vectors=[&](const char*key,std::vector<Vec3>&target){const auto&b=v.at(key).get_binary();target.reserve(b.size()/12);for(size_t at=0;at<b.size();at+=12)target.push_back({std::bit_cast<float>(uint32_t(word(b,at,4))),std::bit_cast<float>(uint32_t(word(b,at+4,4))),std::bit_cast<float>(uint32_t(word(b,at+8,4)))});};
  vectors("positions",m->positions);vectors("normals",m->normals);
  const auto&uv=v.at("uv").get_binary();m->uv.reserve(uv.size()/8);for(size_t at=0;at<uv.size();at+=8)m->uv.push_back({std::bit_cast<float>(uint32_t(word(uv,at,4))),std::bit_cast<float>(uint32_t(word(uv,at+4,4)))});
  const auto&i=v.at("indices").get_binary();m->indices.reserve(i.size()/4);for(size_t at=0;at<i.size();at+=4)m->indices.push_back(uint32_t(word(i,at,4)));
  m->uv_pins=v.at("uv_pins").get_binary();m->uv_seams=v.at("uv_seams").get_binary();m->validate();t->geometry_=std::move(m);
  const auto&f=v.at("faces").get_binary(),&c=v.at("corners").get_binary();t->faces_.reserve(f.size()/8);t->corners_.reserve(c.size()/8);
  for(size_t at=0;at<f.size();at+=8)t->faces_.push_back(word(f,at,8));for(size_t at=0;at<c.size();at+=8)t->corners_.push_back(word(c,at,8));
  t->polygon_ids_=v.at("polygon_ids").get<bool>();out.topology_.push_back(std::move(t));
 }
 return out;
}
void TextureSetArchive::decode(SculptObject&o,const Json&j,const PaintImageArchive&images,const NormalSamplingArchive*normals,
    const PaintSamplingArchive*sampling,const SourcePaintDecodeContext&context){
 if(!j.is_object()||j.size()!=7||integer(j.at("version"),1)!=1||!j.at("additional").is_array()||j.at("additional").empty()||j.at("additional").size()+1>limits_.maximum_sets_per_object)
  throw std::invalid_argument("Invalid owning texture-set collection record");
 if(j.at("additional").size()+1>limits_.maximum_sets_per_scene-sets_)throw std::length_error("Scene texture-set count exceeds bounds");sets_+=j.at("additional").size()+1;
 const auto id=uint32_t(integer(j.at("topology"),topology_.size()-1));const auto&t=topology_.at(id);
 const auto&encoded=blob(j.at("slots"),4,limits_.maximum_triangles);if(encoded.size()/4!=t->faces_.size())throw std::invalid_argument("Texture-set assignment omitted logical faces");
 auto c=std::shared_ptr<TextureSetCollection>(new TextureSetCollection);c->topology_=t;c->active_=TextureSetId(integer(j.at("active"),UINT32_MAX));c->next_=integer(j.at("next_id"),uint64_t(UINT32_MAX)+1);
 std::set<TextureSetId>ids{0};for(const auto&v:j.at("additional")){
  if(!v.is_object()||v.size()!=4)throw std::invalid_argument("Incomplete texture-set member");auto set=TextureSetId(integer(v.at("id"),UINT32_MAX));
  if(!set||set>=c->next_||!ids.insert(set).second)throw std::invalid_argument("Reused texture-set identity");
 }
 if(!ids.contains(c->active_))throw std::invalid_argument("Unknown active texture set");
 auto slots=std::make_shared<std::vector<TextureSetId>>();slots->reserve(encoded.size()/4);std::map<uint64_t,TextureSetId>face_slots;
 for(size_t at=0;at<encoded.size();at+=4){step();auto set=TextureSetId(word(encoded,at,4));if(!ids.contains(set)||!t->faces_[at/4])throw std::invalid_argument("Foreign texture-set face/slot");auto[it,fresh]=face_slots.emplace(t->faces_[at/4],set);if(!fresh&&it->second!=set)throw std::invalid_argument("Polygon face has conflicting texture sets");slots->push_back(set);}
 for(auto corner:t->corners_)if(!corner)throw std::invalid_argument("Texture-set corner identity is missing");
 c->slots_=std::move(slots);
 auto domain=[&](TextureSetId set){auto d=std::shared_ptr<TextureSetFaceDomain>(new TextureSetFaceDomain);d->topology_=t;d->set_=set;d->assignments_=c->slots_;for(size_t f=0;f<c->slots_->size();++f){step();if((*c->slots_)[f]==set)d->triangles_.push_back(uint32_t(f));}return std::shared_ptr<const TextureSetFaceDomain>(std::move(d));};
 c->primary_=domain(0);auto primary=TextureSetPaintAccess::decode(j.at("primary"),&images,bool(o.polygon),normals,sampling,context,c->primary_);
 plain(*primary,o.material_bindings.get(),o.appearance.get(),o.bake_recipe.get());
 for(const auto&v:j.at("additional")){TextureSetRecord r;r.id=TextureSetId(integer(v.at("id"),UINT32_MAX));r.name=v.at("name").get<std::string>();r.material=material(v.at("material"));r.domain=domain(r.id);r.painted=TextureSetPaintAccess::decode(v.at("painted"),&images,bool(o.polygon),normals,sampling,context,r.domain);plain(*r.painted,nullptr,nullptr,nullptr);c->additional_.push_back(std::move(r));}
 // No mutable draft or partially decoded collection escapes a failure.
 auto candidate=o;candidate.painted=std::move(primary);candidate.texture_sets=c;c->validate(candidate,true,limits_);o=std::move(candidate);reached_.insert(id);
}
void TextureSetArchive::require_reachable()const{if(reached_.size()!=topology_.size())throw std::invalid_argument("Unreachable texture-set geometry records");}
} // namespace dome::detail
