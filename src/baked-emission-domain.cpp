// SPDX-License-Identifier: GPL-3.0-or-later
#include "baked-emission-domain.hpp"
#include "emission-bake-source.hpp"
#include "paint-overlay-sample.hpp"
#include "source-writable-paint.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-maps.hpp"
#include "native-read-scope.hpp"
#include "volume-exact.hpp"
#include "paint-sampling-access.hpp"
#include "emission-source-payloads.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace dome::detail {namespace {
constexpr size_t retained_limit=32*1024*1024;
void stop(const std::function<bool()>&cancelled){NativeReadScope::check();if(cancelled&&cancelled())throw std::runtime_error("Retained emission capture cancelled");}
float component(Vec3 p,int k){return k==0?p.x:k==1?p.y:p.z;}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
using Point=std::array<float,2>;
struct Work{const std::function<bool()>&cancelled;size_t spent=0;void step(size_t n=1){if(n>64*1024*1024-spent)throw std::length_error("Retained emission certificate exceeds exact-work budget");spent+=n;if(!(spent%1024))stop(cancelled);}};
using Exact=volume_exact::Arithmetic<Work>;
bool separated(const std::array<Point,3>&a,const std::array<Point,3>&b,Exact&exact){
 for(size_t k=0;k<3;++k){const auto p=a[k],q=a[(k+1)%3];const int side=exact.orient2(p,q,a[(k+2)%3]).sign;bool outside=true;for(auto v:b)outside&=exact.orient2(p,q,v).sign*side<=0;if(outside)return true;}return false;
}
void disjoint(const std::vector<std::array<Point,3>>&triangles,Exact&exact,const std::function<bool()>&cancelled){
 for(size_t i=0;i<triangles.size();++i){stop(cancelled);const auto&a=triangles[i];if(exact.orient2(a[0],a[1],a[2]).sign==0)throw std::invalid_argument("Retained emission correspondence has a degenerate chart");
  for(size_t j=0;j<i;++j)if(!separated(a,triangles[j],exact)&&!separated(triangles[j],a,exact))throw std::invalid_argument("Retained emission correspondence has overlapping chart interiors");
 }
}
std::array<float,3> point(Vec3 p){return {p.x,p.y,p.z};}
int outward(Vec3 a,Vec3 b,Vec3 c,Vec3 normal,Exact&exact){
 std::array<volume_exact::Integer,3>x,y,z;for(size_t k=0;k<3;++k){const auto origin=exact.scalar(point(a)[k]);x[k]=exact.subtract(exact.scalar(point(b)[k]),origin);y[k]=exact.subtract(exact.scalar(point(c)[k]),origin);z[k]=exact.scalar(point(normal)[k]);}
 auto term=[&](int i,int j,int k){return exact.multiply(x[i],exact.subtract(exact.multiply(y[j],z[k]),exact.multiply(y[k],z[j])));};return exact.add(exact.add(term(0,1,2),term(1,2,0)),term(2,0,1)).sign;
}

}
size_t BakedEmissionDomain::preflight(const Scene &scene,const std::function<bool()>&cancelled){
 std::array<const BakedEmissionDomain*,64> active{};std::set<const BakedEmissionDomain*> done;size_t bytes=0;
 auto visit=[&](auto&&self,const Scene&s,size_t depth)->void{
  stop(cancelled);const auto payloads=emission_source_payloads(s,cancelled);
  for(const auto &volume:payloads.volumes){const auto count=volume->stored_samples();if(count>(retained_limit-bytes)/128)throw std::length_error("Retained emission before-volume exceeds prospective budget");bytes+=count*128;}
  for(const auto &image:payloads.images){if(image->rgba.size()>retained_limit-bytes)throw std::length_error("Retained emission source image exceeds prospective budget");bytes+=image->rgba.size();}
  for(const auto &paint:payloads.paints){
   stop(cancelled);const auto &mesh=paint->mesh();const auto raster=paint_sampling_surface_retained_bytes(mesh.positions.size(),mesh.indices.size(),paint->resolution());
   if(raster>retained_limit-bytes)throw std::length_error("Retained emission before-raster exceeds prospective budget");bytes+=raster;
   const auto &diagnostics=paint->surface_diagnostics();const size_t candidates=diagnostics.additional_candidates+diagnostics.linked_texels;
   if(candidates>(retained_limit-bytes)/256)throw std::length_error("Retained emission before-candidate storage exceeds prospective budget");bytes+=candidates*256;
   const auto paint_bytes=paint->storage_bytes();if(paint_bytes>retained_limit-bytes)throw std::length_error("Retained emission before-paint exceeds prospective budget");bytes+=paint_bytes;
   const auto baked=paint->baked();const auto domain=baked?baked->emission_domain:nullptr;if(!domain)continue;
   if(!domain->valid_owner(domain))throw std::invalid_argument("Retained emission ancestor lost its immutable owner");domain->validate_current(paint->mesh_owner(),paint->resolution());
   if(std::find(active.begin(),active.begin()+depth,domain.get())!=active.begin()+depth)throw std::invalid_argument("Retained emission owner graph is cyclic");
   if(done.contains(domain.get()))continue;if(depth==active.size()||done.size()==256)throw std::length_error("Retained emission owner graph exceeds64-depth/256-owner limit");
   active[depth]=domain.get();self(self,*domain->before_,depth+1);done.insert(domain.get());
 }};visit(visit,scene,0);return bytes;
}
void BakedEmissionDomain::preflight(const PaintedMesh &paint,const std::function<bool()>&cancelled){
 const auto baked=paint.baked();if(!baked||!baked->emission_domain)return;const auto domain=baked->emission_domain;
 if(!domain->valid_owner(domain))throw std::invalid_argument("Retained emission source lost its actual immutable owner");domain->validate_current(paint.mesh_owner(),paint.resolution());(void)preflight(*domain->before_,cancelled);
}
std::shared_ptr<const BakedEmissionDomain> BakedEmissionDomain::capture(const Scene &scene,uint64_t source_id,
 std::shared_ptr<const Mesh> target,uint32_t target_resolution,CanvasTransform target_world,const std::function<bool()> &cancelled){
 auto context=emission_source_closure(scene,source_id,cancelled);
 return capture_provenance(context,source_id,std::move(target),target_resolution,target_world,cancelled);
}
std::shared_ptr<const BakedEmissionDomain> BakedEmissionDomain::capture_provenance(const Scene &scene,uint64_t source_id,
 std::shared_ptr<const Mesh> target,uint32_t target_resolution,CanvasTransform target_world,const std::function<bool()> &cancelled){
 const auto prospective=preflight(scene,cancelled);stop(cancelled);if(!target||target_resolution<64||target_resolution>2048||(target_resolution&(target_resolution-1)))throw std::invalid_argument("Invalid retained emission target");
 // Freeze the bounded before-context before deriving coefficients. The
 // retained field and certificate must describe the same immutable values,
 // including externally aliased material DTOs from before capture.
 const auto before_json=scene.encode();const auto before_bytes=nlohmann::json::to_cbor(before_json);
 if(before_bytes.size()>retained_limit)throw std::length_error("Retained emission before-scene exceeds retained budget");
 auto frozen=std::make_shared<const Scene>(Scene::decode(before_json));
 const auto frozen_bytes=preflight(*frozen,cancelled);
 const auto &object=frozen->object(source_id);const auto&paint=object.painted;const auto&binding=object.material_bindings;
 if(!object.mesh_active||!paint||object.appearance||(binding&&binding->has_mtoon)||(paint->source_maps()&&paint->source_maps()->emission.image)||
    (paint->baked()&&paint->baked()->emission_domain))throw std::invalid_argument("This emission function needs its explicit multi-domain transport adapter");
 // This initial certificate is intentionally narrow: no image or animated typed
 // background may be collapsed into the writable field. Every contribution is
 // evaluated on the same genuine native paint grid, with a constant background.
 const auto sampler=emission_bake_source(object,cancelled);if(!sampler)throw std::invalid_argument("Retained emission source has no actual physical function");
 auto packet=material_paint_attachment(binding.get())?PaintOverlayFactory::capture_normal_source(object,cancelled):PaintOverlayFactory::capture(*paint,cancelled);

 if(packet->resolution()!=paint->resolution())throw std::invalid_argument("Retained emission source changes contribution grid");
 if(target->indices.size()!=paint->mesh().indices.size()||target->indices.size()/3>1024)throw std::invalid_argument("Retained emission requires bounded exact ordered source corners");
 auto high=object.deformation?object.deformation->preview(paint->mesh()):paint->mesh();
 const auto relative=ob::compose(ob::inverse(target_world),frozen->world_transform(source_id));
 for(auto&p:high.positions){const auto q=ob::transform(relative,{p.x,p.y,p.z});p={q.x,q.y,q.z};}
 // Supporting-plane predicates prove that an outward ray from any interior
 // point cannot meet another source triangle before the corresponding point.
 // All predicates use the existing bounded exact binary32 dyadic arithmetic.
 Work work{cancelled};Exact exact(work);std::vector<std::array<Point,3>>charts;
 for(size_t tri=0;tri<high.indices.size()/3;++tri){stop(cancelled);std::array<Point,3>chart{};
  const auto a=high.positions.at(high.indices[tri*3]),b=high.positions.at(high.indices[tri*3+1]),c=high.positions.at(high.indices[tri*3+2]);
  for(auto p:high.positions)if(exact.orient3(point(a),point(b),point(c),point(p))>0)throw std::invalid_argument("Retained emission needs exact non-occluding supporting source faces");
  for(size_t k=0;k<3;++k){const size_t corner=tri*3+k;const auto p=high.positions.at(high.indices[corner]),q=target->positions.at(target->indices[corner]);
   if(!same(p,q)||outward(a,b,c,target->normals.at(target->indices[corner]),exact)<=0)throw std::invalid_argument("Retained emission target differs from actual supporting source corners/ray direction");
   std::array<float,3>bary{};bary[k]=1;const auto uv=source_overlay_coordinates(*packet,uint32_t(tri),bary,paint->mesh().uv.at(paint->mesh().indices[corner]));
   if(uv!=target->uv.at(target->indices[corner]))throw std::invalid_argument("Retained emission target UV differs from genuine source paint corners");chart[k]=uv;
  }charts.push_back(chart);
 }
 disjoint(charts,exact,cancelled);
 const auto r=paint->resolution();const auto background=binding&&binding->physical_emission?*binding->physical_emission:object.material.emissive;
 const auto payload_bytes=std::max(prospective,frozen_bytes);
 if(before_bytes.size()>retained_limit-payload_bytes)throw std::length_error("Retained emission metadata plus resident payload exceeds retained budget");
 const auto retained_before=payload_bytes+before_bytes.size();
 if(retained_before>retained_limit||size_t(r)*r>(retained_limit-retained_before)/sizeof(BakedEmissionSample))throw std::length_error("Retained emission source exceeds prospective field budget");
 std::vector<BakedEmissionSample> values(size_t(r)*r);
 for(uint32_t y=0;y<r;++y){stop(cancelled);for(uint32_t x=0;x<r;++x){const auto v=sample_overlay_texel_values(*packet,int(x),int(y),false);BakedEmissionSample layer;layer.coverage=v[3][3];for(size_t c=0;c<3;++c){layer.legacy[c]=v[2][c];layer.absolute[c]=v[4][c];}values[size_t(y)*r+x]={{},layer.resolve(background,1),1};}}
 auto result=std::shared_ptr<BakedEmissionDomain>(new BakedEmissionDomain);result->field_=BakedEmissionField::create(r,values,{retained_limit,cancelled});
 if(before_bytes.size()>retained_limit-result->field_->storage_bytes())throw std::length_error("Retained emission before-scene exceeds retained budget");
 result->before_=std::move(frozen);result->target_=std::move(target);result->target_resolution_=target_resolution;result->target_world_=target_world;result->source_=source_id;
 const auto encoded=nlohmann::json::to_cbor(result->before_->encode());if(encoded.size()>retained_limit-result->field_->storage_bytes())throw std::length_error("Retained emission before-scene exceeds retained budget");
 result->retained_bytes_=std::max(retained_before,encoded.size())+result->field_->storage_bytes()+sizeof(BakedEmissionDomain);result->self_=result;stop(cancelled);return result;
}
void BakedEmissionDomain::validate_current(const std::shared_ptr<const Mesh>&mesh,uint32_t r)const{
 if(!field_||!before_||mesh!=target_||mesh.owner_before(target_)||target_.owner_before(mesh)||r!=target_resolution_)throw std::invalid_argument("Retained emission borrowed another current geometry/grid owner");
}
bool BakedEmissionDomain::valid_owner(const std::shared_ptr<const BakedEmissionDomain>&p)const noexcept{const auto self=self_.lock();return p&&self&&p==self&&!p.owner_before(self)&&!self.owner_before(p);}
nlohmann::json BakedEmissionDomain::encode()const{
 const auto &t=target_world_;return {{"version",1},{"resolution",field_->resolution()},{"source",source_},{"before",before_->encode()},{"position",{t.position.x,t.position.y,t.position.z}},{"rotation",{t.orientation.x,t.orientation.y,t.orientation.z,t.orientation.w}},{"scale",t.scale}};
}
std::shared_ptr<const BakedEmissionDomain>BakedEmissionDomain::decode(const nlohmann::json&j,std::shared_ptr<const Mesh>target,uint32_t r,const std::function<bool()>&cancelled){
 stop(cancelled);if(!j.is_object()||j.size()!=7||j.at("version")!=1||!j.at("source").is_number_unsigned()||j.at("position").size()!=3||j.at("rotation").size()!=4)throw std::invalid_argument("Invalid retained emission source record");
 const auto bytes=nlohmann::json::to_cbor(j.at("before"));if(bytes.size()>retained_limit)throw std::length_error("Retained emission source archive exceeds bound");
 static thread_local size_t depth=0;if(depth>=8)throw std::length_error("Retained emission archive exceeds owning depth");struct Scope{size_t&depth;Scope(size_t&d):depth(d){++depth;}~Scope(){--depth;}}scope(depth);
 auto before=Scene::decode(j.at("before"));CanvasTransform t;t.position={j.at("position")[0].get<float>(),j.at("position")[1].get<float>(),j.at("position")[2].get<float>()};t.orientation.x=j.at("rotation")[0].get<float>();t.orientation.y=j.at("rotation")[1].get<float>();t.orientation.z=j.at("rotation")[2].get<float>();t.orientation.w=j.at("rotation")[3].get<float>();t.scale=j.at("scale").get<float>();validate_transform(t);
 auto result=capture_provenance(before,j.at("source").get<uint64_t>(),std::move(target),r,t,cancelled);if(result->field()->resolution()!=j.at("resolution").get<uint32_t>())throw std::invalid_argument("Retained emission replay changed its actual grid");return result;
}
} // namespace dome::detail
