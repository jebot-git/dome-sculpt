// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/affine.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *m){if(!ok)throw std::runtime_error(m);++checks;}
void near(AffineVector a,AffineVector b,const char*m) {
  for(size_t i=0;i<3;++i)if(std::abs(a[i]-b[i])>1e-12*std::max({1.,std::abs(a[i]),std::abs(b[i])}))throw std::runtime_error(m);
  ++checks;
}
template<class F>void refuses(F f,const char*m){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,m);}
double dot(AffineVector a,AffineVector b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
AffineVector cross(AffineVector a,AffineVector b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
}
int main()try {
  AffineTransform identity;
  near(identity.point({1,2,3}),{1,2,3},"identity point");
  const std::array<double,16> raw{2,0,0,0, 1,3,0,0, 0,1,-4,0, 5,-2,7,1};
  const AffineTransform a(raw);
  check(a.matrix()==raw,"affine source values are not decomposed");
  near(a.point({1,2,3}),{9,7,-5},"analytic sheared reflected point");
  near(a.direction({1,2,3}),{4,9,-12},"direction excludes translation");
  near(a.inverse().point({9,7,-5}),{1,2,3},"analytic inverse point");
  near((a*a.inverse()).point({3,-7,11}),{3,-7,11},"right inverse");
  near((a.inverse()*a).point({-4,6,9}),{-4,6,9},"left inverse");
  near(a.normal({1,0,0}),{.5,-1./6,-1./24},"raw inverse-transpose normal");
  near(a.cofactor_normal({1,0,0}),{-12,4,1},"analytic signed cofactor");
  near(cross(a.direction({0,1,0}),a.direction({0,0,1})),a.cofactor_normal({1,0,0}),"cofactor equals transformed edge cross");
  check(std::abs(dot(a.normal({1,0,0}),a.direction({0,1,0})))<1e-15,"normal perpendicular to sheared edge");
  check(a.orientation()==AffineOrientation::Reversing,"reflection handedness retained");
  check(a.inverse().orientation()==AffineOrientation::Reversing,"inverse reflection handedness");
  check((a*a).orientation()==AffineOrientation::Preserving,"two reflections preserve orientation");
  auto m=identity.matrix();m[0]=1e-30;m[10]=1e30;
  AffineTransform anisotropic(m);
  near(anisotropic.normal({1,1,1}),{1e30,1,1e-30},"anisotropy is not axis dependency");
  const auto extreme_normal=anisotropic.normal({1,1,1});
  check(std::abs(extreme_normal[0]/1e30-1)<1e-12 && extreme_normal[1]==1 &&
        std::abs(extreme_normal[2]/1e-30-1)<1e-12,
        "tiny and large normal components each retain relative precision");
  near(anisotropic.inverse().point(anisotropic.point({2,-3,7})),{2,-3,7},"extreme diagonal inverse");
  m=identity.matrix();m[10]=0;AffineTransform flat(m);
  check(flat.orientation()==AffineOrientation::Singular,"singular forward authority represented");
  near(flat.point({2,3,4}),{2,3,0},"singular forward point remains defined");
  near(flat.cofactor_normal({0,0,1}),{0,0,1},"singular cofactor keeps surviving plane");
  refuses([&]{(void)flat.inverse();},"singular inverse refuses");
  refuses([&]{(void)flat.normal({0,0,1});},"singular inverse-transpose refuses");
  m=identity.matrix();m[4]=1;m[5]=1e-14;AffineTransform parallel(m);
  refuses([&]{(void)parallel.inverse();},"nearly dependent axes refuse explicitly");
  check(parallel.orientation()==AffineOrientation::Preserving,"inverse conditioning does not erase orientation sign");
  const double cancellation_delta=std::ldexp(1.,-(std::numeric_limits<long double>::digits/2+10));
  m=identity.matrix();m[1]=1+cancellation_delta;m[4]=1-cancellation_delta;
  refuses([&]{(void)AffineTransform(m).orientation();},"uncertain determinant cancellation never invents a reflection");
  m=identity.matrix();m[0]=std::numeric_limits<double>::infinity();
  refuses([&]{(void)AffineTransform(m);},"nonfinite matrix refuses");
  m=identity.matrix();m[3]=1;
  refuses([&]{(void)AffineTransform(m);},"projective matrix refuses");
  refuses([&]{(void)a.point({NAN,0,0});},"nonfinite point refuses");
  m=identity.matrix();m[0]=1e300;AffineTransform huge(m);
  refuses([&]{(void)(huge*huge);},"composition overflow refuses");
  refuses([&]{(void)huge.point({1e300,0,0});},"point overflow refuses");
  refuses([&]{(void)huge.float_matrix();},"GPU overflow refuses");
  m=identity.matrix();m[0]=1e-100;
  refuses([&]{(void)AffineTransform(m).float_matrix();},"nonzero GPU underflow refuses");
  const auto packed=a.float_matrix();
  near(AffineTransform(packed).point({1,2,3}),{9,7,-5},"representable float boundary");
  const auto before=a.matrix();
  check(a.matrix()==before,"all arithmetic leaves original authority unchanged");
  for(int k=0;k<20;++k) {
    auto v=identity.matrix();
    v[0]=2+k*.13;v[5]=3+k*.07;v[10]=(k%2?-1.:1.)*(4+k*.11);
    v[1]=(k%3-1)*.13;v[4]=(k%4-2)*.17;v[6]=(k%5-2)*.09;
    v[8]=(k%3-1)*.19;v[9]=(k%4-1)*.11;v[12]=k*.2;
    AffineTransform varied(v);const AffineVector u{.3,-.7,.2},w{-.4,.1,.8};
    near(varied.cofactor_normal(cross(u,w)),cross(varied.direction(u),varied.direction(w)),"diverse cofactors match independent edge cross");
    near(varied.inverse().point(varied.point({.19,-.31,.83})),{.19,-.31,.83},"diverse affine inverse roundtrip");
  }
  std::cout<<checks<<" affine math checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
