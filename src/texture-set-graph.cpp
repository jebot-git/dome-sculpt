// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-graph.hpp"
#include "texture-set-paint-access.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/normal-preparation.hpp"
#include "canonical-normal-owner.hpp"
#include "canonical-normal-uv.hpp"
#include "native-paint-frame-source.hpp"
#include <set>
namespace dome::detail {
namespace {
struct Ledger {
 const TextureSetLimits&limits;TextureSetGraphStats stats;
 using Key=std::pair<unsigned,const void*>;
 std::set<Key>active,done;
 std::set<const TextureSetFaceDomain*>domains;
 std::set<const TextureSetFaceDomain*>associated_domains,paint_domains;
 std::set<const TextureSetTopology*>topologies;
 std::set<const Mesh*>geometry;
 std::set<const TextureSetId*>assignments;
 explicit Ledger(const TextureSetLimits&l):limits(l){l.validate();}
 void stop(){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set graph validation cancelled");}
 template<class F>void node(unsigned kind,const void*p,size_t depth,F&&body){
  if(!p)return;stop();const Key key{kind,p};if(depth>64||active.contains(key))throw std::invalid_argument("Cyclic or overdeep retained texture-set owner graph");if(done.contains(key))return;
  if(active.size()+done.size()>=8192)throw std::length_error("Retained texture-set metadata graph exceeds bounds");active.insert(key);body();active.erase(key);done.insert(key);
 }
 void charge(size_t&counter,size_t n,size_t unit){if(unit&&n>(limits.maximum_bytes-counter)/unit)throw std::length_error("Texture-set retained owner bytes exceed bounds");counter+=n*unit;}
 void domain(const std::shared_ptr<const TextureSetFaceDomain>&d){
  if(!d||!domains.insert(d.get()).second)return;if(++stats.retained_domains>limits.maximum_sets_per_scene)throw std::length_error("Retained texture-set count exceeds the Scene bound");
  charge(stats.membership_bytes,1,sizeof(TextureSetFaceDomain));charge(stats.membership_bytes,d->triangles().size(),sizeof(uint32_t));
  auto&t=d->topology();if(!t||t->triangle_faces().size()>limits.maximum_triangles||t->triangle_corners().size()>limits.maximum_corners)throw std::invalid_argument("Texture-set domain omitted or exceeded its owning topology");
  if(topologies.insert(t.get()).second){++stats.topologies;charge(stats.membership_bytes,1,sizeof(TextureSetTopology));charge(stats.membership_bytes,t->triangle_faces().size()+t->triangle_corners().size(),sizeof(uint64_t));auto&m=t->geometry();if(!m)throw std::invalid_argument("Texture-set topology omitted its owning geometry");if(geometry.insert(m.get()).second){charge(stats.geometry_bytes,1,sizeof(Mesh));charge(stats.geometry_bytes,m->positions.capacity()+m->normals.capacity(),sizeof(Vec3));charge(stats.geometry_bytes,m->uv.capacity(),sizeof(std::array<float,2>));charge(stats.geometry_bytes,m->indices.capacity(),sizeof(uint32_t));charge(stats.geometry_bytes,m->uv_pins.capacity()+m->uv_seams.capacity(),1);}}
 }
 void paint(const std::shared_ptr<const PaintedMesh>&p,size_t depth){node(1,p.get(),depth,[&]{if(!TextureSetPaintAccess::valid_raster(*p))throw std::invalid_argument("Retained set paint lost its actual raster membership");auto&d=TextureSetPaintAccess::domain(*p);if(d&&!d->matches(p->mesh_owner()))throw std::invalid_argument("Retained set paint lost its own immutable mesh owner");domain(d);if(d)paint_domains.insert(d.get());paint(p->source_sampling_original(),depth+1);});}
 void conversion(const std::shared_ptr<const CanonicalNormalConversion>&p,size_t depth){if(p){paint(p->before_paint(),depth+1);material(p->before_material(),depth+1);}}
 void key(const std::shared_ptr<const SourceKeyDerivation>&p,size_t depth){if(p){paint(p->before_paint(),depth+1);material(p->before_material(),depth+1);}}
 void uv_origin(const std::shared_ptr<const SourceUvOrigin>&p,size_t depth){if(p)paint(p->paint,depth+1);}
 void material(const std::shared_ptr<const SourceMaterialBindings>&p,size_t depth){node(2,p.get(),depth,[&]{
  uv_origin(p->uv_origin,depth+1);key(p->key_derivation,depth+1);conversion(p->canonical_attributes,depth+1);if(p->canonical_uv)conversion(p->canonical_uv->origin(),depth+1);
  if(p->native_paint_sampling){paint(p->native_paint_sampling->before_paint(),depth+1);material(p->native_paint_sampling->before_material(),depth+1);}
  if(p->native_paint_frames){paint(p->native_paint_frames->before_paint(),depth+1);material(p->native_paint_frames->before_material(),depth+1);uv_origin(p->native_paint_frames->origin(),depth+1);key(p->native_paint_frames->keys(),depth+1);}
  if(auto&c=p->color_preparation){paint(c->original_paint,depth+1);material(c->original_material,depth+1);if(c->uv_reprojection){paint(c->uv_reprojection->before_paint,depth+1);material(c->uv_reprojection->before_material,depth+1);}if(c->paint_sampling){paint(c->paint_sampling->before_paint(),depth+1);material(c->paint_sampling->before_material(),depth+1);}}
  if(auto&n=p->normal_preparation){paint(n->original_paint,depth+1);material(n->original_material,depth+1);key(n->key_derivation,depth+1);conversion(n->canonical_conversion,depth+1);if(n->uv_reprojection){paint(n->uv_reprojection->before_paint,depth+1);material(n->uv_reprojection->before_material,depth+1);}if(n->sampling_domain){paint(n->sampling_domain->before_paint,depth+1);material(n->sampling_domain->before_material,depth+1);material(n->sampling_domain->sampling_material,depth+1);}}
 });}
 void collection(const std::shared_ptr<const TextureSetCollection>&p,size_t depth){node(3,p.get(),depth,[&]{
  if(p->additional().empty()||p->additional().size()+1>limits.maximum_sets_per_object)throw std::length_error("Retained object texture-set count exceeds bounds");
  ++stats.collections;charge(stats.metadata_bytes,1,sizeof(TextureSetCollection));charge(stats.metadata_bytes,p->additional().size(),sizeof(TextureSetRecord));auto slots=p->triangle_sets();if(assignments.insert(slots.data()).second)charge(stats.membership_bytes,slots.size(),sizeof(TextureSetId));domain(p->primary_domain());associated_domains.insert(p->primary_domain().get());
  for(auto&s:p->additional()){charge(stats.metadata_bytes,s.name.capacity()+s.material.name.capacity()+2,1);if(!s.painted||s.domain!=TextureSetPaintAccess::domain(*s.painted)||!s.domain||s.id!=s.domain->set())throw std::invalid_argument("Retained texture-set record lost its own paint/member identity");domain(s.domain);associated_domains.insert(s.domain.get());paint(s.painted,depth+1);material(s.material_bindings,depth+1);}
 });}
 void origin(const std::shared_ptr<const PolygonOrigin>&p,size_t depth){node(4,p.get(),depth,[&]{
  if(p->texture_sets&&(!p->paint||TextureSetPaintAccess::domain(*p->paint)!=p->texture_sets->primary_domain()||!p->texture_sets->primary_domain()->matches(p->paint->mesh_owner())))throw std::invalid_argument("Retained texture-set history lost its actual primary paint/geometry owner");
  collection(p->texture_sets,depth+1);paint(p->paint,depth+1);material(p->material_bindings,depth+1);polygon(p->polygon,depth+1);origin(p->shape_source,depth+1);origin(p->rig_source,depth+1);});}
 void polygon(const std::shared_ptr<const PolygonAuthority>&p,size_t depth){node(5,p.get(),depth,[&]{origin(p->origin,depth+1);});}
 void object(const SculptObject&o){
  if(o.painted&&TextureSetPaintAccess::domain(*o.painted)&&(!o.texture_sets||TextureSetPaintAccess::domain(*o.painted)!=o.texture_sets->primary_domain()))throw std::invalid_argument("Current set-bound paint requires its own complete collection");
  // Every live object has its implicit material slot, including generated
  // volume surfaces. Additional writable sets cannot evade the scene total.
  const auto count=o.texture_sets?o.texture_sets->additional().size()+1:1;
  if(count>limits.maximum_sets_per_scene-stats.live_sets)throw std::length_error("Live material/texture slots exceed the Scene bound");stats.live_sets+=count;
  collection(o.texture_sets,1);paint(o.painted,1);material(o.material_bindings,1);polygon(o.polygon,1);origin(o.shape_source,1);origin(o.rig_source,1);
 }
};
}
TextureSetGraphStats validate_texture_set_graph(const Scene&scene,const TextureSetLimits&limits){
 Ledger ledger(limits);for(const auto&o:scene.objects())ledger.object(o);
 for(auto domain:ledger.paint_domains)if(!ledger.associated_domains.contains(domain))throw std::invalid_argument("Set-bound retained paint lacks its actual owning collection");
 size_t total=0;ledger.charge(total,ledger.stats.geometry_bytes,1);ledger.charge(total,ledger.stats.membership_bytes,1);ledger.charge(total,ledger.stats.metadata_bytes,1);return ledger.stats;
}
} // namespace dome::detail
