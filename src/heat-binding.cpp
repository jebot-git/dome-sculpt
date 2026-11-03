// SPDX-License-Identifier: GPL-3.0-or-later
// Native cotangent FEM + mass-lumped visibility heat; independent per-bone PCG.
// Informed by pinned Blender meshlaplacian.cc, not a copied/runtime Blender solve.
#include "heat-binding.hpp"
#include "dome/mesh-processing.hpp"
#include <bit>
#include <map>
#include <numeric>
namespace dome::detail {
namespace {
using D=std::array<double,3>;
D point(Vec3 p){return {p.x,p.y,p.z};}
D sub(D a,D b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
double dot(D a,D b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
D cross(D a,D b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double norm(D a){return std::sqrt(dot(a,a));}
Vec3 vec(D a){return {float(a[0]),float(a[1]),float(a[2])};}
struct Budget {
 size_t used=0,maximum;const std::function<bool()>&cancel;
 void step(size_t n=1){if(n>maximum-used)throw std::length_error("Bone heat work budget exceeded");used+=n;if((used&1023)<n)stop();}
 void stop()const{if(cancel&&cancel())throw std::runtime_error("Bone heat cancelled");}
};
struct System {
 std::vector<size_t> starts;
 std::vector<uint32_t> columns,parent;
 std::vector<double> values,diagonal,mass,heat,ties;
 std::vector<D> normals;
 std::vector<std::array<uint64_t,4>> seeds;
 size_t components=0;
 void multiply(std::span<const double>x,std::span<double>out,Budget&budget)const{
  budget.step(diagonal.size()+columns.size());
  for(size_t i=0;i<diagonal.size();++i){double y=diagonal[i]*x[i];for(size_t j=starts[i];j<starts[i+1];++j)y+=values[j]*x[columns[j]];out[i]=y;}
 }
};
System assemble(const Mesh&m,Budget&budget){
 const size_t n=m.positions.size();System s;s.diagonal.assign(n,0);s.mass.assign(n,0);s.normals.resize(n);s.heat.assign(n,0);s.ties.assign(n,0);s.seeds.resize(n);s.parent.resize(n);std::iota(s.parent.begin(),s.parent.end(),0);
 auto root=[&](uint32_t x){while(x!=s.parent[x]){budget.step();s.parent[x]=s.parent[s.parent[x]];x=s.parent[x];}return x;};
 std::map<std::array<uint32_t,2>,double> edges;
 for(size_t t=0;t<m.indices.size();t+=3){budget.step();std::array<uint32_t,3> ids{m.indices[t],m.indices[t+1],m.indices[t+2]};std::array<D,3> p{point(m.positions[ids[0]]),point(m.positions[ids[1]]),point(m.positions[ids[2]])};auto normal=cross(sub(p[1],p[0]),sub(p[2],p[0]));double area2=norm(normal);
  if(!(area2>0)||!std::isfinite(area2))throw std::invalid_argument("Bone heat requires finite nondegenerate triangles");
  for(int k=0;k<3;++k){budget.step();auto id=ids[k];s.mass[id]+=area2/6;for(int a=0;a<3;++a)s.normals[id][a]+=normal[a];auto a=ids[(k+1)%3],b=ids[(k+2)%3];if(a==b)throw std::invalid_argument("Bone heat triangle repeats a vertex");s.parent[root(a)]=root(b);double weight=.5*dot(sub(p[(k+1)%3],p[k]),sub(p[(k+2)%3],p[k]))/area2;if(!std::isfinite(weight))throw std::runtime_error("Bone heat stiffness overflow");edges[{std::min(a,b),std::max(a,b)}]+=weight;}
 }
 s.starts.assign(n+1,0);
 for(auto [edge,w]:edges){budget.step();if(!std::isfinite(w))throw std::runtime_error("Bone heat edge stiffness overflow");++s.starts[edge[0]+1];++s.starts[edge[1]+1];s.diagonal[edge[0]]+=w;s.diagonal[edge[1]]+=w;}
 for(size_t i=0;i<n;++i){budget.step();s.parent[i]=root(uint32_t(i));if(s.parent[i]==i)++s.components;if(!(s.mass[i]>0))throw std::invalid_argument("Bone heat has an unreferenced or zero-area vertex");double length=norm(s.normals[i]);if(length)for(auto&x:s.normals[i])x/=length;s.starts[i+1]+=s.starts[i];}
 s.columns.resize(s.starts.back());s.values.resize(s.starts.back());auto cursor=s.starts;
 for(auto [edge,w]:edges){budget.step();auto a=cursor[edge[0]]++,b=cursor[edge[1]]++;s.columns[a]=edge[1];s.values[a]=-w;s.columns[b]=edge[0];s.values[b]=-w;}
 return s;
}
struct Segment {D a,b;uint32_t bone;};
D closest(D p,const Segment&s){auto d=sub(s.b,s.a);double length=dot(d,d),t=length?std::clamp(dot(sub(p,s.a),d)/length,0.,1.):0;D q=s.a;for(int k=0;k<3;++k)q[k]+=t*d[k];return q;}
void seed(System&s,const Mesh&m,const Skeleton&rig,const HeatBindingOptions&o,Budget&budget){
 auto world=rig.world();std::vector<Segment> segments;segments.reserve(rig.bones.size());
 for(uint32_t i=0;i<rig.bones.size();++i)if(rig.bones[i].deform){budget.step();const auto&w=world[i];D a{w[12],w[13],w[14]},b=a;for(int k=0;k<3;++k)b[k]+=double(w[4+k])*rig.bones[i].length;segments.push_back({a,b,i});}
 if(segments.empty())throw std::invalid_argument("Bone heat needs a deforming source bone");
 SurfaceQuery query(m,[&]{budget.step();return false;});SurfaceQueryOptions qo;qo.cancelled=[&]{budget.step();return false;};qo.triangle_filter=[&](uint32_t){budget.step();return true;};
 struct Candidate {double distance,physical;D target;uint32_t bone;};std::vector<Candidate> candidates;candidates.reserve(segments.size());std::vector<uint8_t> anchored(m.positions.size());
 for(size_t i=0;i<m.positions.size();++i){budget.step();auto p=point(m.positions[i]);candidates.clear();for(auto&source:segments){budget.step();auto q=closest(p,source),d=sub(p,q);double length=norm(d);if(length>o.maximum_seed_distance)continue;double cosine=length?dot(d,s.normals[i])/length:1;double distance=length/(.5*(std::clamp(cosine,-1.,1.)+1.001));candidates.push_back({distance,length,q,source.bone});}
  std::sort(candidates.begin(),candidates.end(),[&](const Candidate&a,const Candidate&b){budget.step();return a.distance!=b.distance?a.distance<b.distance:a.bone<b.bone;});double nearest=INFINITY;size_t ties=0;
  for(const auto&c:candidates){budget.step();if(c.distance>nearest*(1+1e-6))break;auto d=sub(c.target,p);bool visible=true;if(c.physical>0){double trim=std::min(c.physical*1e-5,1e-5);D start=p;for(int k=0;k<3;++k)start[k]+=trim*d[k]/c.physical;visible=!query.raycast(vec(start),vec(d),float(std::max(0.,c.physical-2*trim)),qo);}if(!visible)continue;nearest=std::min(nearest,c.distance);s.seeds[i][c.bone/64]|=uint64_t(1)<<(c.bone%64);++ties;}
  if(ties){double distance=std::max(nearest,1e-4);s.heat[i]=s.mass[i]*o.heat_strength*ties/(distance*distance);s.ties[i]=double(ties);if(!std::isfinite(s.heat[i])||!(s.heat[i]>0))throw std::runtime_error("Bone heat source strength overflow");s.diagonal[i]+=s.heat[i];anchored[s.parent[i]]=1;}
  if(!(s.diagonal[i]>0)||!std::isfinite(s.diagonal[i]))throw std::runtime_error("Bone heat has a nonpositive finite-element diagonal");
 }
 for(size_t i=0;i<s.parent.size();++i){budget.step();if(s.parent[i]==i&&!anchored[i])throw std::runtime_error("Bone heat has a disconnected component without a visible deform-bone seed");}
}
struct Solve {size_t iterations=0;double residual=0,minimum=0;};
Solve solve(const System&s,std::span<const double>rhs,std::span<double>x,const HeatBindingOptions&o,Budget&budget){
 const size_t n=x.size();std::vector<double>r(rhs.begin(),rhs.end()),z(n),p(n),ap(n);std::fill(x.begin(),x.end(),0);double scale=0,rho=0;budget.step(n*3);
 for(size_t i=0;i<n;++i){scale+=rhs[i]*rhs[i];z[i]=r[i]/s.diagonal[i];p[i]=z[i];rho+=r[i]*z[i];}if(!scale)return {};scale=std::sqrt(scale);Solve result;
 for(uint32_t iteration=0;iteration<o.maximum_iterations;++iteration){budget.stop();s.multiply(p,ap,budget);double curvature=0;budget.step(n);for(size_t i=0;i<n;++i)curvature+=p[i]*ap[i];if(!(curvature>0)||!std::isfinite(curvature)||!std::isfinite(rho))throw std::runtime_error("Bone heat PCG lost positive curvature");double alpha=rho/curvature,residual=0;budget.step(n);for(size_t i=0;i<n;++i){x[i]+=alpha*p[i];r[i]-=alpha*ap[i];residual+=r[i]*r[i];}result.iterations=iteration+1;
  if(std::sqrt(residual)<=o.relative_tolerance*scale||iteration+1==o.maximum_iterations){s.multiply(x,ap,budget);residual=0;budget.step(n);for(size_t i=0;i<n;++i){r[i]=rhs[i]-ap[i];residual+=r[i]*r[i];}result.residual=std::sqrt(residual)/scale;if(result.residual<=o.relative_tolerance){for(double v:x){budget.step();if(!std::isfinite(v))throw std::runtime_error("Bone heat produced a nonfinite field");result.minimum=std::min(result.minimum,v);}return result;}if(iteration+1==o.maximum_iterations)break;budget.step(n);rho=0;for(size_t i=0;i<n;++i){z[i]=r[i]/s.diagonal[i];p[i]=z[i];rho+=r[i]*z[i];}continue;}
  double next=0;budget.step(n);for(size_t i=0;i<n;++i){z[i]=r[i]/s.diagonal[i];next+=r[i]*z[i];}double beta=next/rho;budget.step(n);for(size_t i=0;i<n;++i)p[i]=z[i]+beta*p[i];rho=next;
 }
 throw std::runtime_error("Bone heat PCG did not meet its measured residual bound");
}
}
SkinWeights heat_bind_surface(const Mesh&m,const Skeleton&rig,const HeatBindingOptions&o,const std::function<bool()>&cancelled,HeatBindingReport*report){
 Budget budget{0,o.maximum_work,cancelled};budget.stop();const size_t n=m.positions.size(),nt=m.indices.size()/3;
 if(!o.maximum_work||o.maximum_work>500'000'000||o.maximum_bytes<4096||o.maximum_bytes>512u*1024u*1024u||!o.maximum_iterations||o.maximum_iterations>4096||!std::isfinite(o.maximum_seed_distance)||o.maximum_seed_distance<=0||o.maximum_seed_distance>10000||!std::isfinite(o.heat_strength)||o.heat_strength<=0||o.heat_strength>100||!std::isfinite(o.relative_tolerance)||o.relative_tolerance<1e-12||o.relative_tolerance>1e-5||!n||n>2'000'000||!nt||nt>2'000'000||rig.bones.empty()||rig.bones.size()>256)throw std::invalid_argument("Invalid bone heat input/resource bounds");
 size_t planned=0;auto bytes=[&](size_t count,size_t stride){if(count>(o.maximum_bytes-planned)/stride)throw std::length_error("Bone heat memory budget exceeded before construction");planned+=count*stride;};bytes(n,384+sizeof(VertexWeights)+size_t(o.limits.max_influences)*sizeof(Influence));bytes(nt,512);bytes(rig.bones.size()*rig.bones.size(),64);budget.step(n+m.indices.size()+rig.bones.size()*rig.bones.size());budget.step(m.indices.size()*(4*std::bit_width(m.indices.size()+1)+1));m.validate();rig.validate();normalize_weights({{0,1}},rig.bones.size(),{},o.limits);
 auto s=assemble(m,budget);seed(s,m,rig,o,budget);HeatBindingReport proof;proof.vertices=n;proof.components=s.components;proof.planned_bytes=planned;std::vector<double>rhs(n),field(n),sum(n);SkinWeights out;out.vertices.resize(n);for(auto&row:out.vertices)row.reserve(o.limits.max_influences+1);
 for(uint32_t bone=0;bone<rig.bones.size();++bone)if(rig.bones[bone].deform){budget.step(n);bool active=false;for(size_t i=0;i<n;++i){bool selected=s.seeds[i][bone/64]&(uint64_t(1)<<(bone%64));rhs[i]=selected?s.heat[i]/s.ties[i]:0;active|=selected;}if(!active)continue;++proof.seeded_bones;auto solved=solve(s,rhs,field,o,budget);proof.iterations+=solved.iterations;proof.maximum_relative_residual=std::max(proof.maximum_relative_residual,solved.residual);proof.minimum_unclamped_weight=std::min(proof.minimum_unclamped_weight,solved.minimum);if(solved.minimum<-1e-7)throw std::runtime_error("Bone heat FEM field violates its independently checked positivity bound");
  for(size_t i=0;i<n;++i){budget.step();double value=std::max(0.,field[i]);sum[i]+=value;if(value<=0)continue;auto&row=out.vertices[i];row.push_back({bone,float(value)});std::sort(row.begin(),row.end(),[](const Influence&a,const Influence&b){return a.weight!=b.weight?a.weight>b.weight:a.joint<b.joint;});if(row.size()>o.limits.max_influences)row.pop_back();}
 }
 for(size_t i=0;i<n;++i){budget.step();proof.maximum_partition_error=std::max(proof.maximum_partition_error,std::abs(sum[i]-1));out.vertices[i]=normalize_weights(std::move(out.vertices[i]),rig.bones.size(),{},o.limits);}
 out.validate(n,rig.bones.size());budget.stop();proof.work=budget.used;if(report)*report=proof;return out;
}
}
