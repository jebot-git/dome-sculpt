// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/paint-sampling.hpp"
#include "refinement-geometry.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
namespace dome {namespace detail {
struct WeightedPaintLayout {
 std::vector<PaintSamplingPoint> points;
 std::vector<uint32_t> geometry_corners;
};
class WeightedPaintDomain final {
public:
 std::shared_ptr<const PaintSamplingDomain> previous;
 std::shared_ptr<const RefinementGeometry> geometry;
 std::shared_ptr<const WeightedPaintLayout> layout;
};
} namespace {
struct WeightedWorkState {uint64_t limit=0,used=0;size_t depth=0;};
thread_local WeightedWorkState weighted_work;
struct WeightedScope {
 explicit WeightedScope(const PaintSamplingLimits&l){if(!weighted_work.depth){weighted_work.limit=l.max_work;weighted_work.used=0;}++weighted_work.depth;}
 ~WeightedScope(){if(!--weighted_work.depth)weighted_work={};}
};
struct WeightedPath {
 static thread_local std::array<const PaintSamplingDomain*,32> nodes;
 static thread_local size_t depth;
 explicit WeightedPath(const PaintSamplingDomain*p){if(depth>=nodes.size()||std::find(nodes.begin(),nodes.begin()+depth,p)!=nodes.begin()+depth)throw std::length_error("Weighted paint domain exceeds bounded acyclic history");nodes[depth++]=p;}
 ~WeightedPath(){nodes[--depth]=nullptr;}
};
thread_local std::array<const PaintSamplingDomain*,32> WeightedPath::nodes{};
thread_local size_t WeightedPath::depth=0;
struct Budget {
 const PaintSamplingLimits &limits;uint64_t used=0;
 explicit Budget(const PaintSamplingLimits &l):limits(l){
  if(!l.max_vertices||l.max_vertices>2'000'000||!l.max_triangles||l.max_triangles>2'000'000||!l.max_basis_bytes||l.max_basis_bytes>128*1024*1024||!l.max_working_bytes||l.max_working_bytes>512*1024*1024||!l.max_work||l.max_work>256'000'000)throw std::invalid_argument("Invalid writable paint-domain resource limits");stop();
 }
 void stop(){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Writable paint-domain operation cancelled");}
 void work(uint64_t n=1){if(weighted_work.depth){if(n>weighted_work.limit-weighted_work.used)throw std::length_error("Weighted paint aggregate work budget exceeded");weighted_work.used+=n;}if(n>limits.max_work-used)throw std::length_error("Writable paint-domain work budget exceeded");used+=n;if((used&1023)==0)stop();}
 void bytes(size_t total)const{if(total>limits.max_working_bytes)throw std::length_error("Writable paint-domain working bytes exceeded");}
};
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
size_t mesh_bytes(const Mesh &m,Budget &budget,bool paint){
 if(m.positions.empty()||m.positions.size()>budget.limits.max_vertices||m.indices.empty()||m.indices.size()%3||m.indices.size()/3>budget.limits.max_triangles||m.normals.size()!=m.positions.size()||m.uv.size()!=m.positions.size()||(!m.uv_pins.empty()&&m.uv_pins.size()!=m.positions.size())||(!m.uv_seams.empty()&&m.uv_seams.size()!=m.indices.size()))throw std::invalid_argument("Writable paint-domain mesh dimensions exceed their bounds");
 // Every term is bounded above before multiplication; no unchecked sum can wrap.
 const size_t bytes=m.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+m.indices.size()*sizeof(uint32_t)+m.uv_pins.size()+m.uv_seams.size();budget.bytes(bytes);
 budget.work(m.positions.size()+m.indices.size());budget.stop();m.validate();budget.stop();
 if(paint)for(const auto &uv:m.uv){budget.work();if(uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1)throw std::invalid_argument("Writable paint-domain basis UVs must fit its existing unit grid");}
 return bytes;
}
size_t own_bytes(const PaintSamplingDomain &domain,Budget &budget){
 if(domain.version()!=1||!domain.basis())throw std::invalid_argument("Writable paint-domain requires its immutable geometry basis");
 const auto bytes=mesh_bytes(*domain.basis(),budget,true);
 if(domain.source_corners().size()!=domain.basis()->indices.size())throw std::invalid_argument("Writable paint-domain lost complete source-corner correspondence");
 const auto all=bytes+domain.source_corners().size()*sizeof(uint32_t);
 if(all>budget.limits.max_basis_bytes)throw std::length_error("Writable paint-domain retained basis exceeds its byte limit");budget.bytes(all);return all;
}
void correspondence(const Mesh &source,const Mesh &current,std::span<const uint32_t> corners,Budget &budget,size_t owned){
 if(corners.size()!=current.indices.size()||corners.size()!=source.indices.size())throw std::invalid_argument("Writable paint-domain changed its complete triangle count");
 const size_t faces=source.indices.size()/3;budget.bytes(owned+faces);std::vector<uint8_t> seen(faces);
 for(size_t c=0;c<corners.size();c+=3){
  budget.work(3);const auto first=corners[c];
  if(first>=source.indices.size()||first%3||corners[c+1]!=first+1||corners[c+2]!=first+2||seen[first/3]++)throw std::invalid_argument("Writable paint-domain source triangles were reversed, duplicated or lost");
  for(size_t k=0;k<3;++k){const auto a=current.indices[c+k],b=source.indices[corners[c+k]];
   if(!same(current.positions[a],source.positions[b])||!same(current.normals[a],source.normals[b]))throw std::invalid_argument("Writable paint-domain requires exact ordered REST corners before pose/world evaluation");
  }
 }
 budget.stop();
}
size_t validate(const PaintSamplingDomain &domain,const Mesh &current,Budget &budget){
 const auto owned=own_bytes(domain,budget);const auto bytes=mesh_bytes(current,budget,false);budget.bytes(owned+bytes+domain.basis()->indices.size()/3);
 correspondence(*domain.basis(),current,domain.source_corners(),budget,owned+bytes);return owned;
}
}
uint32_t PaintSamplingDomain::version()const noexcept{return weighted_?2:1;}
std::span<const PaintSamplingPoint> PaintSamplingDomain::weighted_corners()const noexcept{return weighted_&&weighted_->layout?std::span<const PaintSamplingPoint>(weighted_->layout->points):std::span<const PaintSamplingPoint>{};}
std::span<const uint32_t> PaintSamplingDomain::geometry_corners()const noexcept{return weighted_&&weighted_->layout?std::span<const uint32_t>(weighted_->layout->geometry_corners):std::span<const uint32_t>{};}
const std::shared_ptr<const detail::RefinementGeometry>&PaintSamplingDomain::refined_geometry()const noexcept{static const std::shared_ptr<const detail::RefinementGeometry> empty;return weighted_?weighted_->geometry:empty;}
const std::shared_ptr<const PaintSamplingDomain>&PaintSamplingDomain::previous_domain()const noexcept{static const std::shared_ptr<const PaintSamplingDomain> empty;return weighted_?weighted_->previous:empty;}
size_t PaintSamplingDomain::storage_bytes(const PaintSamplingLimits &limits)const{Budget b(limits);if(weighted_){WeightedScope scope(limits);WeightedPath path(this);if(!basis_||!weighted_->geometry||!weighted_->layout||!weighted_->geometry->valid_owner(weighted_->geometry))throw std::invalid_argument("Weighted paint lost genuine geometry");size_t bytes=sizeof(PaintSamplingDomain)+sizeof(detail::WeightedPaintDomain)+sizeof(detail::WeightedPaintLayout);auto add=[&](size_t n){if(n>limits.max_basis_bytes-bytes)throw std::length_error("Weighted paint retained basis exceeds byte limit");bytes+=n;};if(bytes>limits.max_basis_bytes)throw std::length_error("Weighted paint retained basis exceeds byte limit");add(weighted_->geometry->retained_bytes());if(weighted_->previous)add(weighted_->previous->storage_bytes(limits));else mesh_bytes(*basis_,b,true);if(weighted_->layout->points.capacity()>limits.max_basis_bytes/sizeof(PaintSamplingPoint))throw std::length_error("Weighted paint retained corner count exceeds byte limit");add(weighted_->layout->points.capacity()*sizeof(PaintSamplingPoint));add(weighted_->layout->geometry_corners.capacity()*sizeof(uint32_t));b.bytes(bytes);return bytes;}return own_bytes(*this,b);}
void PaintSamplingDomain::validate_current(const Mesh &current,const PaintSamplingLimits &limits)const{
 Budget b(limits);if(!weighted_){validate(*this,current,b);return;}
 WeightedScope scope(limits);const auto owned=storage_bytes(limits);const auto &g=weighted_->geometry;const auto before_work=weighted_work.limit-weighted_work.used;b.work(g->refinement().work_units+g->refinement().corners.size()*8);auto geometry_limits=limits;geometry_limits.max_work=before_work;g->validate(geometry_limits);const auto &expected=*g->mesh();const auto current_bytes=mesh_bytes(current,b,false);
 if(weighted_->layout->points.size()!=current.indices.size()||geometry_corners().size()!=current.indices.size()||!source_corners_.empty())throw std::invalid_argument("Weighted paint current corner dimensions differ from actual refinement");
 // Authored UV/pins/seams belong to the current attachment, independently of
 // this root REST/corner proof. Whole logical triangles may be permuted only.
 correspondence(expected,current,geometry_corners(),b,owned+current_bytes);
 auto replay=retain_refined_paint_sampling(weighted_->previous,g,limits);if(replay->basis()!=basis_||replay->basis().owner_before(basis_)||basis_.owner_before(replay->basis())||replay->weighted_corners().size()!=weighted_->layout->points.size())throw std::invalid_argument("Weighted paint basis association differs from actual before grid");
 for(size_t c=0;c<weighted_->layout->points.size();++c){b.work();const auto&a=weighted_->layout->points[c],&v=replay->weighted_corners()[geometry_corners()[c]];if(a.source_corners!=v.source_corners||a.weights!=v.weights)throw std::invalid_argument("Weighted paint stencil differs from actual before grid replay");}b.bytes(owned+g->working_bytes());b.stop();
}
std::vector<std::array<float,2>> PaintSamplingDomain::corner_coordinates(const Mesh &current,const PaintSamplingLimits &limits)const{
 if(weighted_){WeightedScope scope(limits);validate_current(current,limits);Budget b(limits);const auto owned=storage_bytes(limits);b.bytes(owned+weighted_->layout->points.size()*sizeof(std::array<float,2>));std::vector<std::array<float,2>> out;out.reserve(weighted_->layout->points.size());for(const auto&p:weighted_->layout->points){b.work();std::array<double,2> uv{};for(size_t k=0;k<3;++k)for(size_t d=0;d<2;++d)uv[d]+=basis_->uv.at(basis_->indices.at(p.source_corners[k]))[d]*p.weights[k];out.push_back({float(uv[0]),float(uv[1])});}b.stop();return out;}
 Budget b(limits);const auto owned=validate(*this,current,b);b.bytes(owned+source_corners().size()*sizeof(std::array<float,2>));std::vector<std::array<float,2>> result;result.reserve(source_corners().size());
 for(auto corner:source_corners()){b.work();result.push_back(basis()->uv[basis()->indices[corner]]);}b.stop();return result;
}
Mesh PaintSamplingDomain::raster_mesh(const Mesh &current,const PaintSamplingLimits &limits)const{
 if(weighted_){WeightedScope scope(limits);auto uv=corner_coordinates(current,limits);Budget b(limits);const auto owned=storage_bytes(limits);b.bytes(owned+uv.capacity()*sizeof(std::array<float,2>)+current.indices.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t)));Mesh out;out.positions.reserve(uv.size());out.normals.reserve(uv.size());out.uv=std::move(uv);out.indices.reserve(out.uv.size());for(auto v:current.indices){b.work();out.positions.push_back(current.positions[v]);out.normals.push_back(current.normals[v]);out.indices.push_back(uint32_t(out.indices.size()));}b.stop();return out;}
 Budget b(limits);const auto owned=validate(*this,current,b);const size_t count=source_corners().size();b.bytes(owned+count*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t)));
 Mesh raster;raster.positions.reserve(count);raster.normals.reserve(count);raster.uv.reserve(count);raster.indices.reserve(count);
 for(size_t c=0;c<count;++c){b.work();const auto vertex=current.indices[c];raster.positions.push_back(current.positions[vertex]);raster.normals.push_back(current.normals[vertex]);raster.uv.push_back(basis()->uv[basis()->indices[source_corners()[c]]]);raster.indices.push_back(uint32_t(c));}b.stop();return raster;
}
std::shared_ptr<const PaintSamplingDomain> retain_paint_sampling(const Mesh &before,
 std::shared_ptr<const PaintSamplingDomain> previous,const Mesh &current,
 std::span<const uint32_t> corners,const PaintSamplingLimits &limits){
 Budget b(limits);const auto before_bytes=mesh_bytes(before,b,false),current_bytes=mesh_bytes(current,b,false);
 if(corners.size()!=current.indices.size()||corners.size()!=before.indices.size())throw std::invalid_argument("Writable paint-domain source-corner map dimensions differ");
 b.bytes(before_bytes+current_bytes+corners.size()*sizeof(uint32_t));
 correspondence(before,current,corners,b,before_bytes+current_bytes);
 if(previous&&previous->version()==2){
  WeightedScope scope(limits);previous->validate_current(before,limits);
  const auto owned=previous->storage_bytes(limits);
  bool identity=true;for(size_t c=0;c<corners.size();++c){b.work();if(corners[c]!=c)identity=false;}
  const size_t output=identity?0:corners.size()*(sizeof(PaintSamplingPoint)+sizeof(uint32_t));
  b.bytes(before_bytes+current_bytes+owned+output+sizeof(detail::WeightedPaintDomain)+sizeof(PaintSamplingDomain)+previous->refined_geometry()->working_bytes()+corners.size()*(sizeof(PaintSamplingPoint)+sizeof(uint32_t)));
  auto data=std::make_shared<detail::WeightedPaintDomain>();
  // Flatten to the actual refinement root. UV workspaces never become another
  // structural geometry/history node; the owning transition keeps its history.
  data->geometry=previous->weighted_->geometry;data->previous=previous->weighted_->previous;
  if(identity)data->layout=previous->weighted_->layout;
  else{
   auto layout=std::make_shared<detail::WeightedPaintLayout>();layout->points.reserve(corners.size());layout->geometry_corners.reserve(corners.size());
   for(auto corner:corners){b.work();layout->points.push_back(previous->weighted_->layout->points[corner]);layout->geometry_corners.push_back(previous->geometry_corners()[corner]);}
   data->layout=std::move(layout);
  }
  auto result=std::shared_ptr<const PaintSamplingDomain>(new const PaintSamplingDomain(previous->basis(),std::move(data),owned));
  result->validate_current(current,limits);b.stop();return result;
 }
 size_t previous_bytes=0;if(previous)previous_bytes=validate(*previous,before,b);
 const auto owned=previous?previous_bytes:before_bytes+corners.size()*sizeof(uint32_t);b.bytes(before_bytes+current_bytes+owned+corners.size()*sizeof(uint32_t));
 auto basis=previous?previous->basis():std::make_shared<const Mesh>(before);std::vector<uint32_t> mapping;mapping.reserve(corners.size());
 for(auto corner:corners){b.work();mapping.push_back(previous?previous->source_corners()[corner]:corner);}
 auto result=std::shared_ptr<const PaintSamplingDomain>(new const PaintSamplingDomain(std::move(basis),std::move(mapping)));
 validate(*result,current,b);b.stop();return result;
}
std::shared_ptr<const PaintSamplingDomain> retain_refined_paint_sampling(std::shared_ptr<const PaintSamplingDomain> previous,std::shared_ptr<const detail::RefinementGeometry> geometry,const PaintSamplingLimits&limits){
 WeightedScope scope(limits);Budget b(limits);if(!geometry||!geometry->valid_owner(geometry))throw std::invalid_argument("Weighted paint requires genuine owning refinement geometry");std::array<const PaintSamplingDomain*,31> ancestors{};size_t depth=0;for(auto p=previous;p&&p->version()==2;p=p->previous_domain()){if(std::find(ancestors.begin(),ancestors.begin()+depth,p.get())!=ancestors.begin()+depth)throw std::length_error("Weighted paint requires acyclic before history");if(depth>=ancestors.size())throw std::length_error("Weighted paint history exceeds its bounded depth before allocation");ancestors[depth++]=p.get();}const auto before_work=weighted_work.limit-weighted_work.used;b.work(geometry->refinement().work_units+geometry->refinement().corners.size()*8);auto geometry_limits=limits;geometry_limits.max_work=before_work;geometry->validate(geometry_limits);if(previous)previous->validate_current(*geometry->source(),limits);else mesh_bytes(*geometry->source(),b,true);
 const auto n=geometry->refinement().corners.size();const auto previous_bytes=previous?previous->storage_bytes(limits):mesh_bytes(*geometry->source(),b,true);b.bytes(previous_bytes+geometry->working_bytes()+n*(sizeof(PaintSamplingPoint)+sizeof(uint32_t)));
 const auto retained=sizeof(PaintSamplingDomain)+sizeof(detail::WeightedPaintDomain)+sizeof(detail::WeightedPaintLayout)+geometry->retained_bytes()+(previous?previous_bytes:0)+n*(sizeof(PaintSamplingPoint)+sizeof(uint32_t));if(retained>limits.max_basis_bytes)throw std::length_error("Weighted paint complete retained basis exceeds byte limit before allocation");
 auto data=std::make_shared<detail::WeightedPaintDomain>();data->geometry=std::move(geometry);data->previous=std::move(previous);auto layout=std::make_shared<detail::WeightedPaintLayout>();layout->points.reserve(n);layout->geometry_corners.resize(n);std::iota(layout->geometry_corners.begin(),layout->geometry_corners.end(),0u);const auto &r=data->geometry->refinement();
 auto basis=data->previous?data->previous->basis():r.source;
 for(const auto &c:r.corners){b.work();PaintSamplingPoint point;bool set=false;for(size_t k=0;k<3;++k){const size_t old=size_t(c.current.triangle)*3+k;PaintSamplingPoint before;
   if(data->previous&&data->previous->version()==2)before=data->previous->weighted_corners()[old];else{const auto raw=data->previous?data->previous->source_corners()[old]:uint32_t(old);before.source_corners={raw/3*3,raw/3*3+1,raw/3*3+2};before.weights[raw%3]=1;}
   const uint32_t face=before.source_corners[0]/3;if(!set){point.source_corners={face*3,face*3+1,face*3+2};set=true;}for(size_t v=0;v<3;++v){if(before.source_corners[v]/3!=point.source_corners[0]/3)throw std::invalid_argument("Weighted paint triangle crosses distinct genuine source-grid faces");point.weights[before.source_corners[v]%3]+=before.weights[v]*c.current.barycentric[k];}
  }double sum=0;for(double w:point.weights){if(!std::isfinite(w)||w< -1e-12||w>1+1e-12)throw std::invalid_argument("Weighted paint stencil is not a genuine source point");sum+=w;}if(std::abs(sum-1)>1e-12)throw std::invalid_argument("Weighted paint stencil lost affine sum");layout->points.push_back(point);}
 data->layout=std::move(layout);auto result=std::shared_ptr<const PaintSamplingDomain>(new const PaintSamplingDomain(std::move(basis),std::move(data),retained));result->storage_bytes(limits);b.stop();return result;
}
} // namespace dome
