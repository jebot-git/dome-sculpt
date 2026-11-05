// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include "dome/polygon.hpp"
#include <limits>
#include <map>
namespace dome {
// Only placement changes here. IDs, directed corner sides, UVs, hard edges and
// source/color provenance remain actual polygon authority, never re-created.
struct PolygonAffineAccess {
  static void stop(const PolygonLimits &limits) {
    if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Affine polygon edit cancelled");
  }
  static Vec3 point(AffineVector p) {
    Vec3 out{float(p[0]),float(p[1]),float(p[2])};
    if(!finite(out)||(p[0]!=0&&out.x==0)||(p[1]!=0&&out.y==0)||(p[2]!=0&&out.z==0))
      throw std::overflow_error("Affine polygon coordinates exceed finite mesh precision");
    return out;
  }
  static AffineVector vector(Vec3 p) {return {p.x,p.y,p.z};}
  static bool equal(Vec3 a,Vec3 b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
  static AffineTransform linear(const AffineTransform &world) {
    auto matrix=world.matrix();matrix[12]=matrix[13]=matrix[14]=0;
    return AffineTransform(matrix); // Translation is irrelevant to lengths/area.
  }
  static PolygonMesh placed(const PolygonMesh &source,const AffineTransform &frame,
                            const PolygonLimits &limits) {
    source.validate(limits);stop(limits);
    if(source.vertices_.size()>limits.maximum_work)
      throw std::length_error("Affine polygon placement exceeds work budget");
    auto target=source;
    for(auto &v:target.vertices_){stop(limits);v.position=point(frame.point(vector(v.position)));}
    if(frame.orientation()==AffineOrientation::Reversing)
      for(auto &face:target.faces_){stop(limits);std::reverse(face.corners.begin(),face.corners.end());}
    target.validate(limits);return target;
  }
  static PolygonMesh returned(PolygonMesh target,const PolygonMesh &source,
                              const PolygonMesh &placed_source,
                              const AffineTransform &frame,const PolygonLimits &limits) {
    const auto inverse=frame.inverse();std::map<PolygonId,std::pair<Vec3,Vec3>> old;
    for(size_t i=0;i<source.vertices_.size();++i){stop(limits);
      old.emplace(source.vertices_[i].id,std::pair(source.vertices_[i].position,placed_source.vertices_[i].position));}
    for(auto &v:target.vertices_){
      stop(limits);auto found=old.find(v.id);
      if(found!=old.end()&&equal(v.position,found->second.second))v.position=found->second.first;
      else {
        const auto world=v.position;v.position=point(inverse.point(vector(world)));
        const auto replay=frame.point(vector(v.position));
        const auto scale=std::max({1.,std::abs(double(world.x)),std::abs(double(world.y)),std::abs(double(world.z))});
        if(std::hypot(replay[0]-world.x,replay[1]-world.y,replay[2]-world.z)>32*std::numeric_limits<float>::epsilon()*scale)
          throw std::overflow_error("Affine polygon edit cannot preserve its world endpoint precision");
      }
    }
    if(frame.orientation()==AffineOrientation::Reversing)
      for(auto &face:target.faces_){stop(limits);std::reverse(face.corners.begin(),face.corners.end());}
    target.validate(limits);return target;
  }
};
} // namespace dome
