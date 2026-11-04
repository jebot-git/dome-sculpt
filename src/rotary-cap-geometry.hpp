// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include "volume-exact.hpp"
#include <lapacke.h>
#include <map>
#include <numeric>
namespace dome::detail {
// Research descriptor only. Positive geometry is not joint/source authority.
struct RotaryCapGeometry {
  bool disk = false, convex_boundary = false, ellipsoid = false;
  size_t boundary = 0, negative_turns = 0, svd_sweeps = 0;
  std::array<double,10> singular{};
  std::array<double,3> centroid{}, center{}, radii{}, eigenvalues{};
  double scale = 0, maximum_residual = 0, rank_ratio = 0;
};
template<class Budget>
RotaryCapGeometry analyze_rotary_cap(const Mesh&m,size_t maximum_bytes,Budget&w){
 const size_t n=m.positions.size(),faces=m.indices.size()/3;
 if(n<10||n>4096||!faces||faces>16384||m.indices.size()%3)
  throw std::invalid_argument("Rotary cap descriptor dimensions");
 const size_t bytes=n*112+m.indices.size()*160+32768;
 if(bytes>maximum_bytes)throw std::length_error("Rotary cap bytes before copies");
 w.step_items(n,256);w.step_items(faces,512);
 RotaryCapGeometry out;
 struct Edge{uint32_t a,b,count=0;};std::map<std::array<uint32_t,2>,Edge>edges;
 std::vector<double>mass(n);std::array<double,3>area_vector{};
 for(size_t c=0;c<m.indices.size();c+=3){
  std::array<uint32_t,3>ids{m.indices[c],m.indices[c+1],m.indices[c+2]};
  for(auto i:ids)if(i>=n)throw std::invalid_argument("Rotary cap index");
  const auto a=m.positions[ids[0]],b=m.positions[ids[1]],d=m.positions[ids[2]];
  const double ab[3]={double(b.x)-a.x,double(b.y)-a.y,double(b.z)-a.z},ad[3]={double(d.x)-a.x,double(d.y)-a.y,double(d.z)-a.z};
  const std::array<double,3>cross{ab[1]*ad[2]-ab[2]*ad[1],ab[2]*ad[0]-ab[0]*ad[2],ab[0]*ad[1]-ab[1]*ad[0]};
  const double area=std::hypot(cross[0],cross[1],cross[2]);
  if(!(area>0)||!std::isfinite(area))throw std::invalid_argument("Rotary cap degenerate geometry");
  for(size_t k=0;k<3;++k){area_vector[k]+=cross[k];mass[ids[k]]+=area/6;auto a=ids[k],b=ids[(k+1)%3];auto[it,added]=edges.try_emplace({std::min(a,b),std::max(a,b)},Edge{a,b,0});(void)added;if(++it->second.count>2)throw std::invalid_argument("Rotary cap nonmanifold edge");}
 }
 std::vector<uint32_t>next(n,UINT32_MAX),incoming(n,0),ring;
 for(const auto&[key,e]:edges)if(e.count==1){(void)key;++out.boundary;if(next[e.a]!=UINT32_MAX)return out;next[e.a]=e.b;++incoming[e.b];}
 if(out.boundary<3)return out;
 uint32_t start=0;while(start<n&&next[start]==UINT32_MAX)++start;
 auto at=start;do{w.step();if(at>=n||next[at]==UINT32_MAX||incoming[at]!=1||ring.size()>=out.boundary)return out;ring.push_back(at);at=next[at];}while(at!=start);
 if(ring.size()!=out.boundary)return out;
 out.disk=true;
 const size_t drop=size_t(std::max_element(area_vector.begin(),area_vector.end(),[](double a,double b){return std::abs(a)<std::abs(b);})-area_vector.begin());
 auto project=[&](uint32_t i){auto p=m.positions[i];const std::array<float,3>a{p.x,p.y,p.z};return std::array<float,2>{a[(drop+1)%3],a[(drop+2)%3]};};
 volume_exact::Arithmetic exact(w);int orientation=0;
 for(size_t i=0;i<ring.size();++i){auto sign=exact.orient2(project(ring[i]),project(ring[(i+1)%ring.size()]),project(ring[(i+2)%ring.size()])).sign;if(sign&&!orientation)orientation=sign;else if(sign&&sign!=orientation)++out.negative_turns;}
 if(!orientation||out.negative_turns)return out;
 // Every point must be in every closed boundary-edge halfspace. This also
 // rejects self-crossing/projected folded caps, rather than only local turns.
 for(size_t e=0;e<ring.size();++e)for(uint32_t i=0;i<n;++i){auto sign=exact.orient2(project(ring[e]),project(ring[(e+1)%ring.size()]),project(i)).sign;if(sign&&sign!=orientation)return out;}
 out.convex_boundary=true;
 const double total=std::accumulate(mass.begin(),mass.end(),0.);
 if(!(total>0)||!std::isfinite(total))throw std::runtime_error("Rotary cap invalid area");
 for(size_t i=0;i<n;++i){auto p=m.positions[i];const double v[]={p.x,p.y,p.z};for(size_t k=0;k<3;++k){if(!std::isfinite(v[k])||std::abs(v[k])>10000)throw std::invalid_argument("Rotary cap finite coordinate bound");out.centroid[k]+=mass[i]/total*v[k];}}
 double square=0;for(size_t i=0;i<n;++i){auto p=m.positions[i];const double v[]={p.x,p.y,p.z};for(size_t k=0;k<3;++k)square+=mass[i]/total*(v[k]-out.centroid[k])*(v[k]-out.centroid[k]);}
 out.scale=std::sqrt(square);if(!(out.scale>0)||!std::isfinite(out.scale))throw std::runtime_error("Rotary cap invalid scale");
 // Explicit bounded native Jacobi SVD, same30-sweep contract as the affine
 // implementation. Charge its conservative pair/sweep model BEFORE calling.
 // This descriptor remains unadmitted until library-cost/uncertainty audit.
 w.step_items(n,30*45*32+512);w.step(32768);w.stop();
 std::vector<double>a(n*10),work(n+10);std::array<double,100>v{};
 auto row=[&](size_t i){auto p=m.positions[i];const double x=(p.x-out.centroid[0])/out.scale,y=(p.y-out.centroid[1])/out.scale,z=(p.z-out.centroid[2])/out.scale;return std::array<double,10>{x*x,y*y,z*z,2*x*y,2*x*z,2*y*z,2*x,2*y,2*z,1};};
 for(size_t i=0;i<n;++i){auto r=row(i);double scale=std::sqrt(mass[i]/total);for(size_t j=0;j<10;++j)a[j*n+i]=scale*r[j];}
 const auto status=LAPACKE_dgesvj_work(LAPACK_COL_MAJOR,'G','N','V',lapack_int(n),10,a.data(),lapack_int(n),out.singular.data(),10,v.data(),10,work.data(),lapack_int(work.size()));
 w.stop();if(status)throw std::runtime_error("Rotary cap Jacobi SVD did not converge");
 if(!(work[0]>0)||!std::isfinite(work[0])||work[3]<0||work[3]>30||std::floor(work[3])!=work[3])throw std::runtime_error("Rotary cap invalid SVD status");
 out.svd_sweeps=size_t(work[3]);for(auto&s:out.singular){s*=work[0];if(s<0||!std::isfinite(s))throw std::runtime_error("Rotary cap invalid singular value");}
 out.rank_ratio=out.singular[0]?out.singular[8]/out.singular[0]:0;
 std::array<double,10>coef{};for(size_t i=0;i<10;++i)coef[i]=v[9*10+i];
 if(coef[0]+coef[1]+coef[2]<0)for(auto&x:coef)x=-x;
 std::array<double,9>q{coef[0],coef[3],coef[4],coef[3],coef[1],coef[5],coef[4],coef[5],coef[2]};
 auto eigen=q;
 if(LAPACKE_dsyev(LAPACK_COL_MAJOR,'N','U',3,eigen.data(),3,out.eigenvalues.data()))throw std::runtime_error("Rotary cap symmetric eigen failure");
 if(!(out.eigenvalues[0]>0))return out;
 std::array<double,3>center{-coef[6],-coef[7],-coef[8]};std::array<lapack_int,3>pivot{};
 if(LAPACKE_dgesv(LAPACK_COL_MAJOR,3,1,q.data(),3,pivot.data(),center.data(),3))throw std::runtime_error("Rotary cap center solve failure");
 const double level=coef[9]+coef[6]*center[0]+coef[7]*center[1]+coef[8]*center[2];
 if(!(level<0)||!std::isfinite(level))return out;
 out.ellipsoid=true;
 for(size_t k=0;k<3;++k){out.center[k]=out.centroid[k]+out.scale*center[k];out.radii[k]=out.scale*std::sqrt(-level/out.eigenvalues[k]);}
 for(size_t i=0;i<n;++i){auto r=row(i);double f=0;for(size_t k=0;k<10;++k)f+=coef[k]*r[k];out.maximum_residual=std::max(out.maximum_residual,std::abs(f));}
 return out;
}
} // namespace dome::detail
