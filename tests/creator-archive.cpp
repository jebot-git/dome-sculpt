// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-archive.hpp"
#include "dome/projection.hpp"
#include "dome/scene.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
void push_word(std::vector<uint8_t>&b,uint32_t v){for(int k=0;k<4;++k)b.push_back(uint8_t(v>>(8*k)));}
uint32_t read_word(const std::vector<uint8_t>&b,size_t at){if(at+4>b.size())throw std::runtime_error("Truncated fixture");uint32_t v=0;for(int k=0;k<4;++k)v|=uint32_t(b[at+k])<<(8*k);return v;}
std::vector<uint8_t> glb(nlohmann::json j,std::vector<uint8_t> bin){while(bin.size()%4)bin.push_back(0);j["buffers"]={{{"byteLength",bin.size()}}};auto text=j.dump();while(text.size()%4)text.push_back(' ');std::vector<uint8_t>b;push_word(b,0x46546c67);push_word(b,2);push_word(b,uint32_t(28+text.size()+bin.size()));push_word(b,uint32_t(text.size()));push_word(b,0x4e4f534a);b.insert(b.end(),text.begin(),text.end());push_word(b,uint32_t(bin.size()));push_word(b,0x004e4942);b.insert(b.end(),bin.begin(),bin.end());return b;}
struct Fixture{nlohmann::json json;std::vector<uint8_t> bytes;};
Fixture fixture(){
  nlohmann::json j={{"asset",{{"version","2.0"}}},{"bufferViews",nlohmann::json::array()},{"accessors",nlohmann::json::array()}};std::vector<uint8_t>bin;
  auto view=[&](const std::vector<uint8_t>&bytes){while(bin.size()%4)bin.push_back(0);int index=int(j["bufferViews"].size());j["bufferViews"].push_back({{"buffer",0},{"byteOffset",bin.size()},{"byteLength",bytes.size()}});bin.insert(bin.end(),bytes.begin(),bytes.end());return index;};
  auto floats=[&](std::initializer_list<float> f,const char*type,int count){std::vector<uint8_t>b;for(auto v:f)push_word(b,std::bit_cast<uint32_t>(v));int index=int(j["accessors"].size());j["accessors"].push_back({{"bufferView",view(b)},{"componentType",5126},{"count",count},{"type",type}});return index;};
  int pos=floats({0,0,0,1,0,0,0,1,0},"VEC3",3),normal=floats({0,0,1,0,0,1,0,0,1},"VEC3",3),uv=floats({0,0,1,0,0,1},"VEC2",3),uv1=floats({.2f,.3f,.4f,.5f,.6f,.7f},"VEC2",3),color=floats({1,0,0,.5f,0,1,0,.75f,0,0,1,1},"VEC4",3),tangent=floats({1,0,0,-1,1,0,0,-1,1,0,0,-1},"VEC4",3),weights=floats({1,0,0,0,1,0,0,0,1,0,0,0},"VEC4",3);
  std::vector<uint8_t> joints(24);int joint=int(j["accessors"].size());j["accessors"].push_back({{"bufferView",view(joints)},{"componentType",5123},{"count",3},{"type","VEC4"}});
  int morphpos=floats({0,.1f,0,0,.2f,0,0,.3f,0},"VEC3",3),morphnormal=floats({.1f,0,0,.2f,0,0,.3f,0,0},"VEC3",3),morphtangent=floats({0,0,.1f,0,0,.2f,0,0,.3f},"VEC3",3),ibm=floats({1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},"MAT4",1);
  Image image{2,1,{255,0,0,0,0,255,128,255}};int png=view(image.png());
  j["images"]={{{"mimeType","image/png"},{"bufferView",png},{"name","Original alpha"}}};j["textures"]={{{"source",0}}};
  j["materials"]={{{"name","Source material"},{"pbrMetallicRoughness",{{"baseColorTexture",{{"index",0}}}}},{"extras",{{"unknown-original",123}}}}};
  j["meshes"]={{{"name","Source mesh"},{"weights",{.25f}},{"extras",{{"targetNames",{"Original expression"}}}},
    {"primitives",{{{"attributes",{{"POSITION",pos},{"NORMAL",normal},{"TEXCOORD_0",uv},{"TEXCOORD_1",uv1},{"COLOR_0",color},{"TANGENT",tangent},{"JOINTS_0",joint},{"WEIGHTS_0",weights}}},{"material",0},{"targets",{{{"POSITION",morphpos},{"NORMAL",morphnormal},{"TANGENT",morphtangent}}}}}}}}};
  j["nodes"]={{{"name","Original joint"},{"children",{1}}},{{"name","Original instance"},{"mesh",0},{"skin",0},{"extras",{{"unknown-node","retained"}}}}};
  j["skins"]={{{"joints",{0}},{"skeleton",0},{"inverseBindMatrices",ibm}}};j["scenes"]={{{"nodes",{0}}}};j["scene"]=0;
  j["extensions"]={{"VRM",{{"specVersion","0.0"},{"meta",{{"title","Private native fixture"},{"licenseName","CC0"}}},{"materialProperties",{{{"name","Original MToon"},{"shader","VRM/MToon"},{"floatProperties",{{"_UnknownOriginal",.125}}}}}},{"unknown-vrm",{{"must-survive",{"all","metadata"}}}}}}};
  return {j,glb(j,std::move(bin))};
}
CreatorAppearance appearance(const std::shared_ptr<const std::vector<uint8_t>>&bytes){
  auto j=nlohmann::json::parse(bytes->begin()+20,bytes->begin()+20+read_word(*bytes,12));ImportLimits l;l.allow_legacy_vrm_skin_hint=true;
  CreatorAppearance a;a.original_bytes=bytes;a.asset=std::make_shared<const ImportedAsset>(import_glb(*bytes,l));
  auto decoded=std::make_shared<std::vector<std::shared_ptr<const Image>>>();for(auto &image:a.asset->images)decoded->push_back(std::make_shared<const Image>(decode_projection_png(image.bytes)));a.decoded_images=decoded;
  const auto &p=a.asset->primitives.at(0);a.material=int(p.material);a.source_material=a.material-1;a.source_mesh=p.source_mesh;a.source_primitive=p.source_primitive;
  for(size_t i=0;i<a.asset->nodes.size();++i)if(a.asset->nodes[i].mesh==a.source_mesh){a.source_node=int(i);break;}
  for(uint32_t i=0;i<p.mesh.positions.size();++i)a.source_vertices.push_back(i);
  a.material_json=j.at("materials").at(a.source_material).dump();a.mtoon_json=j.at("extensions").at("VRM").at("materialProperties").at(a.source_material).dump();a.vrm_json=std::make_shared<const std::string>(j.at("extensions").at("VRM").dump());return a;
}
}
int main(int argc,char**argv)try{
  auto f=fixture();auto raw=std::make_shared<const std::vector<uint8_t>>(f.bytes);auto a=appearance(raw);CreatorAssetArchive assets;PaintImageArchive images;
  auto record=encode_creator_appearance(a,assets,images);
  check(assets.size()==1&&assets.bytes()==raw->size()&&images.size()==1,"Creator archive retains one original GLB and one shared decoded image table");
  auto another=a;another.source_vertices={2,1,0};auto duplicate=encode_creator_appearance(another,assets,images);
  check(assets.size()==1&&images.size()==1&&duplicate.at("asset")==record.at("asset"),"Duplicated creator objects deduplicate source blobs and images by shared identity");
  auto saved=assets.encode();auto image_data=images.encode();auto restored_images=PaintImageArchive::decode(image_data);auto restored_assets=CreatorAssetArchive::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(saved)),restored_images);
  auto restored=decode_creator_appearance(record,restored_assets),other=decode_creator_appearance(duplicate,restored_assets);
  check(*restored->original_bytes==*raw&&restored->asset==other->asset&&restored->original_bytes==other->original_bytes&&restored->decoded_images==other->decoded_images,
    "Self-contained native restart preserves exact raw source and one immutable reconstructed asset per model");
  check(restored->decoded_images->at(0)==restored_images.at(0)&&restored->decoded_images->at(0)->rgba==a.decoded_images->at(0)->rgba,
    "Creator reload reuses native decoded image IDs without PNG decoding or alpha-data loss");
  check(restored->source_vertices==a.source_vertices&&other->source_vertices==another.source_vertices,"Independent native vertex remaps survive shared-source restart");
  check(restored->material_json==a.material_json&&restored->mtoon_json==a.mtoon_json&&*restored->vrm_json==*a.vrm_json,
    "Original material and arbitrary MToon/VRM metadata survive native restart exactly");
  const auto &p=restored->asset->primitives[0],&original=a.asset->primitives[0];
  check(p.uv1==original.uv1&&p.colors==original.colors&&p.tangents==original.tangents&&p.tangents[0][3]==-1,
    "Original secondary UVs, vertex colors and tangent handedness remain authored source attributes");
  check(p.morph_targets.size()==1&&p.morph_targets[0].normals[2].x==original.morph_targets[0].normals[2].x&&p.morph_targets[0].tangents[2].z==original.morph_targets[0].tangents[2].z&&p.morph_targets[0].positions[2].y==original.morph_targets[0].positions[2].y,
    "Morph position, normal and tangent accessors reconstruct without a lossy subset serializer");
  check(restored->asset->skins[0].inverse_bind_matrices==a.asset->skins[0].inverse_bind_matrices&&restored->asset->nodes[1].extras_json==a.asset->nodes[1].extras_json,
    "Authored inverse binds and source node metadata remain intact");
  auto invalid=record;invalid["asset"]=99;check(rejects([&]{decode_creator_appearance(invalid,restored_assets);}),"Missing creator source references fail explicitly");
  invalid=record;invalid["source_vertices"]=nlohmann::json::binary({1,2,3});check(rejects([&]{decode_creator_appearance(invalid,restored_assets);}),"Truncated native source-vertex mappings are rejected");
  invalid=record;invalid["source_vertices"]=nlohmann::json::binary({255,255,255,127});check(rejects([&]{decode_creator_appearance(invalid,restored_assets);}),"Out-of-range authored vertex references cannot enter a restored appearance");
  invalid=record;invalid["source_node"]=0;check(rejects([&]{decode_creator_appearance(invalid,restored_assets);}),"A creator instance must refer to the correct original mesh node");
  invalid=record;invalid["material"]=0;check(rejects([&]{decode_creator_appearance(invalid,restored_assets);}),"Original material identity cannot silently revert to the importer fallback");
  invalid=record;invalid["source_mesh"]=uint64_t(1)<<32;check(rejects([&]{decode_creator_appearance(invalid,restored_assets);}),"Large source indices cannot truncate into a valid appearance");
  auto malformed=saved;malformed[0]["images"][0]=99;check(rejects([&]{CreatorAssetArchive::decode(malformed,restored_images);}),"Missing decoded image archive references fail before source reconstruction");
  malformed=saved;malformed[0]["images"]=nlohmann::json::array();check(rejects([&]{CreatorAssetArchive::decode(malformed,restored_images);}),"Decoded image table counts must match the original source exactly");
  malformed=saved;auto broken=*raw;broken[0]=0;malformed[0]["original_glb"]=nlohmann::json::binary(broken);check(rejects([&]{CreatorAssetArchive::decode(malformed,restored_images);}),"Malformed embedded GLB headers fail before parser reconstruction");
  auto invalid_chunk=saved;auto chunk_bytes=*raw;size_t header=20+read_word(chunk_bytes,12);for(int k=0;k<4;++k)chunk_bytes[header+k]=255;
  invalid_chunk[0]["original_glb"]=nlohmann::json::binary(chunk_bytes);check(rejects([&]{CreatorAssetArchive::decode(invalid_chunk,restored_images);}),"Embedded source chunk lengths validate before parser allocation");
  CreatorArchiveLimits limits;limits.max_asset_bytes=raw->size()-1;CreatorAssetArchive bounded(limits);PaintImageArchive fresh;
  check(rejects([&]{bounded.add(a,fresh);})&&bounded.size()==0&&fresh.size()==0,"Per-source quota failures leave both registries unchanged");
  check(rejects([&]{CreatorAssetArchive::decode(saved,restored_images,limits);}),"Embedded source size limits apply on native restart");
  limits={};limits.max_total_bytes=raw->size()-1;check(rejects([&]{CreatorAssetArchive(limits).add(a,fresh);}),"Whole source registry byte limits reject oversized submissions");
  limits={};limits.max_json_bytes=8;check(rejects([&]{CreatorAssetArchive::decode(saved,restored_images,limits);}),"Embedded metadata bounds are checked before JSON allocation");
  limits={};limits.max_remap_vertices=2;CreatorAssetArchive compact(limits);check(rejects([&]{compact.add(a,fresh);})&&fresh.size()==0,"Per-object remap limits preserve unmodified source/image registries");
  PaintImageLimits tiny;tiny.max_unique_pixels=1;PaintImageArchive small_images(tiny);CreatorAssetArchive untouched;
  check(rejects([&]{untouched.add(a,small_images);})&&untouched.size()==0&&small_images.size()==0,"Image quota rejection atomically preserves the model and image archives");
  limits={};limits.cancelled=[] {return true;};check(rejects([&]{CreatorAssetArchive::decode(saved,restored_images,limits);}),"Cancelled source restoration cannot publish partial archive data");
  auto edited=a;edited.mtoon_json="{}";check(rejects([&]{assets.add(edited,images);}),"Edited material metadata requires separate editable state instead of silently replacing original provenance");
  auto rebound=a;auto altered_images=std::make_shared<std::vector<std::shared_ptr<const Image>>>(*a.decoded_images);(*altered_images)[0]=std::make_shared<const Image>(*altered_images->at(0));rebound.decoded_images=altered_images;
  check(rejects([&]{assets.add(rebound,images);}),"One shared source identity cannot silently change its decoded image ownership");
  auto wrong_images=PaintImageArchive();wrong_images.add(std::make_shared<const Image>(Image{1,1,{1,2,3,4}}));
  check(rejects([&]{assets.add(a,wrong_images);})&&wrong_images.size()==1,"Reusing a model archive with an incompatible image registry fails atomically");
  auto dimensional_images=image_data;dimensional_images[0]["width"]=1;dimensional_images[0]["height"]=2;auto dimensions=PaintImageArchive::decode(dimensional_images);
  check(rejects([&]{CreatorAssetArchive::decode(saved,dimensions);}),"Reused native PNG image dimensions must match the original encoded source header");
  CreatorAssetArchive content_distinct;PaintImageArchive content_images;content_distinct.add(a,content_images);auto raw_copy=a;raw_copy.original_bytes=std::make_shared<const std::vector<uint8_t>>(*raw);
  content_distinct.add(raw_copy,content_images);check(content_distinct.size()==2&&content_images.size()==1,"Raw source deduplication uses ownership identity without claiming content hashing");
  auto external=f.json;external["images"][0].erase("bufferView");external["images"][0]["uri"]="/etc/passwd";
  size_t bin_at=20+read_word(f.bytes,12)+8;std::vector<uint8_t> bin(f.bytes.begin()+bin_at,f.bytes.end());auto external_archive=saved;external_archive[0]["original_glb"]=nlohmann::json::binary(glb(external,std::move(bin)));
  check(rejects([&]{CreatorAssetArchive::decode(external_archive,restored_images);}),"Embedded model restoration rejects external file references without a filesystem resolver");
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=a.decoded_images->at(0);Scene scene;
  scene.active().painted=PaintedMesh::from_maps(a.asset->primitives[0].mesh,maps,64);scene.active().mesh_active=true;scene.active().appearance=std::make_shared<const CreatorAppearance>(a);
  auto duplicate_id=scene.duplicate(scene.active().id);auto native=scene.encode();
  check(native.at("source_models").size()==1&&native.at("source_images").size()==1,"Scene v5 stores one shared original model and source image across creator duplicates");
  auto native_restart=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(native)));const auto &first=native_restart.objects()[0],&copy=native_restart.object(duplicate_id);
  check(first.appearance->asset==copy.appearance->asset&&*first.appearance->original_bytes==*raw&&first.painted->source_maps()->color.image==first.appearance->decoded_images->at(0),
    "Scene CBOR restart shares exact creator assets and source-backed paint image ownership");
  for(int arg=1;arg<argc;++arg){
    std::ifstream file(argv[arg],std::ios::binary|std::ios::ate);auto size=file.tellg();if(!file||size<28||size>64*1024*1024)throw std::runtime_error("Invalid actual baseline fixture");
    auto bytes=std::make_shared<std::vector<uint8_t>>(size_t(size));file.seekg(0);file.read(reinterpret_cast<char*>(bytes->data()),size);if(!file)throw std::runtime_error("Incomplete actual source read");
    auto actual=appearance(bytes);CreatorAssetArchive models;PaintImageArchive decoded;auto object=encode_creator_appearance(actual,models,decoded);auto second=encode_creator_appearance(actual,models,decoded);
    auto json=nlohmann::json::parse(bytes->begin()+20,bytes->begin()+20+read_word(*bytes,12));std::vector<nlohmann::json> appearances;
    for(auto &primitive:actual.asset->primitives){auto part=actual;part.source_mesh=primitive.source_mesh;part.source_primitive=primitive.source_primitive;part.material=int(primitive.material);part.source_material=part.material-1;
      for(size_t n=0;n<actual.asset->nodes.size();++n)if(actual.asset->nodes[n].mesh==part.source_mesh){part.source_node=int(n);break;}
      part.material_json=json.at("materials").at(part.source_material).dump();part.mtoon_json=json.at("extensions").at("VRM").at("materialProperties").at(part.source_material).dump();
      part.source_vertices.clear();for(uint32_t v=0;v<primitive.mesh.positions.size();++v)part.source_vertices.push_back(v);appearances.push_back(encode_creator_appearance(part,models,decoded));
    }
    auto image_restart=PaintImageArchive::decode(decoded.encode());auto model_restart=CreatorAssetArchive::decode(models.encode(),image_restart);auto one=decode_creator_appearance(object,model_restart),two=decode_creator_appearance(second,model_restart);
    check(*one->original_bytes==*bytes&&one->asset==two->asset&&one->decoded_images->size()==25&&one->decoded_images->at(0)==image_restart.at(0),"Actual CC0 VRoid source and all25 shared images restore without catalog files or PNG redecode");
    bool identities=appearances.size()==19;for(auto &part:appearances){auto restored_part=decode_creator_appearance(part,model_restart);identities&=restored_part->asset==one->asset&&restored_part->decoded_images==one->decoded_images;}
    check(identities&&models.size()==1,"All19 original source primitive identities and remaps reuse one embedded baseline model");
    bool exact=true;for(size_t k=0;k<actual.asset->primitives.size();++k){auto &before=actual.asset->primitives[k],&after=one->asset->primitives[k];
      exact&=before.colors==after.colors&&before.tangents==after.tangents&&before.uv1==after.uv1&&before.morph_targets.size()==after.morph_targets.size();
      for(size_t t=0;t<before.morph_targets.size();++t){auto &b=before.morph_targets[t],&a=after.morph_targets[t];exact&=b.normals.size()==a.normals.size()&&b.tangents.size()==a.tangents.size()&&b.positions.size()==a.positions.size();
        for(size_t v=0;v<b.normals.size();++v)exact&=length(b.normals[v]-a.normals[v])==0;
        for(size_t v=0;v<b.tangents.size();++v)exact&=length(b.tangents[v]-a.tangents[v])==0;
        for(size_t v=0;v<b.positions.size();++v)exact&=length(b.positions[v]-a.positions[v])==0;}
    }
    check(exact&&one->material_json==actual.material_json&&one->mtoon_json==actual.mtoon_json&&*one->vrm_json==*actual.vrm_json,"Actual source attributes and complete original material/MToon/VRM metadata survive native restart");
    std::cout<<"BASELINE "<<argv[arg]<<" raw_bytes="<<bytes->size()<<" images="<<decoded.size()<<" primitives="<<one->asset->primitives.size()<<'\n';
  }
  std::cout<<"Native creator source archive tests complete\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
