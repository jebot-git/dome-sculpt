// SPDX-License-Identifier: GPL-3.0-or-later
#include "baked-emission-domain.hpp"
#include "emission-bake-source.hpp"
#include "paint-overlay-sample.hpp"
#include "source-writable-paint.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-maps.hpp"
#include "native-read-scope.hpp"
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
using Point=std::array<long double,2>;
long double orient(Point a,Point b,Point c){return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);}
bool separated(const std::array<Point,3>&a,const std::array<Point,3>&b){
 for(size_t k=0;k<3;++k){const auto p=a[k],q=a[(k+1)%3];long double low_a=0,high_a=0,low_b=orient(p,q,b[0]),high_b=low_b;
  for(auto v:a){const auto d=orient(p,q,v);low_a=std::min(low_a,d);high_a=std::max(high_a,d);}
  for(auto v:b){const auto d=orient(p,q,v);low_b=std::min(low_b,d);high_b=std::max(high_b,d);}
  if(high_a<=low_b||high_b<=low_a)return true;
 }return false;
}
void disjoint(const std::vector<std::array<Point,3>>&triangles,const std::function<bool()>&cancelled){
 for(size_t i=0;i<triangles.size();++i){stop(cancelled);const auto&a=triangles[i];if(orient(a[0],a[1],a[2])==0)throw std::invalid_argument("Retained emission correspondence has a degenerate chart");
  for(size_t j=0;j<i;++j)if(!separated(a,triangles[j])&&!separated(triangles[j],a))throw std::invalid_argument("Retained emission correspondence has overlapping interiors");
 }
}
}
size_t BakedEmissionDomain::preflight(const Scene &scene,const std::function<bool()>&cancelled){
 std::array<const BakedEmissionDomain*,64> active{};std::set<const BakedEmissionDomain*> done;size_t bytes=0;
 auto visit=[&](auto&&self,const Scene&s,size_t depth)->void{stop(cancelled);for(const auto&o:s.objects()){
  if(o.volume){const auto count=o.volume->stored_samples();if(count>(retained_limit-bytes)/128)throw std::length_error("Retained emission before-volume exceeds prospective budget");bytes+=count*128;}
  if(!o.painted)continue;const auto paint_bytes=o.painted->storage_bytes();if(paint_bytes>retained_limit-bytes)throw std::length_error("Retained emission before-paint exceeds prospective budget");bytes+=paint_bytes;
  const auto baked=o.painted->baked();const auto domain=baked?baked->emission_domain:nullptr;if(!domain)continue;
  if(!domain->valid_owner(domain))throw std::invalid_argument("Retained emission ancestor lost its immutable owner");domain->validate_current(o.painted->mesh_owner(),o.painted->resolution());
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
 const auto prospective=preflight(scene,cancelled);stop(cancelled);if(!target||target_resolution<64||target_resolution>2048||(target_resolution&(target_resolution-1)))throw std::invalid_argument("Invalid retained emission target");
 const auto &object=scene.object(source_id);const auto&paint=object.painted;const auto&binding=object.material_bindings;
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
 const auto relative=ob::compose(ob::inverse(target_world),scene.world_transform(source_id));
 for(auto&p:high.positions){const auto q=ob::transform(relative,{p.x,p.y,p.z});p={q.x,q.y,q.z};}
 // Exact common coordinate plane, then exact separating-axis predicates on
 // FLOAT inputs promoted to long double. No fitted affine map or epsilon grants
 // correspondence. Disjoint interiors prove that a projection onto this plane
 // cannot select an unrelated front surface.
 int axis=-1;for(int k=0;k<3;++k){bool equal=true;for(const auto&p:high.positions)equal&=component(p,k)==component(high.positions.front(),k);if(equal){axis=k;break;}}
 if(axis<0)throw std::invalid_argument("Retained emission requires an exact planar correspondence certificate");
 const int x=(axis+1)%3,y=(axis+2)%3;std::vector<std::array<Point,3>> surface,charts;
 for(size_t tri=0;tri<high.indices.size()/3;++tri){stop(cancelled);std::array<Point,3> geometry{},chart{};
  for(size_t k=0;k<3;++k){const size_t corner=tri*3+k;const auto p=high.positions.at(high.indices[corner]),q=target->positions.at(target->indices[corner]);
   if(!same(p,q))throw std::invalid_argument("Retained emission target differs from actual evaluated placed source corners");
   std::array<float,3>bary{};bary[k]=1;const auto uv=source_overlay_coordinates(*packet,uint32_t(tri),bary,paint->mesh().uv.at(paint->mesh().indices[corner]));
   if(uv!=target->uv.at(target->indices[corner]))throw std::invalid_argument("Retained emission target UV differs from genuine source paint corners");
   geometry[k]={component(p,x),component(p,y)};chart[k]={uv[0],uv[1]};
  }surface.push_back(geometry);charts.push_back(chart);
 }
 disjoint(surface,cancelled);disjoint(charts,cancelled);
 const auto r=paint->resolution();const auto background=binding&&binding->physical_emission?*binding->physical_emission:object.material.emissive;
 if(prospective>retained_limit||size_t(r)*r>(retained_limit-prospective)/sizeof(BakedEmissionSample))throw std::length_error("Retained emission source exceeds prospective field budget");
 std::vector<BakedEmissionSample> values(size_t(r)*r);
 for(uint32_t y=0;y<r;++y){stop(cancelled);for(uint32_t x=0;x<r;++x){const auto v=sample_overlay_texel_values(*packet,int(x),int(y),false);BakedEmissionSample layer;layer.coverage=v[3][3];for(size_t c=0;c<3;++c){layer.legacy[c]=v[2][c];layer.absolute[c]=v[4][c];}values[size_t(y)*r+x]={{},layer.resolve(background,1),1};}}
 auto result=std::shared_ptr<BakedEmissionDomain>(new BakedEmissionDomain);result->field_=BakedEmissionField::create(r,values,{retained_limit,cancelled});
 result->before_=std::make_shared<const Scene>(scene.snapshot());result->target_=std::move(target);result->target_resolution_=target_resolution;result->target_world_=target_world;result->source_=source_id;
 const auto encoded=nlohmann::json::to_cbor(result->before_->encode());if(encoded.size()>retained_limit-result->field_->storage_bytes())throw std::length_error("Retained emission before-scene exceeds retained budget");
 result->retained_bytes_=encoded.size()+result->field_->storage_bytes()+sizeof(BakedEmissionDomain);result->self_=result;stop(cancelled);return result;
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
 auto before=Scene::decode(j.at("before"));CanvasTransform t;t.position={j.at("position")[0].get<float>(),j.at("position")[1].get<float>(),j.at("position")[2].get<float>()};t.orientation={j.at("rotation")[3].get<float>(),j.at("rotation")[0].get<float>(),j.at("rotation")[1].get<float>(),j.at("rotation")[2].get<float>()};t.scale=j.at("scale").get<float>();
 auto result=capture(before,j.at("source").get<uint64_t>(),std::move(target),r,t,cancelled);if(result->field()->resolution()!=j.at("resolution").get<uint32_t>())throw std::invalid_argument("Retained emission replay changed its actual grid");return result;
}
} // namespace dome::detail
