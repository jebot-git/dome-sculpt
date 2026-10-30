// SPDX-License-Identifier: GPL-3.0-or-later
#include "affine-factor.hpp"
#include "affine-matrix.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <lapacke.h>
#include <new>
#include <stdexcept>
#include <unistd.h>
using namespace dome;
using namespace dome::detail;
namespace {
enum class Driver { Real, Nonconverged, Arguments, Orthogonal, Singular, Sweep, Scale, Reconstruction };
Driver driver=Driver::Real;
size_t checks=0,allocations=0;bool armed=false;
void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;}
template<class F>void refuses(F f,const char*m){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,m);}
void verify(const AffineTransform &m) {
  const auto before=m.matrix();auto result=factor_edit_for_gltf(m);
  check(result.sweeps<=30,"actual driver obeys documented sweep bound");
  check(result.column_relative_error<=2e-11 && result.float_column_relative_error<=2e-6,"factor quality reports bounded reconstruction");
  check(gltf_trs_matrix(result.nodes[0].float_matrix()) && gltf_trs_matrix(result.nodes[1].float_matrix()),"each float output node is independently glTF TRS-decomposable");
  check(result.nodes[1].orientation()==AffineOrientation::Preserving,"right factor remains proper rotation");
  check((result.nodes[0]*result.nodes[1]).orientation()==m.orientation(),"reflection carried by signed scale");
  // Independent row/column product, not the production composition helper.
  const auto&a=result.nodes[0].matrix(),&b=result.nodes[1].matrix();
  for(int c=0;c<4;++c)for(int r=0;r<3;++r) {
    long double v=0,normalizer=0;
    for(int k=0;k<4;++k)v+=static_cast<long double>(a[k*4+r])*b[c*4+k];
    for(int k=0;k<3;++k)normalizer+=std::abs(before[c*4+k]);
    check(std::abs(v-before[c*4+r])<=std::max(1e-30L,normalizer*2e-11L),"independent coefficient reconstruction");
  }
  check(m.matrix()==before,"factorization leaves input authority unchanged");
}
}
extern "C" lapack_int __real_LAPACKE_dgesvj_work(int,char,char,char,lapack_int,lapack_int,double*,lapack_int,double*,lapack_int,double*,lapack_int,double*,lapack_int);
extern "C" lapack_int __wrap_LAPACKE_dgesvj_work(int layout,char joba,char jobu,char jobv,lapack_int m,lapack_int n,double*a,lapack_int lda,double*s,lapack_int mv,double*v,lapack_int ldv,double*w,lapack_int count) {
  if(layout!=LAPACK_COL_MAJOR || joba!='G'||jobu!='U'||jobv!='V'||m!=3||n!=3||lda!=3||mv!=3||ldv!=3||count!=6)_exit(65);
  if(driver==Driver::Nonconverged)return 1;
  if(driver==Driver::Arguments)return -8;
  auto status=__real_LAPACKE_dgesvj_work(layout,joba,jobu,jobv,m,n,a,lda,s,mv,v,ldv,w,count);
  if(driver==Driver::Orthogonal)a[0]+=.1;
  if(driver==Driver::Singular)s[2]=0;
  if(driver==Driver::Sweep)w[3]=31;
  if(driver==Driver::Scale)w[0]=NAN;
  if(driver==Driver::Reconstruction)s[0]*=1.1;
  return status;
}
void *operator new(size_t n){if(armed){++allocations;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try {
  static_assert(sizeof(lapack_int)==4,"This target links the pkg-config LP64 lapacke C interface");
  lapack_int major=0,minor=0,patch=0;LAPACKE_ilaver(&major,&minor,&patch);
  AffineTransform identity;verify(identity);
  auto raw=identity.matrix();raw={2,0,0,0, 1,3,0,0, 0,1,-4,0, 5,-2,7,1};
  AffineTransform shear(raw);verify(shear);
  auto diagonal=identity.matrix();diagonal[0]=1e-30;diagonal[10]=-1e30;verify(AffineTransform(diagonal));
  const double c=std::sqrt(.5);auto trs=identity.matrix();
  trs[0]=2*c;trs[1]=2*c;trs[4]=-.5*c;trs[5]=.5*c;trs[10]=-1.7;trs[12]=.3;trs[13]=-.2;
  AffineTransform original(trs);check(gltf_trs_matrix(original.float_matrix()),"original local node is legal TRS");
  check(!gltf_trs_matrix(original.inverse().float_matrix()),"inverse local TRS genuinely needs multiple nodes");
  verify(original.inverse());
  for(int n=0;n<256;++n) {
    const double a=n*.113,b=n*.071;
    auto left=identity.matrix(),right=identity.matrix();
    const double sx=std::exp((n%13-6)*.65),sy=std::exp((n%11-5)*.41),sz=(n%2?-1.:1.)*std::exp((n%17-8)*.27);
    left[0]=std::cos(a)*sx;left[1]=std::sin(a)*sx;left[4]=-std::sin(a)*sy;left[5]=std::cos(a)*sy;left[10]=sz;
    left[12]=n*.01;left[13]=-.03*n;left[14]=.17;
    right[0]=std::cos(b);right[2]=-std::sin(b);right[8]=std::sin(b);right[10]=std::cos(b);
    verify(AffineTransform(left)*AffineTransform(right));
  }
  const auto before=shear.matrix();
  for(auto fault:{Driver::Nonconverged,Driver::Arguments,Driver::Orthogonal,Driver::Singular,Driver::Sweep,Driver::Scale,Driver::Reconstruction}) {
    driver=fault;refuses([&]{(void)factor_edit_for_gltf(shear);},"driver failure never emits partial/unverified glTF factors");
    check(shear.matrix()==before,"driver refusal preserves original edit authority");
  }
  driver=Driver::Real;verify(shear);
  for(int at=1;at<=2;++at){int observed=0;refuses([&]{(void)factor_edit_for_gltf(shear,[&]{return ++observed>=at;});},"before/after driver cancellation refuses atomically");check(observed==at,"factor cancellation sites acknowledged exactly");}
  auto flat=identity.matrix();flat[10]=0;refuses([&]{(void)factor_edit_for_gltf(AffineTransform(flat));},"singular native edit refuses");
  auto tiny=identity.matrix();tiny[0]=1e-100;refuses([&]{(void)factor_edit_for_gltf(AffineTransform(tiny));},"unrepresentable float factor refuses");
  allocations=0;armed=true;
  for(int n=0;n<128;++n){auto f=factor_edit_for_gltf(shear);(void)f;}
  armed=false;check(allocations==0,"fixed workspace valid wrapper path makes no C++ allocations after warmup");
  const auto started=std::chrono::steady_clock::now();
  for(int n=0;n<1000;++n){auto f=factor_edit_for_gltf(shear);(void)f;}
  const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/1000;
  std::cout<<checks<<" affine factor assertions passed; 128 warmed calls made 0 C++ allocations; mean of 1000 calls: "
           <<elapsed<<" us; LAPACK "<<major<<'.'<<minor<<'.'<<patch<<" (export preparation only)\n";return 0;
}catch(const std::exception&e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
