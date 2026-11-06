// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-transfer.hpp"
#include <nlohmann/json.hpp>
#include <map>
#include <bit>
#include <set>
namespace dome::detail {namespace {
void stop(const PolygonSurfaceTransferLimits &l){if(l.geometry.cancelled&&l.geometry.cancelled())throw std::runtime_error("Polygon surface transfer cancelled");}
void policy(const PolygonSurfaceTransferLimits &l){stop(l);if(!l.maximum_steps||l.maximum_steps>128||!l.maximum_entries||l.maximum_entries>8'000'000||!l.maximum_working_bytes||l.maximum_working_bytes>512*1024*1024)throw std::invalid_argument("Invalid polygon surface transfer resource policy");}
size_t mesh_bytes(const PolygonMesh &m){size_t bytes=sizeof(PolygonMesh)+m.vertices().size()*sizeof(PolygonVertex)+m.edges().size()*sizeof(PolygonEdge)+m.faces().size()*sizeof(PolygonFace);for(const auto &v:m.vertices())bytes+=v.provenance.size()*sizeof(v.provenance[0]);for(const auto &f:m.faces())bytes+=f.corners.size()*sizeof(PolygonCorner);return bytes;}
size_t step_bytes(const PolygonSurfaceStep &s){return sizeof(s)+s.edit.selection.ids.size()*sizeof(PolygonId);}
PolygonMesh perform(const PolygonMesh &source,const PolygonSurfaceStep &s,const PolygonSurfaceTransferLimits &l){
 stop(l);if(s.edit.operation!=PolygonOperation::Move&&s.edit.operation!=PolygonOperation::FlipFaces)throw std::invalid_argument("Polygon topology transfer requires its further exact corner/refinement adapter");
 // The native affine Move is already supplied in local coordinates by its
 // exact ray/drag adapter. Flip changes ordered corners, not physical lengths.
 // Store the declared world for the later field evaluator; do not fit a TRS.
 if(s.world){(void)s.world->inverse();(void)s.world->orientation();}
 auto result=edit_polygon_with_corner_sources(source,s.edit,l.geometry);
 if(!result.geometry.replacement)return source;
 if(!result.corner_sources.empty())throw std::logic_error("Move/Flip unexpectedly introduced a new corner field");
 return std::move(*result.geometry.replacement);
}
using D=normal_bake_math::D;
D subtract(D a,D b){for(size_t k=0;k<3;++k)a[k]-=b[k];return a;}
D scale(D a,double x){for(auto &v:a)v*=x;return a;}
D inverse_apply(const NormalBakeBasis &m,D v,double minimum){
 using namespace normal_bake_math;finite_basis(m);const auto t=vec(m.t),b=vec(m.b),n=vec(m.n);const auto determinant=dot(t,cross(b,n));const auto sizes=std::sqrt(dot(t,t)*dot(b,b)*dot(n,n));
 if(!std::isfinite(minimum)||minimum<=0||minimum>1||!std::isfinite(determinant)||!std::isfinite(sizes)||!sizes||std::abs(determinant)/sizes<minimum)throw std::invalid_argument("Polygon normal transport basis is singular or ill-conditioned");
 return {dot(v,cross(b,n))/determinant,dot(t,cross(v,n))/determinant,dot(t,cross(b,v))/determinant};
}
void handedness(float w){if(!std::isfinite(w)||(w!=1&&w!=-1))throw std::invalid_argument("Polygon normal transport requires actual tangent handedness");}
void frame(const PolygonGeometryFrame &f,double minimum){handedness(f.handedness);(void)inverse_apply(f.basis,{0,0,1},minimum);using namespace normal_bake_math;const auto d=dot(vec(f.basis.t),cross(vec(f.basis.b),vec(f.basis.n)));if((d<0?-1:1)!=f.handedness)throw std::invalid_argument("Polygon geometric frame and its actual W disagree");}
}
size_t PolygonSurfaceTrace::storage_bytes() const{size_t n=sizeof(*this);if(original_)n+=mesh_bytes(*original_);if(current_&&current_!=original_)n+=mesh_bytes(*current_);for(const auto &s:steps_)n+=step_bytes(s);return n;}
PolygonSurfaceTrace PolygonSurfaceTrace::begin(const PolygonMesh &m,const PolygonSurfaceTransferLimits &l){policy(l);m.validate(l.geometry);if(mesh_bytes(m)>l.maximum_working_bytes/2)throw std::length_error("Polygon surface capture exceeds working quota");PolygonSurfaceTrace t;t.original_=std::make_shared<const PolygonMesh>(m);t.current_=t.original_;return t;}
std::optional<PolygonSurfaceTrace> PolygonSurfaceTrace::advance(const PolygonSurfaceStep &s,const PolygonSurfaceTransferLimits &l) const{
 policy(l);if(!original_||!current_)throw std::invalid_argument("Polygon surface transfer lost its immutable geometry");
 // Move/Flip do not expand topology. Charge the source, candidate copy and
 // future edit metadata before the kernel can allocate its replacement.
 const auto retained=storage_bytes(),mesh=mesh_bytes(*current_),extra=step_bytes(s);if(retained>l.maximum_working_bytes||extra>l.maximum_working_bytes-retained||mesh>(l.maximum_working_bytes-retained-extra)/2)throw std::length_error("Polygon surface edit exceeds aggregate working quota");
 validate(l);auto next=perform(*current_,s,l);if(next.encode()==current_->encode())return {};if(steps_.size()>=l.maximum_steps)throw std::length_error("Polygon surface edit program exceeds step quota");
 PolygonSurfaceTrace out=*this;out.steps_.push_back(s);out.current_=std::make_shared<const PolygonMesh>(std::move(next));return out;
}
void PolygonSurfaceTrace::validate(const PolygonSurfaceTransferLimits &l) const{
 policy(l);if(!original_||!current_||steps_.size()>l.maximum_steps||storage_bytes()>l.maximum_working_bytes)throw std::invalid_argument("Incomplete polygon surface edit program");
 const auto retained=storage_bytes(),scratch=mesh_bytes(*original_);if(retained>l.maximum_working_bytes||scratch>(l.maximum_working_bytes-retained)/8)throw std::length_error("Polygon surface replay exceeds prospective working quota");size_t entries=0;for(const auto &s:steps_){if(s.edit.selection.ids.size()>l.maximum_entries-entries)throw std::length_error("Polygon surface edit metadata exceeds entry quota");entries+=s.edit.selection.ids.size();}auto replay=*original_;size_t work=0;for(const auto &s:steps_){stop(l);const auto entries=replay.vertices().size()+replay.edges().size()+replay.faces().size()+s.edit.selection.ids.size();if(entries>l.geometry.maximum_work-work)throw std::length_error("Polygon surface replay exceeds aggregate work quota");work+=entries;replay=perform(replay,s,l);}if(replay.encode()!=current_->encode())throw std::invalid_argument("Polygon current geometry differs from exact edit replay");
}
std::vector<PolygonSurfaceTriangle> PolygonSurfaceTrace::triangle_permutation(const PolygonSurfaceTransferLimits &l) const{
 validate(l);auto corners=[](const PolygonMesh&m){size_t n=0;for(const auto&f:m.faces())n+=f.corners.size();return n;};const auto old_corners=corners(*original_),new_corners=corners(*current_);if(new_corners>l.maximum_entries)throw std::length_error("Polygon triangle source map exceeds entry quota");
 // Two complete render derivatives, tree nodes and the candidate permutation
 // must fit BEFORE allocating either triangulation or lookup container.
 const auto retained=storage_bytes();const size_t unit=2*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+2*sizeof(PolygonId)+3*sizeof(uint32_t)+sizeof(PolygonId))+128+sizeof(PolygonSurfaceTriangle);
 if(retained>l.maximum_working_bytes||old_corners>l.maximum_working_bytes/unit||new_corners>(l.maximum_working_bytes-retained)/unit)throw std::length_error("Polygon triangle source map exceeds prospective working quota");
 const auto before=original_->triangulate(l.geometry),after=current_->triangulate(l.geometry);const auto count=after.mesh.indices.size()/3;std::vector<PolygonSurfaceTriangle> result;result.reserve(count);
 using Key=std::pair<PolygonId,std::array<PolygonId,3>>;std::map<Key,uint32_t> source;size_t work=0;
 auto charge=[&]{stop(l);const size_t cost=std::bit_width(before.mesh.indices.size()/3)+8;if(cost>l.geometry.maximum_work-work)throw std::length_error("Polygon triangle source lookup exceeds work quota");work+=cost;};
 auto key=[](const PolygonRender&r,size_t t){std::array<PolygonId,3> ids;for(size_t k=0;k<3;++k)ids[k]=r.corners.at(r.mesh.indices[t*3+k]);std::sort(ids.begin(),ids.end());return Key{r.triangle_faces.at(t),ids};};
 for(size_t t=0;t<before.mesh.indices.size()/3;++t){charge();if(!source.emplace(key(before,t),uint32_t(t)).second)throw std::invalid_argument("Polygon triangle field ancestry is ambiguous");}
 for(size_t t=0;t<count;++t){charge();const auto found=source.find(key(after,t));if(found==source.end())throw std::invalid_argument("Polygon changed triangulation requires actual field common refinement");PolygonSurfaceTriangle row;row.source_triangle=found->second;for(size_t c=0;c<3;++c){const auto id=after.corners.at(after.mesh.indices[t*3+c]);for(size_t k=0;k<3;++k)if(before.corners.at(before.mesh.indices[found->second*3+k])==id)row.source_barycentrics[c][k]=1;}result.push_back(row);}return result;
}

PolygonGeometryFrame polygon_geometry_frame(const std::array<Vec3,3> &p,const std::array<std::array<float,2>,3> &uv,const AffineTransform &world,double minimum){
 using namespace normal_bake_math;for(auto q:p)if(!finite(q))throw std::invalid_argument("Nonfinite polygon surface geometry");for(auto q:uv)for(auto x:q)if(!std::isfinite(x))throw std::invalid_argument("Nonfinite polygon role coordinate");
 const auto e1=subtract(vec(p[1]),vec(p[0])),e2=subtract(vec(p[2]),vec(p[0]));const double u1=double(uv[1][0])-uv[0][0],v1=double(uv[1][1])-uv[0][1],u2=double(uv[2][0])-uv[0][0],v2=double(uv[2][1])-uv[0][1],det=u1*v2-u2*v1,uvscale=std::hypot(u1,v1)*std::hypot(u2,v2);
 if(!std::isfinite(det)||!uvscale||std::abs(det)<=minimum*uvscale)throw std::invalid_argument("Polygon normal role coordinates are singular or ill-conditioned");
 const auto local_n=unit(cross(e1,e2)),local_t=scale(subtract(scale(e1,v2),scale(e2,v1)),1/det),local_b=scale(subtract(scale(e2,u1),scale(e1,u2)),1/det);
 const auto n=unit(world.normal(local_n)),raw_t=world.direction(local_t),raw_b=world.direction(local_b);const auto t=unit(subtract(raw_t,scale(n,dot(n,raw_t))));const auto cross_nt=unit(cross(n,t));const double sign=dot(cross_nt,raw_b);if(!std::isfinite(sign)||std::abs(sign)<=minimum*std::sqrt(dot(raw_b,raw_b)))throw std::invalid_argument("Polygon geometric tangent handedness is unrepresentable");
 const float w=sign<0?-1.f:1.f;PolygonGeometryFrame result{{vec(t),vec(scale(cross_nt,w)),vec(n)},w};frame(result,minimum);return result;
}
Vec3 transport_polygon_direction(Vec3 direction,const PolygonGeometryFrame&before,const PolygonGeometryFrame&current,double minimum){
 using namespace normal_bake_math;frame(before,minimum);frame(current,minimum);const auto detail=inverse_apply(before.basis,unit(vec(direction)),minimum);return vec(unit(apply(current.basis,detail)));
}
Vec3 transport_polygon_normal(const PolygonBeforeRole &role,Vec3 sample,const PolygonGeometryFrame &before,const PolygonGeometryFrame &current,double minimum){
 using namespace normal_bake_math;handedness(role.tangent_handedness);if(role.basis_orientation!=1&&role.basis_orientation!=-1)throw std::invalid_argument("Missing actual before shader basis orientation");if(!finite(sample))throw std::invalid_argument("Nonfinite polygon normal sample");frame(before,minimum);frame(current,minimum);(void)inverse_apply(role.before_vs,{0,0,1},minimum);const auto determinant=dot(vec(role.before_vs.t),cross(vec(role.before_vs.b),vec(role.before_vs.n)));if((determinant<0?-1:1)!=role.basis_orientation*role.tangent_handedness)throw std::invalid_argument("Before shader basis differs from its actual tangent W/orientation policy");
 const auto desired_before=unit(apply(role.before_vs,vec(sample)));return transport_polygon_direction(vec(desired_before),before,current,minimum);
}
} // namespace dome::detail
