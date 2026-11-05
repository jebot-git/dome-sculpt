// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-archive.hpp"
#include "paint-archive-shape.hpp"
#include "paint-sampling-access.hpp"
namespace dome::detail {
thread_local CanonicalPaintArchive *CanonicalPaintArchive::current_=nullptr;
namespace {
size_t ref(const nlohmann::json &v,size_t count){
 if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>=count)
  throw std::invalid_argument("Canonical paint archive reference is outside its actual owning pool");
 return size_t(v.get<uint64_t>());
}
uint32_t word(const nlohmann::json::binary_t &bytes,size_t at){return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;}
template<class T>bool own(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
}
CanonicalPaintArchive::CanonicalPaintArchive(PolygonSceneArchive &p,BakedNormalArchive &n,PaintSamplingArchive &d)
 :previous_(current_),write_origins_(&p),normals_(n),write_domains_(&d),domains_(d){current_=this;}
CanonicalPaintArchive::CanonicalPaintArchive(const nlohmann::json &j,PolygonSceneReader &p,BakedNormalArchive &n,const PaintSamplingArchive &d)
 :previous_(current_),read_origins_(&p),normals_(n),domains_(d),input_(&j){
 if(!j.is_object()||j.size()!=2||j.at("version")!=1||!j.at("nodes").is_array()||j.at("nodes").empty()||j.at("nodes").size()>128)
  throw std::invalid_argument("Invalid owning canonical paint pool");
 nodes_.resize(j.at("nodes").size());busy_.resize(nodes_.size());used_.resize(nodes_.size());current_=this;
}
CanonicalPaintArchive::~CanonicalPaintArchive(){current_=previous_;}
CanonicalPaintArchive &CanonicalPaintArchive::writer(){if(!current_||!current_->write_origins_)throw std::invalid_argument("Canonical paint requires its owning Scene37 writer");return *current_;}
CanonicalPaintArchive &CanonicalPaintArchive::reader(){if(!current_||!current_->input_)throw std::invalid_argument("Canonical paint requires its owning Scene37 reader");return *current_;}
uint32_t CanonicalPaintArchive::add(std::shared_ptr<const CanonicalPaintSampling> p){
 if(!write_origins_||!write_domains_||!p||!p->valid_owner(p))throw std::invalid_argument("Canonical paint writer requires a genuine factory lease");
 if(p->kind()!=CanonicalPaintKind::CanonicalUv)throw std::invalid_argument("Independent paint geometry requires its owning Scene38 codec");
 const auto found=std::find(nodes_.begin(),nodes_.end(),p);if(found!=nodes_.end())return uint32_t(found-nodes_.begin());
 if(nodes_.size()>=128)throw std::length_error("Canonical paint node count exceeds its owning pool limit");
 p->validate();write_origins_->add_origin(p->before());normals_.add_uv(p->normal_uv());write_domains_->add(p->attachment()->domain());
 nodes_.push_back(std::move(p));return uint32_t(nodes_.size()-1);
}
nlohmann::json CanonicalPaintArchive::encode() const{
 if(!write_origins_||!write_domains_)throw std::logic_error("Cannot encode a canonical paint reader");
 nlohmann::json out={{"version",1},{"nodes",nlohmann::json::array()}};
 for(const auto &p:nodes_){
  std::vector<uint8_t> bytes;bytes.reserve(p->current_sources().size()*4);
  for(auto v:p->current_sources())for(unsigned k=0;k<4;++k)bytes.push_back(uint8_t(v>>(k*8)));
  out["nodes"].push_back({{"version",1},{"before_origin",write_origins_->add_origin(p->before())},
   {"normal_uv",normals_.add_uv(p->normal_uv())},{"domain",write_domains_->add(p->attachment()->domain())},
   {"vertices",nlohmann::json::binary(std::move(bytes))}});
 }
 return out;
}
std::shared_ptr<const CanonicalPaintSampling> CanonicalPaintArchive::at(uint32_t id){
 if(!input_||!read_origins_||id>=nodes_.size())throw std::invalid_argument("Canonical paint reader identity is unavailable");
 if(busy_[id])throw std::invalid_argument("Canonical paint material/origin history contains a cycle");
 busy_[id]=1;struct Reset{uint8_t &b;~Reset(){b=0;}}reset{busy_[id]};
 const auto &j=input_->at("nodes")[id];
 auto before=read_origins_->origin_at(uint32_t(ref(j.at("before_origin"),UINT32_MAX)));
 auto uv=normals_.uv_at(uint32_t(ref(j.at("normal_uv"),UINT32_MAX)));
 auto domain=domains_.at(uint32_t(ref(j.at("domain"),domains_.size())));
 if(nodes_[id]){
  if(!own(nodes_[id]->before(),before)||!own(nodes_[id]->normal_uv(),uv)||!own(nodes_[id]->attachment()->domain(),domain))
   throw std::invalid_argument("Canonical paint completed node changed its exact owning context");
  nodes_[id]->validate();used_[id]=1;return nodes_[id];
 }
 const auto &bytes=j.at("vertices").get_binary();if(bytes.empty()||bytes.size()>8'000'000||bytes.size()%4)throw std::length_error("Canonical paint current map exceeds its bounded dimensions");std::vector<uint32_t> vertices;vertices.reserve(bytes.size()/4);
 for(size_t at=0;at<bytes.size();at+=4)vertices.push_back(word(bytes,at));
 auto owner=CanonicalPaintSamplingCodec::replay(std::move(before),std::move(uv),std::move(domain),vertices);
 nodes_[id]=std::move(owner);used_[id]=1;return nodes_[id];
}
PolygonSceneReader::MaterialResolver CanonicalPaintArchive::material_resolver(PolygonSceneReader::MaterialResolver next){
 if(!read_origins_)throw std::logic_error("Canonical paint material resolver is not in a reader");return read_origins_->material_resolver(std::move(next));
}
void CanonicalPaintArchive::restore_material_resolver(PolygonSceneReader::MaterialResolver previous) noexcept{
 read_origins_->restore_material_resolver(std::move(previous));
}
void CanonicalPaintArchive::require_complete() const{
 if(!input_||std::any_of(used_.begin(),used_.end(),[](auto n){return !n;}))throw std::invalid_argument("Unreferenced canonical paint owning node");
}
size_t CanonicalPaintArchive::preflight(const nlohmann::json &j,const nlohmann::json &polygons,
 const nlohmann::json &normals,const nlohmann::json &domains,const nlohmann::json &materials,const SourcePaintDecodeContext &context){
 if(!j.is_object()||j.size()!=2||!j.at("version").is_number_integer()||j.at("version")!=1||
    !j.at("nodes").is_array()||j.at("nodes").empty()||j.at("nodes").size()>128||normals.at("version")!=2)
  throw std::invalid_argument("Scene37 requires actual bounded canonical paint and normal UV pools");
 size_t bytes=0,peak_scratch=0;constexpr size_t limit=256*1024*1024;
 for(const auto &p:j.at("nodes")){
  if(!p.is_object()||p.size()!=5||!p.at("version").is_number_integer()||p.at("version")!=1||!p.at("vertices").is_binary())
   throw std::invalid_argument("Invalid canonical paint owning correspondence record");
  const auto &origin=polygons.at("origins").at(ref(p.at("before_origin"),polygons.at("origins").size()));
  const auto &before=materials.at("bindings").at(ref(origin.at("material_bindings"),materials.at("bindings").size()));
  const auto &uv=normals.at("uv").at(ref(p.at("normal_uv"),normals.at("uv").size()));
  ref(p.at("domain"),domains.at("domains").size());
  if(before.at("canonical_attributes").is_null()||before.at("canonical_attributes")!=uv.at("origin"))
   throw std::invalid_argument("Canonical paint before binding and independent normal root differ");
  const auto &layout=uv.at("layout");SourceNormalUvLayout::encoded_storage_bytes(layout);
  const auto &map=p.at("vertices").get_binary();const auto vertices=map.size()/4;
  if(!vertices||vertices>2'000'000||map.size()!=vertices*4||map.size()!=layout.at("source_vertices").get_binary().size())
   throw std::length_error("Canonical paint current/before map dimensions exceed their bound");
  const auto &record=polygons.at("paints").at(ref(origin.at("painted"),polygons.at("paints").size()));
  const auto &paint=paint_archive_payload(record,true,context);
  if(paint.contains("uv_policy"))throw std::invalid_argument("Canonical paint source needs its actual strict writable raster");
  const auto &positions=paint.at("mesh").at("positions");if(!positions.is_binary()||positions.get_binary().size()%12)throw std::invalid_argument("Canonical paint before geometry is unavailable");
  for(size_t at=0;at<map.size();at+=4)if(word(map,at)>=positions.get_binary().size()/12)throw std::invalid_argument("Canonical paint map exceeds its genuine before vertex domain");
  const auto surface=paint_sampling_surface_working_bytes(vertices,layout.at("indices").get_binary().size()/4,paint.at("resolution").get<uint32_t>());
  const auto retained=paint_sampling_surface_retained_bytes(vertices,layout.at("indices").get_binary().size()/4,paint.at("resolution").get<uint32_t>());
  // Each node keeps one authenticated SurfaceMap. Raster and padding scratch
  // are sequential per replay, not an extra persistent grid for every node.
  peak_scratch=std::max(peak_scratch,surface-retained);
  if(retained>limit-bytes||map.size()>limit-bytes-retained)throw std::length_error("Canonical paint retained replay storage exceeds its owning limit before allocation");
  bytes+=retained+map.size();
  if(peak_scratch>limit-bytes)throw std::length_error("Canonical paint retained owners plus peak replay scratch exceed their owning limit before allocation");
 }
 // Compute context-independent subtree heights and greatest material IDs.
 // A completed hit still pays the caller's depth/earlier-prefix requirement.
 // This prevents both cross-pool cycles and a shared shallow child hiding an
 // over-depth material -> origin -> material path before payload allocation.
 struct Reach {size_t height=1,material=0;bool has_material=false;};
 const auto &oo=polygons.at("origins"),&aa=polygons.at("authorities");
 std::vector<Reach> origins(oo.size()),authorities(aa.size());std::vector<uint8_t> os(oo.size()),as(aa.size());
 std::function<Reach(size_t,size_t)> origin,authority;
 auto merge=[](Reach &a,const Reach &b,size_t distance){a.height=std::max(a.height,b.height+distance);if(b.has_material){a.has_material=true;a.material=std::max(a.material,b.material);}};
 origin=[&](size_t id,size_t depth)->Reach{
  if(depth>32||os[id]==1)throw std::invalid_argument("Canonical paint before history has a cycle or excessive depth");
  if(os[id]==2){if(origins[id].height>33-depth)throw std::length_error("Canonical paint shared before height exceeds depth");return origins[id];}
  os[id]=1;Reach result;const auto &o=oo[id];
  if(!o.at("material_bindings").is_null()){result.has_material=true;result.material=ref(o.at("material_bindings"),materials.at("bindings").size());}
  if(!o.at("polygon").is_null())merge(result,authority(ref(o.at("polygon"),aa.size()),depth+1),1);
  for(const char *field:{"shape_source","rig_source"})if(o.contains(field)&&!o.at(field).is_null())merge(result,origin(ref(o.at(field),oo.size()),depth+1),1);
  os[id]=2;return origins[id]=result;
 };
 authority=[&](size_t id,size_t depth)->Reach{
  if(depth>32||as[id]==1)throw std::invalid_argument("Canonical paint before authority has a cycle or excessive depth");
  if(as[id]==2){if(authorities[id].height>33-depth)throw std::length_error("Canonical paint shared authority height exceeds depth");return authorities[id];}
  as[id]=1;Reach result;const auto &a=aa[id];if(!a.at("origin").is_null())merge(result,origin(ref(a.at("origin"),oo.size()),depth),0);
  as[id]=2;return authorities[id]=result;
 };
 for(size_t id=0;id<materials.at("bindings").size();++id){const auto &binding=materials.at("bindings")[id];
  if(binding.at("canonical_paint_sampling").is_null())continue;
  const auto &node=j.at("nodes").at(ref(binding.at("canonical_paint_sampling"),j.at("nodes").size()));
  const auto before=origin(ref(node.at("before_origin"),oo.size()),1);
  if(before.has_material&&before.material>=id)throw std::invalid_argument("Canonical paint before history must resolve only actual earlier materials");
 }
 return bytes;
}
}
