// SPDX-License-Identifier: GPL-3.0-or-later
#include "affine-interaction.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
using namespace dome;
using namespace dome::detail;
namespace {
bool armed=false;size_t allocations=0,checks=0;
void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;}
void near(AffineVector a,AffineVector b,const char*m){for(size_t i=0;i<3;++i)if(std::abs(a[i]-b[i])>1e-11*std::max({1.,std::abs(a[i]),std::abs(b[i])}))throw std::runtime_error(m);++checks;}
template<class F>void refuses(F f,const char*m){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,m);}
}
void *operator new(size_t n){if(armed){++allocations;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try {
  AffineTransform identity;auto diagonal=identity.matrix();diagonal[0]=2;diagonal[10]=-3;
  diagonal[12]=4;diagonal[13]=-2;diagonal[14]=7;
  const AffineTransform placement(diagonal);const AffineInteractionFrame frame(placement);
  auto ray=affine_local_ray(frame,{6,0,13},{0,0,-4},20);
  near(ray.origin,{1,2,-2},"affine ray origin includes inverse translation/reflection");
  near(ray.direction,{0,0,1},"local ray direction preserves reflection and normalizes");
  check(std::abs(ray.local_per_world-1./3)<1e-15,"ray distance scale is directional");
  check(std::abs(ray.maximum_distance-20./3)<1e-14,"ray maximum converts using directional length");
  check(std::abs(ray.world_distance(5)-15)<1e-14,"world hit distance is not average object scale");
  near(placement.point({ray.origin[0],ray.origin[1],ray.origin[2]+5}),{6,0,-2},"independent analytic world hit point");
  refuses([&]{(void)affine_local_ray(frame,{0,0,0},{0,0,0},20);},"zero direction refuses");
  refuses([&]{(void)affine_local_ray(frame,{0,0,0},{0,1,0},-1);},"negative max distance refuses");
  refuses([&]{(void)ray.world_distance(-1);},"negative local hit distance refuses");
  AffineBrushMetric sphere(frame,{4,-2,7},{4,-2,7},1);
  check(sphere.contains({.49,0,0})&&!sphere.contains({.51,0,0}),"brush X extent uses full metric");
  check(sphere.contains({0,.99,0})&&!sphere.contains({0,1.01,0}),"brush Y extent differs from X");
  check(sphere.contains({0,0,.33})&&!sphere.contains({0,0,.34}),"brush Z reflection retains physical distance");
  near(sphere.local_bounds().minimum,{-.5,-1,-1./3},"analytic local sphere support minimum");
  near(sphere.local_bounds().maximum,{.5,1,1./3},"analytic local sphere support maximum");
  const auto from=placement.point({-1,0,0}),to=placement.point({1,1,0});
  AffineBrushMetric capsule(frame,from,to,.7);
  check(std::abs(capsule.distance({0,.5,.2})-.6)<1e-14,"capsule projection uses world metric");
  check(capsule.contains({0,.5,.2})&&!capsule.contains({0,.5,.4}),"capsule rejects locally close but physically distant point");
  auto sheared=identity.matrix();sheared={2,0,0,0, 1,3,0,0, 0,1,-4,0, 5,-2,7,1};
  AffineInteractionFrame skew{AffineTransform(sheared)};
  near(skew.normal({1,0,0}),{.5,-1./6,-1./24},"cached normal matrix is inverse transpose without normalization");
  check(skew.orientation()==AffineOrientation::Reversing,"interaction facing retains determinant sign");
  const AffineVector center{.3,-.2,.7};const double radius=.31;
  AffineBrushMetric skew_sphere(skew,center,center,radius);auto bounds=skew_sphere.local_bounds();
  for(int n=0;n<128;++n) {
    const double z=-1+2.*n/127,angle=n*2.399963229728653;
    const AffineVector unit{std::sqrt(1-z*z)*std::cos(angle),std::sqrt(1-z*z)*std::sin(angle),z};
    AffineVector edge{},inside{},outside{};
    for(size_t k=0;k<3;++k){edge[k]=center[k]+radius*unit[k];inside[k]=center[k]+.999*radius*unit[k];outside[k]=center[k]+1.001*radius*unit[k];}
    auto local=skew.inverse().point(edge);
    for(size_t k=0;k<3;++k)check(local[k]>=bounds.minimum[k] && local[k]<=bounds.maximum[k],"sheared support encloses independent world boundary");
    check(skew_sphere.contains(skew.inverse().point(inside)),"inside world sphere remains inside local metric");
    check(!skew_sphere.contains(skew.inverse().point(outside)),"outside world sphere remains outside local metric");
  }
  const double c=std::sqrt(.5);auto rotation=identity.matrix();rotation[0]=c;rotation[1]=c;rotation[4]=-c;rotation[5]=c;
  rotation[12]=.2;rotation[13]=-.3;rotation[14]=.1;
  AffineTransform gesture(rotation);auto local=identity.matrix();local[12]=.13;local[13]=-.27;
  AffineTransform old_local(local),edited=frame.apply_world_edit(old_local,gesture);
  for(auto point:{AffineVector{0,0,0},AffineVector{.4,-.2,.7},AffineVector{-1,2,3}})
    near((placement*edited).point(point),(gesture*placement*old_local).point(point),"world gesture conjugation preserves every local point");
  const auto&e=edited.matrix();
  check(std::abs(e[0]*e[4]+e[1]*e[5]+e[2]*e[6])>.1,"world similarity gesture genuinely requires local shear");
  auto moved_parent=diagonal;moved_parent[12]+=3;
  near((AffineTransform(moved_parent)*edited).point({.1,.2,.3}),
       AffineTransform(moved_parent).point(edited.point({.1,.2,.3})),"later parent edit retains committed local edit semantics");
  auto extreme=identity.matrix();extreme[0]=1e-20;
  const std::array<AffineTransform,2> factors{AffineTransform(extreme),gesture};
  const auto factored=AffineInteractionFrame::factored(factors);
  refuses([&]{AffineInteractionFrame flattened(factored.world());},"flattened extreme frame has explicit conditioning refusal");
  auto normal=factored.normal({1,0,0});
  check(std::abs(normal[0])>1e19 && std::abs(normal[1])>.1,"factored normal frame preserves eligible original scale range");
  refuses([&]{AffineBrushMetric bad(frame,{0,0,0},{0,0,0},0);},"invalid metric radius refuses");
  allocations=0;armed=true;double accumulated=0;
  for(int n=0;n<128;++n) {
    accumulated+=capsule.distance({n*.001,.2,.3});
    auto r=affine_local_ray(frame,{6,0,13},{0,0,-4},20);accumulated+=r.world_distance(5);
    auto moved=frame.apply_world_edit(old_local,gesture);accumulated+=moved.point({.1,.2,.3})[0];
  }
  armed=false;
  check(allocations==0 && std::isfinite(accumulated),"valid per-sample metric/ray/edit path makes no C++ allocations");
  std::cout<<checks<<" affine interaction checks passed; valid sample path: 0 allocations\n";return 0;
}catch(const std::exception&e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
