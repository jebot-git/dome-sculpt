// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-field-codec.hpp"
#include "texture-set-paint-access.hpp"
#include "polygon-surface-archive.hpp"
#include "polygon-surface-preview.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/material-bindings.hpp"
namespace dome::detail {namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set field codec cancelled");}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
uint64_t number(const nlohmann::json&v,uint64_t maximum){if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>maximum)throw std::invalid_argument("Invalid source field archive identity");return v.get<uint64_t>();}
size_t reference(const nlohmann::json&v,size_t count){const auto n=number(v,SIZE_MAX);if(n>=count)throw std::invalid_argument("Source field archive refers outside its actual pool");return size_t(n);}
nlohmann::json material(const Material&m){return {{"name",m.name},{"color",m.color},{"metallic",m.metallic},{"roughness",m.roughness},{"emissive",m.emissive},{"toon",m.toon},{"double_sided",m.double_sided},{"alpha",int(m.alpha)},{"alpha_cutoff",m.alpha_cutoff},{"unlit",m.unlit}};}
Material material(const nlohmann::json&j){if(!j.is_object()||j.size()!=10)throw std::invalid_argument("Source field material is incomplete");Material m{j.at("name").get<std::string>(),j.at("color").get<std::array<float,4>>(),j.at("metallic").get<float>(),j.at("roughness").get<float>(),j.at("emissive").get<std::array<float,3>>(),j.at("toon").get<bool>(),j.at("double_sided").get<bool>()};m.alpha=Material::AlphaMode(number(j.at("alpha"),3));m.alpha_cutoff=j.at("alpha_cutoff").get<float>();m.unlit=j.at("unlit").get<bool>();m.validate();return m;}
void payload_budget(const nlohmann::json&j,const TextureSetLimits&l,size_t&bytes,size_t&entries,size_t depth=0){
 stop(l);if(depth>24||++entries>l.maximum_work)throw std::length_error("Source field payload depth/entries exceed quota");const auto add=[&](size_t n){if(n>l.maximum_bytes-bytes)throw std::length_error("Source field payload storage exceeds quota");bytes+=n;};add(sizeof(nlohmann::json)+32);
 if(j.is_binary())add(j.get_binary().size());else if(j.is_string())add(j.get_ref<const std::string&>().size());else if(j.is_array())for(const auto&v:j)payload_budget(v,l,bytes,entries,depth+1);else if(j.is_object())for(auto i=j.begin();i!=j.end();++i){add(i.key().size()+32);payload_budget(i.value(),l,bytes,entries,depth+1);}
}
}
TextureSetFieldCodec::TextureSetFieldCodec(TextureSetLimits l):limits_(std::move(l)){limits_.validate();}
TextureSetFieldCodec::TextureSetFieldCodec(const nlohmann::json&records,GeometryReader geometry,size_t geometries,size_t nodes,TextureSetLimits l):limits_(std::move(l)),input_(&records),geometry_(std::move(geometry)),geometry_count_(geometries),node_count_(nodes){limits_.validate();preflight();read_.resize(records.size());used_.resize(records.size());}
void TextureSetFieldCodec::preflight()const{
 if(!input_||!input_->is_array()||input_->size()>512||!geometry_||!geometry_count_||geometry_count_>256||node_count_>128)throw std::invalid_argument("Source fields require their actual bounded owning geometry/node pools");
 size_t bytes=0,entries=0;
 for(const auto&r:*input_){
  stop(limits_);if(!r.is_object()||r.size()!=10||!r.at("version").is_number_integer()||r.at("version")!=1||r.at("kind")!="polygon"||!r.at("physical_paint").is_boolean()||!r.at("original").is_object()||r.at("original").contains("mesh")||r.at("original").contains("version")||!r.at("name").is_string()||r.at("name").get_ref<const std::string&>().size()>256)throw std::invalid_argument("Incomplete actual retained source field seed");
  (void)reference(r.at("geometry"),geometry_count_);(void)reference(r.at("node"),node_count_);(void)number(r.at("binding"),255);(void)material(r.at("material"));PolygonSurfaceTransferLimits policy;policy.maximum_working_bytes=limits_.maximum_bytes;policy.geometry.cancelled=limits_.cancelled;(void)PolygonSurfacePaintCodec::preflight_grid(r.at("original"),r.at("physical_paint").get<bool>(),true,policy);payload_budget(r,limits_,bytes,entries);
 }
}
nlohmann::json TextureSetFieldCodec::encode(const TextureSetView&view,uint32_t geometry,PaintImageArchive&images,NormalSamplingArchive*normal,PaintSamplingArchive*sampling,const MaterialWriter&materials){
 stop(limits_);if(!view.source)return TextureSetPaintAccess::encode(*view.painted,&images,normal,sampling);
 view.source->authenticate_owner(view.source);view.source->validate_field(view,limits_);if(!materials||!view.material_bindings||!view.source->polygon_owner())throw std::invalid_argument("Source Paint7 requires its actual material/node writer");
 const auto binding=materials(view.material_bindings),node=PolygonSurfaceArchive::writer().add(view.material_bindings);size_t id=0;
 for(;id<written_.size();++id){const auto&w=written_[id];if(w.geometry==geometry&&owner(w.source->original_paint(),view.source->original_paint())&&owner(w.source->field().material_bindings,view.material_bindings)&&owner(w.source->field().bake_recipe,view.bake_recipe)){if(w.node!=node||w.binding!=binding)throw std::invalid_argument("Actual field seed changed its node/material archive identity");break;}}
 if(id==written_.size()){
  if(id>=512)throw std::length_error("Source field seed count exceeds its owning quota");
  auto original=TextureSetPaintAccess::encode_source_grid(*view.source->original_paint(),view.source,&images,normal,sampling);
  nlohmann::json record={{"version",1},{"kind","polygon"},{"geometry",geometry},{"node",node},{"binding",binding},{"physical_paint",view.source->original_paint()->has_physical_emission()},{"original",std::move(original)},{"name",view.source->field().name},{"material",material(view.source->field().material)},{"bake_recipe",view.bake_recipe?view.bake_recipe->encode():nlohmann::json(nullptr)}};
  written_.push_back({view.source,geometry,node,binding,std::move(record)});
 }
 const auto readable=view.source->read_view(view,limits_);auto payload=TextureSetPaintAccess::encode_source_grid(*readable,view.source,&images,normal,sampling);payload.erase("baked");
 return {{"version",7},{"set",view.id},{"field_source",id},{"physical_paint",view.painted->has_physical_emission()},{"payload",std::move(payload)}};
}
DecodedTextureSetField TextureSetFieldCodec::decode(const nlohmann::json&j,const std::shared_ptr<const TextureSetFaceDomain>&domain,uint32_t geometry,const Material&current,const PaintImageArchive&images,const NormalSamplingArchive*normal,const PaintSamplingArchive*sampling,const MaterialReader&materials,bool polygon,const PaintedMesh*raster,const SourcePaintDecodeContext&context){
 stop(limits_);if(!j.is_object()||!j.contains("field_source"))return {TextureSetPaintAccess::decode(j,&images,polygon,normal,sampling,context,domain,raster),{},{},{},{}};
 preflight();if(!domain||!materials||!j.is_object()||j.size()!=5||!j.at("version").is_number_integer()||j.at("version")!=7||number(j.at("set"),UINT32_MAX)!=domain->set()||!j.at("physical_paint").is_boolean()||!j.at("payload").is_object()||j.at("payload").contains("mesh")||j.at("payload").contains("baked")||j.at("payload").contains("version"))throw std::invalid_argument("Source Paint7 lacks its actual member/current-grid envelope");domain->authenticate_owner(domain);
 PolygonSurfaceTransferLimits wire_policy;wire_policy.maximum_working_bytes=limits_.maximum_bytes;wire_policy.geometry.cancelled=limits_.cancelled;(void)PolygonSurfacePaintCodec::preflight_grid(j.at("payload"),j.at("physical_paint").get<bool>(),true,wire_policy);
 const auto id=reference(j.at("field_source"),read_.size());const auto&record=input_->at(id);if(reference(record.at("geometry"),geometry_count_)!=geometry||!owner(domain->topology()->geometry(),geometry_(geometry)))throw std::invalid_argument("Source Paint7 names another actual geometry owner");
 const auto node=uint32_t(reference(record.at("node"),node_count_));const auto binding=materials(uint32_t(number(record.at("binding"),255)));const auto&stage=PolygonSurfaceArchive::reader().stage_at(node);
 if(!binding||!owner(binding->polygon_surface_transfer,stage.owner)||!owner(stage.paint->mesh_owner(),domain->topology()->geometry()))throw std::invalid_argument("Source Paint7 material/node/geometry controls do not belong to the same replay");
 if(!read_[id]){
  const auto physical=record.at("physical_paint").get<bool>();if(physical&&!context.physical_emission)throw std::invalid_argument("Actual source FLOAT planes require their enclosing Scene physical context");auto original=TextureSetPaintAccess::decode_source_grid(record.at("original"),&images,polygon,normal,sampling,SourcePaintDecodeContext{false,physical},stage,*stage.paint);
  if(original->has_physical_emission()!=physical)throw std::invalid_argument("Source seed physical interpretation differs from its actual planes");
  Read seed;seed.node=node;seed.geometry=geometry;seed.seed={0,record.at("name").get<std::string>(),std::move(original),material(record.at("material")),{},binding,record.at("bake_recipe").is_null()?nullptr:ObjectBakeRecipe::decode(record.at("bake_recipe")),{}, {}};
  PolygonSurfaceTransferLimits policy;policy.maximum_working_bytes=limits_.maximum_bytes;policy.geometry.cancelled=limits_.cancelled;seed.preview=PolygonSurfacePreview::create(*seed.seed.painted,binding,policy);read_[id]=std::move(seed);
 }
 auto&seed=*read_[id];if(!owner(seed.seed.material_bindings,binding)||seed.node!=node||seed.geometry!=geometry)throw std::invalid_argument("Source field seed changed its actual owning reader prefix");
 const auto proto=TextureSetFieldSource::capture_prepared(seed.seed,domain,seed.preview,limits_,raster);const auto physical=j.at("physical_paint").get<bool>();if(physical&&!context.physical_emission)throw std::invalid_argument("Current source FLOAT planes require their actual enclosing Scene context");auto paint=TextureSetPaintAccess::decode_source_grid(j.at("payload"),&images,polygon,normal,sampling,SourcePaintDecodeContext{false,physical},stage,*proto->field().painted,domain,proto);
 if(paint->has_physical_emission()!=physical)throw std::invalid_argument("Source member physical interpretation differs from its actual current planes");auto view=proto->field();view.painted=paint;view.material=current;auto source=proto->with_current(view,limits_);used_[id]=1;return {std::move(paint),std::move(source),binding,{},seed.seed.bake_recipe};
}
nlohmann::json TextureSetFieldCodec::records()const{if(input_)return *input_;auto out=nlohmann::json::array();for(const auto&r:written_)out.push_back(r.record);return out;}
std::shared_ptr<const Mesh>TextureSetFieldCodec::geometry_for_node(uint32_t node)const{
 if(!input_)throw std::invalid_argument("Source geometry replay requires an actual reader table");preflight();std::optional<size_t>index;
 for(const auto&r:*input_)if(number(r.at("node"),node_count_?node_count_-1:0)==node){const auto i=reference(r.at("geometry"),geometry_count_);if(index&&*index!=i)throw std::invalid_argument("One actual polygon node cannot own two different native geometry entries");index=i;}
 return index?geometry_(*index):std::shared_ptr<const Mesh>{};
}
void TextureSetFieldCodec::require_complete()const{if(input_)for(size_t i=0;i<used_.size();++i)if(!used_[i])throw std::invalid_argument("Unreferenced actual source field seed in the owning archive");}
}
