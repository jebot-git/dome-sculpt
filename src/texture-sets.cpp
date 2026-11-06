// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/texture-sets.hpp"
#include "dome/polygon-scene.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-graph.hpp"
#include "text-utf8.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include <map>
#include <set>
namespace dome {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set operation cancelled");}
bool same_owner(const std::shared_ptr<const Mesh>&a,const std::shared_ptr<const Mesh>&b){return a&&b&&a==b&&!a.owner_before(b)&&!b.owner_before(a);}
void name_valid(const std::string&s){if(s.empty()||s.size()>256||s.find('\0')!=std::string::npos||!text::valid_utf8(s))throw std::invalid_argument("Invalid texture-set name");}
void charge(size_t&total,size_t count,size_t unit,const TextureSetLimits&l){if(unit&&count>(l.maximum_bytes-total)/unit)throw std::length_error("Texture-set owning request exceeds its byte budget");total+=count*unit;}
}
void TextureSetLimits::validate()const{
 if(!maximum_sets_per_object||maximum_sets_per_object>64||!maximum_sets_per_scene||maximum_sets_per_scene>256||maximum_sets_per_object>maximum_sets_per_scene||!maximum_triangles||maximum_triangles>2'000'000||!maximum_corners||maximum_corners>6'000'000||!maximum_work||maximum_work>512'000'000||!maximum_bytes||maximum_bytes>512*1024*1024)throw std::invalid_argument("Invalid texture-set resource bounds");
}
bool TextureSetFaceDomain::contains(uint32_t triangle)const noexcept{return assignments_&&triangle<assignments_->size()&&(*assignments_)[triangle]==set_;}
bool TextureSetFaceDomain::matches(const std::shared_ptr<const Mesh>&mesh)const noexcept{return topology_&&same_owner(topology_->geometry(),mesh);}
const TextureSetRecord&TextureSetCollection::at(TextureSetId id)const{
 auto i=std::find_if(additional_.begin(),additional_.end(),[&](const auto&s){return s.id==id;});if(i==additional_.end())throw std::out_of_range("Unknown additional texture set");return *i;
}
TextureSetView texture_set_view(const SculptObject&o,TextureSetId id){
 if(!o.mesh_active||!o.painted)throw std::logic_error("Texture sets require an active painting mesh");
 if(!id)return {0,o.material.name,o.painted,o.material,o.appearance,o.material_bindings,o.bake_recipe,o.texture_sets?o.texture_sets->primary_domain():nullptr};
 if(!o.texture_sets)throw std::out_of_range("This mesh has only its original texture set");const auto&s=o.texture_sets->at(id);return {s.id,s.name,s.painted,s.material,s.appearance,s.material_bindings,s.bake_recipe,s.domain};
}
TextureSetView active_texture_set(const SculptObject&o){return texture_set_view(o,o.texture_sets?o.texture_sets->active():0);}
void TextureSetCollection::validate(const SculptObject&o,bool deep,const TextureSetLimits&l)const{
 l.validate();if(!o.mesh_active||!o.painted||!detail::TextureSetPaintAccess::valid_raster(*o.painted)||!topology_||!same_owner(topology_->geometry(),o.painted->mesh_owner())||!primary_||primary_->set()!=0||primary_->topology()!=topology_||primary_->assignments_!=slots_||detail::TextureSetPaintAccess::domain(*o.painted)!=primary_||additional_.empty()||additional_.size()+1>l.maximum_sets_per_object||!next_||next_>uint64_t(UINT32_MAX)+1||!slots_||slots_->size()!=o.painted->mesh().indices.size()/3||topology_->triangle_faces().size()!=slots_->size()||topology_->triangle_corners().size()!=slots_->size()*3)throw std::invalid_argument("Texture-set collection lost its actual mesh/material membership");
 if(slots_->size()>l.maximum_triangles||topology_->triangle_corners().size()>l.maximum_corners||slots_->size()>l.maximum_work)throw std::length_error("Texture-set collection exceeds its declared geometry/work bounds");
 std::set<TextureSetId>ids{0};bool active=active_==0;uint64_t texels=uint64_t(o.painted->resolution())*o.painted->resolution();
 for(const auto&s:additional_){if(!s.id||s.id>=next_||!ids.insert(s.id).second||!s.painted||!detail::TextureSetPaintAccess::valid_raster(*s.painted)||s.painted->in_stroke()||!same_owner(s.painted->mesh_owner(),topology_->geometry())||!s.domain||s.domain->set()!=s.id||s.domain->topology()!=topology_||s.domain->assignments_!=slots_||detail::TextureSetPaintAccess::domain(*s.painted)!=s.domain)throw std::invalid_argument("Texture-set paint/material belongs to another face or geometry owner");name_valid(s.name);s.material.validate();texels+=uint64_t(s.painted->resolution())*s.painted->resolution();active|=active_==s.id;}
 if(!active||texels>16*1024*1024)throw std::length_error("Texture-set active ID or aggregate paint-map budget is invalid");
 if(topology_->polygon_ids()!=bool(o.polygon&&o.polygon->active))throw std::invalid_argument("Texture-set identity kind differs from its actual current polygon authority");
 if(!deep)return;
 stop(l);topology_->geometry()->validate();
 if(topology_->polygon_ids()){
  if(!o.polygon||!o.polygon->active)throw std::invalid_argument("Texture-set polygon IDs require their actual current authority");
  validate_polygon_derivative(*o.polygon,o.painted.get(),true);const auto derivative=o.polygon->topology.triangulate();
  if(!std::equal(derivative.triangle_faces.begin(),derivative.triangle_faces.end(),topology_->triangle_faces().begin(),topology_->triangle_faces().end())||derivative.mesh.indices.size()!=topology_->triangle_corners().size())throw std::invalid_argument("Texture-set polygon face correspondence differs from its current authority");
  for(size_t c=0;c<derivative.mesh.indices.size();++c){if((c&1023)==0)stop(l);if(derivative.corners.at(derivative.mesh.indices[c])!=topology_->triangle_corners()[c])throw std::invalid_argument("Texture-set polygon corner correspondence differs from its actual derivative");}
 }else{
  std::set<uint64_t> faces,corners;
  for(auto id:topology_->triangle_faces())if(!id||!faces.insert(id).second)throw std::invalid_argument("Triangle texture-set face identities must be unique");
  for(auto id:topology_->triangle_corners())if(!id||!corners.insert(id).second)throw std::invalid_argument("Triangle texture-set corner identities must be unique");
 }
 size_t visited=0;std::vector<uint8_t>seen(slots_->size());std::map<uint64_t,TextureSetId>face_slots;
 auto domain=[&](const auto&d){uint32_t prior=0;bool first=true;for(auto face:d->triangles()){if((visited++&1023)==0)stop(l);if(visited>l.maximum_work||face>=slots_->size()||(!first&&face<=prior)||seen[face]++||(*slots_)[face]!=d->set())throw std::invalid_argument("Texture-set domains do not partition actual logical faces");prior=face;first=false;}};
 domain(primary_);for(const auto&s:additional_)domain(s.domain);if(visited!=slots_->size())throw std::invalid_argument("Texture sets omitted logical faces");
 for(size_t f=0;f<slots_->size();++f){if((f&1023)==0)stop(l);if(!ids.contains((*slots_)[f])||!topology_->triangle_faces()[f])throw std::invalid_argument("Unknown persistent texture-set face/slot identity");auto [at,fresh]=face_slots.emplace(topology_->triangle_faces()[f],(*slots_)[f]);if(!fresh&&at->second!=(*slots_)[f])throw std::invalid_argument("A polygon face was split into conflicting texture sets");for(size_t c=0;c<3;++c)if(!topology_->triangle_corners()[f*3+c])throw std::invalid_argument("Missing persistent texture-set corner identity");}
}
namespace detail {
struct TextureSetAccess {
 static std::shared_ptr<TextureSetCollection>initial(const Mesh&mesh,std::span<const TextureSetId>assignments){
  auto t=std::shared_ptr<TextureSetTopology>(new TextureSetTopology);t->geometry_=std::make_shared<const Mesh>(mesh);
  t->faces_.reserve(assignments.size());t->corners_.reserve(mesh.indices.size());
  for(size_t f=0;f<assignments.size();++f){t->faces_.push_back(f+1);for(size_t k=0;k<3;++k)t->corners_.push_back(f*3+k+1);}
  auto c=std::shared_ptr<TextureSetCollection>(new TextureSetCollection);c->topology_=std::move(t);c->slots_=std::make_shared<const std::vector<TextureSetId>>(assignments.begin(),assignments.end());return c;
 }
 static std::shared_ptr<TextureSetCollection>begin(const SculptObject&o,const TextureSetLimits&l){
  l.validate();stop(l);if(!o.visible||o.locked||!o.mesh_active||!o.painted||o.painted->in_stroke())throw std::logic_error("Texture sets require a finished editable painting mesh");
  if(o.texture_sets){o.texture_sets->validate(o,true,l);return std::shared_ptr<TextureSetCollection>(new TextureSetCollection(*o.texture_sets));}
  const auto&m=o.painted->mesh();if(m.indices.size()/3>l.maximum_triangles||m.indices.size()>l.maximum_corners)throw std::length_error("Texture-set topology exceeds its geometry bounds");size_t bytes=0;charge(bytes,m.indices.size()/3,sizeof(uint64_t)+2*sizeof(uint32_t),l);charge(bytes,m.indices.size(),sizeof(uint64_t),l);
  auto t=std::shared_ptr<TextureSetTopology>(new TextureSetTopology);t->geometry_=o.painted->mesh_owner();t->faces_.reserve(m.indices.size()/3);t->corners_.reserve(m.indices.size());
  if(o.polygon&&o.polygon->active){
   // This actual triangulation supplies IDs; equal geometry/counts are not a
   // substitute for an owning polygon correspondence.
   validate_polygon_derivative(*o.polygon,o.painted.get(),true);const auto p=o.polygon->topology.triangulate();if(p.mesh.indices.size()!=m.indices.size())throw std::invalid_argument("Polygon texture-set triangle correspondence differs");t->polygon_ids_=true;t->faces_=p.triangle_faces;for(auto v:p.mesh.indices)t->corners_.push_back(p.corners.at(v));
  }else{
   for(size_t f=0;f<m.indices.size()/3;++f){if((f&1023)==0)stop(l);t->faces_.push_back(f+1);for(size_t k=0;k<3;++k)t->corners_.push_back(f*3+k+1);}
  }
  auto c=std::shared_ptr<TextureSetCollection>(new TextureSetCollection);c->topology_=std::move(t);c->slots_=std::make_shared<const std::vector<TextureSetId>>(m.indices.size()/3,0);return c;
 }
 static std::shared_ptr<const TextureSetFaceDomain>domain(const std::shared_ptr<const TextureSetTopology>&t,TextureSetId id,const std::shared_ptr<const std::vector<TextureSetId>>&slots){
  auto d=std::shared_ptr<TextureSetFaceDomain>(new TextureSetFaceDomain);d->topology_=t;d->set_=id;d->assignments_=slots;for(size_t f=0;f<slots->size();++f)if((*slots)[f]==id)d->triangles_.push_back(uint32_t(f));return d;
 }
 static std::vector<uint32_t>selected(const TextureSetCollection&c,std::span<const uint64_t>faces,const TextureSetLimits&l){
  if(faces.empty())throw std::invalid_argument("Select actual faces for a texture set");if(faces.size()>l.maximum_triangles)throw std::length_error("Texture-set face selection exceeds bounds");std::set<uint64_t>wanted;for(auto id:faces)if(!id||!wanted.insert(id).second)throw std::invalid_argument("Invalid or repeated texture-set face selection");std::set<uint64_t>found;std::vector<uint32_t>indices;
  for(size_t f=0;f<c.slots_->size();++f){if((f&1023)==0)stop(l);if(wanted.contains(c.topology_->triangle_faces()[f])){found.insert(c.topology_->triangle_faces()[f]);indices.push_back(uint32_t(f));}}if(found!=wanted)throw std::out_of_range("Texture-set face selection belongs to another topology");return indices;
 }
 static void bind(SculptObject&o,TextureSetCollection&c,const TextureSetLimits&l){
  size_t bytes=0;charge(bytes,c.slots_->size(),2*sizeof(uint32_t)+sizeof(uint64_t),l);charge(bytes,c.topology_->corners_.size(),sizeof(uint64_t),l);auto add=[&](const PaintedMesh&p){charge(bytes,1,TextureSetPaintAccess::working_bytes(p,p.resolution()),l);};add(*o.painted);for(const auto&s:c.additional_)add(*s.painted);
  auto actual_domain=[&](const PaintedMesh&p,TextureSetId id){const auto&existing=TextureSetPaintAccess::domain(p);if(existing&&existing->topology_==c.topology_&&existing->assignments_==c.slots_&&existing->set_==id)return existing;return domain(c.topology_,id,c.slots_);};
  auto primary=actual_domain(*o.painted,0);auto paint=TextureSetPaintAccess::bind(*o.painted,primary,o.painted->resolution(),l);std::vector<std::pair<std::shared_ptr<const TextureSetFaceDomain>,std::shared_ptr<PaintedMesh>>>next;next.reserve(c.additional_.size());for(const auto&s:c.additional_){stop(l);auto d=actual_domain(*s.painted,s.id);next.emplace_back(d,TextureSetPaintAccess::bind(*s.painted,d,s.painted->resolution(),l));}
  o.painted=std::move(paint);c.primary_=std::move(primary);for(size_t i=0;i<next.size();++i){c.additional_[i].domain=std::move(next[i].first);c.additional_[i].painted=std::move(next[i].second);}
 }
};
} // namespace detail
void initialize_texture_sets(Scene&scene,uint64_t object,const Mesh&mesh,
    std::span<const TextureSetInitialField>fields,std::span<const TextureSetId>assignments,const TextureSetLimits&limits){
 limits.validate();stop(limits);const auto&old=scene.object(object);const auto graph=detail::validate_texture_set_graph(scene,limits);
 if(!old.visible||old.locked||old.mesh_active||old.painted||!old.volume||old.volume->in_stroke()||!old.volume->voxels().empty()||old.volume->stored_samples()||old.texture_sets||old.deformation||old.appearance||old.material_bindings||old.retopology||old.polygon||old.shape_source||old.rig_source||old.bake_recipe)
  throw std::invalid_argument("Initial material membership requires a genuinely new empty mesh actor");
 if(fields.size()<2||fields.size()>limits.maximum_sets_per_object||fields.size()-1>limits.maximum_sets_per_scene-graph.live_sets||fields.size()>limits.maximum_sets_per_scene-graph.retained_domains)
  throw std::length_error("Initial texture-set slots exceed their complete Scene bound");
 if(mesh.indices.size()%3||mesh.indices.size()/3>limits.maximum_triangles||mesh.indices.size()>limits.maximum_corners||assignments.size()!=mesh.indices.size()/3||mesh.uv.size()!=mesh.positions.size())throw std::invalid_argument("Initial texture sets need complete current geometry/UV/triangle membership");
 size_t bytes=sizeof(TextureSetTopology)+sizeof(TextureSetCollection)+sizeof(Mesh)+256,work=0,pixels=0;
 auto work_items=[&](size_t n,size_t unit){if(work>limits.maximum_work||(unit&&n>(limits.maximum_work-work)/unit))throw std::length_error("Initial texture-set geometry/raster work exceeds bounds");work+=n*unit;};
 charge(bytes,mesh.positions.size()+mesh.normals.size(),sizeof(Vec3),limits);charge(bytes,mesh.uv.size(),sizeof(std::array<float,2>),limits);charge(bytes,mesh.indices.size(),sizeof(uint32_t),limits);charge(bytes,mesh.uv_pins.size()+mesh.uv_seams.size(),1,limits);
 charge(bytes,assignments.size(),sizeof(uint64_t)+3*sizeof(uint32_t),limits);charge(bytes,mesh.indices.size(),sizeof(uint64_t),limits);
 charge(bytes,fields.size(),sizeof(TextureSetFaceDomain)+sizeof(TextureSetRecord)+128,limits);
 charge(bytes,scene.objects().size(),sizeof(SculptObject)+514,limits);charge(bytes,scene.groups().size(),sizeof(SceneGroup)+257,limits);
 work_items(mesh.positions.size()+mesh.normals.size()+mesh.uv.size(),4*(fields.size()+1));work_items(mesh.indices.size(),4*(fields.size()+1));
 for(const auto&field:fields){field.material.validate();name_valid(field.material.name);charge(bytes,field.material.name.size()*2+2,1,limits);charge(bytes,1,detail::TextureSetPaintAccess::initial_raster_bytes(field.resolution),limits);const size_t count=size_t(field.resolution)*field.resolution;if(count>16*1024*1024-pixels)throw std::length_error("Initial texture-set grids exceed the aggregate Scene paint bound");pixels+=count;work_items(count,20);}
 mesh.validate();for(auto uv:mesh.uv)if(uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1)throw std::invalid_argument("Initial plain material slots require their actual unit paint coordinates");
 for(size_t face=0;face<assignments.size();++face){
  if((face&1023)==0)stop(limits);const auto set=assignments[face];if(set>=fields.size())throw std::invalid_argument("Initial face assignment refers to a foreign material slot");const auto r=fields[set].resolution;
  const auto a=mesh.uv[mesh.indices[face*3]],b=mesh.uv[mesh.indices[face*3+1]],c=mesh.uv[mesh.indices[face*3+2]];
  const int x0=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})*r))),x1=std::min(int(r)-1,int(std::ceil(std::max({a[0],b[0],c[0]})*r)));
  const int y0=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})*r))),y1=std::min(int(r)-1,int(std::ceil(std::max({a[1],b[1],c[1]})*r)));
  work_items(size_t(std::max(0,x1-x0+1))*std::max(0,y1-y0+1),1);
 }
 // Every forecast/shape/slot check precedes the sole detached geometry copy.
 auto collection=detail::TextureSetAccess::initial(mesh,assignments);collection->next_=fields.size();collection->primary_=detail::TextureSetAccess::domain(collection->topology_,0,collection->slots_);collection->additional_.reserve(fields.size()-1);
 auto primary=detail::TextureSetPaintAccess::empty(collection->primary_,fields[0].resolution,limits);
 for(size_t k=1;k<fields.size();++k){stop(limits);auto domain=detail::TextureSetAccess::domain(collection->topology_,TextureSetId(k),collection->slots_);auto paint=detail::TextureSetPaintAccess::empty(domain,fields[k].resolution,limits);collection->additional_.push_back({TextureSetId(k),fields[k].material.name,std::move(paint),fields[k].material,{},{},{},std::move(domain)});}
 auto next=scene;auto&target=next.object(object);target.mesh_active=true;target.painted=std::move(primary);target.material=fields[0].material;target.texture_sets=std::move(collection);
 target.texture_sets->validate(target,true,limits);next.validate();detail::validate_texture_set_graph(next,limits);stop(limits);scene=std::move(next);
}
TextureSetId create_texture_set(Scene&scene,uint64_t object,std::span<const uint64_t>faces,std::string name,uint32_t resolution,const TextureSetLimits&l){
 name_valid(name);l.validate();const auto graph=detail::validate_texture_set_graph(scene,l);if(graph.live_sets>=l.maximum_sets_per_scene)throw std::length_error("New texture set exceeds the whole Scene material-slot bound");stop(l);auto next=scene;auto&o=next.object(object);auto c=detail::TextureSetAccess::begin(o,l);
 if(c->additional_.size()+1>=l.maximum_sets_per_object||c->next_>UINT32_MAX)throw std::length_error("Texture-set identity/count budget exceeded");
 auto chosen=detail::TextureSetAccess::selected(*c,faces,l);const auto from=c->slots_->at(chosen.front());
 for(auto f:chosen)if((*c->slots_)[f]!=from)throw std::invalid_argument("Fork a texture set from faces with one actual current material");
 const auto source=texture_set_view(o,from);const auto id=TextureSetId(c->next_++);
 auto slots=std::make_shared<std::vector<TextureSetId>>(*c->slots_);for(auto f:chosen)(*slots)[f]=id;c->slots_=std::move(slots);c->active_=id;
 auto domain=detail::TextureSetAccess::domain(c->topology_,id,c->slots_);if(!resolution)resolution=source.painted->resolution();
 auto paint=detail::TextureSetPaintAccess::bind(*source.painted,domain,resolution,l);
 TextureSetRecord record{id,std::move(name),std::move(paint),source.material,source.appearance,source.material_bindings,source.bake_recipe,std::move(domain)};
 c->additional_.push_back(std::move(record));
 detail::TextureSetAccess::bind(o,*c,l);o.texture_sets=c;c->validate(o,true,l);detail::validate_texture_set_graph(next,l);stop(l);scene=std::move(next);return id;
}
bool assign_texture_set_faces(Scene&scene,uint64_t object,TextureSetId set,std::span<const uint64_t>faces,const TextureSetLimits&l){
 l.validate();detail::validate_texture_set_graph(scene,l);stop(l);auto next=scene;auto&o=next.object(object);auto c=detail::TextureSetAccess::begin(o,l);if(set)(void)c->at(set);
 auto chosen=detail::TextureSetAccess::selected(*c,faces,l);bool changed=false;for(auto f:chosen)changed|=(*c->slots_)[f]!=set;if(!changed)return false;
 auto slots=std::make_shared<std::vector<TextureSetId>>(*c->slots_);for(auto f:chosen)(*slots)[f]=set;c->slots_=std::move(slots);
 detail::TextureSetAccess::bind(o,*c,l);o.texture_sets=c;c->validate(o,true,l);detail::validate_texture_set_graph(next,l);stop(l);scene=std::move(next);return true;
}
bool select_texture_set(Scene&scene,uint64_t object,TextureSetId set){
 auto&o=scene.object(object);if(!o.texture_sets){if(!set)return false;throw std::out_of_range("This mesh has only its original texture set");}
 if(set)(void)o.texture_sets->at(set);if(o.texture_sets->active()==set)return false;
 auto c=std::shared_ptr<TextureSetCollection>(new TextureSetCollection(*o.texture_sets));c->active_=set;o.texture_sets=std::move(c);return true;
}
bool rename_texture_set(Scene&scene,uint64_t object,TextureSetId id,std::string name,const TextureSetLimits&limits){
 name_valid(name);limits.validate();stop(limits);const auto&before=scene.object(object);
 if(!before.texture_sets||!before.visible||before.locked||!before.mesh_active||!before.painted||before.painted->in_stroke())throw std::invalid_argument("Texture-set names need an actual editable member");
 before.texture_sets->validate(before,false,limits);detail::validate_texture_set_graph(scene,limits);
 const auto field=texture_set_view(before,id);if(field.name==name)return false;
 auto next=scene;auto&o=next.object(object);auto c=std::shared_ptr<TextureSetCollection>(new TextureSetCollection(*before.texture_sets));
 if(!id)o.material.name=std::move(name);else{auto it=std::find_if(c->additional_.begin(),c->additional_.end(),[&](const auto&r){return r.id==id;});it->name=std::move(name);}o.texture_sets=std::move(c);
 next.validate(false);stop(limits);scene=std::move(next);return true;
}
bool edit_texture_set(Scene&scene,uint64_t object,TextureSetId id,
    const std::function<bool(PaintedMesh&,Material&)>&edit,const TextureSetLimits&limits){
 limits.validate();stop(limits);if(!edit)throw std::invalid_argument("Missing texture-set edit operation");
 const auto &before=scene.object(object);
 if(!before.visible||before.locked||!before.mesh_active||!before.painted||before.painted->in_stroke())throw std::invalid_argument("Texture-set editing requires a visible unlocked finished mesh");
 if(!before.texture_sets)throw std::invalid_argument("Explicit texture-set editing needs actual membership");
 before.texture_sets->validate(before,true,limits);detail::validate_texture_set_graph(scene,limits);
 const auto source=texture_set_view(before,id);
 if(detail::TextureSetPaintAccess::working_bytes(*source.painted,source.painted->resolution())>limits.maximum_bytes)throw std::length_error("Texture-set edit staging exceeds its owning byte budget");
 auto paint=source.painted->snapshot();auto material=source.material;
 if(!edit(*paint,material)){stop(limits);return false;}
 if(paint->in_stroke())throw std::invalid_argument("Finish or cancel the texture-set stroke before publication");
 material.validate();stop(limits);
 auto next=scene;auto &target=next.object(object);auto collection=std::shared_ptr<TextureSetCollection>(new TextureSetCollection(*target.texture_sets));
 // A second snapshot excludes mutable callback aliases. COW tiles and the
 // immutable shared geometry remain exact without copying the authoring mesh.
 auto sealed=paint->snapshot();
 if(!id){target.painted=std::move(sealed);target.material=std::move(material);}
 else{auto at=std::find_if(collection->additional_.begin(),collection->additional_.end(),[&](const auto&r){return r.id==id;});if(at==collection->additional_.end())throw std::out_of_range("Unknown texture-set edit target");at->painted=std::move(sealed);at->material=std::move(material);}
 target.texture_sets=std::move(collection);next.validate();detail::validate_texture_set_graph(next,limits);stop(limits);scene=std::move(next);return true;
}
} // namespace dome
