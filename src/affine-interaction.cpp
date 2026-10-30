// SPDX-License-Identifier: GPL-3.0-or-later
#include "affine-interaction.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace dome::detail {
namespace {
double checked(long double value) {
  const auto result=static_cast<double>(value);
  if(!std::isfinite(value) || !std::isfinite(result) || (value!=0 && result==0))
    throw std::overflow_error("Affine interaction arithmetic exceeds finite precision range");
  return result;
}
AffineVector subtract(AffineVector a,AffineVector b) {
  AffineVector out;for(size_t i=0;i<3;++i)out[i]=checked(static_cast<long double>(a[i])-b[i]);return out;
}
double norm(AffineVector v) {
  const double value=std::hypot(v[0],v[1],v[2]);
  if(!std::isfinite(value))throw std::invalid_argument("Affine interaction vector length is nonfinite");
  return value;
}
}
AffineInteractionFrame::AffineInteractionFrame(const AffineTransform &world)
    :world_(world),inverse_(world.inverse()),orientation_(world.orientation()){}
AffineInteractionFrame AffineInteractionFrame::factored(std::span<const AffineTransform> factors) {
  if(factors.size()>1024)throw std::length_error("Affine interaction factor budget exceeded");
  AffineInteractionFrame out;int sign=1;
  for(const auto &factor:factors) {
    out.world_=out.world_*factor;
    out.inverse_=factor.inverse()*out.inverse_;
    sign*=int(factor.orientation());
  }
  out.orientation_=static_cast<AffineOrientation>(sign);return out;
}
std::array<double,9> AffineInteractionFrame::normal_matrix() const noexcept {
  std::array<double,9> out;const auto&m=inverse_.matrix();
  for(size_t c=0;c<3;++c)for(size_t r=0;r<3;++r)out[c*3+r]=m[r*4+c];
  return out;
}
AffineVector AffineInteractionFrame::normal(AffineVector v) const {
  const auto matrix=normal_matrix();AffineVector out{};
  for(size_t r=0;r<3;++r) {
    long double value=0;for(size_t c=0;c<3;++c)value+=static_cast<long double>(matrix[c*3+r])*v[c];
    out[r]=checked(value);
  }
  return out;
}
AffineTransform AffineInteractionFrame::apply_world_edit(const AffineTransform &local,
                                                         const AffineTransform &delta) const {
  return inverse_*delta*world_*local;
}
double AffineLocalRay::world_distance(double local) const {
  if(!std::isfinite(local) || local<0 || !std::isfinite(local_per_world) || local_per_world<=0)
    throw std::invalid_argument("Invalid affine ray hit distance");
  return checked(static_cast<long double>(local)/local_per_world);
}
AffineLocalRay affine_local_ray(const AffineInteractionFrame &frame,AffineVector origin,
                                AffineVector direction,double maximum) {
  if(!std::isfinite(maximum) || maximum<0)throw std::invalid_argument("Invalid affine world ray distance");
  const auto length=norm(direction);
  if(length==0)throw std::invalid_argument("Affine world ray direction must be nonzero");
  for(double&v:direction)v/=length;
  auto local_direction=frame.inverse().direction(direction);
  const auto scale=norm(local_direction);
  if(scale==0)throw std::invalid_argument("Affine local ray direction collapsed");
  for(double&v:local_direction)v/=scale;
  return {frame.inverse().point(origin),local_direction,
          checked(static_cast<long double>(maximum)*scale),scale};
}
AffineBrushMetric::AffineBrushMetric(AffineInteractionFrame frame,AffineVector from,
                                    AffineVector to,double radius)
    :frame_(std::move(frame)),from_(frame_.inverse().point(from)),
      to_(frame_.inverse().point(to)),radius_(radius) {
  if(!std::isfinite(radius) || radius<=0)throw std::invalid_argument("Invalid affine world brush radius");
}
double AffineBrushMetric::distance(AffineVector point) const {
  // Factored L avoids explicitly squaring huge/small scales into an
  // unrepresentable G=L^T*L matrix. Projection still uses the full world metric.
  auto q=frame_.world().direction(subtract(point,from_));
  auto d=frame_.world().direction(subtract(to_,from_));
  const double length=norm(d);
  if(length==0)return norm(q);
  for(double&v:d)v/=length;
  long double along=0;for(size_t i=0;i<3;++i)along+=static_cast<long double>(q[i])*d[i];
  along=std::clamp(along,0.L,static_cast<long double>(length));
  for(size_t i=0;i<3;++i)q[i]=checked(static_cast<long double>(q[i])-along*d[i]);
  return norm(q);
}
bool AffineBrushMetric::contains(AffineVector p) const{return distance(p)<=radius_;}
AffineBounds AffineBrushMetric::local_bounds() const {
  AffineBounds out;const auto&m=frame_.inverse().matrix();
  for(size_t r=0;r<3;++r) {
    const auto magnitude=std::hypot(static_cast<long double>(m[r]),
                                    static_cast<long double>(m[4+r]),
                                    static_cast<long double>(m[8+r]));
    const auto extent=std::nextafter(checked(static_cast<long double>(radius_)*magnitude),
                                      std::numeric_limits<double>::infinity());
    out.minimum[r]=std::nextafter(checked(static_cast<long double>(std::min(from_[r],to_[r]))-extent),
                                -std::numeric_limits<double>::infinity());
    out.maximum[r]=std::nextafter(checked(static_cast<long double>(std::max(from_[r],to_[r]))+extent),
                                std::numeric_limits<double>::infinity());
    if(!std::isfinite(out.minimum[r]) || !std::isfinite(out.maximum[r]))
      throw std::overflow_error("Affine brush support exceeds finite coordinate range");
  }
  return out;
}
} // namespace dome::detail
