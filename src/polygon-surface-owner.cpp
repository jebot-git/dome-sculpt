// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-owner.hpp"
#include "polygon-conversion.hpp"
#include "document-authority.hpp"
#include "dome/polygon-source.hpp"
#include "dome/deformation.hpp"
#include "normal-frame-runtime.hpp"
#include "source-paint-pairing.hpp"
#include "polygon-scene-archive.hpp"
#include "paint-sampling-access.hpp"
#include "native-paint-sampling.hpp"
#include <limits>
#include <numeric>
#include <nlohmann/json.hpp>
namespace dome::detail {namespace {
void stop(const PolygonSurfaceTransferLimits &l){if(l.geometry.cancelled&&l.geometry.cancelled())throw std::runtime_error("Polygon surface owner cancelled");}
bool own(const auto&a,const auto&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
void charge(size_t &n,size_t count,size_t unit,const PolygonSurfaceTransferLimits &l){
 if(n>l.maximum_working_bytes||!unit||count>(l.maximum_working_bytes-n)/unit)throw std::length_error("Polygon original-field capture exceeds aggregate working quota");
 n+=count*unit;
}
size_t forecast(const PolygonOrigin &o,const PolygonSurfaceTransferLimits &l){
 stop(l);if(!l.maximum_steps||l.maximum_steps>128||!l.maximum_entries||l.maximum_entries>8'000'000||!l.maximum_working_bytes||l.maximum_working_bytes>512*1024*1024||!o.paint)throw std::invalid_argument("Polygon surface owner needs a bounded complete Paint source");
 size_t n=64*1024;const auto &m=o.paint->mesh();
 if(m.positions.size()>l.geometry.maximum_vertices||m.indices.size()>l.geometry.maximum_corners||m.indices.size()/3>l.geometry.maximum_faces)throw std::length_error("Polygon original geometry exceeds its exact component quota");
 // Simultaneous source preview, polygon preparation/replay, derivatives,
 // source maps and three-role raw posed corner buffers, including containers.
 charge(n,m.positions.size(),512,l);charge(n,m.indices.size(),1024,l);
 charge(n,1,sizeof(PolygonOrigin)+sizeof(Volume)+sizeof(PaintedMesh)+sizeof(SourceMaterialBindings)+sizeof(DeformationState),l);
 if(o.material_bindings)charge(n,o.material_bindings->material.copy_working_bytes(l.maximum_working_bytes-n,l.geometry.cancelled),1,l);
 if(o.deformation){const auto&d=*o.deformation;charge(n,d.resource_entries(),sizeof(Vec3),l);
  charge(n,d.pose.size(),sizeof(JointTransform),l);
  if(d.skin)charge(n,d.skin->vertices.size(),sizeof(VertexWeights),l);
  if(d.skeleton){if(d.skeleton->inverse_bind_override)charge(n,d.skeleton->inverse_bind_override->size(),sizeof(RigMatrix),l);for(const auto&bone:d.skeleton->bones){charge(n,1,sizeof(Bone)+64,l);charge(n,bone.name.size()+1,1,l);}}
  if(d.morphs)for(const auto&t:d.morphs->targets){charge(n,1,sizeof(MorphTarget),l);charge(n,t.name.size()+1,1,l);}
  if(d.humanoid)for(const auto&[name,joint]:d.humanoid->joints){(void)joint;charge(n,1,128,l);charge(n,name.size()+1,1,l);}
  if(d.landmarks)for(const auto&[name,point]:d.landmarks->points){(void)point;charge(n,1,128,l);charge(n,name.size()+1,1,l);}
  if(d.expressions)for(const auto&[name,bindings]:d.expressions->visemes){charge(n,1,128,l);charge(n,name.size()+1,1,l);for(const auto&v:bindings){charge(n,1,sizeof(VisemeBinding),l);charge(n,v.shape.size()+1,1,l);}}
 }
 for(const auto &layer:o.paint->layers()){charge(n,1,sizeof(PaintLayer),l);charge(n,layer.name.size()+1,1,l);charge(n,layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),128,l);}
 charge(n,o.name.size()+o.material.name.size()+2,1,l);return n;
}
Mesh evaluated(const PolygonOrigin&o){return o.deformation?o.deformation->preview(o.paint->mesh()):o.paint->mesh();}
struct SourceDomain {PolygonSourceTopology topology;std::vector<uint32_t> corners;std::vector<std::array<float,2>> paint,normal;};
SourceDomain source_domain(const PolygonOrigin&o,const PolygonSurfaceTransferLimits&l){
 stop(l);forecast(o,l);o.paint->mesh().validate();o.material.validate();
 if(!o.mesh_active||o.appearance||(o.polygon&&o.polygon->active&&o.polygon->colors)||o.paint->source_maps()||o.paint->normal_projection()||o.paint->color_projection())
  throw std::invalid_argument("Polygon original field requires its native canonical or normal-free capture adapter");
 if(o.material_bindings)o.material_bindings->validate();
 source_paint_pairing(o.paint.get(),o.material_bindings.get(),o.appearance.get(),true);
 if(o.deformation)o.deformation->validate(o.paint->mesh());
 const auto &binding=o.material_bindings;const auto field=o.paint->baked()?o.paint->baked()->normal_field:nullptr;
 if(!field&&o.paint->mesh().normals.size()!=o.paint->mesh().positions.size())throw std::invalid_argument("Polygon native N-only transfer requires its actual authored/evaluated normals");
 if(!field&&o.paint->baked()&&(!o.paint->baked()->maps.normal.rgba.empty()||!o.paint->baked()->maps.mtoon_normal.rgba.empty()))throw std::invalid_argument("Polygon raw baked normal detail requires its actual native/converted normal adapter");
 // A real normal-free native UV retention already owns a version1 original
 // paint basis and ordered source corners. Keep that strong owner in before;
 // paint_domain composes its exact correspondence into the current domain3.
 // Native KeepPaint requires sampling_reparameterized=true for its before
 // binding. The new current polygon binding separately requires false.
 // No normal/artist/prepared role is authorized by this paint-only adapter.
 if(binding&&binding->native_paint_sampling){
  const auto &native=binding->native_paint_sampling;
  if(field||binding->has_mtoon||binding->native_paint_frames||binding->normal_frames||binding->canonical_attributes||binding->canonical_uv||binding->canonical_paint_sampling||binding->normal_preparation||binding->color_preparation||binding->key_derivation||binding->normal_frame!=SourceMaterialNormalFrame::UvDerivative||!binding->sampling_reparameterized||!native->valid_owner(native)||!o.paint->paint_sampling_domain()||o.paint->paint_sampling_domain()->version()!=1)
   throw std::invalid_argument("Polygon native UV field requires its genuine normal-free paint-only source adapter");
  for(const auto &role:binding->roles)if(role.image)throw std::invalid_argument("Polygon native UV field cannot borrow an independently typed image role");
  PaintSamplingLimits native_limits;native_limits.max_vertices=l.geometry.maximum_vertices;native_limits.max_triangles=l.geometry.maximum_faces;native_limits.max_work=l.geometry.maximum_work;native_limits.max_working_bytes=l.maximum_working_bytes;native_limits.max_basis_bytes=std::min(size_t(128*1024*1024),native_limits.max_working_bytes);native_limits.cancelled=l.geometry.cancelled;
  native->validate_current(*o.paint,native_limits);
 }
 if(binding&&(binding->has_mtoon||binding->native_paint_frames||binding->normal_preparation||binding->color_preparation))
  throw std::invalid_argument("Polygon typed/native/prepared field needs its separate actual role adapter");
 if(binding&&binding->normal_frames&&!field)throw std::invalid_argument("Polygon retained artist frame without active canonical detail needs its separate native frame adapter");
 if(field){
  if(!binding||!binding->canonical_attributes||!binding->normal_frames||!own(field->origin(),binding->canonical_attributes))throw std::invalid_argument("Polygon field lost its actual canonical origin");
  normal_frame_current_authority(*binding,o.paint->mesh(),o.deformation.get(),l.geometry.cancelled);
 }
 auto pose=evaluated(o);SourceDomain out;
 if(o.polygon&&o.polygon->active){
  validate_polygon_derivative(*o.polygon,o.paint.get(),true,l.geometry);
  out.topology.topology=o.deformation?evaluate_polygon_source_topology(*o.polygon,o.paint->mesh(),pose,l.geometry):o.polygon->topology;
  const auto derivative=out.topology.topology.triangulate(l.geometry);
  if(derivative.mesh.positions.size()!=pose.positions.size()||derivative.mesh.indices!=pose.indices)throw std::invalid_argument("Polygon original field needs its genuine posed corner/refinement adapter");
  out.topology.source_vertices.resize(pose.positions.size());std::iota(out.topology.source_vertices.begin(),out.topology.source_vertices.end(),0u);
 }else out.topology=prepare_polygon_topology(pose,nullptr,l.geometry);
 const auto derivative=out.topology.topology.triangulate(l.geometry);
 if(derivative.mesh.indices.size()!=pose.indices.size())throw std::invalid_argument("Polygon source triangulation needs its genuine common-refinement adapter");
 out.corners.reserve(pose.indices.size());out.paint.reserve(pose.indices.size());if(field)out.normal.reserve(pose.indices.size());
 std::vector<std::array<float,2>> paint;if(o.paint->paint_sampling_domain())paint=o.paint->paint_sampling_domain()->corner_coordinates(o.paint->mesh());
 for(size_t c=0;c<pose.indices.size();++c){
  if((c&255)==0)stop(l);
  const auto v=out.topology.source_vertices.at(derivative.mesh.indices[c]);
  if(v!=pose.indices[c])throw std::invalid_argument("Polygon original corner order needs its genuine source/refinement witness");
  out.corners.push_back(uint32_t(c));out.paint.push_back(paint.empty()?o.paint->mesh().uv.at(v):paint.at(c));
  if(field){const auto &uv=binding->canonical_uv;out.normal.push_back(uv?uv->coordinates().at(v):field->origin()->before_paint()->mesh().uv.at(field->origin()->before_paint()->mesh().indices.at(c)));}
 }
 stop(l);return out;
}
void barycentric(std::array<double,3>b){double total=0;for(auto x:b){if(!std::isfinite(x)||x<0||x>1)throw std::invalid_argument("Polygon field sample is outside its declared triangle");total+=x;}if(std::abs(total-1)>1e-6)throw std::invalid_argument("Polygon field sample barycentrics must sum to one");}
std::array<float,2> interpolate(const std::vector<std::array<float,2>>&uv,size_t t,std::array<double,3>b){std::array<double,2> q{};for(size_t k=0;k<3;++k)for(size_t d=0;d<2;++d)q[d]+=uv.at(t*3+k)[d]*b[k];return {float(q[0]),float(q[1])};}
}
bool PolygonSurfaceOwner::valid_owner(const std::shared_ptr<const PolygonSurfaceOwner>&p)const noexcept{return p.get()==this&&!self_.expired()&&!self_.owner_before(p)&&!p.owner_before(self_);}
bool polygon_surface_transfer_required(const SculptObject&o){return o.mesh_active&&o.painted&&(o.painted->paint_sampling_domain()||o.painted->has_physical_emission()||(o.painted->baked()&&o.painted->baked()->normal_field)||(o.material_bindings&&o.material_bindings->polygon_surface_transfer));}
void validate_polygon_surface_model(bool active,const PaintedMesh*paint,
 const PolygonAuthority*polygon,const SourceMaterialBindings*binding,
 const DeformationState*deformation,const CreatorAppearance*appearance){
 if(!binding||!binding->polygon_surface_transfer)return;
 const auto&field=binding->polygon_surface_transfer;
 if(!field->valid_owner(field)||!active||!paint||!polygon||!polygon->active||polygon->colors||deformation||appearance||!own(polygon->origin,field->before())||polygon->topology.encode()!=field->trace().current()->encode())throw std::invalid_argument("Polygon field model lost its exact current editable authority and complete source");
}
bool PolygonSurfaceOwner::has_normal_field()const noexcept{return before_&&before_->paint&&before_->paint->baked()&&bool(before_->paint->baked()->normal_field);}
size_t PolygonSurfaceOwner::source_working_bytes(const PolygonOrigin&before,const PolygonSurfaceTransferLimits&l){return forecast(before,l);}
std::shared_ptr<const PolygonSurfaceOwner> PolygonSurfaceOwner::capture(const Scene&s,uint64_t id,const PolygonSurfaceTransferLimits&l){
 stop(l);const auto &object=s.object(id);if(!object.painted||!object.volume||object.volume->in_stroke()||object.painted->in_stroke())throw std::invalid_argument("Polygon field capture requires finalized current source state");
 PolygonOrigin forecast_source;forecast_source.paint=object.painted;forecast_source.deformation=object.deformation;forecast_source.material_bindings=object.material_bindings;forecast_source.name=object.name;forecast_source.material=object.material;auto prospective=forecast(forecast_source,l);
 charge(prospective,s.groups().size(),sizeof(SceneGroup),l);for(const auto&g:s.groups())charge(prospective,g.name.size()+1,1,l);
 charge(prospective,object.volume->layers().size(),sizeof(Volume::Layer),l);for(const auto&layer:object.volume->layers())charge(prospective,layer.name.size()+1,1,l);
 s.validate();stop(l);auto original=std::make_shared<PolygonOrigin>(*capture_polygon_origin(s,id));
 // Freeze editable metadata, not merely a const view of the live object.
 if(original->deformation)original->deformation=std::make_shared<const DeformationState>(*original->deformation);
 if(original->material_bindings)original->material_bindings=std::make_shared<const SourceMaterialBindings>(*original->material_bindings);
 const auto domains=source_domain(*original,l);auto out=std::shared_ptr<PolygonSurfaceOwner>(new PolygonSurfaceOwner);out->before_=std::move(original);out->trace_=PolygonSurfaceTrace::begin(domains.topology.topology,l);
 out->native_corners_=domains.corners;out->paint_corners_=domains.paint;out->normal_corners_=domains.normal;out->self_=out;out->validate(l);return out;
}
void PolygonSurfaceOwner::validate(const PolygonSurfaceTransferLimits&l)const{
 stop(l);if(!valid_owner(self_.lock())||!before_||!before_->paint)throw std::invalid_argument("Polygon surface transfer has no authentic original owner");
 forecast(*before_,l);PolygonSceneArchive retained;retained.add_origin(before_,true);
 if(retained.component_entries()>l.maximum_entries)throw std::length_error("Polygon original retained graph exceeds its component quota");
 const auto domains=source_domain(*before_,l);trace_.validate(l);
 if(domains.topology.topology.encode()!=trace_.original()->encode()||domains.corners!=native_corners_||domains.paint!=paint_corners_||domains.normal!=normal_corners_)throw std::invalid_argument("Polygon original field differs from its owning source replay");
 (void)trace_.triangle_permutation(l);stop(l);
}
std::shared_ptr<const PolygonSurfaceOwner> PolygonSurfaceOwner::advance(const PolygonSurfaceStep&step,const PolygonSurfaceTransferLimits&l)const{
 validate(l);if(attachment_)throw std::invalid_argument("Advancing connected polygon paint requires its actual current-value capture adapter");auto next=trace_.advance(step,l);if(!next)return self_.lock();auto out=std::shared_ptr<PolygonSurfaceOwner>(new PolygonSurfaceOwner);out->before_=before_;out->trace_=std::move(*next);out->native_corners_=native_corners_;out->paint_corners_=paint_corners_;out->normal_corners_=normal_corners_;out->self_=out;out->validate(l);return out;
}
std::array<double,3> PolygonSurfaceOwner::source_barycentrics(size_t t,std::array<double,3>b,const std::vector<PolygonSurfaceTriangle>&map)const{barycentric(b);std::array<double,3> q{};for(size_t c=0;c<3;++c)for(size_t k=0;k<3;++k)q[k]+=b[c]*map.at(t).source_barycentrics[c][k];barycentric(q);return q;}
std::array<float,2> PolygonSurfaceOwner::paint_coordinates(size_t t,std::array<double,3>b,const PolygonSurfaceTransferLimits&l)const{validate(l);const auto map=trace_.triangle_permutation(l);return interpolate(paint_corners_,map.at(t).source_triangle,source_barycentrics(t,b,map));}
std::array<float,2> PolygonSurfaceOwner::normal_coordinates(size_t t,std::array<double,3>b,const PolygonSurfaceTransferLimits&l)const{validate(l);if(!has_normal_field())throw std::invalid_argument("Normal-free polygon owner has no invented normal coordinates");const auto map=trace_.triangle_permutation(l);return interpolate(normal_corners_,map.at(t).source_triangle,source_barycentrics(t,b,map));}
Vec3 PolygonSurfaceOwner::native_normal(size_t t,std::array<double,3>b,const AffineTransform&world,const PolygonSurfaceTransferLimits&l)const{
 validate(l);if(has_normal_field())return pbr_normal(t,b,world,l);
 const auto map=trace_.triangle_permutation(l);const auto source=map.at(t).source_triangle;const auto sb=source_barycentrics(t,b,map);const auto native=evaluated(*before_);Vec3 direction{};
 for(size_t k=0;k<3;++k){const auto v=native.indices.at(native_corners_.at(source*3+k));const auto n=normal_bake_math::vec(world.normal(normal_bake_math::vec(native.normals.at(v))));direction+=normal_bake_math::unit(n)*float(sb[k]);}
 const auto before_mesh=trace_.original()->triangulate(l.geometry).mesh,after_mesh=trace_.current()->triangulate(l.geometry).mesh;std::array<Vec3,3>p{},q{};
 const std::array<std::array<float,2>,3>chart{{{0,0},{1,0},{0,1}}};std::array<std::array<float,2>,3>current_chart{};
 for(size_t k=0;k<3;++k){p[k]=before_mesh.positions.at(before_mesh.indices.at(source*3+k));q[k]=after_mesh.positions.at(after_mesh.indices.at(t*3+k));for(size_t j=0;j<3;++j)for(size_t d=0;d<2;++d)current_chart[k][d]+=float(map[t].source_barycentrics[k][j]*chart[j][d]);}
 return transport_polygon_direction(direction,polygon_geometry_frame(p,chart,world),polygon_geometry_frame(q,current_chart,world));
}
Vec3 PolygonSurfaceOwner::pbr_normal(size_t t,std::array<double,3>b,const AffineTransform&world,const PolygonSurfaceTransferLimits&l)const{
 validate(l);if(!has_normal_field())throw std::invalid_argument("Normal-free polygon owner has no prepared role to evaluate");
 const auto map=trace_.triangle_permutation(l);const auto st=map.at(t).source_triangle;const auto sb=source_barycentrics(t,b,map);
 const auto &binding=*before_->material_bindings;const auto &mesh=before_->paint->mesh();
 NormalFrameLayout layout;layout.vertices=mesh.indices;layout.indices.resize(mesh.indices.size());layout.corners.resize(mesh.indices.size());
 for(size_t c=0;c<mesh.indices.size();++c)layout.indices[c]=layout.corners[c]=uint32_t(c);
 const auto raw=normal_frame_preview(*binding.normal_frames,layout,before_->deformation.get(),l.geometry.cancelled,1);
 std::array<NormalBakeVertex,3> vs;const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 for(size_t k=0;k<3;++k){const auto c=native_corners_.at(st*3+k);const auto a=raw.tangents[0].at(c);const auto n=world.normal(normal_bake_math::vec(raw.normals[0].at(c))),v=world.direction({a[0],a[1],a[2]});vs[k]=normal_bake_vertex(normal_bake_math::vec(n),normal_bake_math::vec(v),a[3]*hand,NormalVertexBitangent::UnitCross);}
 const auto basis=normal_bake_basis(vs,sb,NormalFragmentBasis::UnitColumns);
 const auto before_mesh=trace_.original()->triangulate(l.geometry).mesh,after_mesh=trace_.current()->triangulate(l.geometry).mesh;
 std::array<Vec3,3> p{},q{};std::array<std::array<float,2>,3> old_uv{},new_uv{};
 for(size_t k=0;k<3;++k){p[k]=before_mesh.positions.at(before_mesh.indices.at(st*3+k));q[k]=after_mesh.positions.at(after_mesh.indices.at(t*3+k));old_uv[k]=normal_corners_.at(st*3+k);}
 for(size_t k=0;k<3;++k)for(size_t j=0;j<3;++j)for(size_t d=0;d<2;++d)new_uv[k][d]+=float(map[t].source_barycentrics[k][j]*old_uv[j][d]);
 const auto uv=interpolate(normal_corners_,st,sb);PaintSourceTexture texture;texture.image=before_->paint->baked()->normal_field->pbr();texture.wrap_s=texture.wrap_t=33071;texture.min_filter=texture.mag_filter=9729;const auto sample=sample_source_texture(texture,uv[0],uv[1],false);
 Vec3 decoded{sample[0]*2-1,sample[1]*2-1,sample[2]*2-1};if(std::max({std::abs(decoded.x),std::abs(decoded.y),std::abs(decoded.z)})<=1e-20f)decoded={0,0,1};
 return transport_polygon_normal({basis,vs[0].w,1},decoded,polygon_geometry_frame(p,old_uv,world),polygon_geometry_frame(q,new_uv,world));
}

namespace {
uint64_t number(const nlohmann::json &j,uint64_t limit){
 if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>limit)throw std::invalid_argument("Polygon field recipe integer is outside its exact range");return j.get<uint64_t>();
}
}
nlohmann::json PolygonSurfaceOwner::encode_recipe(const PolygonSurfaceTransferLimits&l)const{
 validate(l);size_t bytes=trace_.storage_bytes();
 // JSON arrays/object nodes coexist with the full immutable trace. Count the
 // actual edit-ID payload before constructing any encoded selection array.
 charge(bytes,trace_.steps().size(),4096,l);
 for(const auto&step:trace_.steps())charge(bytes,step.edit.selection.ids.size(),sizeof(nlohmann::json)+sizeof(PolygonId),l);
 nlohmann::json operations=nlohmann::json::array();
 for(const auto&s:trace_.steps()){
  stop(l);const auto&e=s.edit;operations.push_back({{"operation",e.operation==PolygonOperation::Move?"move":"flip"},{"selection",{{"mode",int(e.selection.mode)},{"ids",e.selection.ids}}},{"delta",{e.delta.x,e.delta.y,e.delta.z}},{"amount",e.amount},{"cuts",e.cuts},{"first_id",e.first_id},{"world",s.world?nlohmann::json(s.world->matrix()):nlohmann::json(nullptr)}});
 }
 return {{"version",1},{"model","polygon-original-field-current-surface-v1"},{"normal_evaluation",has_normal_field()?"core-pbr-unit-columns":"native-normal-only"},{"operations",std::move(operations)}};
}
std::shared_ptr<const PolygonSurfaceOwner> PolygonSurfaceOwner::replay(
 std::shared_ptr<const PolygonOrigin>before,const nlohmann::json&record,const PolygonSurfaceTransferLimits&l){
 stop(l);if(!before||!record.is_object()||record.size()!=4||record.at("version")!=1||record.at("model")!="polygon-original-field-current-surface-v1"||(record.at("normal_evaluation")!="core-pbr-unit-columns"&&record.at("normal_evaluation")!="native-normal-only")||!record.at("operations").is_array()||record.at("operations").size()>l.maximum_steps)throw std::invalid_argument("Unsupported owning polygon field recipe");
 forecast(*before,l);if(record.at("normal_evaluation")!=(before->paint->baked()&&before->paint->baked()->normal_field?"core-pbr-unit-columns":"native-normal-only"))throw std::invalid_argument("Polygon field recipe claimed a different actual normal model");size_t entries=0,bytes=0;
 // Count the complete edit array before copying any selection or replaying a
 // mesh. The outer owner/archive must also charge its retained before DAG.
 for(const auto&item:record.at("operations")){
  stop(l);if(!item.is_object()||item.size()!=7||!item.at("selection").is_object()||item.at("selection").size()!=2||!item.at("selection").at("ids").is_array())throw std::invalid_argument("Invalid polygon field operation record");
  const auto count=item.at("selection").at("ids").size();if(entries>l.maximum_entries||count>l.maximum_entries-entries)throw std::length_error("Polygon field recipe exceeds entry quota");entries+=count;charge(bytes,1,sizeof(PolygonSurfaceStep),l);charge(bytes,count,sizeof(PolygonId),l);
 }
 const auto domain=source_domain(*before,l);auto out=std::shared_ptr<PolygonSurfaceOwner>(new PolygonSurfaceOwner);out->before_=std::move(before);out->trace_=PolygonSurfaceTrace::begin(domain.topology.topology,l);out->native_corners_=domain.corners;out->paint_corners_=domain.paint;out->normal_corners_=domain.normal;out->self_=out;
 for(const auto&item:record.at("operations")){
  stop(l);PolygonSurfaceStep step;auto&e=step.edit;
  if(item.at("operation")=="move")e.operation=PolygonOperation::Move;else if(item.at("operation")=="flip")e.operation=PolygonOperation::FlipFaces;else throw std::invalid_argument("Unsupported polygon field operation kind");
  e.selection.mode=PolygonSelectionMode(number(item.at("selection").at("mode"),2));for(const auto&id:item.at("selection").at("ids"))e.selection.ids.push_back(number(id,UINT64_MAX));
  const auto delta=item.at("delta").get<std::array<float,3>>();e.delta={delta[0],delta[1],delta[2]};e.amount=item.at("amount").get<float>();e.cuts=unsigned(number(item.at("cuts"),std::numeric_limits<unsigned>::max()));e.first_id=number(item.at("first_id"),UINT64_MAX);
  if(!finite(e.delta)||!std::isfinite(e.amount))throw std::invalid_argument("Nonfinite polygon field edit scalar");
  if(!item.at("world").is_null())step.world=AffineTransform(item.at("world").get<std::array<double,16>>());
  auto next=out->trace_.advance(step,l);if(!next)throw std::invalid_argument("Polygon field recipe contains a fabricated no-op history step");out->trace_=std::move(*next);
 }
 out->validate(l);if(out->encode_recipe(l)!=record)throw std::invalid_argument("Polygon field recipe differs from its canonical replay");stop(l);return out;
}
void PolygonSurfaceOwner::validate_current_geometry(const Mesh&mesh,const PolygonSurfaceTransferLimits&l)const{
 validate(l);mesh.validate();const auto target=trace_.current()->triangulate(l.geometry).mesh;
 const auto vectors=[](const std::vector<Vec3>&a,const std::vector<Vec3>&b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](Vec3 x,Vec3 y){return x.x==y.x&&x.y==y.y&&x.z==y.z;});};
 if(!vectors(mesh.positions,target.positions)||!vectors(mesh.normals,target.normals)||mesh.indices!=target.indices||mesh.uv!=target.uv||mesh.uv_pins!=target.uv_pins||mesh.uv_seams!=target.uv_seams)throw std::invalid_argument("Polygon field current derivative differs from its exact replayed surface");stop(l);
}

std::shared_ptr<const PaintSamplingDomain> PolygonSurfaceOwner::paint_domain(const PolygonSurfaceTransferLimits&l)const{
 return paint_domain({},l);
}
std::shared_ptr<const PaintSamplingDomain> PolygonSurfaceOwner::paint_domain(std::shared_ptr<const Mesh>geometry,const PolygonSurfaceTransferLimits&l)const{
 validate(l);if(trace_.current()->encode()==trace_.original()->encode())throw std::invalid_argument("Unchanged polygon does not require a new paint-domain interpretation");
 const auto previous=before_->paint->paint_sampling_domain();
 if(previous&&previous->version()!=1)throw std::invalid_argument("Polygon paint source needs its actual prior transition composition adapter");
 const auto basis=previous?previous->basis():before_->paint->mesh_owner();
 size_t prospective=trace_.storage_bytes(),corners=0;for(const auto&face:trace_.current()->faces()){if(face.corners.size()>l.maximum_entries-corners)throw std::length_error("Polygon paint candidate exceeds its corner quota");corners+=face.corners.size();}
 // Two current derivatives, map vectors and output snapshot/control blocks
 // coexist here. Raster/grid construction has its own later aggregate stage.
 charge(prospective,corners,2*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t))+sizeof(PolygonSurfaceTriangle)+2*sizeof(uint32_t),l);
 const auto mapping=trace_.triangle_permutation(l);Mesh derivative;if(geometry)validate_current_geometry(*geometry,l);else derivative=trace_.current()->triangulate(l.geometry).mesh;std::vector<uint32_t> sources;sources.reserve(geometry?geometry->indices.size():derivative.indices.size());
 for(const auto&triangle:mapping)for(const auto&row:triangle.source_barycentrics){stop(l);size_t hit=3;for(size_t k=0;k<3;++k){if(row[k]==1&&hit==3)hit=k;else if(row[k]!=0)throw std::invalid_argument("Polygon paint common refinement requires its actual barycentric record");}if(hit==3)throw std::invalid_argument("Polygon paint corner lost its genuine original side");const auto native=native_corners_.at(triangle.source_triangle*3+hit);sources.push_back(previous?previous->source_corners()[native]:native);}
 auto snapshot=geometry?std::move(geometry):std::make_shared<const Mesh>(std::move(derivative));auto domain=std::shared_ptr<const PaintSamplingDomain>(new PaintSamplingDomain(basis,std::move(sources),snapshot));
 PaintSamplingLimits limits;limits.max_vertices=l.geometry.maximum_vertices;limits.max_triangles=l.geometry.maximum_faces;limits.max_work=l.geometry.maximum_work;limits.max_working_bytes=l.maximum_working_bytes;limits.max_basis_bytes=std::min(size_t(128*1024*1024),l.maximum_working_bytes);limits.cancelled=l.geometry.cancelled;
 domain->validate_current(*snapshot,limits);stop(l);return domain;
}

bool PolygonSurfaceOwner::matches_current(const PaintedMesh&paint)const noexcept{
 return attachment_&&own(attachment_,paint.paint_sampling_attachment())&&paint.valid_paint_sampling_attachment()&&paint.paint_sampling_domain()&&paint.paint_sampling_domain()->version()==3;
}
void PolygonSurfaceOwner::validate_current(const PaintedMesh&paint,const PolygonSurfaceTransferLimits&l)const{
 validate(l);if(!matches_current(paint)||paint.source_maps()||paint.source_sampling_prepared()||paint.normal_projection()||paint.color_projection())throw std::invalid_argument("Polygon original-field/current-Paint attachment differs from its genuine owner");
 if(paint.baked()&&(paint.baked()->normal_authority||!paint.baked()->maps.normal.rgba.empty()||!paint.baked()->maps.mtoon_normal.rgba.empty()))throw std::invalid_argument("Polygon normal-detail writes require their actual owning field transition");
 validate_current_geometry(paint.mesh(),l);const auto domain=paint.paint_sampling_domain();const auto source=before_->paint->paint_sampling_domain();
 if(!own(domain->basis(),source?source->basis():before_->paint->mesh_owner()))throw std::invalid_argument("Polygon current Paint borrowed a different original writable basis");
 // Clear/rebake need the actual role write adapter. Neither clearing a field nor
 // accepting a different field with equal pixels is a valid first transition.
 const auto old_field=before_->paint->baked()?before_->paint->baked()->normal_field:nullptr,new_field=paint.baked()?paint.baked()->normal_field:nullptr;
 if(!own(old_field,new_field))throw std::invalid_argument("Polygon changed normal detail requires its actual current-field write adapter");
 const auto map=trace_.triangle_permutation(l);const auto actual=domain->source_corners();if(actual.size()!=map.size()*3)throw std::invalid_argument("Polygon current Paint lost its exact source corner stream");
 for(size_t t=0;t<map.size();++t)for(size_t c=0;c<3;++c){stop(l);size_t k=0;while(k<3&&map[t].source_barycentrics[c][k]!=1)++k;if(k==3)throw std::invalid_argument("Polygon current Paint requires its barycentric refinement adapter");const auto native=native_corners_.at(map[t].source_triangle*3+k);if(actual[t*3+c]!=(source?source->source_corners()[native]:native))throw std::invalid_argument("Polygon current Paint correspondence differs from its authentic edit replay");}
 stop(l);
}
void PolygonSurfaceOwner::validate_binding(const SourceMaterialBindings&b,const PolygonSurfaceTransferLimits&l)const{
 if(!valid_owner(b.polygon_surface_transfer)||!attachment_||b.has_mtoon||b.normal_frames||b.canonical_attributes||b.canonical_uv||b.canonical_paint_sampling||b.native_paint_sampling||b.native_paint_frames||b.key_derivation||b.normal_preparation||b.color_preparation||b.normal_frame!=SourceMaterialNormalFrame::UvDerivative||b.sampling_reparameterized)throw std::invalid_argument("Polygon surface current binding lost its actual separate field/geometry owner");
 const auto old=before_->material_bindings;SourceNativeMaterial reference;
 if(old)reference=old->native_reference;else{reference.color=before_->material.color;reference.emission=before_->material.emissive;reference.alpha=int(before_->material.alpha);reference.alpha_cutoff=before_->material.alpha_cutoff;reference.double_sided=before_->material.double_sided;}
 if((old?!own(old->uv_origin,b.uv_origin):bool(b.uv_origin))||b.native_reference.color!=reference.color||b.native_reference.emission!=reference.emission||b.native_reference.alpha!=reference.alpha||b.native_reference.alpha_cutoff!=reference.alpha_cutoff||b.native_reference.double_sided!=reference.double_sided)throw std::invalid_argument("Polygon surface binding changed its authentic original source/reference units");
 for(const auto&role:b.roles)if(role.image)throw std::invalid_argument("Polygon surface typed image roles require their actual role adapter");
 validate(l);
}
PolygonSurfaceStage PolygonSurfaceOwner::stage_paint(const PolygonSurfaceTransferLimits&l)const{
 return stage_paint({},l);
}
PolygonSurfaceStage PolygonSurfaceOwner::stage_paint(std::shared_ptr<const Mesh>geometry,const PolygonSurfaceTransferLimits&l)const{
 validate(l);if(attachment_)throw std::invalid_argument("Polygon current Paint stage is already connected; advance an actual edit before publishing again");
 const auto domain=paint_domain(std::move(geometry),l);size_t copied=trace_.storage_bytes();charge(copied,1,sizeof(PolygonSurfaceOwner)+sizeof(PaintedMesh)+256,l);
 for(const auto&layer:before_->paint->layers()){charge(copied,1,sizeof(PaintLayer),l);charge(copied,layer.name.size()+1,1,l);charge(copied,layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),sizeof(std::shared_ptr<const PaintTile>),l);}
 charge(copied,paint_corners_.size()+normal_corners_.size(),sizeof(std::array<float,2>),l);charge(copied,native_corners_.size(),sizeof(uint32_t),l);for(const auto&step:trace_.steps())charge(copied,step.edit.selection.ids.size(),sizeof(PolygonId),l);
 if(copied>=l.maximum_working_bytes)throw std::length_error("Polygon Paint stage exhausted its prospective owner/snapshot quota");
 PaintSamplingLimits limits;limits.max_working_bytes=l.maximum_working_bytes-copied;limits.max_basis_bytes=std::min(size_t(128*1024*1024),limits.max_working_bytes);limits.max_vertices=l.geometry.maximum_vertices;limits.max_triangles=l.geometry.maximum_faces;limits.max_work=l.geometry.maximum_work;limits.cancelled=l.geometry.cancelled;
 auto paint=retain_paint_grid_shared(*before_->paint,domain->derived_geometry(),domain,limits);auto owner=std::shared_ptr<PolygonSurfaceOwner>(new PolygonSurfaceOwner);owner->before_=before_;owner->trace_=trace_;owner->native_corners_=native_corners_;owner->paint_corners_=paint_corners_;owner->normal_corners_=normal_corners_;owner->attachment_=paint->paint_sampling_attachment();owner->self_=owner;owner->validate_current(*paint,l);
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->polygon_surface_transfer=owner;
 if(const auto old=before_->material_bindings){binding->uv_origin=old->uv_origin;binding->native_reference=old->native_reference;binding->physical_emission=old->physical_emission;}
 else{binding->native_reference.color=before_->material.color;binding->native_reference.emission=before_->material.emissive;binding->native_reference.alpha=int(before_->material.alpha);binding->native_reference.alpha_cutoff=before_->material.alpha_cutoff;binding->native_reference.double_sided=before_->material.double_sided;}
 binding->validate();stop(l);return {std::move(owner),std::move(paint),std::move(binding)};
}
bool adopt_polygon_surface_edit(Scene&scene,uint64_t id,const PolygonView&view,const Scene&original,const PolygonSurfaceTransferLimits&l){
 stop(l);const auto&source=original.object(id);const auto&current=scene.object(id);
 if(!polygon_surface_transfer_required(source)||!view.topology||view.colors||!view.replay_complete||!view.replay)throw std::invalid_argument("Polygon field adoption requires its actual complete geometry operation program");
 if(source.material_bindings&&source.material_bindings->polygon_surface_transfer)throw std::invalid_argument("Repeated polygon field authoring needs its actual current-paint checkpoint adapter");
 original.validate();
 if(current.id!=source.id||current.group!=source.group||current.volume->geometry_owner()!=source.volume->geometry_owner()||current.volume->revision()!=source.volume->revision()||current.mesh_active!=source.mesh_active||current.deformation!=source.deformation||current.appearance!=source.appearance||current.material_bindings!=source.material_bindings||current.polygon!=source.polygon||!current.painted||current.painted->mesh_owner()!=source.painted->mesh_owner()||current.painted->revision()!=source.painted->revision()||!same_material(current.material,source.material))throw std::invalid_argument("Polygon field source authority changed before adoption");
 std::array<const PolygonReplayNode*,128>program{};size_t count=0,entries=0;
 for(auto n=view.replay;n;n=n->previous){stop(l);if(count>=program.size()||n->step.edit.selection.ids.size()>l.maximum_entries-entries)throw std::length_error("Polygon field authoring program exceeds its exact replay quota");entries+=n->step.edit.selection.ids.size();program[count++]=n.get();}
 if(count!=view.replay->depth||entries!=view.replay->entries)throw std::invalid_argument("Polygon field authoring program counters differ from actual retained operations");
 auto owner=PolygonSurfaceOwner::capture(original,id,l);if(owner->trace().original()->encode()==view.topology->encode()){stop(l);return false;}while(count)owner=owner->advance(program[--count]->step,l);
 if(owner->trace().current()->encode()!=view.topology->encode())throw std::invalid_argument("Polygon field authoring program differs from the actual accepted topology");
 auto stage=owner->stage_paint(l);auto authority=std::make_shared<PolygonAuthority>();authority->topology=*view.topology;authority->origin=stage.owner->before();
 auto result=scene;auto&target=result.object(id);target.mesh_active=true;target.painted=std::move(stage.paint);target.material_bindings=std::move(stage.binding);target.deformation.reset();target.appearance.reset();target.retopology.reset();target.polygon=std::move(authority);
 result.validate();stop(l);scene=std::move(result);return true;
}
} // namespace dome::detail
