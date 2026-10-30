// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <optional>
namespace dome::detail {
// Private mathematical foundation, not a persisted material/skin convention.
// The caller selects the proven vertex/fragment policy and supplies transported
// directions; this helper never infers a Jacobian, UV basis or pose authority.
enum class NormalVertexBitangent { RawCross, UnitCross };
enum class NormalFragmentBasis { RawColumns, UnitColumns, UnitCross };
struct NormalBakeVertex {
  Vec3 n, t, b;
  float w = 1;
};
struct NormalBakeBasis { Vec3 t, b, n; };
struct NormalBakeEncoding {
  std::array<uint8_t,4> rgba{};
  Vec3 tangent;
  double relative_determinant = 0, quantized_angle_radians = 0;
};
namespace normal_bake_math {
using D = std::array<double,3>;
inline D vec(Vec3 v) { return {v.x,v.y,v.z}; }
inline Vec3 vec(D v) { return {float(v[0]),float(v[1]),float(v[2])}; }
inline double dot(D a,D b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline D cross(D a,D b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
inline D unit(D v) {
  for(double x:v)if(!std::isfinite(x))throw std::invalid_argument("Nonfinite normal encoding direction");
  const double m=std::max({std::abs(v[0]),std::abs(v[1]),std::abs(v[2])});
  if(m<1e-30)throw std::invalid_argument("Zero normal encoding direction");
  for(auto &x:v)x/=m;
  const double length=std::sqrt(dot(v,v));for(auto &x:v)x/=length;return v;
}
inline Vec3 unit(Vec3 v) { return vec(unit(vec(v))); }
inline D apply(const NormalBakeBasis &basis,D q) {
  const auto t=vec(basis.t),b=vec(basis.b),n=vec(basis.n);D out{};
  for(size_t k=0;k<3;++k)out[k]=t[k]*q[0]+b[k]*q[1]+n[k]*q[2];
  return out;
}
inline void finite_basis(const NormalBakeBasis &basis) {
  if(!finite(basis.t)||!finite(basis.b)||!finite(basis.n))
    throw std::invalid_argument("Nonfinite normal encoding basis");
}
} // namespace normal_bake_math
inline NormalBakeVertex normal_bake_vertex(Vec3 transported_n,
    Vec3 transported_t,float w,NormalVertexBitangent policy) {
  if(!std::isfinite(w)||std::abs(std::abs(w)-1)>1e-6f)
    throw std::invalid_argument("Normal encoding requires explicit tangent handedness");
  auto n=normal_bake_math::unit(transported_n),t=normal_bake_math::unit(transported_t);
  auto b=cross(n,t)*w;
  if(policy==NormalVertexBitangent::UnitCross)b=normal_bake_math::unit(b);
  else if(policy!=NormalVertexBitangent::RawCross)throw std::invalid_argument("Unknown vertex normal evaluation");
  return {n,t,b,w};
}
inline NormalBakeBasis normal_bake_basis(const std::array<NormalBakeVertex,3> &v,
    std::array<double,3> bary,NormalFragmentBasis policy) {
  double total=0;for(auto x:bary){if(!std::isfinite(x)||x<-.00001||x>1.00001)throw std::invalid_argument("Invalid normal encoding barycentric coordinates");total+=x;}
  if(std::abs(total-1)>1e-6)throw std::invalid_argument("Normal encoding barycentrics must sum to one");
  normal_bake_math::D n{},t{},b{};
  for(size_t i=0;i<3;++i){if(v[i].w!=v[0].w)throw std::invalid_argument("Normal encoding handedness changes inside a triangle");
    if(!finite(v[i].n)||!finite(v[i].t)||!finite(v[i].b))throw std::invalid_argument("Nonfinite vertex normal basis");
    for(size_t k=0;k<3;++k){n[k]+=normal_bake_math::vec(v[i].n)[k]*bary[i];t[k]+=normal_bake_math::vec(v[i].t)[k]*bary[i];b[k]+=normal_bake_math::vec(v[i].b)[k]*bary[i];}}
  if(policy==NormalFragmentBasis::UnitColumns){n=normal_bake_math::unit(n);t=normal_bake_math::unit(t);b=normal_bake_math::unit(b);}
  else if(policy==NormalFragmentBasis::UnitCross){n=normal_bake_math::unit(n);t=normal_bake_math::unit(t);b=normal_bake_math::unit(normal_bake_math::cross(n,t));for(auto &x:b)x*=v[0].w;}
  else if(policy!=NormalFragmentBasis::RawColumns)throw std::invalid_argument("Unknown fragment normal evaluation");
  return {normal_bake_math::vec(t),normal_bake_math::vec(b),normal_bake_math::vec(n)};
}
inline Vec3 normal_bake_evaluate(const NormalBakeBasis &basis,
    std::array<uint8_t,4> rgba,double scale=1) {
  normal_bake_math::finite_basis(basis);
  if(!std::isfinite(scale))throw std::invalid_argument("Nonfinite normal scale");
  normal_bake_math::D q{(double(rgba[0])/255*2-1)*scale,(double(rgba[1])/255*2-1)*scale,double(rgba[2])/255*2-1};
  return normal_bake_math::vec(normal_bake_math::unit(normal_bake_math::apply(basis,q)));
}
inline std::optional<NormalBakeEncoding> encode_normal_bake(
    const NormalBakeBasis &basis,Vec3 desired,double scale=1,
    double minimum_relative_determinant=1e-6) {
  using namespace normal_bake_math;
  finite_basis(basis);
  if(!std::isfinite(scale)||!std::isfinite(minimum_relative_determinant)||
     minimum_relative_determinant<=0||minimum_relative_determinant>1)
    throw std::invalid_argument("Invalid normal encoding limits");
  const auto t=vec(basis.t),b=vec(basis.b),n=vec(basis.n),d=unit(vec(desired));
  const double determinant=dot(t,cross(b,n));
  const double lengths=std::sqrt(dot(t,t))*std::sqrt(dot(b,b))*std::sqrt(dot(n,n));
  const double relative=lengths?std::abs(determinant)/lengths:0;
  if(!std::isfinite(relative)||relative<minimum_relative_determinant)return {};
  D q{dot(d,cross(b,n))/determinant,dot(t,cross(d,n))/determinant,dot(t,cross(b,d))/determinant};
  q=unit(q);const auto tangent=q;
  if(scale==0){if(std::hypot(q[0],q[1])>1e-10)return {};q={0,0,q[2]};}
  else {q[0]/=scale;q[1]/=scale;q=unit(q);}
  NormalBakeEncoding result;result.tangent=vec(tangent);result.relative_determinant=relative;
  for(size_t k=0;k<3;++k)result.rgba[k]=uint8_t(std::lround(std::clamp((q[k]+1)*127.5,0.,255.)));
  result.rgba[3]=255;
  const auto reconstructed=vec(normal_bake_evaluate(basis,result.rgba,scale));
  const auto error=cross(d,reconstructed);result.quantized_angle_radians=std::atan2(std::sqrt(dot(error,error)),std::clamp(dot(d,reconstructed),-1.,1.));
  return result;
}
} // namespace dome::detail
