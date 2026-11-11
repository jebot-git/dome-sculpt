// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Independent authored-fixture geometry, binary64 role transport, image
// filtering and black-albedo GGX. No product normal/field/shader evaluator.
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace source_oracle {
constexpr int side=256;
constexpr double projection_scale=.8,shear=.125,rough=128./255.;
struct V {
  double x=0,y=0,z=0;
  V operator+(V v)const{return{x+v.x,y+v.y,z+v.z};}
  V operator-(V v)const{return{x-v.x,y-v.y,z-v.z};}
  V operator*(double s)const{return{x*s,y*s,z*s};}
};
inline double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline V unit(V p){const auto n=std::sqrt(dot(p,p));if(!(n>1e-12))throw std::runtime_error("Degenerate independent source basis");return p*(1/n);}
inline constexpr std::array<V,6> authored{{{-.9,-.5,0},{-.1,-.5,0},{-.5,.5,0},{.1,-.5,0},{.9,-.5,0},{.5,.5,0}}};
inline constexpr std::array<std::array<double,2>,6> atlas{{{.05,.1},{.45,.1},{.25,.9},{.55,.1},{.95,.1},{.75,.9}}};
inline constexpr std::array<std::array<unsigned,3>,2> faces{{{0,1,2},{3,4,5}}};
inline constexpr std::array<double,2> height{{.45,-.30}};
inline std::array<double,2> current_uv(unsigned vertex){const auto p=atlas.at(vertex);const double cx=vertex<3?.25:.75,cy=1.1/3.;return{cx+.8*(p[0]-cx),cy+.8*(p[1]-cy)};}
inline V artist(size_t triangle,bool flip){return triangle==0?(flip?unit({.15,-.75,.65}):unit({-.15,.75,.65})):(flip?unit({-.25,-.65,.72}):unit({.25,.65,.72}));}
struct World {
  bool reflected=false;double yaw=0,scale=1;V offset{};
  V rotate(V p)const{return{std::cos(yaw)*p.x+std::sin(yaw)*p.z,p.y,-std::sin(yaw)*p.x+std::cos(yaw)*p.z};}
  V direction(V p)const{return rotate({(reflected?-1.:1.)*p.x+shear*p.y,p.y,p.z})*scale;}
  V point(V p)const{return direction(p)+offset;}
  V normal(V p)const{const double sign=reflected?-1:1;return unit(rotate({sign*p.x,p.y-sign*shear*p.x,p.z}));}
  std::array<double,16> matrix()const{
    const auto x=direction({1,0,0}),y=direction({0,1,0}),z=direction({0,0,1});
    return{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,offset.x,offset.y,offset.z,1};
  }
};
inline std::array<unsigned,3> order(size_t triangle,bool flip){auto out=faces.at(triangle);if(flip)std::reverse(out.begin(),out.end());return out;}
inline V edited(unsigned vertex){auto p=authored.at(vertex);if(vertex%3==2)p.z+=height.at(vertex/3);return p;}
inline std::array<std::array<double,2>,3> chart(size_t triangle,bool prepared,bool flip){
  std::array<std::array<double,2>,3> uv{};const auto ids=order(triangle,flip);
  for(size_t c=0;c<3;++c){if(prepared)uv[c]=atlas[ids[c]];else{size_t k=0;while(faces[triangle][k]!=ids[c])++k;uv[c]=k==0?std::array<double,2>{0,0}:k==1?std::array<double,2>{1,0}:std::array<double,2>{0,1};}}
  return uv;
}
struct Basis{V t,b,n;};
inline Basis basis(const std::array<V,3>&p,const std::array<std::array<double,2>,3>&uv,const World&w){
  const auto a=p[1]-p[0],b=p[2]-p[0];const auto u=uv[1][0]-uv[0][0],v=uv[1][1]-uv[0][1],s=uv[2][0]-uv[0][0],t=uv[2][1]-uv[0][1],det=u*t-s*v;
  if(std::abs(det)<1e-8)throw std::runtime_error("Degenerate independent source chart");
  const auto n=w.normal(unit(cross(a,b))),raw=w.direction((a*t-b*v)*(1/det)),raw_b=w.direction((b*u-a*s)*(1/det));
  const auto T=unit(raw-n*dot(n,raw)),B=unit(cross(n,T));return{T,B*(dot(B,raw_b)<0?-1:1),n};
}
inline V encoded_texel(int x,int y){
  x=std::clamp(x,0,63);y=std::clamp(y,0,63);
  const auto p=unit({(95+(x/16)*18)/255.*2-1,(110+(y/32)*30)/255.*2-1,240/255.*2-1});
  auto encode=[](double n){return std::clamp(std::lround((n*.5+.5)*255),0l,255l)/255.*2-1;};return{encode(p.x),encode(p.y),encode(p.z)};
}
inline V sample(std::array<double,2>uv){
  const double x=uv[0]*64-.5,y=uv[1]*64-.5;const int a=int(std::floor(x)),b=int(std::floor(y));const auto fx=x-a,fy=y-b;
  return unit((encoded_texel(a,b)*(1-fx)+encoded_texel(a+1,b)*fx)*(1-fy)+(encoded_texel(a,b+1)*(1-fx)+encoded_texel(a+1,b+1)*fx)*fy);
}
inline V normal(size_t triangle,std::array<double,3>b,bool prepared,bool flip,const World&w,bool wrong_uv=false,bool geometric_only=false,bool wrong_frame=false){
  const auto frame_triangle=wrong_frame?1-triangle:triangle;
  std::array<V,3> before{},current{};const auto ids=order(frame_triangle,flip);
  for(size_t k=0;k<3;++k){before[k]=authored[faces[frame_triangle][k]];current[k]=edited(ids[k]);}
  const auto old=basis(before,chart(frame_triangle,prepared,false),w),now=basis(current,chart(frame_triangle,prepared,flip),w);
  const auto N=artist(triangle,flip);V desired=w.normal(N);
  if(prepared){
    std::array<double,2> uv{};const auto actual=order(triangle,flip);for(size_t k=0;k<3;++k)for(size_t d=0;d<2;++d)uv[d]+=b[k]*atlas[actual[k]][d];
    if(wrong_uv){const double cx=triangle==0?.25:.75,cy=1.1/3.;uv={cx+.8*(uv[0]-cx),cy+.8*(uv[1]-cy)};}
    const auto q=sample(uv),t=unit(w.direction(unit(V{1,0,0}-N*N.x))),bitangent=unit(cross(desired,t))*(w.reflected?-1:1);
    desired=unit(t*q.x+bitangent*q.y+desired*q.z);
  }
  return (geometric_only?now.n:unit(now.t*dot(old.t,desired)+now.b*dot(old.b,desired)+now.n*dot(old.n,desired)))*(flip?-1:1);
}
inline V brdf(V n,V view,V light,V energy){
  const auto h=unit(view+light);const double nv=std::max(dot(n,view),.0001),nl=std::max(dot(n,light),0.),nh=std::max(dot(n,h),0.),vh=std::max(dot(view,h),0.);
  const double a=rough*rough,aa=a*a,D=aa/(std::numbers::pi*std::pow(nh*nh*(aa-1)+1,2)),k=std::pow(rough+1,2)/8,G=nv/(nv*(1-k)+k)*nl/(nl*(1-k)+k),F=.04+.96*std::pow(1-vh,5);
  return energy*(D*G*F/std::max(4*nv*nl,.0001)*nl);
}
inline V specular(V n,V point){const auto view=unit(V{0,0,4}-point);auto p=brdf(n,view,unit({-.4,.8,.7}),{3.8,3.8,3.8})+brdf(n,view,unit({.7,.2,-.4}),{.65,.8,1});return{p.x/(1+p.x),p.y/(1+p.y),p.z/(1+p.z)};}
inline std::array<int,4> finish(V linear,bool ghost){
  const double alpha=ghost?.18:1.;if(ghost)linear=(linear*.5+V{.35,.5,.7}*.5)*alpha;
  auto byte=[](double p){return int(std::lround(std::clamp(p<=.0031308?p*12.92:1.055*std::pow(p,1/2.4)-.055,0.,1.)*255));};
  return{byte(linear.x),byte(linear.y),byte(linear.z),int(std::lround(alpha*255))};
}
struct Triangle{std::array<V,3>p;std::array<unsigned,3>ids;size_t set;};
inline std::array<Triangle,2> triangles(bool flip,const World&w){
  std::array<Triangle,2> out{};for(size_t t=0;t<2;++t){out[t].ids=order(t,flip);out[t].set=t;for(size_t k=0;k<3;++k)out[t].p[k]=w.point(edited(out[t].ids[k]));}return out;
}
inline std::array<double,3> barycentric(const Triangle&t,double x,double y){
  const auto&a=t.p[0],&b=t.p[1],&c=t.p[2];const double den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
  if(std::abs(den)<1e-10)throw std::runtime_error("Collapsed independent screen triangle");
  const auto u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/den,v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/den;return{u,v,1-u-v};
}
}
