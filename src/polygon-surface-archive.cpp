// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-archive.hpp"
#include "polygon-surface-context.hpp"
#include "material-archive-version.hpp"
#include "paint-archive-shape.hpp"
#include "paint-sampling-access.hpp"
#include "deformation-json.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
namespace dome::detail {
thread_local const PolygonSurfacePreflight*PolygonSurfacePreflight::current_=nullptr;
thread_local PolygonSurfaceArchive*PolygonSurfaceArchive::current_=nullptr;
namespace {
// Resolved source roots outlive the prefix reader's finish/move operation.
// Reserve the complete fixed index/control block before allocating it; current
// Paint6 decoding must not reopen an already finished polygon graph reader.
constexpr size_t reader_root_index_bytes=16*1024;
PolygonSurfaceTransferLimits reader_limits(){PolygonSurfaceTransferLimits limits;limits.maximum_working_bytes-=reader_root_index_bytes;return limits;}
size_t ref(const nlohmann::json&v,size_t n){if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>=n)throw std::invalid_argument("Polygon surface owning reference exceeds its actual pool");return size_t(v.get<uint64_t>());}
size_t deformation_copy_bytes(const nlohmann::json&d){
 if(d.is_null())return 0;size_t entries=encoded_pose_matrix_entries(d);const auto add=[&](size_t n){if(n>8'000'000-entries)throw std::length_error("Scene39 before deformation exceeds its actual replay quota");entries+=n;};
 if(!d.at("pose").is_array()||!d.at("morph_weights").is_array()||d.at("morph_weights").size()>128)throw std::invalid_argument("Scene39 before pose/morph dimensions differ");add(d.at("pose").size());add(d.at("morph_weights").size());size_t vertices=0,bones=0,names=0;
 if(!d.at("skin").is_null()){const auto&rows=d.at("skin").at("vertices");if(!rows.is_array()||rows.size()>2'000'000)throw std::length_error("Scene39 before skin dimensions exceed their bound");vertices=rows.size();for(const auto&row:rows){if(!row.is_array()||row.size()>32)throw std::invalid_argument("Invalid Scene39 before skin row");add(row.size());}}
 if(!d.at("morphs").is_null()){const auto&targets=d.at("morphs").at("targets");if(!targets.is_array()||targets.size()>128)throw std::length_error("Scene39 before morph target count exceeds its bound");for(const auto&t:targets){if(!t.at("delta").is_array()||!t.at("name").is_string())throw std::invalid_argument("Invalid Scene39 before morph payload");add(t.at("delta").size());const auto n=t.at("name").get_ref<const std::string&>().size();if(n>16*1024*1024-names)throw std::length_error("Scene39 before name metadata exceeds its bound");names+=n;}}
 if(!d.at("skeleton").is_null()){const auto&joints=d.at("skeleton").at("bones");if(!joints.is_array()||joints.size()>256)throw std::length_error("Scene39 before skeleton dimensions exceed their bound");bones=joints.size();for(const auto&bone:joints){if(!bone.at("name").is_string())throw std::invalid_argument("Invalid Scene39 before bone name");const auto n=bone.at("name").get_ref<const std::string&>().size();if(n>16*1024*1024-names)throw std::length_error("Scene39 before name metadata exceeds its bound");names+=n;}}
 return entries*sizeof(Vec3)+d.at("pose").size()*sizeof(JointTransform)+vertices*sizeof(VertexWeights)+bones*(sizeof(Bone)+sizeof(RigMatrix)*3+64)+names;
}
// Scene40 may carry an independent genuine39 actor. Establish the owning40
// archive structurally before allowing that outer context; a larger version
// alone never grants a polygon field or geometry/membership authority.
void texture_set_context(const nlohmann::json&scene){
 if(!scene.at("objects").is_array()||scene.at("objects").empty()||scene.at("objects").size()>256)throw std::invalid_argument("Mixed Scene40 object table exceeds its bounded domain");
 const auto&table=scene.at("texture_set_geometry");
 if(!table.is_object()||table.size()!=3||!table.at("version").is_number_integer()||table.at("version")!=1||!table.at("meshes").is_array()||table.at("meshes").empty()||table.at("meshes").size()>256||!table.at("topologies").is_array()||table.at("topologies").empty()||table.at("topologies").size()>256)throw std::invalid_argument("Mixed Scene40 needs its actual bounded texture-set geometry archive");
 const auto&meshes=table.at("meshes"),&topologies=table.at("topologies");
 size_t bytes=0,work=0,sets=0;std::array<uint8_t,256>used_mesh{},used_topology{};
 const auto charge=[&](size_t n){if(n>256*1024*1024-bytes)throw std::length_error("Mixed Scene40 texture-set encoded geometry exceeds its quota");bytes+=n;};
 const auto step=[&](size_t n){if(n>64'000'000-work)throw std::length_error("Mixed Scene40 texture-set preflight exceeds its work quota");work+=n;};
 const auto blob=[&](const nlohmann::json&v,size_t stride,size_t maximum,bool empty=false)->const auto&{
  if(!v.is_binary()||(!empty&&v.get_binary().empty())||v.get_binary().size()%stride||v.get_binary().size()/stride>maximum)throw std::invalid_argument("Mixed Scene40 texture-set geometry/identity buffer is invalid");charge(v.get_binary().size());return v.get_binary();
 };
 const auto word=[](const auto&v,size_t at,size_t count){uint64_t n=0;for(size_t k=0;k<count;++k)n|=uint64_t(v.at(at+k))<<(8*k);return n;};
 for(const auto&m:meshes){
  if(!m.is_object()||m.size()!=6)throw std::invalid_argument("Mixed Scene40 texture-set mesh record is incomplete");
  const auto&p=blob(m.at("positions"),12,2'000'000),&n=blob(m.at("normals"),12,2'000'000),&uv=blob(m.at("uv"),8,2'000'000),&indices=blob(m.at("indices"),12,2'000'000);const size_t vertices=p.size()/12;
  if(n.size()!=p.size()||uv.size()/8!=vertices)throw std::invalid_argument("Mixed Scene40 texture-set vertex arrays differ");
  const auto&pins=blob(m.at("uv_pins"),1,vertices,true),&seams=blob(m.at("uv_seams"),1,indices.size()/4,true);
  if((!pins.empty()&&pins.size()!=vertices)||(!seams.empty()&&seams.size()!=indices.size()/4))throw std::invalid_argument("Mixed Scene40 texture-set UV flags differ");
  step(indices.size()/4);for(size_t i=0;i<indices.size();i+=4)if(word(indices,i,4)>=vertices)throw std::invalid_argument("Mixed Scene40 texture-set index lies outside its actual geometry");
 }
 for(const auto&t:topologies){
  if(!t.is_object()||t.size()!=4||!t.at("polygon_ids").is_boolean())throw std::invalid_argument("Mixed Scene40 texture-set topology record is incomplete");
  const auto mesh=ref(t.at("mesh"),meshes.size());used_mesh[mesh]=1;const size_t corners=meshes[mesh].at("indices").get_binary().size()/4;
  const auto&faces=blob(t.at("faces"),8,2'000'000),&ids=blob(t.at("corners"),8,6'000'000);
  if(faces.size()/8!=corners/3||ids.size()/8!=corners)throw std::invalid_argument("Mixed Scene40 texture-set topology IDs differ from actual triangles");
  step(faces.size()/8+ids.size()/8);for(size_t i=0;i<faces.size();i+=8)if(!word(faces,i,8))throw std::invalid_argument("Mixed Scene40 material face identity is absent");for(size_t i=0;i<ids.size();i+=8)if(!word(ids,i,8))throw std::invalid_argument("Mixed Scene40 material corner identity is absent");
 }
 for(const auto&o:scene.at("objects")){
  if(!o.contains("texture_sets"))throw std::invalid_argument("Mixed Scene40 omits actual per-object texture-set membership");
  const auto&c=o.at("texture_sets");if(c.is_null())continue;
  if(!c.is_object()||c.size()!=7||!c.at("version").is_number_integer()||c.at("version")!=1||!c.at("additional").is_array()||c.at("additional").empty()||c.at("additional").size()>63||!o.at("painted").is_null()||o.at("mesh_active")!=true)throw std::invalid_argument("Mixed Scene40 needs its actual owning material collection");
  if(c.at("additional").size()+1>256-sets)throw std::length_error("Mixed Scene40 aggregate material slots exceed quota");sets+=c.at("additional").size()+1;
  const auto topology=ref(c.at("topology"),topologies.size());used_topology[topology]=1;const auto&slots=blob(c.at("slots"),4,2'000'000);
  if(slots.size()/4!=topologies[topology].at("faces").get_binary().size()/8)throw std::invalid_argument("Mixed Scene40 membership omits genuine logical faces");
  std::array<uint64_t,64>ids{};size_t count=1;
  const auto integer=[](const nlohmann::json&v,uint64_t maximum){if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>maximum)throw std::invalid_argument("Mixed Scene40 material-slot identity is invalid");return v.get<uint64_t>();};
  const auto next=integer(c.at("next_id"),uint64_t(UINT32_MAX)+1);if(!next)throw std::invalid_argument("Mixed Scene40 material high-water identity is absent");
  const auto field=[&](const nlohmann::json&paint,uint64_t id){if(!paint.is_object()||paint.size()!=3||!paint.at("version").is_number_integer()||paint.at("version")!=7||integer(paint.at("set"),UINT32_MAX)!=id||!paint.at("payload").is_object())throw std::invalid_argument("Mixed Scene40 field lacks its genuine Paint7 membership envelope");};
  field(c.at("primary"),0);
  for(const auto&f:c.at("additional")){if(!f.is_object()||f.size()!=4||!f.at("name").is_string()||f.at("name").get_ref<const std::string&>().empty()||f.at("name").get_ref<const std::string&>().size()>256||!f.at("material").is_object())throw std::invalid_argument("Mixed Scene40 member field is incomplete");const auto id=integer(f.at("id"),UINT32_MAX);if(!id||id>=next)throw std::invalid_argument("Mixed Scene40 material identity is outside its actual high-water mark");ids[count++]=id;field(f.at("painted"),id);}
  std::sort(ids.begin(),ids.begin()+count);if(std::adjacent_find(ids.begin(),ids.begin()+count)!=ids.begin()+count)throw std::invalid_argument("Mixed Scene40 material identities repeat");
  if(!std::binary_search(ids.begin(),ids.begin()+count,integer(c.at("active"),UINT32_MAX)))throw std::invalid_argument("Mixed Scene40 active set has no actual material owner");
  step(slots.size()/4*7);for(size_t i=0;i<slots.size();i+=4)if(!std::binary_search(ids.begin(),ids.begin()+count,word(slots,i,4)))throw std::invalid_argument("Mixed Scene40 logical face names a foreign material set");
 }
 if(!sets)throw std::invalid_argument("Mixed outer Scene40 has no genuine material collection");
 for(size_t i=0;i<meshes.size();++i)if(!used_mesh[i])throw std::invalid_argument("Mixed Scene40 has an unreachable geometry record");
 for(size_t i=0;i<topologies.size();++i)if(!used_topology[i])throw std::invalid_argument("Mixed Scene40 has an unreachable topology record");
}
void graph_preflight(const nlohmann::json&scene){
 if(!scene.at("objects").is_array()||scene.at("objects").empty()||scene.at("objects").size()>256)throw std::invalid_argument("Scene39 object table exceeds its bounded domain");
 const auto&pool=scene.at("polygon_surfaces"),&materials=scene.at("source_materials"),&polygons=scene.at("polygon_sources");const auto&nodes=pool.at("nodes"),&bindings=materials.at("bindings"),&origins=polygons.at("origins"),&authorities=polygons.at("authorities");
 if(!pool.is_object()||pool.size()!=2||!pool.at("version").is_number_integer()||pool.at("version")!=1||!nodes.is_array()||nodes.empty()||nodes.size()>128||!origins.is_array()||origins.empty()||origins.size()>512||!authorities.is_array()||authorities.size()>512||!bindings.is_array()||bindings.empty()||bindings.size()>256)throw std::invalid_argument("Scene39 requires its actual bounded polygon field/source tables");
 for(const char*name:{"paints","volumes"})if(!polygons.at(name).is_array()||polygons.at(name).size()>512)throw std::invalid_argument("Scene39 source payload tables exceed their actual bounded domain");
 size_t entries=0,working=0;std::array<uint8_t,128>used{};
 for(const auto&node:nodes){const auto n=preflight_polygon_surface_node(node,origins.size());if(n>8'000'000-entries)throw std::length_error("Scene39 aggregate polygon edit entries exceed quota");entries+=n;
  const auto&before=origins.at(ref(node.at("before_origin"),origins.size()));const auto&encoded=polygons.at("paints").at(ref(before.at("painted"),polygons.at("paints").size()));const auto&paint=paint_archive_payload(encoded,true,SourcePaintDecodeContext{true,true});
  const auto&mesh=paint.at("mesh");const auto&positions=mesh.at("positions"),&indices=mesh.at("indices");if(!positions.is_binary()||positions.get_binary().empty()||positions.get_binary().size()%12||positions.get_binary().size()/12>2'000'000||!indices.is_binary()||indices.get_binary().empty()||indices.get_binary().size()%12||indices.get_binary().size()/4>6'000'000)throw std::invalid_argument("Scene39 before geometry exceeds its actual replay domain");
  const auto vertices=positions.get_binary().size()/12,corners=indices.get_binary().size()/4;const auto&r=paint.at("resolution");if(!r.is_number_integer()||r.get<int64_t>()<64||r.get<int64_t>()>2048)throw std::invalid_argument("Scene39 source grid dimensions differ");
  const size_t grid=paint_sampling_surface_working_bytes(corners,corners,r.get<uint32_t>());const size_t bytes=65536+vertices*512+corners*1024+grid+n*(sizeof(PolygonId)+3*sizeof(nlohmann::json))+node.at("recipe").at("operations").size()*4096+deformation_copy_bytes(before.at("deformation"));if(bytes>256*1024*1024-working)throw std::length_error("Scene39 retained node/replay peaks exceed aggregate quota before allocation");working+=bytes;
 }
 // Fixed bounded DFS records actual origin/authority subtree height and the
 // greatest material it needs. Every field node must depend on an earlier
 // complete material prefix, including shared rig/shape/polygon before roots.
 struct Reach{size_t height=1,material=0;bool has=false;};std::array<Reach,512>oroot{},aroot{};std::array<uint8_t,512>os{},as{};
 const auto merge=[](Reach&a,const Reach&b,size_t distance){a.height=std::max(a.height,b.height+distance);if(b.has){a.has=true;a.material=std::max(a.material,b.material);}};
 const auto visit=[&](auto&&self,bool authority,size_t id,size_t depth)->Reach{
  if(depth>32)throw std::length_error("Scene39 before graph exceeds its bounded depth");auto&state=authority?as[id]:os[id];auto&saved=authority?aroot[id]:oroot[id];if(state==1)throw std::invalid_argument("Scene39 polygon field/source graph contains a cycle");if(state==2){if(saved.height>33-depth)throw std::length_error("Scene39 shared before graph exceeds depth");return saved;}state=1;Reach result;
  if(authority){const auto&a=authorities[id];if(!a.at("origin").is_null())merge(result,self(self,false,ref(a.at("origin"),origins.size()),depth),0);}
  else{const auto&o=origins[id];if(!o.at("material_bindings").is_null()){result.has=true;result.material=ref(o.at("material_bindings"),bindings.size());}if(!o.at("polygon").is_null())merge(result,self(self,true,ref(o.at("polygon"),authorities.size()),depth+1),1);for(const char*field:{"shape_source","rig_source"})if(o.contains(field)&&!o.at(field).is_null())merge(result,self(self,false,ref(o.at(field),origins.size()),depth+1),1);}
  state=2;saved=result;return result;
 };
 for(size_t id=0;id<bindings.size();++id){const auto&b=bindings[id];if(!b.contains("polygon_surface_transfer"))throw std::invalid_argument("Material21 lacks actual per-binding polygon field references");if(b.at("polygon_surface_transfer").is_null())continue;const auto node=ref(b.at("polygon_surface_transfer"),nodes.size());used[node]=1;const auto reach=visit(visit,false,ref(nodes[node].at("before_origin"),origins.size()),1);if(reach.has&&reach.material>=id)throw std::invalid_argument("Scene39 field before-root requires an incomplete/future material prefix");}
 for(size_t id=0;id<nodes.size();++id)if(!used[id])throw std::invalid_argument("Scene39 contains an unreferenced polygon field node");
 const auto paired=[&](const nlohmann::json&paint,const nlohmann::json&binding){if(!paint.is_object()||!paint.contains("version")||paint.at("version")!=6)return;if(binding.is_null()||bindings.at(ref(binding,bindings.size())).at("polygon_surface_transfer").is_null()||paint.at("node")!=bindings.at(ref(binding,bindings.size())).at("polygon_surface_transfer"))throw std::invalid_argument("Scene39 Paint6 and its actual material node differ");PolygonSurfacePaintCodec::preflight(paint,nodes.size());};
 const auto nested=[&](const nlohmann::json&o){
  if(!o.contains("texture_sets")||o.at("texture_sets").is_null())return;
  const auto&collection=o.at("texture_sets");
  const auto field=[&](const nlohmann::json&paint,const nlohmann::json&binding){
   paired(paint,binding);
   if(paint.is_object()&&paint.contains("version")&&paint.at("version")==7&&paint.contains("payload"))paired(paint.at("payload"),binding);
   if(!binding.is_null()||(paint.is_object()&&paint.contains("version")&&paint.at("version")==6)||(paint.is_object()&&paint.contains("payload")&&paint.at("payload").is_object()&&paint.at("payload").contains("version")&&paint.at("payload").at("version")==6))throw std::invalid_argument("Nested polygon field Paint6 needs its actual texture-set field/source adapter");
  };
  field(collection.at("primary"),o.at("material_bindings"));
  for(const auto&f:collection.at("additional"))field(f.at("painted"),f.contains("material_bindings")?f.at("material_bindings"):nlohmann::json(nullptr));
 };
 for(const auto&o:scene.at("objects")){if(!o.at("painted").is_null())paired(o.at("painted"),o.at("material_bindings"));nested(o);}
 for(const auto&o:origins){if(!o.at("painted").is_null())paired(polygons.at("paints").at(ref(o.at("painted"),polygons.at("paints").size())),o.at("material_bindings"));nested(o);}
}
}
PolygonSurfacePreflight::PolygonSurfacePreflight(const nlohmann::json&scene):previous_(current_),scene_(scene){
 if(!scene.is_object()||(!scene.at("version").is_number_integer())||(scene.at("version")!=39&&scene.at("version")!=40)||!scene.contains("polygon_surfaces")||!scene.contains("source_materials")||!polygon_surface_material_archive(scene.at("source_materials")))throw std::invalid_argument("Polygon Paint6 requires genuine Scene39/material21 ownership or its owning Scene40 composition");
 if(scene.at("version")==40){if(!scene.contains("texture_set_geometry"))throw std::invalid_argument("Mixed Scene40 polygon fields require the actual texture-set archive");texture_set_context(scene);}
 else{if(scene.contains("texture_set_geometry"))throw std::invalid_argument("Scene39 cannot conceal an owning Scene40 texture-set archive");for(const auto&o:scene.at("objects"))if(o.contains("texture_sets")&&!o.at("texture_sets").is_null())throw std::invalid_argument("Scene39 cannot borrow current texture-set membership");}
 current_=this;try{graph_preflight(scene);}catch(...){current_=previous_;throw;}
}
PolygonSurfacePreflight::~PolygonSurfacePreflight(){current_=previous_;}
const PolygonSurfacePreflight&PolygonSurfacePreflight::current(){if(!current_)throw std::invalid_argument("Polygon Paint6 requires its actual Scene39 preflight context");return *current_;}
const nlohmann::json&PolygonSurfacePreflight::nodes()const{return scene_.at("polygon_surfaces");}
const nlohmann::json&polygon_surface_payload_shape(const nlohmann::json&paint){const auto&pool=PolygonSurfacePreflight::current().nodes();PolygonSurfacePaintCodec::preflight(paint,pool.at("nodes").size());return paint.at("payload");}
bool polygon_surface_physical_payload(const nlohmann::json&paint){(void)polygon_surface_payload_shape(paint);return paint.at("physical_paint").get<bool>();}
PolygonSurfaceArchive::PolygonSurfaceArchive(PolygonSceneArchive&origins):previous_(current_),origins_(&origins),writer_(std::make_unique<PolygonSurfaceNodeWriter>()){current_=this;}
PolygonSurfaceArchive::PolygonSurfaceArchive(const nlohmann::json&pool,PolygonSceneReader&origins,size_t count):previous_(current_),read_origins_(&origins){
 if(&pool!=&PolygonSurfacePreflight::current().nodes()||!count||count>512)throw std::invalid_argument("Polygon reader borrowed another Scene39 pool context");
 using Roots=std::array<std::shared_ptr<const PolygonOrigin>,512>;
 static_assert(sizeof(Roots)+256<=reader_root_index_bytes);
 auto roots=std::make_shared<Roots>();
 reader_=std::make_unique<PolygonSurfaceNodeReader>(pool,count,[&origins,roots,count](uint32_t id){
  if(id>=count)throw std::invalid_argument("Polygon field before-root exceeds its actual source index");
  auto&root=roots->at(id);if(!root)root=origins.origin_at(id);return root;
 },reader_limits());current_=this;
}
PolygonSurfaceArchive::~PolygonSurfaceArchive(){current_=previous_;}
PolygonSurfaceArchive&PolygonSurfaceArchive::writer(){if(!current_||!current_->writer_)throw std::invalid_argument("Polygon Paint6 requires its actual owning Scene39 writer");return *current_;}
PolygonSurfaceArchive&PolygonSurfaceArchive::reader(){if(!current_||!current_->reader_)throw std::invalid_argument("Polygon Paint6 requires its actual owning Scene39 reader");return *current_;}
uint32_t PolygonSurfaceArchive::add(const std::shared_ptr<const SourceMaterialBindings>&b){if(!writer_||!b||!b->polygon_surface_transfer)throw std::invalid_argument("Polygon archive requires an actual material field owner");b->validate();const auto id=writer_->add(b->polygon_surface_transfer);origins_->add_origin(b->polygon_surface_transfer->before());if(std::find(bindings_.begin(),bindings_.end(),b)==bindings_.end())bindings_.push_back(b);return id;}
std::shared_ptr<const PolygonSurfaceOwner>PolygonSurfaceArchive::at(uint32_t id){return reader_->at(id).owner;}
size_t PolygonSurfaceArchive::size()const noexcept{return writer_?writer_->size():reader_->size();}
nlohmann::json PolygonSurfaceArchive::encode(){const auto count=origins_->origins().size();auto result=writer_->encode([&](auto origin){return origins_->add_origin(std::move(origin));});if(origins_->origins().size()!=count)throw std::logic_error("Polygon field writer changed a closed source-root pool");return result;}
nlohmann::json PolygonSurfaceArchive::encode_paint(const PaintedMesh&paint,PaintImageArchive&images,NormalSamplingArchive*normal){for(const auto&b:bindings_)if(b->polygon_surface_transfer->matches_current(paint))return PolygonSurfacePaintCodec::encode(paint,b,*writer_,images,normal);throw std::invalid_argument("Polygon Paint6 has no actual matching current material owner");}
std::shared_ptr<PaintedMesh>PolygonSurfaceArchive::decode_paint(const nlohmann::json&paint,const PaintImageArchive&images,const NormalSamplingArchive*normal){return PolygonSurfacePaintCodec::decode(paint,*reader_,images,normal,reader_limits());}
PolygonSceneReader::MaterialResolver PolygonSurfaceArchive::material_resolver(PolygonSceneReader::MaterialResolver next){return read_origins_->material_resolver(std::move(next));}
void PolygonSurfaceArchive::restore_material_resolver(PolygonSceneReader::MaterialResolver previous)noexcept{read_origins_->restore_material_resolver(std::move(previous));}
void PolygonSurfaceArchive::require_complete(){reader_->require_complete();}
}
