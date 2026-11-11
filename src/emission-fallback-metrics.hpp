// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace dome::detail {
inline constexpr const char *resolved_fallback_units=
  "strength:linear-cd/m2;maximumAbsoluteError:linear-cd/m2;maximumRelativeError:dimensionless";
// A FLOAT source and its nonnegative FLOAT-scale reconstruction are <=F32max.
// Their absolute error is <=F32max. Positive source >=2^-149, hence E/source
// <=F32max/2^-149 (<2^277), safely finite in binary64. This ratio is NOT F32.
inline constexpr double maximum_resolved_fallback_relative_error=
    double(std::numeric_limits<float>::max())/double(std::numeric_limits<float>::denorm_min());
inline double resolved_fallback_radiance(uint8_t encoded,float factor,float strength,bool ignored=false){
  if(!std::isfinite(factor)||factor<0||factor>1||!std::isfinite(strength)||strength<1)
    throw std::invalid_argument("Invalid resolved fallback factor/strength");
  if(ignored)return 0;
  const double x=double(encoded)/255;
  const double linear=x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);
  const double value=linear*double(factor)*double(strength);
  if(!std::isfinite(value)||value<0||value>std::numeric_limits<float>::max())
    throw std::overflow_error("Resolved fallback radiance exceeds FLOAT scale");
  return value;
}
struct ResolvedFallbackMetrics {
  double maximum_absolute_error=0,maximum_relative_error=0;
  size_t erased_nonzero_samples=0;
  void sample(float source,double reconstructed){
    if(!std::isfinite(source)||source<0||!std::isfinite(reconstructed)||reconstructed<0||reconstructed>std::numeric_limits<float>::max())
      throw std::invalid_argument("Invalid resolved fallback sample");
    const double error=std::abs(double(source)-reconstructed);
    if(source==0){
      // Relative error is zero only for exact zero reconstruction. Invented
      // energy at a zero source has no finite ratio; never encode NaN/Infinity
      // or silently call that ratio zero.
      if(reconstructed!=0)throw std::invalid_argument("Nonzero fallback at zero source has undefined relative error");
    }else{
      const double relative=error/double(source);
      if(!std::isfinite(relative)||relative>maximum_resolved_fallback_relative_error)
        throw std::overflow_error("Resolved fallback relative error exceeds its proven binary64 bound");
      maximum_relative_error=std::max(maximum_relative_error,relative);
      if(reconstructed==0)++erased_nonzero_samples;
    }
    maximum_absolute_error=std::max(maximum_absolute_error,error);
  }
};
} // namespace dome::detail
