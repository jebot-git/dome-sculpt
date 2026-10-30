// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-archive.hpp"
#include "dome/import-source.hpp"
#include "dome/import-image.hpp"
#include "cgltf.h"
#include <bit>
#include <nlohmann/json.hpp>
#include <span>
#include <set>
namespace dome {
namespace {
void stop(const CreatorArchiveLimits &l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Creator source persistence cancelled");}
size_t source_size(const CreatorSourceModel &a,const CreatorArchiveLimits &l){
  size_t bytes=a.original_bytes?a.original_bytes->size():0;
  if(a.source_receipt){a.source_receipt->validate(l.import_limits);auto raw=a.source_receipt->stored_bytes();
    if(raw>l.max_asset_bytes || bytes>l.max_asset_bytes-raw)throw std::length_error("Original source plus canonical adapter exceeds native asset budget");
    bytes+=raw;}
  return bytes;
}
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
    if(encoded.mime_type=="image/jpeg"){
      auto size=imported_image_dimensions(encoded);
      if(size[0]!=images[i]->width || size[1]!=images[i]->height)
        throw std::runtime_error("Creator decoded image dimensions differ from original JPEG");
    }
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
// A model-wide hierarchy has no primitive mapping to vouch for its node table.
// Authenticate its local graph against the captured canonical JSON. The same
// cgltf transform primitive as the original parser preserves float evaluation
// and signed zeros; no geometry/image reimport or filesystem read is needed.
void validate_model_nodes(const CreatorSourceModel &source,const nlohmann::json &json,
                          const CreatorArchiveLimits &limits) {
  const size_t count=json.contains("nodes")?json.at("nodes").size():0;
  if((json.contains("nodes")&&!json.at("nodes").is_array()) || count>100000 ||
      count!=source.asset->nodes.size())
    throw std::runtime_error("Original model node table differs from canonical source");
  std::vector<int> parents(count,-1);size_t work=0;
  auto visit=[&]{if(++work>limits.import_limits.max_metadata_visits)throw std::length_error("Original model node witness budget exceeded");if((work&255)==0)stop(limits);};
  for(size_t i=0;i<count;++i) {
    visit();const auto &raw=json.at("nodes").at(i);const auto &node=source.asset->nodes[i];
    if(!raw.is_object())throw std::runtime_error("Original model node must be an object");
    cgltf_node value{};value.rotation[3]=1;value.scale[0]=value.scale[1]=value.scale[2]=1;
    value.has_matrix=raw.contains("matrix");value.has_translation=raw.contains("translation");
    value.has_rotation=raw.contains("rotation");value.has_scale=raw.contains("scale");
    if(value.has_matrix&&(value.has_translation||value.has_rotation||value.has_scale))
      throw std::runtime_error("Original model node mixes matrix and TRS authority");
    if(value.has_matrix){auto m=raw.at("matrix").get<std::array<float,16>>();std::copy(m.begin(),m.end(),value.matrix);}
    if(value.has_translation){auto p=raw.at("translation").get<std::array<float,3>>();std::copy(p.begin(),p.end(),value.translation);}
    if(value.has_rotation){auto q=raw.at("rotation").get<std::array<float,4>>();std::copy(q.begin(),q.end(),value.rotation);}
    if(value.has_scale){auto s=raw.at("scale").get<std::array<float,3>>();std::copy(s.begin(),s.end(),value.scale);}
    ImportedMatrix local{};cgltf_node_transform_local(&value,local.data());
    for(float x:local)if(!std::isfinite(x))throw std::runtime_error("Original model node has a nonfinite canonical matrix");
    if(bool(value.has_matrix)!=node.has_matrix || bool(value.has_translation)!=node.has_translation ||
        bool(value.has_rotation)!=node.has_rotation || bool(value.has_scale)!=node.has_scale ||
        std::bit_cast<std::array<uint32_t,16>>(local)!=std::bit_cast<std::array<uint32_t,16>>(node.local))
      throw std::runtime_error("Original model local transform differs from captured source");
    auto reference=[&](const char*key){return raw.contains(key)?int(integer(raw.at(key))):-1;};
    if(reference("mesh")!=node.mesh || reference("skin")!=node.skin)
      throw std::runtime_error("Original model node mesh/skin reference differs from captured source");
    const size_t children=raw.contains("children")?raw.at("children").size():0;
    if((raw.contains("children")&&!raw.at("children").is_array()) || children!=node.children.size())
      throw std::runtime_error("Original model child table differs from captured source");
    for(size_t child=0;child<children;++child) {
      visit();const auto id=integer(raw.at("children").at(child));
      if(id>=count || node.children[child]!=id || parents[id]!=-1)
        throw std::runtime_error("Original model child identity is repeated or differs from captured source");
      parents[id]=int(i);
    }
    const size_t weights=raw.contains("weights")?raw.at("weights").size():0;
    if((raw.contains("weights")&&!raw.at("weights").is_array()) || weights!=node.weights.size())
      throw std::runtime_error("Original model node morph weights differ from captured source");
    for(size_t w=0;w<weights;++w){visit();const float value=raw.at("weights").at(w).get<float>();if(!std::isfinite(value)||value!=node.weights[w])throw std::runtime_error("Original model node morph weight differs from captured source");}
  }
  for(size_t i=0;i<count;++i)if(parents[i]!=source.asset->nodes[i].parent)
    throw std::runtime_error("Original model parent authority differs from captured source");
  std::vector<uint8_t> state(count);
  for(size_t i=0;i<count;++i) {
    int at=int(i);
    while(at>=0 && state[size_t(at)]==0){visit();state[size_t(at)]=1;at=parents[size_t(at)];}
    if(at>=0 && state[size_t(at)]==1)throw std::runtime_error("Original model node hierarchy is cyclic");
    at=int(i);
    while(at>=0 && state[size_t(at)]==1){visit();state[size_t(at)]=2;at=parents[size_t(at)];}
  }
  stop(limits);
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
  std::shared_ptr<const CreatorSourceModel> source;
  nlohmann::json json;
  std::vector<uint32_t> image_ids;
};
CreatorAssetArchive::CreatorAssetArchive(CreatorArchiveLimits l):limits_(std::move(l)){limits_valid(limits_);}
uint32_t CreatorAssetArchive::add(const CreatorAppearance &a,PaintImageArchive &images){
  return add_source(CreatorSourceModel(a),&a,images);
}
uint32_t CreatorAssetArchive::add_model(const CreatorSourceModel &a,PaintImageArchive &images){
  return add_source(a,nullptr,images);
}
std::shared_ptr<const CreatorSourceModel> CreatorAssetArchive::model(uint32_t id) const {
  stop(limits_);
  if(id>=entries_.size())throw std::out_of_range("Original source model reference is missing");
  return entries_[id]->source;
}
uint32_t CreatorAssetArchive::add_source(const CreatorSourceModel &a,const CreatorAppearance *appearance,PaintImageArchive &images){
  stop(limits_);
  if(!a.original_bytes||!a.asset||!a.decoded_images)throw std::invalid_argument("Missing retained creator source");
  if(a.asset->mode!=ImportMode::PreserveGltf)throw std::invalid_argument("Original source model must retain mesh-local source data");
  for(size_t i=0;i<entries_.size();++i)if(entries_[i]->source->original_bytes.get()==a.original_bytes.get()){
    const auto &entry=*entries_[i];
    if(appearance)validate_mapping(*appearance,entry.json,limits_);
    if(a.asset!=entry.source->asset || a.source_receipt!=entry.source->source_receipt)
      throw std::runtime_error("Shared canonical source has different parsed/raw source authority");
    if(entry.source->decoded_images->size()!=a.decoded_images->size())throw std::runtime_error("Creator shared source image count changed");
    auto next_images=images;
    for(size_t k=0;k<a.decoded_images->size();++k){
      if(a.decoded_images->at(k)!=entry.source->decoded_images->at(k))throw std::runtime_error("Creator shared source image identity changed");
      if(next_images.add(a.decoded_images->at(k))!=entry.image_ids[k])throw std::runtime_error("Creator source and image archive identities differ");
    }
    images=std::move(next_images);return uint32_t(i);
  }
  const auto stored=source_size(a,limits_);
  if(entries_.size()>=limits_.max_assets||stored>limits_.max_total_bytes-bytes_)
    throw std::length_error("Creator original source archive budget exceeded");
  auto entry=std::make_shared<Entry>();entry->json=source_json(*a.original_bytes,limits_);
  validate_model_nodes(a,entry->json,limits_);
  if(appearance)validate_mapping(*appearance,entry->json,limits_);
  validate_images(*a.asset,*a.decoded_images);
  const auto source_vrm=vrm_string(entry->json);
  if(bool(a.vrm_json)!=bool(source_vrm) ||
      (a.vrm_json && *a.vrm_json!=*source_vrm))
    throw std::runtime_error("Original model VRM metadata differs from canonical source");
  entry->source=std::make_shared<const CreatorSourceModel>(a);
  // Commit both archives together only after every reference and quota validates.
  auto next_images=images;
  for(auto &image:*a.decoded_images){stop(limits_);entry->image_ids.push_back(next_images.add(image));}
  entries_.push_back(std::move(entry));bytes_+=stored;images=std::move(next_images);return uint32_t(entries_.size()-1);
}
nlohmann::json CreatorAssetArchive::encode()const{
  auto result=nlohmann::json::array();
  for(auto &e:entries_){stop(limits_);nlohmann::json entry={{"original_glb",nlohmann::json::binary(*e->source->original_bytes)},{"images",e->image_ids}};
    if(e->source->source_receipt)entry["original_source"]=e->source->source_receipt->encode();
    result.push_back(std::move(entry));}
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
    size_t stored=raw.get_binary().size();
    if(value.contains("original_source")){
      auto bytes=ImportSourceReceipt::encoded_bytes(value.at("original_source"),l.import_limits);
      if(bytes>l.max_asset_bytes || stored>l.max_asset_bytes-bytes || bytes>l.max_total_bytes-total-stored)
        throw std::length_error("Original source and canonical adapter exceed native source budget");
      stored+=bytes;
    }
    total+=stored;
    for(auto &id:ids)images.at(integer(id,UINT32_MAX));
  }
  for(auto &value:j){
    stop(l);auto entry=std::make_shared<Entry>();const auto &raw=value.at("original_glb").get_binary();
    auto model=std::make_shared<CreatorSourceModel>();
    if(value.contains("original_source"))model->source_receipt=ImportSourceReceipt::decode(value.at("original_source"),l.import_limits);
    model->original_bytes=std::make_shared<const std::vector<uint8_t>>(raw.begin(),raw.end());entry->json=source_json(*model->original_bytes,l);
    ImportLimits options=l.import_limits;options.mode=ImportMode::PreserveGltf;options.external_resources=false;
    options.max_file_bytes=std::min(options.max_file_bytes,l.max_asset_bytes);
    options.cancelled=[a=l.cancelled,b=options.cancelled]{return (a&&a())||(b&&b());};
    options.allow_legacy_vrm_skin_hint=entry->json.contains("extensions")&&entry->json.at("extensions").contains("VRM");
    model->asset=std::make_shared<const ImportedAsset>(import_glb(*model->original_bytes,options,ImportMode::PreserveGltf));
    auto decoded=std::make_shared<std::vector<std::shared_ptr<const Image>>>();
    for(auto &id:value.at("images")){auto index=integer(id,UINT32_MAX);entry->image_ids.push_back(index);decoded->push_back(images.at(index));}
    validate_images(*model->asset,*decoded);model->decoded_images=std::move(decoded);model->vrm_json=vrm_string(entry->json);
    result.bytes_+=source_size(*model,l);entry->source=std::move(model);result.entries_.push_back(std::move(entry));stop(l);
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
  const auto &entry=*assets.entries_[id];auto result=std::make_shared<CreatorAppearance>();
  const auto &model=*entry.source;
  result->original_bytes=model.original_bytes;result->source_receipt=model.source_receipt;
  result->asset=model.asset;result->decoded_images=model.decoded_images;result->vrm_json=model.vrm_json;
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
