// SPDX-License-Identifier: GPL-3.0-or-later
#include "volume-binding.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <set>
#include <map>
#include <algorithm>
using namespace dome;
namespace {
int checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F>void rejects(F f,const char *message){bool failed=false;try{f();}catch(const std::exception&e){failed=true;std::cout<<"REFUSED "<<e.what()<<'\n';}check(failed,message);}
using Cell=std::array<int,3>;
Mesh cells(const std::set<Cell>&occupied,float h,bool seams=false) {
  Mesh out;std::map<Cell,uint32_t> vertices;
  auto vertex=[&](Cell p){
    if(!seams){auto found=vertices.find(p);if(found!=vertices.end())return found->second;}
    auto id=uint32_t(out.positions.size());out.positions.push_back({p[0]*h,p[1]*h,p[2]*h});out.normals.push_back({});out.uv.push_back({p[0]*h,p[1]*h});vertices[p]=id;return id;
  };
  for(auto c:occupied)for(int a=0;a<3;++a)for(int sign:{-1,1}){
    auto neighbor=c;neighbor[a]+=sign;if(occupied.contains(neighbor))continue;
    auto base=c;base[a]+=sign>0;int u=(a+1)%3,v=(a+2)%3;
    auto p1=base,p2=base,p3=base;++p1[u];++p2[u];++p2[v];++p3[v];
    std::array<Cell,4> points{base,p1,p2,p3};if(sign<0)std::swap(points[1],points[3]);
    std::array<uint32_t,4> ids;for(int k=0;k<4;++k)ids[k]=vertex(points[k]);
    auto normal=Vec3{};if(a==0)normal.x=sign;else if(a==1)normal.y=sign;else normal.z=sign;
    for(auto id:ids)out.normals[id]+=normal;
    out.indices.insert(out.indices.end(),{ids[0],ids[1],ids[2],ids[0],ids[2],ids[3]});
  }
  for(auto &n:out.normals)n=normalized(n);
  out.validate();return out;
}
Skeleton pair(float right,float bottom,float length) {
  Skeleton s;Bone a;a.name="Left";a.rest.translation={.01f,bottom,.01f};a.length=length;
  auto b=a;b.name="Right";b.rest.translation.x=right;s.bones={a,b};s.validate();return s;
}
float weight(const VertexWeights&w,uint32_t bone){float v=0;for(auto i:w)if(i.joint==bone)v+=i.weight;return v;}
detail::VolumeBindingOptions options(){detail::VolumeBindingOptions o;o.cell_size=.004f;o.binding.maximum_seed_distance=.1f;o.binding.seed_margin=.01f;return o;}
std::set<Cell> arms(bool joined){std::set<Cell>s;for(int z=0;z<2;++z)for(int y=0;y<20;++y)for(int x=0;x<5;++x)if(x<2||x>=3||(joined&&y<2))s.insert({x,y,z});return s;}
bool same_positions(const Mesh&a,const Mesh&b){if(a.positions.size()!=b.positions.size()||a.indices!=b.indices||a.uv!=b.uv)return false;for(size_t i=0;i<a.positions.size();++i)if(a.positions[i].x!=b.positions[i].x||a.positions[i].y!=b.positions[i].y||a.positions[i].z!=b.positions[i].z)return false;return true;}
// Independent continuous shortest path in the extruded rectilinear U. Only
// its two reflex corners can bend a shortest planar path. The Z product then
// adds its squared separation; no grid/native label data enters this oracle.
double u_distance(Vec3 p,const Bone &bone) {
  using P=std::array<double,2>;const double h=double(.01f);
  std::array<P,3> points{P{p.x,p.y},P{2*h,2*h},P{3*h,2*h}};
  auto inside=[&](P q){return q[0]>=-1e-8&&q[0]<=5*h+1e-8&&q[1]>=-1e-8&&q[1]<=20*h+1e-8&&
    (q[1]<=2*h+1e-8||q[0]<=2*h+1e-8||q[0]>=3*h-1e-8);};
  auto visible=[&](P a,P b){std::vector<double> cuts{0,1};
    for(int axis=0;axis<2;++axis)for(double boundary:{0.,2*h,3*h,5*h,20*h})if(b[axis]!=a[axis]){double t=(boundary-a[axis])/(b[axis]-a[axis]);if(t>0&&t<1)cuts.push_back(t);}
    std::sort(cuts.begin(),cuts.end());for(size_t i=1;i<cuts.size();++i){double t=(cuts[i-1]+cuts[i])*.5;if(!inside({a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])}))return false;}return true;};
  auto distance=[](P a,P b){return std::hypot(a[0]-b[0],a[1]-b[1]);};
  std::array<double,3> best{0,INFINITY,INFINITY};std::array<bool,3> done{};double planar=INFINITY;
  for(int pass=0;pass<3;++pass){size_t at=3;for(size_t i=0;i<3;++i)if(!done[i]&&(at==3||best[i]<best[at]))at=i;done[at]=true;
    P target{bone.rest.translation.x,std::clamp(points[at][1],double(bone.rest.translation.y),double(bone.rest.translation.y)+bone.length)};
    if(visible(points[at],target))planar=std::min(planar,best[at]+distance(points[at],target));
    for(size_t i=0;i<3;++i)if(!done[i]&&visible(points[at],points[i]))best[i]=std::min(best[i],best[at]+distance(points[at],points[i]));
  }
  return std::hypot(planar,double(p.z)-bone.rest.translation.z);
}
}
int main()try{
  auto source=cells(arms(false),.01f);auto before=source;auto rig=pair(.04f,.03f,.14f);auto o=options();detail::VolumeBindingReport report;
  auto bound=detail::volume_bind_native(source,rig,o,{},&report);
  float leak=0;for(size_t i=0;i<source.positions.size();++i)leak=std::max(leak,weight(bound.vertices[i],source.positions[i].x<.025f?1:0));
  std::cout<<"VOLUME grid="<<report.grid_points<<" interior="<<report.interior_points<<" components="<<report.volume_components<<" work="<<report.work<<" planned="<<report.peak_planned_bytes<<" wrong_limb="<<leak<<'\n';
  check(report.volume_components==2&&leak==0,"Nearby disconnected thin solids have exact independent internal-bone influence");
  auto old=automatic_bind(source,rig,o.binding);float old_leak=0;for(size_t i=0;i<source.positions.size();++i)old_leak=std::max(old_leak,weight(old.vertices[i],source.positions[i].x<.025f?1:0));
  std::cout<<"SURFACE wrong_limb="<<old_leak<<'\n';check(old_leak>.1f,"Existing surface seed control detects cross-gap influence on the same physical input");
  auto pose=std::vector<JointTransform>{rig.bones[0].rest,rig.bones[1].rest};pose[1].translation.x+=.25f;
  auto moved=deform_mesh(source,bound,rig,pose);double error=0;
  for(size_t i=0;i<source.positions.size();++i){auto expected=source.positions[i];if(expected.x>.025f)expected.x+=.25f;error=std::max(error,double(length(moved.positions[i]-expected)));}
  check(error<1e-6,"Independent posed-limb oracle preserves one solid while translating only the other");
  check(same_positions(source,before),"Volume binding leaves source topology, positions and UVs unchanged");
  for(auto phase:{std::array<float,3>{.1f,.2f,.3f},std::array<float,3>{.9f,.8f,.7f}}){auto next=o;next.grid_phase=phase;auto w=detail::volume_bind_native(source,rig,next);check(w.encode()==bound.encode(),"Grid-phase changes preserve exact separate-solid skin ownership");}
  auto fine=o;fine.cell_size=.0025f;check(detail::volume_bind_native(source,rig,fine).encode()==bound.encode(),"Refinement preserves independent thin-solid ownership");
  auto seam=cells(arms(false),.01f,true);auto sw=detail::volume_bind_native(seam,rig,o);bool seam_ok=true;
  for(size_t i=0;i<seam.positions.size();++i)seam_ok&=weight(sw.vertices[i],seam.positions[i].x<.025f?0:1)==1;
  check(seam_ok,"Original seam-duplicated vertices retain distinct IDs and correct volume influences");
  auto reversed=source;for(size_t c=0;c<reversed.indices.size();c+=3)std::swap(reversed.indices[c+1],reversed.indices[c+2]);
  check(detail::volume_bind_native(reversed,rig,o).encode()==bound.encode(),"Globally reversed closed orientation retains odd-even volume semantics");
  auto open=source;open.indices.resize(open.indices.size()-3);rejects([&]{detail::volume_bind_native(open,rig,o);},"Open geometry is refused without substituting surface weighting");
  auto coarse=o;coarse.cell_size=.015f;rejects([&]{detail::volume_bind_native(source,rig,coarse);},"Under-resolved thin solids are explicitly refused");
  auto budget=o;budget.binding.maximum_work=20;rejects([&]{detail::volume_bind_native(source,rig,budget);},"Construction obeys the explicit work budget");
  budget=o;budget.maximum_grid_points=20;rejects([&]{detail::volume_bind_native(source,rig,budget);},"Grid budget is enforced before allocating its cells");
  budget=o;budget.maximum_bytes=4096;rejects([&]{detail::volume_bind_native(source,rig,budget);},"Memory budget is enforced before detached mesh/container copies");
  unsigned polls=0;rejects([&]{detail::volume_bind_native(source,rig,o,[&]{return ++polls>12;});},"Actual construction cancellation returns no partial result");
  check(same_positions(source,before),"All refusals retain the complete original input geometry");
  auto nondeform=rig;nondeform.bones[1].deform=false;rejects([&]{detail::volume_bind_native(source,nondeform,o);},"A disconnected component cannot borrow a nondeforming bone seed");
  auto u=cells(arms(true),.01f);auto uw=detail::volume_bind_native(u,rig,o);float tip_leak=0;double reference_leak=0,field_error=0;
  double scale=std::max(double(rig.bones[0].length),double(rig.bones[1].length))*.02;
  for(size_t i=0;i<u.positions.size();++i){auto p=u.positions[i];double a=u_distance(p,rig.bones[0]),b=u_distance(p,rig.bones[1]);double w0=1/(1+std::pow((a+scale)/(b+scale),o.binding.falloff));field_error=std::max(field_error,std::abs(weight(uw.vertices[i],0)-w0));if(p.y>.15f){tip_leak=std::max(tip_leak,weight(uw.vertices[i],p.x<.025f?1:0));reference_leak=std::max(reference_leak,p.x<.025f?1-w0:w0);}}
  std::cout<<"U_TIP wrong_limb="<<tip_leak<<" continuous_reference="<<reference_leak<<" max_field_error="<<field_error<<'\n';
  check(reference_leak<.03,"Independent continuous volume weighting meets the unchanged U-tip intent");
  check(tip_leak<.03f,"Connected U limbs propagate through the material base instead of crossing the air gap");
  check(field_error<.01,"Discrete weights remain within one percentage point of the independent continuous U field");
  std::cout<<"PASS "<<checks<<" private volume binding checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
