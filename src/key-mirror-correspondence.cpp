// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-mirror-correspondence.hpp"
#include <unordered_map>
#include <cmath>
#include <limits>
namespace dome::detail {
namespace {
struct Budget {const KeyMirrorLimits &limits;size_t work=0;explicit Budget(const KeyMirrorLimits&v):limits(v){if(!v.max_bytes||v.max_bytes>256*1024*1024||!v.max_work||v.max_work>256*1024*1024)throw std::invalid_argument("Invalid key mirror resource policy");check();}void check(size_t n=1){if(n>limits.max_work-work)throw std::length_error("Key mirror correspondence exceeds bounded work");work+=n;if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Key mirror correspondence cancelled");}};
struct Cell {int64_t x,y,z;bool operator==(const Cell&)const=default;};
struct Hash {size_t operator()(Cell c)const noexcept{auto h=std::hash<int64_t>{}(c.x);h^=std::hash<int64_t>{}(c.y)+0x9e3779b9+(h<<6)+(h>>2);h^=std::hash<int64_t>{}(c.z)+0x9e3779b9+(h<<6)+(h>>2);return h;}};
Vec3 reflect(Vec3 p){return {-p.x,p.y,p.z};}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}
KeyMirrorCorrespondence reflected_key_correspondence(const Mesh &mesh,float tolerance,const KeyMirrorLimits &limits){return reflected_key_correspondence(mesh,KeyMirrorPlane{},tolerance,limits);}
KeyMirrorCorrespondence reflected_key_correspondence(const Mesh &mesh,KeyMirrorPlane plane,float tolerance,const KeyMirrorLimits &limits){
 Budget budget(limits);mesh.validate();const auto count=mesh.indices.size(),triangles=count/3,vertices=mesh.positions.size();
 if(!std::isfinite(plane.offset)||std::abs(plane.offset)>1e6||!std::isfinite(tolerance)||tolerance<=0||tolerance>1||!triangles||vertices>2'000'000||count>6'000'000)throw std::invalid_argument("Key mirror needs bounded triangles and an explicit local tolerance");
 // Conservative hash-node/bucket/vector allowance plus two complete maps.
 if(vertices>limits.max_bytes/8||count>(limits.max_bytes-vertices*8)/8||triangles>(limits.max_bytes-vertices*8-count*8)/128)throw std::length_error("Key mirror preflight exceeds its declared scratch budget");
 const double size=std::max(double(tolerance),1e-6),square=double(tolerance)*tolerance;
 auto centroid=[&](size_t t,bool mirrored){double v[3]{};for(size_t k=0;k<3;++k){const auto p=mesh.positions[mesh.indices[t*3+k]];v[0]+=p.x/3.;v[1]+=p.y/3.;v[2]+=p.z/3.;}if(mirrored)v[0]=2*plane.offset-v[0];auto q=[&](double x){x=std::floor(x/size);if(!std::isfinite(x)||std::abs(x)>=0x1p63-4096.0)throw std::length_error("Key mirror coordinates exceed bounded spatial indexing");return int64_t(x);};return Cell{q(v[0]),q(v[1]),q(v[2])};};
 std::unordered_map<Cell,std::vector<uint32_t>,Hash> bins;bins.reserve(triangles);
 for(size_t t=0;t<triangles;++t){budget.check();const auto a=mesh.positions[mesh.indices[t*3]],b=mesh.positions[mesh.indices[t*3+1]],c=mesh.positions[mesh.indices[t*3+2]];if(length(cross(b-a,c-a))<1e-12f)throw std::invalid_argument("Degenerate triangles have no authentic mirrored corner frame");bins[centroid(t,false)].push_back(uint32_t(t));}
 KeyMirrorCorrespondence out;out.tolerance=tolerance;out.corners.resize(count);out.vertices.assign(vertices,UINT32_MAX);
 for(size_t t=0;t<triangles;++t){budget.check();const auto cell=centroid(t,true);size_t matches=0;std::array<uint32_t,3> mapping{};double error=0;
  for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(int dz=-1;dz<=1;++dz){budget.check();const auto it=bins.find({cell.x+dx,cell.y+dy,cell.z+dz});if(it==bins.end())continue;for(auto source:it->second)for(size_t start=0;start<3;++start){budget.check(3);bool good=true;double largest=0;std::array<uint32_t,3> candidate{};for(size_t k=0;k<3;++k){candidate[k]=source*3+uint32_t((start+3-k)%3);const auto a=mesh.positions[mesh.indices[t*3+k]],b=mesh.positions[mesh.indices[candidate[k]]];const double x=double(a.x)+b.x-2*plane.offset,y=double(a.y)-b.y,z=double(a.z)-b.z;const auto d=x*x+y*y+z*z;good&=d<=square;largest=std::max(largest,d);}if(good){if(++matches>1)throw std::invalid_argument("Ambiguous mirrored triangles require explicit independent correspondence");mapping=candidate;error=largest;}}}
  if(matches!=1)throw std::invalid_argument("Reflected topology needs triangle refinement; no exact corner-bijection Mirror exists");out.maximum_rest_error=std::max(out.maximum_rest_error,std::sqrt(error));for(size_t k=0;k<3;++k){const size_t c=t*3+k;const auto v=mesh.indices[c],source=mesh.indices[mapping[k]];out.corners[c]=mapping[k];if(out.vertices[v]!=UINT32_MAX&&out.vertices[v]!=source)throw std::invalid_argument("Mirrored corners need an independent current vertex partition; no seam may be chosen or averaged");out.vertices[v]=source;}
 }
 for(size_t c=0;c<count;++c){budget.check();if(out.corners[out.corners[c]]!=c)throw std::invalid_argument("Key mirror corners do not form an authentic involution");}
 for(size_t v=0;v<vertices;++v){budget.check();if(out.vertices[v]==UINT32_MAX||out.vertices[out.vertices[v]]!=v)throw std::invalid_argument("Unreferenced or non-bijective vertices need explicit mirror authority");}return out;
}
std::vector<Vec3> reflected_key_vertex_deltas(const Mesh &mesh,std::span<const Vec3> delta,
    KeyMirrorPlane plane,float tolerance,const KeyMirrorLimits &limits){
 Budget budget(limits);mesh.validate();const auto count=mesh.positions.size();
 if(!count||count>2'000'000||delta.size()!=count||!std::isfinite(plane.offset)||std::abs(plane.offset)>1e6||
    !std::isfinite(tolerance)||tolerance<=0||tolerance>1)
  throw std::invalid_argument("Vertex-position Mirror needs a complete bounded displacement field and local plane/tolerance");
 // Worst-case hash nodes, buckets, per-cell vectors and output, including
 // allocator overhead. Spatial candidates are additionally work-metered.
 if(count>limits.max_bytes/160)throw std::length_error("Vertex-position Mirror exceeds prospective scratch/output budget");
 const double size=std::max(double(tolerance),1e-6),square=double(tolerance)*tolerance;
 auto cell=[&](double x,double y,double z){auto q=[&](double v){v=std::floor(v/size);if(!std::isfinite(v)||std::abs(v)>=0x1p63-4096.0)throw std::length_error("Vertex-position Mirror exceeds spatial coordinate bounds");return int64_t(v);};return Cell{q(x),q(y),q(z)};};
 std::unordered_map<Cell,std::vector<uint32_t>,Hash> bins;bins.reserve(count);
 for(size_t v=0;v<count;++v){budget.check();if(!finite(delta[v]))throw std::invalid_argument("Vertex-position Mirror requires finite source displacements");const auto p=mesh.positions[v];bins[cell(p.x,p.y,p.z)].push_back(uint32_t(v));}
 std::vector<Vec3> out;out.reserve(count);
 for(const auto p:mesh.positions){budget.check();const auto key=cell(2*plane.offset-p.x,p.y,p.z);bool found=false;Vec3 value;
  for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(int dz=-1;dz<=1;++dz){budget.check();const auto it=bins.find({key.x+dx,key.y+dy,key.z+dz});if(it==bins.end())continue;
   for(auto v:it->second){budget.check();const auto q=mesh.positions[v];const double x=double(p.x)+q.x-2*plane.offset,y=double(p.y)-q.y,z=double(p.z)-q.z;
    if(x*x+y*y+z*z>square)continue;
    const auto candidate=delta[v];
    if(found&&!same(value,candidate))throw std::invalid_argument("Coincident mirrored vertices have different shapekey displacements; select an explicit seam correspondence");
    value=candidate;found=true;
   }
  }
  if(!found)throw std::invalid_argument("Vertex-position Mirror has no reflected source within the explicit local tolerance");
  out.push_back({-value.x,value.y,value.z});
 }
 return out;
}
std::vector<int8_t> reflected_key_role_signs(const NormalFrames &frame,std::span<const std::array<float,2>> uv,const KeyMirrorCorrespondence &map,const KeyMirrorLimits &limits){
 Budget budget(limits);frame.validate();const auto count=frame.normals.size();if(map.corners.size()!=count||count%3||count>limits.max_bytes||(!frame.tangents.empty()&&uv.size()!=count))throw std::invalid_argument("Mirror role/corner-coordinate dimensions differ");
 std::vector<int8_t> signs(count,0);for(size_t c=0;c<count;++c){budget.check();if(map.corners[c]>=count||map.corners[map.corners[c]]!=c||!same(frame.normals[c],reflect(frame.normals[map.corners[c]])))throw std::invalid_argument("Mirror role requires actual base normal parity; zero key must remain zero");}
 if(frame.tangents.empty())return signs;
 for(size_t c=0;c<count;c+=3){budget.check();const auto first=map.corners[c];for(size_t k=1;k<3;++k)if(map.corners[c+k]/3!=first/3||map.corners[c+k]%3!=(first%3+3-k)%3)throw std::invalid_argument("Mirror role requires actual reversed triangle-corner correspondence");double u[3],v[3],su[3],sv[3];for(size_t k=0;k<3;++k){u[k]=uv[c+k][0];v[k]=uv[c+k][1];su[k]=uv[map.corners[c+k]][0];sv[k]=uv[map.corners[c+k]][1];if(!std::isfinite(u[k])||!std::isfinite(v[k])||!std::isfinite(su[k])||!std::isfinite(sv[k]))throw std::invalid_argument("Nonfinite mirror role coordinates");}
  const double u1=u[1]-u[0],u2=u[2]-u[0],v1=v[1]-v[0],v2=v[2]-v[0],a1=su[1]-su[0],a2=su[2]-su[0],b1=sv[1]-sv[0],b2=sv[2]-sv[0],det=u1*v2-u2*v1,a=a1*v2-a2*v1,b=a2*u1-a1*u2,d=b2*u1-b1*u2,e=b1*v2-b2*v1;
  if(!std::isfinite(det)||det==0||b!=0||e!=0||a==0||d==0||(a>0)==(d>0))throw std::invalid_argument("Mirror role needs a genuine diagonal reflected UV Jacobian; mixed tangent axes require an owning frame conversion");const int8_t sign=(a>0)==(det>0)?1:-1;
  for(size_t k=0;k<3;++k){const auto &t=frame.tangents[c+k],&s=frame.tangents[map.corners[c+k]];if(t[3]!=s[3]||!same({t[0],t[1],t[2]},reflect({s[0],s[1],s[2]})*float(sign)))throw std::invalid_argument("Mirror role must preserve its actual tangent and handedness parity");signs[c+k]=sign;}
 }return signs;
}
} // namespace dome::detail
