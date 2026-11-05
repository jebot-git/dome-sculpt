// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "closest-ray-visibility.hpp"
#include "weight-surface-system.hpp"
#include "volume-topology.hpp"
#include <map>
namespace dome::detail::visibility_geodesic {
// Owning closed-material classifier paired with ONE immutable exact BVH.
// Closed indexed manifold components use unsigned solid parity. Open or
// nonmanifold components explicitly remain intrinsic surface domains. There
// is no coordinate weld, normal-vector heuristic, epsilon or posed geometry.
struct ClosedEmbeddingFailure:std::invalid_argument {
  uint32_t component,first,second;volume_topology::Triangle a,b;bool coincident;
  ClosedEmbeddingFailure(uint32_t c,uint32_t f,uint32_t s,volume_topology::Triangle x,volume_topology::Triangle y,bool same)
    :std::invalid_argument(same?"Closed material has an unowned coincident vertex contact":"Closed material has a self-intersection/overlap"),component(c),first(f),second(s),a(x),b(y),coincident(same){}
};
class ClosedMeshSourceDomain {
  using Q=ClosestRayVisibility;using I=Q::I;using V=Q::V;using P=Q::P;
  std::shared_ptr<const Q>geometry_;
  std::vector<uint8_t>closed_;
  using Work=weight_surface::Budget;
  template<class E>static I edge_side(const V&a,const V&b,const V&q,const I&den,size_t drop,E&x){const size_t u=(drop+1)%3,v=(drop+2)%3;auto dx=x.subtract(b[u],a[u]),dy=x.subtract(b[v],a[v]);return x.subtract(x.multiply(dx,x.subtract(q[v],x.multiply(a[v],den))),x.multiply(dy,x.subtract(q[u],x.multiply(a[u],den))));}
  template<class E>static I plane_side(const std::array<V,3>&f,const V&q,const I&den,E&x){V u,v,r,zero{};for(size_t k=0;k<3;++k){u[k]=x.subtract(f[1][k],f[0][k]);v[k]=x.subtract(f[2][k],f[0][k]);r[k]=x.subtract(q[k],x.multiply(f[0][k],den));}return Q::orient3(zero,u,v,r,x);}
  // Exact positive infinitesimal along a finite source parameter. Every
  // predicate is affine in that parameter: first nonzero coefficient decides
  // its sign. No floating epsilon, sampled midpoint or normal receipt.
  template<class E>static int plane_sign(const std::array<V,3>&f,const V&q,const I&den,const V*direction,E&x){auto base=plane_side(f,q,den,x);return base.sign?base.sign:direction?plane_side(f,*direction,I{},x).sign:0;}
  template<class E>static int edge_sign(const V&a,const V&b,const V&q,const I&den,size_t drop,const V*direction,E&x){auto base=edge_side(a,b,q,den,drop,x);return base.sign?base.sign:direction?edge_side(a,b,*direction,I{},drop,x).sign:0;}
  template<class E>static bool on_face(const std::array<V,3>&f,const V&q,const I&den,E&x,const V*direction=nullptr){if(plane_sign(f,q,den,direction,x))return false;for(size_t drop=0;drop<3;++drop){const auto orientation=Q::orient2(f[0],f[1],f[2],drop,x).sign;if(!orientation)continue;for(size_t k=0;k<3;++k){auto side=edge_sign(f[k],f[(k+1)%3],q,den,drop,direction,x);if(side&&side!=orientation)return false;}return true;}throw std::invalid_argument("Closed source domain has a degenerate face");}
  template<class E>static bool raster_inside(std::array<V,3>f,const V&q,const I&den,size_t drop,E&x,const V*direction=nullptr){auto orientation=Q::orient2(f[0],f[1],f[2],drop,x).sign;if(!orientation)return false;if(orientation<0)std::swap(f[1],f[2]);const size_t u=(drop+1)%3,v=(drop+2)%3;for(size_t k=0;k<3;++k){const auto&a=f[k];const auto&b=f[(k+1)%3];const auto side=edge_sign(a,b,q,den,drop,direction,x);if(side<0)return false;if(!side){const int dy=x.subtract(b[v],a[v]).sign,dx=x.subtract(b[u],a[u]).sign;const bool top_left=dy>0||(!dy&&dx<0);if(!top_left)return false;}}return true;}
  void scratch(const Q::Scratch&s)const{const auto&q=*geometry_;if(s.geometry.get()!=q.identity_.get()||s.geometry.owner_before(q.identity_)||q.identity_.owner_before(s.geometry)||s.stack.capacity()<q.nodes_.size())throw std::invalid_argument("Closed source scratch differs from actual geometry owner");}
  template<class E>bool ray_box(const Q::Box&box,const V&q,const I&den,size_t axis,uint32_t delta,E&x,const V*direction=nullptr)const{const auto&g=*geometry_;auto sign=[&](I value,size_t k){return value.sign?value.sign:direction?(*direction)[k].sign:0;};for(size_t k=0;k<3;++k){auto hi=x.multiply(g.coordinate(box.high[k],k,delta),den);if(sign(x.subtract(q[k],hi),k)>0)return false;if(k!=axis){auto lo=x.multiply(g.coordinate(box.low[k],k,delta),den);if(sign(x.subtract(q[k],lo),k)<0)return false;}}return true;}
public:
  enum class Location{Outside,Inside,Boundary};
  size_t planned_bytes=0,closed_components=0;
  ClosedMeshSourceDomain(std::shared_ptr<const Q>geometry,size_t maximum_bytes,Work&w):geometry_(std::move(geometry)){
    if(!geometry_||geometry_->components_.size()!=geometry_->points_.size())throw std::invalid_argument("Closed source domain requires an owning intrinsic-component BVH");const auto&q=*geometry_;const size_t n=q.points_.size(),faces=q.faces_.size(),corners=faces*3;
    planned_bytes=65536+n*(sizeof(uint8_t)+4*sizeof(uint32_t))+corners*(192+sizeof(uint32_t))+faces*3*sizeof(uint32_t)+q.nodes_.size()*16;
    if(planned_bytes>maximum_bytes)throw std::length_error("Closed topology/embedding before allocation");
    w.step_items(n,8);closed_.assign(n,1);std::vector<std::array<uint32_t,3>>adj(faces);for(auto&a:adj)a.fill(UINT32_MAX);
    struct Edge{uint32_t face=0,local=0,count=0;};struct Less{Work*w;bool operator()(const std::array<uint32_t,2>&a,const std::array<uint32_t,2>&b)const{w->step(2);return a<b;}};
    std::map<std::array<uint32_t,2>,Edge,Less>edges(Less{&w});std::vector<size_t>starts(n+1);
    for(uint32_t f=0;f<faces;++f){const auto&ids=q.faces_[f];for(auto v:ids){w.step();++starts[v+1];}for(size_t k=0;k<3;++k){const auto a=ids[k],b=ids[(k+1)%3];if(a==b)throw std::invalid_argument("Closed source repeats a face vertex");auto[it,added]=edges.try_emplace({std::min(a,b),std::max(a,b)},Edge{f,uint32_t(k),0});auto&e=it->second;++e.count;if(!added&&e.count==2){adj[f][k]=e.face;adj[e.face][e.local]=f;}}}
    for(const auto&[edge,e]:edges){w.step();if(e.count!=2)closed_[q.components_[edge[0]]]=0;}
    for(size_t i=0;i<n;++i){w.step();starts[i+1]+=starts[i];}auto cursor=starts;std::vector<uint32_t>incident(corners);for(uint32_t f=0;f<faces;++f)for(auto v:q.faces_[f]){w.step();incident[cursor[v]++]=f;}
    for(uint32_t v=0;v<n;++v)if(closed_[q.components_[v]]){const auto degree=starts[v+1]-starts[v];if(!degree){closed_[q.components_[v]]=0;continue;}uint32_t first=incident[starts[v]],current=first,previous=UINT32_MAX;size_t visited=0;do{w.step();uint32_t next=UINT32_MAX;for(size_t k=0;k<3;++k){const auto&f=q.faces_[current];if(f[k]!=v&&f[(k+1)%3]!=v)continue;auto candidate=adj[current][k];if(candidate!=previous){next=candidate;break;}}if(next==UINT32_MAX||++visited>degree){closed_[q.components_[v]]=0;break;}previous=current;current=next;}while(current!=first);if(visited!=degree)closed_[q.components_[v]]=0;}
    for(size_t i=0;i<n;++i)if(q.components_[i]==i&&closed_[i])++closed_components;
    if(!closed_components)return;
    // Certify actual embedding for closed candidates, using BVH node pairs.
    // Equal coordinates with distinct indices never invent shared incidence.
    std::vector<std::array<uint32_t,2>>stack;stack.reserve(q.nodes_.size()*2);stack.push_back({0,0});
    auto overlap=[&](const Q::Box&a,const Q::Box&b){for(size_t k=0;k<3;++k){w.step();if(a.hi[k]<b.lo[k]||b.hi[k]<a.lo[k])return false;}return true;};
    while(!stack.empty()){w.stop();auto pair=stack.back();stack.pop_back();const auto&a=q.nodes_[pair[0]],&b=q.nodes_[pair[1]];if(!overlap(a.box,b.box))continue;if(a.count&&b.count){for(uint32_t i=a.first;i<a.first+a.count;++i)for(uint32_t j=b.first;j<b.first+b.count;++j){w.step();auto first=q.order_[i],second=q.order_[j];if(first>=second&&pair[0]==pair[1])continue;const auto&x=q.faces_[first],&y=q.faces_[second];if(q.components_[x[0]]!=q.components_[y[0]]||!closed_[q.components_[x[0]]])continue;bool bounds=true;for(size_t k=0;k<3;++k){w.step();float xl=INFINITY,xh=-INFINITY,yl=INFINITY,yh=-INFINITY;for(auto v:x){xl=std::min(xl,q.points_[v][k]);xh=std::max(xh,q.points_[v][k]);}for(auto v:y){yl=std::min(yl,q.points_[v][k]);yh=std::max(yh,q.points_[v][k]);}bounds&=xh>=yl&&yh>=xl;}if(!bounds)continue;volume_topology::Triangle tx,ty;for(size_t k=0;k<3;++k){tx[k]=q.points_[x[k]];ty[k]=q.points_[y[k]];}for(auto u:x)for(auto v:y){w.step();if(u!=v&&q.points_[u]==q.points_[v])throw ClosedEmbeddingFailure(q.components_[x[0]],first,second,tx,ty,true);}if(volume_topology::unexpected_contact(tx,ty,w))throw ClosedEmbeddingFailure(q.components_[x[0]],first,second,tx,ty,false);}}else if(pair[0]==pair[1]){stack.push_back({a.left,a.left});stack.push_back({a.right,a.right});stack.push_back({a.left,a.right});}else if(!a.count){stack.push_back({a.left,pair[1]});stack.push_back({a.right,pair[1]});}else{stack.push_back({pair[0],b.left});stack.push_back({pair[0],b.right});}}
  }
  uint32_t component(uint32_t row)const{if(row>=geometry_->components_.size())throw std::invalid_argument("Closed source row outside owner");return geometry_->components_[row];}
  bool closed(uint32_t row)const{if(row>=geometry_->components_.size())throw std::invalid_argument("Closed source row outside owner");return closed_[geometry_->components_[row]]!=0;}
private:
  template<class E>Location locate(uint32_t component,const V&point,const I&den,uint32_t delta,Q::Scratch&s,E&x,Work&w,const V*direction=nullptr)const{
    const auto&q=*geometry_;int agreed=-1;
    for(size_t axis=0;axis<3;++axis){unsigned parity=0;s.stack.clear();s.stack.push_back(0);while(!s.stack.empty()){w.stop();auto id=s.stack.back();s.stack.pop_back();const auto&node=q.nodes_[id];if(!ray_box(node.box,point,den,axis,delta,x,direction))continue;if(!node.count){s.stack.push_back(node.left);s.stack.push_back(node.right);continue;}for(uint32_t at=node.first;at<node.first+node.count;++at){w.step();const auto&ids=q.faces_[q.order_[at]];if(q.components_[ids[0]]!=component)continue;std::array<V,3>f{q.coordinate(ids[0],delta),q.coordinate(ids[1],delta),q.coordinate(ids[2],delta)};if(on_face(f,point,den,x,direction))return Location::Boundary;const auto area=Q::orient2(f[0],f[1],f[2],axis,x).sign;if(!area||!raster_inside(f,point,den,axis,x,direction))continue;const auto side=plane_sign(f,point,den,direction,x);if(side&&side!=area)parity^=1;}}
      if(agreed<0)agreed=int(parity);else if(agreed!=int(parity))throw std::runtime_error("Exact closed-solid ray-axis parity disagreement");
    }
    return agreed?Location::Inside:Location::Outside;
  }
  template<class E>uint32_t source_coordinates(P a,P b,V&head,V&tail,E&x)const{const auto&q=*geometry_;uint32_t unit=q.unit_;for(auto p:{a,b})for(float value:p){auto n=x.scalar(value);if(n.sign)unit=std::min(unit,n.shift);}for(size_t k=0;k<3;++k){head[k]=Q::reduced(a[k],unit,x);tail[k]=Q::reduced(b[k],unit,x);}return q.unit_-unit;}
public:
  Location closest(uint32_t row,P a,P b,Q::Scratch&s,Work&w)const{scratch(s);if(!closed(row))throw std::invalid_argument("No closed-solid authority for intrinsic open surface");const auto&q=*geometry_;volume_exact::Arithmetic x(w);V head,tail;auto delta=source_coordinates(a,b,head,tail,x);auto p=q.coordinate(row,delta);auto t=Q::closest(p,head,tail,x);V point;for(size_t k=0;k<3;++k)point[k]=Q::blend(head[k],tail[k],t,x);return locate(q.components_[row],point,t.denominator,delta,s,x,w);}
  bool nearest_material_closure(uint32_t row,P a,P b,Q::Scratch&s,Work&w)const{
    scratch(s);if(!closed(row))throw std::invalid_argument("No closed material source closure for open surface");const auto&q=*geometry_;volume_exact::Arithmetic x(w);V head,tail;auto delta=source_coordinates(a,b,head,tail,x);auto p=q.coordinate(row,delta);auto t=Q::closest(p,head,tail,x);V point;for(size_t k=0;k<3;++k)point[k]=Q::blend(head[k],tail[k],t,x);const auto component=q.components_[row];auto location=locate(component,point,t.denominator,delta,s,x,w);if(location==Location::Inside)return true;if(location==Location::Outside)return false;
    // A boundary nearest point is in closure of positive-measure interior
    // source only if one actual finite parameter side enters the solid.
    // A point control, tangent, coplanar segment or isolated grazing event
    // cannot borrow this one-dimensional interior-continuation certificate.
    if(t.numerator.sign){V direction;for(size_t k=0;k<3;++k)direction[k]=x.subtract(head[k],tail[k]);if(locate(component,point,t.denominator,delta,s,x,w,&direction)==Location::Inside)return true;}
    if(x.compare(t,x.one())<0){V direction;for(size_t k=0;k<3;++k)direction[k]=x.subtract(tail[k],head[k]);if(locate(component,point,t.denominator,delta,s,x,w,&direction)==Location::Inside)return true;}
    return false;
  }
  // True means the entire finite source is proved strictly exterior. Any
  // boundary/coplanar/grazing contact remains potentially interior, never
  // grounds for an exterior-attachment fallback after a failed field.
  bool exterior(uint32_t row,P a,P b,Q::Scratch&s,Work&w)const{scratch(s);if(!closed(row))throw std::invalid_argument("Exterior classification needs closed material");const auto&q=*geometry_;volume_exact::Arithmetic x(w);V head,tail;auto delta=source_coordinates(a,b,head,tail,x);const auto one=x.integer(1);const auto component=q.components_[row];if(locate(component,head,one,delta,s,x,w)!=Location::Outside||locate(component,tail,one,delta,s,x,w)!=Location::Outside)return false;
    s.stack.clear();s.stack.push_back(0);auto zero=x.zero();while(!s.stack.empty()){w.stop();auto id=s.stack.back();s.stack.pop_back();const auto&node=q.nodes_[id];bool overlap=true;for(size_t k=0;k<3;++k){w.step();if(std::max(a[k],b[k])<node.box.lo[k]||std::min(a[k],b[k])>node.box.hi[k])overlap=false;}if(!overlap)continue;if(!node.count){s.stack.push_back(node.left);s.stack.push_back(node.right);continue;}for(uint32_t at=node.first;at<node.first+node.count;++at){w.step();const auto&ids=q.faces_[q.order_[at]];if(q.components_[ids[0]]!=component)continue;std::array<V,3>f{q.coordinate(ids[0],delta),q.coordinate(ids[1],delta),q.coordinate(ids[2],delta)};if(on_face(f,head,one,x)||Q::triangle_hit(head,tail,tail,tail,zero,f,x))return false;}}
    return true;
  }
};
}
