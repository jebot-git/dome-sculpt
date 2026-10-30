// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include <span>
namespace dome::detail {
// Scene/model-space placement (not headset/tracking space). Every factor is a
// parent-to-child local transform. Source factors stay separate from native
// edit authority; this value adapter never edits/persists either one.
class AffineInteractionFrame {
public:
  explicit AffineInteractionFrame(const AffineTransform &);
  static AffineInteractionFrame factored(std::span<const AffineTransform>);
  const AffineTransform &world() const noexcept{return world_;}
  const AffineTransform &inverse() const noexcept{return inverse_;}
  AffineOrientation orientation() const noexcept{return orientation_;}
  std::array<double,9> normal_matrix() const noexcept;
  AffineVector normal(AffineVector) const;
  // A world gesture is conjugated into the current parent's local frame. The
  // result may contain shear even when delta is a native similarity gesture.
  AffineTransform apply_world_edit(const AffineTransform &local,
                                  const AffineTransform &delta) const;
private:
  AffineTransform world_,inverse_;
  AffineOrientation orientation_=AffineOrientation::Preserving;
  AffineInteractionFrame()=default;
};
struct AffineLocalRay {
  AffineVector origin,direction; // normalized LOCAL direction
  double maximum_distance=0,local_per_world=1;
  double world_distance(double local_distance) const;
};
// max distance is in scene/model metres, independent of supplied direction
// magnitude. The ray parameter conversion is explicit; there is no guessed
// uniform object scale.
AffineLocalRay affine_local_ray(const AffineInteractionFrame &,AffineVector origin,
                               AffineVector direction,double maximum_distance);
struct AffineBounds {AffineVector minimum,maximum;};
class AffineBrushMetric {
public:
  AffineBrushMetric(AffineInteractionFrame,AffineVector world_from,
                    AffineVector world_to,double world_radius);
  double distance(AffineVector local_point) const;
  bool contains(AffineVector local_point) const;
  AffineBounds local_bounds() const;
private:
  AffineInteractionFrame frame_;
  AffineVector from_,to_;
  double radius_;
};
} // namespace dome::detail
