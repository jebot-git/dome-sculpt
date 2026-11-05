// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-shadow-query.hpp"
#include "weight-surface-system.hpp"
#include "dome/rig.hpp"
#include <numeric>
namespace dome::detail {
struct VisibilityGeodesicResult {
  SkinWeights weights;
  size_t work=0, workspace=0, visible_seeds=0, occluded_candidates=0;
};
namespace visibility_geodesic {
using Budget=weight_surface::Budget;
using Point=bone_shadow::Point;
struct Bytes {
  size_t used=0,maximum;
  void add(size_t count,size_t unit=1) {
    if(unit&&count>(maximum-used)/unit)
      throw std::length_error("Visibility geodesic workspace forecast exceeded");
    used+=count*unit;
  }
};
// One live entry per vertex. Positive edges never reopen finalized rows.
class Heap {
  std::vector<uint32_t> heap_,position_;
  const std::vector<float>&distance_;
  Budget&work_;
  bool before(uint32_t a,uint32_t b) {
    work_.step();return distance_[a]<distance_[b] ||
      (distance_[a]==distance_[b]&&a<b);
  }
  void swap(size_t a,size_t b) {
    work_.step(4);std::swap(heap_[a],heap_[b]);
    position_[heap_[a]]=uint32_t(a);position_[heap_[b]]=uint32_t(b);
  }
public:
  Heap(size_t count,const std::vector<float>&distance,Budget&work)
    :position_(count,UINT32_MAX),distance_(distance),work_(work) {heap_.reserve(count);}
  bool empty()const{return heap_.empty();}
  void reset(){work_.step(position_.size());heap_.clear();std::fill(position_.begin(),position_.end(),UINT32_MAX);}
  void decrease(uint32_t vertex) {
    work_.step();size_t at=position_[vertex];
    if(at==UINT32_MAX){at=heap_.size();heap_.push_back(vertex);position_[vertex]=uint32_t(at);}
    while(at){size_t parent=(at-1)/2;if(!before(heap_[at],heap_[parent]))break;swap(at,parent);at=parent;}
  }
  uint32_t pop(){
    work_.step();auto result=heap_.front();heap_.front()=heap_.back();
    position_[heap_.front()]=0;heap_.pop_back();position_[result]=UINT32_MAX;
    size_t at=0;
    while(at<heap_.size()){
      size_t left=at*2+1;if(left>=heap_.size())break;size_t child=left;
      if(left+1<heap_.size()&&before(heap_[left+1],heap_[left]))child=left+1;
      if(!before(heap_[child],heap_[at]))break;swap(at,child);at=child;
    }
    return result;
  }
};
inline float distance(Point p,Point a,Point b) {
  std::array<double,3>r,d;double rr=0,rd=0,dd=0;
  for(size_t k=0;k<3;++k){r[k]=double(p[k])-a[k];d[k]=double(b[k])-a[k];dd+=d[k]*d[k];rd+=r[k]*d[k];}
  const double t=dd?std::clamp(rd/dd,0.,1.):0;
  for(size_t k=0;k<3;++k){double e=r[k]-t*d[k];rr+=e*e;}
  return float(std::sqrt(rr));
}
// The closest parameter is a genuine exact rational of captured binary32
// endpoints. Never cast a projected point to float or shift its ray origin.
inline bool closest_visible(const BoneShadowQuery&q,uint32_t row,Point p,
                            Point a,Point b,BoneShadowQuery::Scratch&scratch,
                            Budget&work,BoneQueryReport&report) {
  q.integrate(row,a,b,1.,0.,1.,scratch,work,report,
              BoneWallDomain::WholeGroup,true,true,BoneOriginPolicy::StrictOwnership);
  volume_exact::Arithmetic exact(work);
  volume_exact::Integer numerator,denominator;
  for(size_t k=0;k<3;++k){
    auto delta=exact.subtract(exact.scalar(b[k]),exact.scalar(a[k]));
    auto offset=exact.subtract(exact.scalar(p[k]),exact.scalar(a[k]));
    numerator=exact.add(numerator,exact.multiply(offset,delta));
    denominator=exact.add(denominator,exact.multiply(delta,delta));
  }
  auto t=exact.zero();
  if(numerator.sign>0&&denominator.sign)
    t=exact.subtract(numerator,denominator).sign>=0?exact.one():exact.fraction(numerator,denominator);
  for(const auto&interval:scratch.shadows){
    work.step();
    // Closed shadow boundaries conservatively include edge/grazing contact.
    if(exact.compare(t,interval.enter)>=0&&exact.compare(t,interval.exit)<=0)return false;
  }
  return true;
}
} // namespace visibility_geodesic
// Private candidate, not a public method or an alteration of Automatic0.
// Uses actual shared vertex topology only. Source-authenticated seam transport
// remains a separate prerequisite; no coordinate-only topology weld is made.
inline VisibilityGeodesicResult visibility_geodesic_bind(
    const Mesh&m,const Skeleton&rig,BindOptions options={},
    const std::function<bool()>&cancelled={},size_t maximum_bytes=256u*1024u*1024u) {
  using namespace visibility_geodesic;
  const size_t n=m.positions.size(),corners=m.indices.size(),bones=rig.bones.size();
  if(!n||n>2'000'000||!corners||corners>6'000'000||corners%3||!bones||bones>256||
     !options.maximum_work||options.maximum_work>500'000'000||maximum_bytes<65536||
     maximum_bytes>256u*1024u*1024u||!std::isfinite(options.maximum_seed_distance)||
     options.maximum_seed_distance<=0||options.maximum_seed_distance>10000||
     !std::isfinite(options.seed_margin)||options.seed_margin<0||options.seed_margin>options.maximum_seed_distance||
     !std::isfinite(options.falloff)||options.falloff<1||options.falloff>8||
     !options.limits.max_influences||options.limits.max_influences>32||
     !std::isfinite(options.limits.prune)||options.limits.prune<0||options.limits.prune>=1)
    throw std::invalid_argument("Invalid bounded visibility geodesic domain/options");
  Budget work{0,options.maximum_work,cancelled};work.stop();
  // Validation and every owned construction are charged before that phase.
  work.step_items(n,4);work.step_items(corners,1);work.step_items(bones,256);
  m.validate();rig.validate();
  Bytes bytes{0,maximum_bytes};bytes.add(65536);
  bytes.add(n,sizeof(Point)+3*sizeof(size_t)+7*sizeof(uint32_t)+3*sizeof(float)+2*sizeof(uint8_t));
  bytes.add(n,sizeof(VertexWeights)+options.limits.max_influences*sizeof(Influence)+64);
  bytes.add(corners,2*(sizeof(uint32_t)+sizeof(float)));
  bytes.add(corners,2*sizeof(uint32_t)); // explicit two-sided wall overlay
  bytes.add(bones,2*sizeof(Point)+4*sizeof(RigMatrix)+512);
  std::vector<Point>points;points.reserve(n);
  for(auto p:m.positions){work.step(3);if(std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>10000)throw std::invalid_argument("Visibility mesh exceeds metric workspace");points.push_back({p.x,p.y,p.z});}
  // Both orientations implement two-sided intersection without altering the
  // caller mesh or the existing entry-wall query semantics.
  std::vector<uint32_t>two_sided;two_sided.reserve(corners*2);
  for(size_t t=0;t<corners;t+=3){work.step(6);auto a=m.indices[t],b=m.indices[t+1],c=m.indices[t+2];two_sided.insert(two_sided.end(),{a,b,c,a,c,b});}
  BoneShadowQuery query(points,two_sided,maximum_bytes-bytes.used,work);
  bytes.add(query.planned_bytes);BoneShadowQuery::Scratch scratch(query);BoneQueryReport query_report;
  std::vector<size_t>starts(n+1),cursor(n);
  std::vector<uint32_t>parent(n);std::iota(parent.begin(),parent.end(),0);
  auto root=[&](uint32_t a){while(parent[a]!=a){work.step();parent[a]=parent[parent[a]];a=parent[a];}return a;};
  for(size_t t=0;t<corners;t+=3)for(size_t k=0;k<3;++k){
    work.step(8);auto a=m.indices[t+k],b=m.indices[t+(k+1)%3];++starts[a+1];++starts[b+1];
    auto x=root(a),y=root(b);if(x!=y)parent[std::max(x,y)]=std::min(x,y);
  }
  for(size_t i=0;i<n;++i){work.step(3);if(!starts[i+1])throw std::invalid_argument("Visibility binding needs incident current surface for every vertex");starts[i+1]+=starts[i];cursor[i]=starts[i];parent[i]=root(uint32_t(i));}
  std::vector<uint32_t>neighbors(corners*2);std::vector<float>costs(corners*2);
  for(size_t t=0;t<corners;t+=3)for(size_t k=0;k<3;++k){
    work.step(16);auto a=m.indices[t+k],b=m.indices[t+(k+1)%3];const float cost=length(m.positions[a]-m.positions[b]);
    if(!(cost>0)||!std::isfinite(cost))throw std::invalid_argument("Visibility graph has a degenerate edge");
    for(auto edge:{std::pair{a,b},std::pair{b,a}}){auto at=cursor[edge.first]++;neighbors[at]=edge.second;costs[at]=cost;}
  }
  auto segments=rig.segments();float scale=1e-5f;
  for(size_t b=0;b<bones;++b){work.step();if(rig.bones[b].deform)scale=std::max(scale,length(segments[b][1]-segments[b][0])*.02f);}
  std::vector<VertexWeights>candidate(n);for(auto&row:candidate)row.reserve(options.limits.max_influences);
  std::vector<float>direct(n),dist(n),best(n);
  std::vector<uint32_t>order(n);std::vector<uint8_t>finalized(n),seeded(n);
  Heap heap(n,dist,work);VisibilityGeodesicResult result;
  for(uint32_t bone=0;bone<bones;++bone)if(rig.bones[bone].deform){
    Point a{segments[bone][0].x,segments[bone][0].y,segments[bone][0].z},b{segments[bone][1].x,segments[bone][1].y,segments[bone][1].z};
    std::fill(best.begin(),best.end(),INFINITY);std::fill(dist.begin(),dist.end(),INFINITY);std::fill(finalized.begin(),finalized.end(),0);heap.reset();
    for(uint32_t v=0;v<n;++v){work.step(32);direct[v]=distance(points[v],a,b);order[v]=v;}
    std::sort(order.begin(),order.end(),[&](auto x,auto y){work.step();return direct[x]<direct[y]||(direct[x]==direct[y]&&x<y);});
    for(auto v:order){work.step();if(direct[v]>options.maximum_seed_distance)break;auto component=parent[v];
      if(direct[v]>best[component]+options.seed_margin)continue;
      if(!closest_visible(query,v,points[v],a,b,scratch,work,query_report)){++result.occluded_candidates;continue;}
      best[component]=std::min(best[component],direct[v]);seeded[component]=1;dist[v]=direct[v];heap.decrease(v);++result.visible_seeds;
    }
    while(!heap.empty()){
      auto v=heap.pop();work.step();if(finalized[v])throw std::logic_error("Indexed visibility heap reopened a finalized row");finalized[v]=1;
      for(size_t e=starts[v];e<starts[v+1];++e){work.step(4);auto u=neighbors[e];float d=dist[v]+costs[e];if(!finalized[u]&&d<dist[u]){dist[u]=d;heap.decrease(u);}}
    }
    for(size_t v=0;v<n;++v){work.step();if(!std::isfinite(dist[v]))continue;auto&row=candidate[v];Influence next{bone,dist[v]};if(row.size()<options.limits.max_influences)row.push_back(next);else{auto worst=std::max_element(row.begin(),row.end(),[&](auto x,auto y){work.step();return x.weight==y.weight?x.joint<y.joint:x.weight<y.weight;});if(next.weight<worst->weight)*worst=next;}}
  }
  for(size_t v=0;v<n;++v){work.step();if(!seeded[parent[v]])throw std::runtime_error("No visible deform-bone seed for current surface component "+std::to_string(parent[v]));}
  result.weights.vertices.reserve(n);
  for(auto&row:candidate){work.step(64+row.size()*8);float nearest=INFINITY;for(auto i:row)nearest=std::min(nearest,i.weight);for(auto&i:row)i.weight=std::pow((nearest+scale)/(i.weight+scale),options.falloff);result.weights.vertices.push_back(normalize_weights(std::move(row),bones,{},options.limits));}
  work.step_items(n,options.limits.max_influences+1);result.weights.validate(n,bones);work.stop();result.work=work.used;result.workspace=bytes.used;return result;
}
} // namespace dome::detail
