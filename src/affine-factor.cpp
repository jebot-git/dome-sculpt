// SPDX-License-Identifier: GPL-3.0-or-later
#include "affine-factor.hpp"
#include "affine-matrix.hpp"
#include <algorithm>
#include <cmath>
#include <lapacke.h>
#include <stdexcept>
namespace dome::detail {
namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Affine export factorization cancelled");}
AffineTransform rotation(const std::array<double,9> &m,bool transpose=false) {
  std::array<double,16> out{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  for(size_t c=0;c<3;++c)for(size_t r=0;r<3;++r)out[c*4+r]=m[transpose?r*3+c:c*3+r];
  return AffineTransform(out);
}
double orthogonality(const std::array<double,9> &m) {
  long double error=0;
  for(size_t p=0;p<3;++p)for(size_t q=0;q<3;++q) {
    long double value=0;for(size_t r=0;r<3;++r)value+=static_cast<long double>(m[p*3+r])*m[q*3+r];
    error=std::max(error,std::abs(value-(p==q?1.L:0.L)));
  }
  return double(error);
}
double reconstruction(const AffineTransform &target,const AffineTransform &value) {
  const auto&a=target.matrix(),&b=value.matrix();long double error=0;
  for(size_t c=0;c<4;++c) {
    long double delta[3],original[3];
    for(size_t r=0;r<3;++r){delta[r]=static_cast<long double>(a[c*4+r])-b[c*4+r];original[r]=a[c*4+r];}
    auto norm=std::hypot(original[0],original[1],original[2]);
    const auto difference=std::hypot(delta[0],delta[1],delta[2]);
    if(norm==0){if(difference!=0)return INFINITY;continue;}
    error=std::max(error,difference/norm);
  }
  return double(error);
}
}
AffineGltfFactors factor_edit_for_gltf(const AffineTransform &edit,
                                       const std::function<bool()> &cancelled) {
  stop(cancelled);
  std::array<double,9> left{},right{};std::array<double,3> sigma{};
  std::array<double,6> work{};const auto &input=edit.matrix();
  for(size_t c=0;c<3;++c)for(size_t r=0;r<3;++r)left[c*3+r]=input[c*4+r];
  // COL_MAJOR's middle-level C wrapper uses the supplied workspace directly;
  // unlike its row-major branch, it allocates no transpose buffers.
  const lapack_int status=LAPACKE_dgesvj_work(LAPACK_COL_MAJOR,'G','U','V',3,3,
      left.data(),3,sigma.data(),3,right.data(),3,work.data(),6);
  stop(cancelled);
  if(status>0)throw std::runtime_error("Affine export factorization did not converge within LAPACK's30-sweep bound");
  if(status<0)throw std::runtime_error("Affine export factorization rejected LAPACKE arguments/status");
  for(double x:work)if(!std::isfinite(x))throw std::runtime_error("Affine export factorization returned nonfinite status metadata");
  if(work[0]<=0 || work[1]!=3 || work[3]<0 || work[3]>30 || std::floor(work[3])!=work[3])
    throw std::runtime_error("Affine export factorization is singular, underflowed or outside its sweep contract");
  for(double &x:sigma) {
    const long double scaled=static_cast<long double>(x)*work[0];
    x=double(scaled);
    if(!std::isfinite(x) || x<=0)throw std::runtime_error("Affine export scale is singular or unrepresentable");
  }
  AffineGltfFactors result;result.sweeps=unsigned(work[3]);
  result.orthogonality_error=std::max(orthogonality(left),orthogonality(right));
  if(!std::isfinite(result.orthogonality_error) || result.orthogonality_error>2e-12)
    throw std::runtime_error("Affine export rotations failed independent orthogonality verification");
  for(auto *basis:{&left,&right}) {
    const auto sign=rotation(*basis).orientation();
    if(sign==AffineOrientation::Singular)throw std::runtime_error("Affine export rotation is singular");
    if(sign==AffineOrientation::Reversing) {
      for(size_t r=0;r<3;++r)(*basis)[6+r]=-(*basis)[6+r];
      sigma[2]=-sigma[2];
    }
  }
  auto first=rotation(left).matrix();
  for(size_t c=0;c<3;++c)for(size_t r=0;r<3;++r) {
    const long double value=static_cast<long double>(first[c*4+r])*sigma[c];
    first[c*4+r]=double(value);
    if(!std::isfinite(first[c*4+r]) || (value!=0 && first[c*4+r]==0))
      throw std::runtime_error("Affine export factor exceeds finite matrix precision");
  }
  for(size_t r=0;r<3;++r)first[12+r]=input[12+r];
  result.nodes={AffineTransform(first),rotation(right,true)};
  const auto product=result.nodes[0]*result.nodes[1];
  result.column_relative_error=reconstruction(edit,product);
  if(!std::isfinite(result.column_relative_error) || result.column_relative_error>2e-11)
    throw std::runtime_error("Affine export factors failed independent column reconstruction verification");
  const auto float_first=result.nodes[0].float_matrix(),float_second=result.nodes[1].float_matrix();
  if(!gltf_trs_matrix(float_first) || !gltf_trs_matrix(float_second))
    throw std::runtime_error("Affine export factor is not a representable glTF TRS node");
  result.float_column_relative_error=reconstruction(edit,AffineTransform(float_first)*AffineTransform(float_second));
  if(!std::isfinite(result.float_column_relative_error) || result.float_column_relative_error>2e-6)
    throw std::runtime_error("Affine export factors exceed the float consumer reconstruction bound");
  if(product.orientation()!=edit.orientation())
    throw std::runtime_error("Affine export factors changed reflection orientation");
  return result;
}
} // namespace dome::detail
