// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include <span>
namespace dome::detail {
struct ColorProjectionBound {
  std::array<double,4> maximum_error{};
  uint64_t tested_cells=0, intersecting_cells=0;
};
// Bound only the static linear vertex-color field versus its linear RGBA8
// clamp/bilinear projection. This does not claim a bound for resampling the
// complete textured material, its mip selection or its lighting.
inline ColorProjectionBound color_projection_bound(
    const Mesh &mesh,std::span<const std::array<float,4>> colors,
    const Image &projection,uint64_t max_cells=128'000'000,
    const std::function<bool()> &cancelled={}) {
  mesh.validate();projection.validate();
  if(colors.size()!=mesh.positions.size() || mesh.uv.size()!=colors.size() || !max_cells)
    throw std::invalid_argument("Color projection bound requires complete current UV/color authority");
  for(const auto &c:colors)for(float x:c)if(!std::isfinite(x) || x<0 || x>1)
    throw std::invalid_argument("Color projection bound requires finite linear unit RGBA");
  for(auto uv:mesh.uv)for(float x:uv)if(x<0 || x>1)
    throw std::invalid_argument("Color projection atlas must fit the unit square");
  using Point=std::array<double,2>;
  struct Polygon {
    std::array<Point,16> points{};size_t count=0;
    void push(Point p){if(count && points[count-1]==p)return;if(count==points.size())throw std::logic_error("Clipped projection polygon exceeds bounded capacity");points[count++]=p;}
  };
  auto clip=[](const Polygon &polygon,int axis,double edge,bool lower) {
    Polygon out;
    if(!polygon.count)return out;
    auto distance=[&](Point p){return lower?p[axis]-edge:edge-p[axis];};
    Point previous=polygon.points[polygon.count-1];double a=distance(previous);
    for(size_t i=0;i<polygon.count;++i) {auto next=polygon.points[i];
      const double b=distance(next);
      if((a>=0)!=(b>=0)) {
        const double t=a/(a-b);
        out.push({previous[0]+t*(next[0]-previous[0]),previous[1]+t*(next[1]-previous[1])});
      }
      if(b>=0)out.push(next);
      previous=next;a=b;
    }
    if(out.count>1 && out.points[0]==out.points[out.count-1])--out.count;
    return out;
  };
  auto check=[&]{if(cancelled && cancelled())throw std::runtime_error("Color projection quality analysis cancelled");};
  ColorProjectionBound result;check();
  const int width=int(projection.width),height=int(projection.height);
  for(size_t face=0;face<mesh.indices.size()/3;++face) {
    check();std::array<uint32_t,3> ids;std::array<Point,3> triangle;
    for(size_t k=0;k<3;++k){ids[k]=mesh.indices[face*3+k];auto q=mesh.uv[ids[k]];triangle[k]={q[0],q[1]};}
    const double dx1=triangle[1][0]-triangle[0][0],dy1=triangle[1][1]-triangle[0][1];
    const double dx2=triangle[2][0]-triangle[0][0],dy2=triangle[2][1]-triangle[0][1];
    const double det=dx1*dy2-dy1*dx2;
    if(det==0)throw std::invalid_argument("Color projection requires nondegenerate atlas faces");
    // Near-cancelling barycentric solves do not justify a tight floating bound.
    // Retain the universal unit-field bound rather than silently trusting an
    // ill-conditioned calculation or rejecting all tiny well-shaped faces.
    const double scale=std::abs(dx1*dy2)+std::abs(dy1*dx2);
    const bool loose=std::abs(det)<=scale*1e-10;
    if(loose)result.maximum_error.fill(1);

    const int x0=std::max(-1,int(std::floor(std::min({triangle[0][0],triangle[1][0],triangle[2][0]})*width-.5)));
    const int x1=std::min(width-1,int(std::floor(std::max({triangle[0][0],triangle[1][0],triangle[2][0]})*width-.5)));
    const int y0=std::max(-1,int(std::floor(std::min({triangle[0][1],triangle[1][1],triangle[2][1]})*height-.5)));
    const int y1=std::min(height-1,int(std::floor(std::max({triangle[0][1],triangle[1][1],triangle[2][1]})*height-.5)));
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
      if(++result.tested_cells>max_cells)throw std::length_error("Color projection quality work budget exceeded");
      if((result.tested_cells&1023)==0)check();
      Polygon polygon;for(auto p:triangle)polygon.push(p);
      polygon=clip(polygon,0,(x+.5)/width,true);
      polygon=clip(polygon,0,(x+1.5)/width,false);
      polygon=clip(polygon,1,(y+.5)/height,true);
      polygon=clip(polygon,1,(y+1.5)/height,false);
      if(!polygon.count)continue;
      ++result.intersecting_cells;
      if(loose)continue;
      auto difference=[&](Point p,size_t component) {
        const double px=p[0]-triangle[0][0],py=p[1]-triangle[0][1];
        const double w1=(px*dy2-py*dx2)/det,w2=(py*dx1-px*dy1)/det;
        const double authored=colors[ids[0]][component]*(1-w1-w2)+colors[ids[1]][component]*w1+colors[ids[2]][component]*w2;
        const double fx=p[0]*width-.5-x,fy=p[1]*height-.5-y;
        double baked=0;
        for(int k=0;k<4;++k) {
          const auto sx=std::clamp(x+(k&1),0,width-1),sy=std::clamp(y+(k>>1),0,height-1);
          const double weight=((k&1)?fx:1-fx)*((k>>1)?fy:1-fy);
          baked+=projection.rgba[(size_t(sy)*width+sx)*4+component]/255.0*weight;
        }
        return authored-baked;
      };
      // The error is affine minus bilinear. Its Hessian is zero or indefinite,
      // so an interior stationary point cannot be a strict maximum/minimum.
      // On each clipped polygon edge it is quadratic; include its analytic
      // stationary point as well as both endpoints.
      for(size_t component=0;component<4;++component)for(size_t edge=0;edge<polygon.count;++edge) {
        const auto a=polygon.points[edge],b=polygon.points[(edge+1)%polygon.count];
        const Point midpoint{(a[0]+b[0])*.5,(a[1]+b[1])*.5};
        const double f0=difference(a,component),f1=difference(b,component),fm=difference(midpoint,component);
        auto &bound=result.maximum_error[component];bound=std::max({bound,std::abs(f0),std::abs(f1)});
        const double quadratic=2*(f0+f1-2*fm),linear=f1-f0-quadratic;
        if(quadratic!=0) {
          const double t=-linear/(2*quadratic);
          if(t>0 && t<1)bound=std::max(bound,std::abs(difference({a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])},component)));
        }
      }
    }
  }
  // Conservative floating arithmetic allowance; both fields are unit-valued.
  for(auto &bound:result.maximum_error)bound=std::min(1.0,bound+1e-6);
  check();return result;
}
} // namespace dome::detail
