// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-source-shadow.hpp"
#include <memory>
#include <numeric>
#include <span>
#include <vector>
namespace dome::detail::visibility_geodesic {
// A different query from the finite-source integral/entry-wall policy. This
// class tests the genuine closest ray, two-sided, excluding only its exact
// origin. It supplies no topology, source-bone or material authority.
class ClosestRayVisibility {
  using I=volume_exact::Integer;
  using V=std::array<I,3>;
  using P=bone_shadow::Point;
  struct Box {
    P lo{},hi{};
    std::array<uint32_t,3>low{},high{};
  };
  struct Node {Box box;uint32_t first=0,count=0,left=0,right=0;};
  std::vector<P>points_;
  std::vector<V>coordinates_;
  std::vector<std::array<uint32_t,3>>faces_;
  std::vector<uint32_t>order_;
  std::vector<Node>nodes_;
  // Optional explicit intrinsic-sheet wall domain. Never merges vertices or
  // changes geometry: disconnected indexed surfaces are independent domains.
  std::vector<uint32_t>components_;
  uint32_t unit_=0;
  std::shared_ptr<const uint8_t>identity_;
  template<class E>static I reduced(float value,uint32_t unit,E&exact){auto x=exact.scalar(value);if(x.sign){if(x.shift<unit)throw std::logic_error("Exact ray dyadic unit changed");x.shift-=unit;}return x;}
  V coordinate(uint32_t index,uint32_t delta)const{auto p=coordinates_[index];for(auto&x:p)if(x.sign)x.shift+=delta;return p;}
  I coordinate(uint32_t index,size_t axis,uint32_t delta)const{auto x=coordinates_[index][axis];if(x.sign)x.shift+=delta;return x;}
  template<class E>static I orient2(const V&a,const V&b,const V&c,size_t drop,E&x){size_t u=(drop+1)%3,v=(drop+2)%3;return x.subtract(x.multiply(x.subtract(b[u],a[u]),x.subtract(c[v],a[v])),x.multiply(x.subtract(b[v],a[v]),x.subtract(c[u],a[u])));}
  template<class E>static I orient3(const V&a,const V&b,const V&c,const V&d,E&x){V u,v,w;for(size_t k=0;k<3;++k){u[k]=x.subtract(b[k],a[k]);v[k]=x.subtract(c[k],a[k]);w[k]=x.subtract(d[k],a[k]);}auto term=[&](size_t i,size_t j,size_t k){return x.multiply(u[i],x.subtract(x.multiply(v[j],w[k]),x.multiply(v[k],w[j])));};return x.add(x.add(term(0,1,2),term(1,2,0)),term(2,0,1));}
  template<class E>static I blend(const I&a,const I&b,const volume_exact::Fraction&t,E&x){return x.add(x.multiply(a,x.subtract(t.denominator,t.numerator)),x.multiply(b,t.numerator));}
  template<class E>static volume_exact::Fraction closest(const V&p,const V&a,const V&b,E&x){I numerator,denominator;for(size_t k=0;k<3;++k){auto d=x.subtract(b[k],a[k]);numerator=x.add(numerator,x.multiply(x.subtract(p[k],a[k]),d));denominator=x.add(denominator,x.multiply(d,d));}if(numerator.sign<=0||!denominator.sign)return x.zero();if(x.subtract(numerator,denominator).sign>=0)return x.one();return bone_shadow::ratio(numerator,denominator,x);}
  template<class E>static bool equal(const V&a,const V&b,E&x){for(size_t k=0;k<3;++k)if(x.subtract(a[k],b[k]).sign)return false;return true;}
  template<class E>static bool on_ray_bounds(const V&u,const V&p,const V&qnum,const I&den,size_t drop,E&x){for(size_t k=0;k<3;++k)if(k!=drop){int a=x.subtract(u[k],p[k]).sign,b=x.subtract(x.multiply(u[k],den),qnum[k]).sign;if(a&&a==b)return false;}return true;}
  template<class E>static bool coplanar_hit(const V&p,const V&a,const V&b,const V&qnum,const volume_exact::Fraction&t,const std::array<V,3>&f,size_t drop,E&x){
    const int orientation=orient2(f[0],f[1],f[2],drop,x).sign;bool p_inside=true,q_inside=true;
    for(size_t i=0;i<3;++i){const auto&u=f[i];const auto&v=f[(i+1)%3];int ps=orient2(u,v,p,drop,x).sign,qs=blend(orient2(u,v,a,drop,x),orient2(u,v,b,drop,x),t,x).sign;p_inside&=ps==orientation;q_inside&=!qs||qs==orientation;}
    if(p_inside||q_inside)return true;
    for(size_t i=0;i<3;++i){const auto&u=f[i];const auto&v=f[(i+1)%3];int s1=blend(orient2(p,a,u,drop,x),orient2(p,b,u,drop,x),t,x).sign,s2=blend(orient2(p,a,v,drop,x),orient2(p,b,v,drop,x),t,x).sign,s3=orient2(u,v,p,drop,x).sign,s4=blend(orient2(u,v,a,drop,x),orient2(u,v,b,drop,x),t,x).sign;
      if(s1*s2<0&&s3*s4<0)return true;
      if(!s1&&!equal(u,p,x)&&on_ray_bounds(u,p,qnum,t.denominator,drop,x))return true;
      if(!s2&&!equal(v,p,x)&&on_ray_bounds(v,p,qnum,t.denominator,drop,x))return true;
    }
    return false;
  }
  template<class E>static bool triangle_hit(const V&p,const V&a,const V&b,const V&qnum,const volume_exact::Fraction&t,const std::array<V,3>&f,E&x){
    size_t drop=3;for(size_t k=0;k<3;++k)if(orient2(f[0],f[1],f[2],k,x).sign){drop=k;break;}if(drop==3)throw std::invalid_argument("Closest-ray query has a degenerate wall");
    const int side=orient3(f[0],f[1],f[2],p,x).sign,end=blend(orient3(f[0],f[1],f[2],a,x),orient3(f[0],f[1],f[2],b,x),t,x).sign;
    if(!side){if(end)return false;return coplanar_hit(p,a,b,qnum,t,f,drop,x);}
    if(end==side)return false;
    for(size_t k=0;k<3;++k)if(blend(orient3(p,a,f[k],f[(k+1)%3],x),orient3(p,b,f[k],f[(k+1)%3],x),t,x).sign==side)return false;
    return true;
  }
  template<class W>uint32_t build(uint32_t first,uint32_t last,W&w){
    const auto index=uint32_t(nodes_.size());nodes_.emplace_back();Box box;box.lo.fill(INFINITY);box.hi.fill(-INFINITY);
    for(uint32_t i=first;i<last;++i)for(auto v:faces_[order_[i]])for(size_t k=0;k<3;++k){w.step(2);if(points_[v][k]<box.lo[k]){box.lo[k]=points_[v][k];box.low[k]=v;}if(points_[v][k]>box.hi[k]){box.hi[k]=points_[v][k];box.high[k]=v;}}
    nodes_[index].box=box;if(last-first<=8){nodes_[index].first=first;nodes_[index].count=last-first;return index;}
    size_t axis=0;for(size_t k=1;k<3;++k)if(double(box.hi[k])-box.lo[k]>double(box.hi[axis])-box.lo[axis])axis=k;
    auto center=[&](uint32_t f){double lo=INFINITY,hi=-INFINITY;for(auto v:faces_[f]){lo=std::min(lo,double(points_[v][axis]));hi=std::max(hi,double(points_[v][axis]));}return lo+hi;};
    std::sort(order_.begin()+first,order_.begin()+last,[&](auto a,auto b){w.step(8);double x=center(a),y=center(b);return x<y||(x==y&&a<b);});auto middle=first+(last-first)/2;
    auto left=build(first,middle,w),right=build(middle,last,w);nodes_[index].left=left;nodes_[index].right=right;return index;
  }
public:
  size_t planned_bytes=0,construction_forecast=0;
  struct Scratch {
    std::vector<uint32_t>stack;
    std::shared_ptr<const uint8_t>geometry;
    explicit Scratch(const ClosestRayVisibility&q):geometry(q.identity_){stack.reserve(q.nodes_.size());}
  };
  ClosestRayVisibility(const ClosestRayVisibility&)=delete;
  ClosestRayVisibility&operator=(const ClosestRayVisibility&)=delete;
  template<class W>ClosestRayVisibility(std::span<const P>points,std::span<const uint32_t>indices,size_t maximum_bytes,W&w,bool component_self_occlusion=false){
    w.stop();size_t triangles=indices.size()/3;if(points.empty()||points.size()>2'000'000||!triangles||triangles>2'000'000||indices.size()%3)throw std::invalid_argument("Invalid closest-ray owning geometry");size_t depth=std::bit_width(triangles);
    planned_bytes=65536+points.size()*(sizeof(P)+sizeof(V))+triangles*(sizeof(std::array<uint32_t,3>)+sizeof(uint32_t)+2*sizeof(Node)+2*sizeof(uint32_t));construction_forecast=points.size()*16+triangles*(64+32*depth*depth);
    if(component_self_occlusion){planned_bytes+=points.size()*2*sizeof(uint32_t);construction_forecast+=points.size()*4+indices.size()*(16+8*std::bit_width(points.size()));}
    if(planned_bytes>maximum_bytes||construction_forecast>w.maximum-w.used)throw std::length_error("Closest-ray owning construction exceeds bound before copies");
    volume_exact::Arithmetic exact(w);unit_=UINT32_MAX;
    for(auto p:points)for(float value:p){auto n=exact.scalar(value);if(n.sign)unit_=std::min(unit_,n.shift);}if(unit_==UINT32_MAX)unit_=0;
    points_.assign(points.begin(),points.end());coordinates_.reserve(points.size());for(auto p:points_){w.step(3);V v;for(size_t k=0;k<3;++k)v[k]=reduced(p[k],unit_,exact);coordinates_.push_back(v);}
    faces_.reserve(triangles);for(size_t at=0;at<indices.size();at+=3){w.step(3);std::array<uint32_t,3>f{indices[at],indices[at+1],indices[at+2]};for(auto i:f)if(i>=points_.size())throw std::invalid_argument("Closest-ray corner outside owning geometry");faces_.push_back(f);}if(component_self_occlusion){
      components_.resize(points.size());std::iota(components_.begin(),components_.end(),0);std::vector<uint32_t>sizes(points.size(),1);
      auto root=[&](uint32_t a){while(components_[a]!=a){w.step(3);components_[a]=components_[components_[a]];a=components_[a];}return a;};
      for(const auto&f:faces_)for(size_t k=0;k<3;++k){w.step(4);auto a=root(f[k]),b=root(f[(k+1)%3]);if(a!=b){if(sizes[a]<sizes[b])std::swap(a,b);components_[b]=a;sizes[a]+=sizes[b];}}
      for(uint32_t v=0;v<components_.size();++v){w.step();components_[v]=root(v);}
    }
    order_.resize(triangles);std::iota(order_.begin(),order_.end(),0);nodes_.reserve(triangles*2);build(0,uint32_t(triangles),w);identity_=std::make_shared<const uint8_t>(0);
  }
  template<class W>bool visible(uint32_t row,P head,P tail,Scratch&scratch,W&w)const{
    w.stop();if(row>=points_.size()||scratch.geometry.get()!=identity_.get()||scratch.geometry.owner_before(identity_)||identity_.owner_before(scratch.geometry)||scratch.stack.capacity()<nodes_.size())throw std::invalid_argument("Closest-ray scratch/row owner differs");
    volume_exact::Arithmetic exact(w);uint32_t unit=unit_;for(auto p:{head,tail})for(float value:p){auto n=exact.scalar(value);if(n.sign)unit=std::min(unit,n.shift);}const uint32_t delta=unit_-unit;V p=coordinate(row,delta),a,b;for(size_t k=0;k<3;++k){a[k]=reduced(head[k],unit,exact);b[k]=reduced(tail[k],unit,exact);}auto parameter=closest(p,a,b,exact);V qnum;for(size_t k=0;k<3;++k)qnum[k]=blend(a[k],b[k],parameter,exact);bool same=true;for(size_t k=0;k<3;++k)same&=!exact.subtract(qnum[k],exact.multiply(p[k],parameter.denominator)).sign;if(same)return true;
    auto overlaps=[&](const Box&box){for(size_t k=0;k<3;++k){w.step(2);const float current=points_[row][k];if(current<box.lo[k]){if(std::max(head[k],tail[k])<box.lo[k])return false;auto bound=exact.multiply(coordinate(box.low[k],k,delta),parameter.denominator);if(exact.subtract(qnum[k],bound).sign<0)return false;}if(current>box.hi[k]){if(std::min(head[k],tail[k])>box.hi[k])return false;auto bound=exact.multiply(coordinate(box.high[k],k,delta),parameter.denominator);if(exact.subtract(qnum[k],bound).sign>0)return false;}}return true;};
    scratch.stack.clear();scratch.stack.push_back(0);while(!scratch.stack.empty()){w.stop();auto id=scratch.stack.back();scratch.stack.pop_back();const auto&node=nodes_[id];if(!overlaps(node.box))continue;if(!node.count){scratch.stack.push_back(node.right);scratch.stack.push_back(node.left);continue;}for(uint32_t i=node.first;i<node.first+node.count;++i){w.step(4);const auto&face=faces_[order_[i]];if(!components_.empty()&&components_[row]!=components_[face[0]])continue;if(std::find(face.begin(),face.end(),row)!=face.end())continue;std::array<V,3>f{coordinate(face[0],delta),coordinate(face[1],delta),coordinate(face[2],delta)};if(triangle_hit(p,a,b,qnum,parameter,f,exact))return false;}}return true;
  }
};
} // namespace dome::detail::visibility_geodesic
