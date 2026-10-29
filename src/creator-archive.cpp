// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-archive.hpp"
#include <nlohmann/json.hpp>
#include <span>
#include <set>
namespace dome {
namespace {
void stop(const CreatorArchiveLimits &l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Creator source persistence cancelled");}
void limits_valid(const CreatorArchiveLimits &l){
  if(!l.max_assets||l.max_assets>256||!l.max_asset_bytes||l.max_asset_bytes>128*1024*1024||
      !l.max_total_bytes||l.max_total_bytes>256*1024*1024||!l.max_json_bytes||l.max_json_bytes>16*1024*1024||
      !l.max_remap_vertices||l.max_remap_vertices>2'000'000)
    throw std::invalid_argument("Invalid creator source archive limits");
}
uint32_t word(std::span<const uint8_t>b,size_t at){
  if(at>b.size()||b.size()-at<4)throw std::runtime_error("Truncated embedded creator source");
  return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
nlohmann::json source_json(std::span<const uint8_t>bytes,const CreatorArchiveLimits &l){
  if(bytes.size()<28||bytes.size()>l.max_asset_bytes||word(bytes,0)!=0x46546c67||word(bytes,4)!=2||word(bytes,8)!=bytes.size()||word(bytes,16)!=0x4e4f534a)
    throw std::runtime_error("Invalid embedded creator GLB source");
  size_t chunk=12;bool binary=false;
  while(chunk<bytes.size()){
    if(bytes.size()-chunk<8)throw std::runtime_error("Truncated creator GLB chunk header");
    size_t count=word(bytes,chunk);uint32_t type=word(bytes,chunk+4);
    if(count%4||count>bytes.size()-chunk-8||(type==0x4e4f534a&&chunk!=12)||(type==0x004e4942&&binary))
      throw std::runtime_error("Invalid creator GLB chunk bounds");
    binary|=type==0x004e4942;chunk+=8+count;
  }
  size_t length=word(bytes,12);
  if(length>l.max_json_bytes||length>bytes.size()-20)
    throw std::length_error("Embedded creator JSON exceeds its resource bounds");
  size_t visits=0;auto json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+length,
    [&](int nesting,nlohmann::json::parse_event_t,const nlohmann::json&){
      if(nesting>64||++visits>l.import_limits.max_metadata_visits)throw std::length_error("Embedded creator metadata exceeds its resource bounds");
      if((visits&4095)==0)stop(l);
      return true;
    });
  if(!json.is_object())throw std::runtime_error("Embedded creator JSON is not an object");
  return json;
}
uint32_t integer(const nlohmann::json &v,uint32_t maximum=INT32_MAX){
  if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>maximum:v.get<int64_t>()<0||uint64_t(v.get<int64_t>())>maximum))
    throw std::runtime_error("Invalid creator source index");
  return v.get<uint32_t>();
}
std::shared_ptr<const std::string> vrm_string(const nlohmann::json &json){
  if(json.contains("extensions")&&json.at("extensions").contains("VRM"))
    return std::make_shared<const std::string>(json.at("extensions").at("VRM").dump());
  return {};
}
std::pair<std::string,std::string> material_strings(const nlohmann::json &json,int material){
  if(material<0||!json.contains("materials")||size_t(material)>=json.at("materials").size())
    throw std::runtime_error("Creator material is outside its original source");
  std::string mtoon;
  if(json.contains("extensions")&&json.at("extensions").contains("VRM")){
    const auto &vrm=json.at("extensions").at("VRM");
    if(vrm.contains("materialProperties")){
      if(size_t(material)>=vrm.at("materialProperties").size())throw std::runtime_error("Creator material is outside its VRM source");
      mtoon=vrm.at("materialProperties").at(material).dump();
    }
  }
  return {json.at("materials").at(material).dump(),mtoon};
}
void validate_images(const ImportedAsset &asset,const std::vector<std::shared_ptr<const Image>>&images){
  if(images.size()>256||images.size()!=asset.images.size())throw std::runtime_error("Creator decoded image table differs from its source");
  for(size_t i=0;i<images.size();++i){
    if(!images[i])throw std::runtime_error("Creator decoded source image is missing");
    images[i]->validate();const auto &encoded=asset.images[i];
    if(encoded.mime_type=="image/png"){
      const auto &b=encoded.bytes;
      constexpr uint8_t magic[]={137,80,78,71,13,10,26,10};
      if(b.size()<33||!std::equal(std::begin(magic),std::end(magic),b.begin())||b[12]!='I'||b[13]!='H'||b[14]!='D'||b[15]!='R')
        throw std::runtime_error("Creator original PNG header is invalid");
      auto big=[&](size_t at){return uint32_t(b[at])<<24|uint32_t(b[at+1])<<16|uint32_t(b[at+2])<<8|b[at+3];};
      if(big(16)!=images[i]->width||big(20)!=images[i]->height)
        throw std::runtime_error("Creator decoded image dimensions differ from original PNG");
    }
  }
}
const ImportedPrimitive &mapped_primitive(const CreatorAppearance&a){
  if(!a.asset||!a.original_bytes||!a.decoded_images||a.asset->mode!=ImportMode::PreserveGltf)
    throw std::invalid_argument("Creator appearance lacks immutable preserved source data");
  if(a.material<1||size_t(a.material)>=a.asset->materials.size()||a.source_material!=a.material-1||a.source_mesh<0||
      size_t(a.source_mesh)>=a.asset->meshes.size()||a.source_node<0||size_t(a.source_node)>=a.asset->nodes.size()||
      a.asset->nodes[a.source_node].mesh!=a.source_mesh||a.source_primitive<0)
    throw std::runtime_error("Creator appearance identity differs from original source");
  const ImportedPrimitive *primitive=nullptr;
  for(auto &p:a.asset->primitives)if(p.source_mesh==a.source_mesh&&p.source_primitive==a.source_primitive){
    if(primitive)throw std::runtime_error("Creator source primitive identity is ambiguous");
    primitive=&p;
  }
  if(!primitive||primitive->material!=uint32_t(a.material))throw std::runtime_error("Creator source primitive/material mapping is invalid");
  return *primitive;
}
void validate_mapping(const CreatorAppearance&a,const nlohmann::json &json,const CreatorArchiveLimits &l){
  const auto &primitive=mapped_primitive(a);
  if(a.source_vertices.empty()||a.source_vertices.size()>l.max_remap_vertices)
    throw std::runtime_error("Creator source vertex mapping is invalid");
  size_t k=0;
  for(auto v:a.source_vertices){
    if(v>=primitive.mesh.positions.size())throw std::runtime_error("Creator vertex mapping is outside its authored accessor");
    if((++k&4095)==0)stop(l);
  }
  if(a.source_joint_nodes.size()>256) throw std::length_error("Creator source joint mapping exceeds bounds");
  std::set<uint32_t> mapped_joints;
  for(auto node:a.source_joint_nodes)
    if(node>=a.asset->nodes.size() || !mapped_joints.insert(node).second)
      throw std::runtime_error("Creator source joint mapping is not unique and in range");
  if(a.source_morph_targets.size()>128) throw std::length_error("Creator source morph mapping exceeds bounds");
  std::set<uint32_t> mapped_targets;
  for(auto target:a.source_morph_targets)
    if(target>=primitive.morph_targets.size() || !mapped_targets.insert(target).second)
      throw std::runtime_error("Creator source morph mapping is not unique and in range");
  if(a.source_basis_quaternion) {
    auto q=*a.source_basis_quaternion;float norm=q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;
    if(!std::isfinite(norm)||std::abs(norm-1)>1e-5f)
      throw std::runtime_error("Creator source basis must be a finite unit quaternion");
  }
  auto [material,mtoon]=material_strings(json,a.source_material);
  if(a.material_json!=material||a.mtoon_json!=mtoon)
    throw std::runtime_error("Retained creator material differs from original source; preserve edits separately");
  auto vrm=vrm_string(json);
  if(bool(a.vrm_json)!=bool(vrm)||(vrm&&*a.vrm_json!=*vrm))
    throw std::runtime_error("Retained creator VRM metadata differs from original source");
}
}
struct CreatorAssetArchive::Entry {
  CreatorAppearance source;
  nlohmann::json json;
  std::vector<uint32_t> image_ids;
};
CreatorAssetArchive::CreatorAssetArchive(CreatorArchiveLimits l):limits_(std::move(l)){limits_valid(limits_);}
uint32_t CreatorAssetArchive::add(const CreatorAppearance &a,PaintImageArchive &images){
  stop(limits_);
  if(!a.original_bytes||!a.asset||!a.decoded_images)throw std::invalid_argument("Missing retained creator source");
  for(size_t i=0;i<entries_.size();++i)if(entries_[i]->source.original_bytes.get()==a.original_bytes.get()){
    const auto &entry=*entries_[i];
    validate_mapping(a,entry.json,limits_);
    if(entry.source.decoded_images->size()!=a.decoded_images->size())throw std::runtime_error("Creator shared source image count changed");
    auto next_images=images;
    for(size_t k=0;k<a.decoded_images->size();++k){
      if(a.decoded_images->at(k)!=entry.source.decoded_images->at(k))throw std::runtime_error("Creator shared source image identity changed");
      if(next_images.add(a.decoded_images->at(k))!=entry.image_ids[k])throw std::runtime_error("Creator source and image archive identities differ");
    }
    images=std::move(next_images);return uint32_t(i);
  }
  if(entries_.size()>=limits_.max_assets||a.original_bytes->size()>limits_.max_total_bytes-bytes_)
    throw std::length_error("Creator original source archive budget exceeded");
  auto entry=std::make_shared<Entry>();entry->json=source_json(*a.original_bytes,limits_);
  validate_mapping(a,entry->json,limits_);validate_images(*a.asset,*a.decoded_images);
  entry->source.original_bytes=a.original_bytes;entry->source.asset=a.asset;entry->source.decoded_images=a.decoded_images;entry->source.vrm_json=a.vrm_json;
  // Commit both archives together only after every reference and quota validates.
  auto next_images=images;
  for(auto &image:*a.decoded_images){stop(limits_);entry->image_ids.push_back(next_images.add(image));}
  entries_.push_back(std::move(entry));bytes_+=a.original_bytes->size();images=std::move(next_images);return uint32_t(entries_.size()-1);
}
nlohmann::json CreatorAssetArchive::encode()const{
  auto result=nlohmann::json::array();
  for(auto &e:entries_){stop(limits_);result.push_back({{"original_glb",nlohmann::json::binary(*e->source.original_bytes)},{"images",e->image_ids}});}
  return result;
}
CreatorAssetArchive CreatorAssetArchive::decode(const nlohmann::json &j,const PaintImageArchive &images,CreatorArchiveLimits l){
  CreatorAssetArchive result(l);
  if(!j.is_array()||j.size()>l.max_assets)throw std::length_error("Invalid creator original source archive count");
  // Validate all outer budgets/references before copying/importing any source.
  size_t total=0;
  for(auto &value:j){
    auto &raw=value.at("original_glb"),&ids=value.at("images");
    if(!raw.is_binary()||raw.get_binary().size()>l.max_asset_bytes||raw.get_binary().size()>l.max_total_bytes-total||!ids.is_array()||ids.size()>256)
      throw std::length_error("Invalid creator original source archive payload");
    auto json=source_json(raw.get_binary(),l);
    size_t count=json.contains("images")?json.at("images").size():0;
    if(count!=ids.size())throw std::runtime_error("Creator decoded image count differs from embedded source");
    total+=raw.get_binary().size();
    for(auto &id:ids)images.at(integer(id,UINT32_MAX));
  }
  for(auto &value:j){
    stop(l);auto entry=std::make_shared<Entry>();const auto &raw=value.at("original_glb").get_binary();
    entry->source.original_bytes=std::make_shared<const std::vector<uint8_t>>(raw.begin(),raw.end());entry->json=source_json(*entry->source.original_bytes,l);
    ImportLimits options=l.import_limits;options.mode=ImportMode::PreserveGltf;options.external_resources=false;
    options.max_file_bytes=std::min(options.max_file_bytes,l.max_asset_bytes);
    options.cancelled=[a=l.cancelled,b=options.cancelled]{return (a&&a())||(b&&b());};
    options.allow_legacy_vrm_skin_hint=entry->json.contains("extensions")&&entry->json.at("extensions").contains("VRM");
    entry->source.asset=std::make_shared<const ImportedAsset>(import_glb(*entry->source.original_bytes,options,ImportMode::PreserveGltf));
    auto decoded=std::make_shared<std::vector<std::shared_ptr<const Image>>>();
    for(auto &id:value.at("images")){auto index=integer(id,UINT32_MAX);entry->image_ids.push_back(index);decoded->push_back(images.at(index));}
    validate_images(*entry->source.asset,*decoded);entry->source.decoded_images=std::move(decoded);entry->source.vrm_json=vrm_string(entry->json);
    result.bytes_+=raw.size();result.entries_.push_back(std::move(entry));stop(l);
  }return result;
}
nlohmann::json encode_creator_appearance(const CreatorAppearance&a,CreatorAssetArchive &assets,PaintImageArchive &images){
  uint32_t source=assets.add(a,images);std::vector<uint8_t> remap;remap.reserve(a.source_vertices.size()*4);
  for(auto v:a.source_vertices)for(int k=0;k<4;++k)remap.push_back(uint8_t(v>>(8*k)));
  std::vector<uint8_t> joints;
  for(auto node:a.source_joint_nodes) for(int k=0;k<4;++k) joints.push_back(uint8_t(node>>(8*k)));
  std::vector<uint8_t> targets;
  for(auto target:a.source_morph_targets) for(int k=0;k<4;++k) targets.push_back(uint8_t(target>>(8*k)));
  nlohmann::json basis=nullptr;
  if(a.source_basis_quaternion){auto q=*a.source_basis_quaternion;basis={q.w,q.x,q.y,q.z};}
  return {{"source_basis_quaternion",basis},{"source_morph_targets",nlohmann::json::binary(targets)},{"source_joint_nodes",nlohmann::json::binary(joints)},{"asset",source},{"material",a.material},{"source_material",a.source_material},{"source_mesh",a.source_mesh},
    {"source_node",a.source_node},{"source_primitive",a.source_primitive},{"source_vertices",nlohmann::json::binary(remap)}};
}
std::shared_ptr<const CreatorAppearance> decode_creator_appearance(const nlohmann::json&j,const CreatorAssetArchive&assets){
  stop(assets.limits_);auto id=integer(j.at("asset"),UINT32_MAX);
  if(id>=assets.entries_.size())throw std::runtime_error("Creator source asset reference is missing");
  const auto &entry=*assets.entries_[id];auto result=std::make_shared<CreatorAppearance>(entry.source);
  result->material=int(integer(j.at("material")));result->source_material=int(integer(j.at("source_material")));
  result->source_mesh=int(integer(j.at("source_mesh")));result->source_node=int(integer(j.at("source_node")));result->source_primitive=int(integer(j.at("source_primitive")));
  auto &bytes=j.at("source_vertices");
  if(!bytes.is_binary()||bytes.get_binary().empty()||bytes.get_binary().size()%4||bytes.get_binary().size()/4>assets.limits_.max_remap_vertices)
    throw std::length_error("Invalid persisted creator source vertex mapping");
  mapped_primitive(*result);
  auto [material,mtoon]=material_strings(entry.json,result->source_material);
  const auto &raw=bytes.get_binary();result->source_vertices.reserve(raw.size()/4);
  for(size_t at=0;at<raw.size();at+=4)result->source_vertices.push_back(word(raw,at));
  result->material_json=std::move(material);result->mtoon_json=std::move(mtoon);
  if(j.contains("source_joint_nodes")) {
    const auto &nodes=j.at("source_joint_nodes");
    if(!nodes.is_binary() || nodes.get_binary().size()%4 || nodes.get_binary().size()>256*4)
      throw std::runtime_error("Invalid persisted creator source joint mapping");
    for(size_t at=0;at<nodes.get_binary().size();at+=4)
      result->source_joint_nodes.push_back(word(nodes.get_binary(),at));
  }
  if(j.contains("source_morph_targets")) {
    const auto &targets=j.at("source_morph_targets");
    if(!targets.is_binary() || targets.get_binary().size()%4 || targets.get_binary().size()>128*4)
      throw std::runtime_error("Invalid persisted creator source morph mapping");
    for(size_t at=0;at<targets.get_binary().size();at+=4)
      result->source_morph_targets.push_back(word(targets.get_binary(),at));
  }
  if(j.contains("source_basis_quaternion") && !j.at("source_basis_quaternion").is_null()) {
    const auto &basis=j.at("source_basis_quaternion");
    if(!basis.is_array()||basis.size()!=4)throw std::runtime_error("Invalid persisted creator source basis");
    for(auto &component:basis)if(!component.is_number())throw std::runtime_error("Invalid creator basis component");
    result->source_basis_quaternion=Quat{basis[0].get<float>(),basis[1].get<float>(),basis[2].get<float>(),basis[3].get<float>()};
  }
  validate_mapping(*result,entry.json,assets.limits_);return result;
}
} // namespace dome
