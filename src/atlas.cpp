// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh.hpp"
#include "atlas-detail.hpp"
#include "uv-overlap.hpp"
#include "xatlas.h"
#include <memory>
namespace dome {
namespace {
// Use precisely the float differences consumed by strict paint rasterization.
double paint_uv_area(const std::array<float,2> &a,const std::array<float,2> &b,
                     const std::array<float,2> &c) {
  return double(b[1]-c[1])*(a[0]-c[0])+double(c[0]-b[0])*(a[1]-c[1]);
}
}
AtlasInfo detail::unwrap_atlas(Mesh &mesh, uint32_t resolution, uint32_t padding,AtlasDiagnostics *diagnostics) {
  mesh.validate();
  if (resolution < 64 || resolution > 8192 || padding > resolution / 8)
    throw std::invalid_argument("Invalid atlas resolution or padding");
  constexpr size_t maximum_vertices=2'000'000, maximum_corners=6'000'000;
  if(mesh.positions.size()>maximum_vertices || mesh.indices.size()>maximum_corners)
    throw std::length_error("Atlas mesh exceeds the bounded vertex/face policy");
  std::unique_ptr<xatlas::Atlas, decltype(&xatlas::Destroy)> atlas(
      xatlas::Create(), xatlas::Destroy);
  if (!atlas)
    throw std::bad_alloc();
  // xatlas rejects triangles using an absolute area epsilon. Authoring units
  // are metres, and valid small sculpt features can fall below that epsilon.
  // Parameterize normalized positions, then keep original metre-space geometry
  // through xref. Do not discard tiny faces and leave holes in the export.
  Vec3 low = mesh.positions.front(), high = low;
  for (auto p : mesh.positions) {
    low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
    high = {std::max(high.x, p.x), std::max(high.y, p.y),
            std::max(high.z, p.z)};
  }
  double minimum_area = INFINITY;
  for (size_t i = 0; i < mesh.indices.size(); i += 3) {
    auto a = mesh.positions[mesh.indices[i]],
         b = mesh.positions[mesh.indices[i + 1]],
         c = mesh.positions[mesh.indices[i + 2]];
    double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z,
           vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
    minimum_area = std::min(minimum_area,std::hypot(uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx)*.5);
  }
  double extent = std::max({double(high.x)-low.x,double(high.y)-low.y,double(high.z)-low.z});
  double scale = std::max(1024. / extent, std::sqrt(.0001 / minimum_area));
  // Keep xatlas's float cross products and their squared lengths finite. A
  // geometry range too wide for this common float frame can still have ignored
  // tiny faces; the independent chart fallback below preserves those faces.
  scale=std::min(scale,1e7/extent);
  std::array<double,3> center{(double(low.x)+high.x)*.5,(double(low.y)+high.y)*.5,(double(low.z)+high.z)*.5};
  std::vector<Vec3> normalized_positions;
  normalized_positions.reserve(mesh.positions.size());
  for (auto p : mesh.positions)
    normalized_positions.push_back({float((double(p.x)-center[0])*scale),float((double(p.y)-center[1])*scale),float((double(p.z)-center[2])*scale)});
  xatlas::MeshDecl input{};
  input.vertexCount = uint32_t(mesh.positions.size());
  input.vertexPositionData = normalized_positions.data();
  input.vertexPositionStride = sizeof(Vec3);
  input.vertexNormalData = mesh.normals.data();
  input.vertexNormalStride = sizeof(Vec3);
  input.indexCount = uint32_t(mesh.indices.size());
  input.indexData = mesh.indices.data();
  input.indexFormat = xatlas::IndexFormat::UInt32;
  auto error = xatlas::AddMesh(atlas.get(), input);
  if (error != xatlas::AddMeshError::Success)
    throw std::runtime_error(std::string("Atlas input: ") +
                             xatlas::StringForEnum(error));
  xatlas::ChartOptions chart{};
  chart.maxIterations = 4;
  xatlas::PackOptions pack{};
  pack.resolution = resolution;
  pack.padding = padding;
  pack.bilinear = true;
  pack.rotateCharts = true;
  pack.bruteForce = true;
  xatlas::Generate(atlas.get(), chart, pack);
  if (atlas->meshCount != 1 || atlas->atlasCount != 1 || !atlas->width ||
      !atlas->height || atlas->width>8192 || atlas->height>8192)
    throw std::runtime_error(
        "The mesh needs multiple atlases; increase the texture resolution");
  const xatlas::Mesh *generated=&atlas->meshes[0];
  auto read_output=[&] {
    Mesh value;
    if(generated->vertexCount>maximum_vertices || generated->indexCount!=mesh.indices.size())
      throw std::length_error("Generated atlas exceeds native vertex limits or changed the source face count");
    value.positions.reserve(generated->vertexCount);value.normals.reserve(generated->vertexCount);value.uv.reserve(generated->vertexCount);
    for(uint32_t i=0;i<generated->vertexCount;++i) {
      const auto &v=generated->vertexArray[i];
      if(v.xref>=mesh.positions.size())throw std::runtime_error("Invalid generated chart mapping");
      value.positions.push_back(mesh.positions[v.xref]);value.normals.push_back(mesh.normals[v.xref]);
      value.uv.push_back({v.uv[0]/atlas->width,v.uv[1]/atlas->height});
    }
    value.indices.assign(generated->indexArray,generated->indexArray+generated->indexCount);
    for(size_t c=0;c<value.indices.size();++c)
      if(value.indices[c]>=generated->vertexCount || generated->vertexArray[value.indices[c]].xref!=mesh.indices[c])
        throw std::runtime_error("Generated atlas changed source face/corner order");
    return value;
  };
  Mesh output=read_output();
  // A physically valid sculpt sliver may be rounded to a line or point by
  // xatlas's float UVs. Never move just one shared vertex (which can fold its
  // neighbours), discard the face, or relax strict paint validation. Promote
  // the affected face to its own padded, integer-texel chart in a reserved band.
  const size_t faces=output.indices.size()/3;
  std::vector<uint8_t> separate(faces);
  size_t separated=0;
  constexpr double safe_area=1e-12; // Margin above strict paint's 1e-14 cutoff.
  auto invalid=[&](size_t face,float u_scale,float u_offset=0) {
    std::array<std::array<float,2>,3> uv;
    for(size_t k=0;k<3;++k) {
      auto id=output.indices[face*3+k];uv[k]=output.uv[id];
      if(generated->vertexArray[id].atlasIndex!=0 || generated->vertexArray[id].chartIndex<0 || !std::isfinite(uv[k][0]) || !std::isfinite(uv[k][1]) ||
         uv[k][0]<0 || uv[k][0]>1 || uv[k][1]<0 || uv[k][1]>1)return true;
      uv[k][0]=u_offset+uv[k][0]*u_scale;
    }
    return std::abs(paint_uv_area(uv[0],uv[1],uv[2]))<safe_area;
  };
  auto separate_overlaps=[&](float scale,float offset) {
    std::vector<std::array<float,2>> transformed;
    std::span<const std::array<float,2>> coordinates=output.uv;
    if(scale!=1||offset!=0){transformed=output.uv;for(auto &uv:transformed)uv[0]=offset+uv[0]*scale;coordinates=transformed;}
    auto area=[&](uint32_t f){auto a=coordinates[output.indices[f*3]],b=coordinates[output.indices[f*3+1]],c=coordinates[output.indices[f*3+2]];
      return std::abs((double(b[0])-a[0])*(double(c[1])-a[1])-(double(b[1])-a[1])*(double(c[0])-a[0]));};
    detail::visit_uv_overlaps(output,coordinates,[&](uint32_t f){return separate[f]!=0;},[&](uint32_t a,uint32_t b){
      // Promote one colliding face at a time, preferring the smallest footprint.
      // No vertex is moved in its old chart, so adjacent faces cannot fold.
      auto chosen=area(a)<area(b)?a:b;if(!separate[chosen]){separate[chosen]=1;++separated;}return true;
    });
  };
  for(size_t face=0;face<faces;++face)if(invalid(face,1)){separate[face]=1;++separated;}
  separate_overlaps(1,0);
  if(separated) {
    const size_t initial=separated;
    // The later affine U compression must not shrink existing chart padding.
    // Repack only on fallback, with at least twice the requested padding and
    // bilinear margin; the reserved-band bound below guarantees U scale >= .5.
    pack.padding=2*(padding+1);xatlas::PackCharts(atlas.get(),pack);
    if(atlas->meshCount!=1 || atlas->atlasCount!=1 || !atlas->width || !atlas->height || atlas->width>8192 || atlas->height>8192)
      throw std::length_error("Atlas at requested resolution "+std::to_string(resolution)+" cannot preserve chart padding while reserving "+
        std::to_string(initial)+" independent padded small-face charts; increase resolution or reduce padding");
    // PackCharts can replace output arrays. Do not retain the previous mesh or
    // vertex references across the repack.
    generated=&atlas->meshes[0];output=read_output();std::fill(separate.begin(),separate.end(),0);separated=0;
    for(size_t face=0;face<faces;++face)if(invalid(face,1)){separate[face]=1;++separated;}
  separate_overlaps(1,0);
  }
  uint32_t band=0, columns=0;
  const uint32_t gutter=padding+1, side=4, cell=side+2*gutter;
  const uint32_t rows=atlas->height/cell;
  uint32_t chart_count=atlas->chartCount;
  float u_scale=1,u_offset=0;
  if(separated) {
    auto capacity=[&] {
      return std::length_error("Atlas at requested resolution "+std::to_string(resolution)+
        " ("+std::to_string(atlas->width)+"x"+std::to_string(atlas->height)+") cannot fit "+
        std::to_string(separated)+" independent padded small-face charts; increase resolution or reduce padding");
    };
    // Columns double only when promotions exceed the current band capacity.
    // At most 13 passes are possible for the supported 8192-pixel bound.
    bool stable=false;
    for(unsigned pass=0;pass<14;++pass) {
      if(!rows)throw capacity();
      const size_t needed=(separated+rows-1)/rows,half_width=atlas->width/2;
      if(half_width<=2*gutter)throw capacity();
      const size_t maximum_columns=(half_width-2*gutter)/cell;
      if(needed>maximum_columns)throw capacity();
      columns=1;
      // Retain bounded geometric growth, but use the final non-power-of-two
      // capacity rather than refusing a valid fit (e.g. 9 legal columns).
      while(columns<needed)columns=uint32_t(std::min(size_t(columns)*2,maximum_columns));
      band=columns*cell;u_offset=float(double(gutter)/atlas->width);u_scale=float(double(atlas->width-band-2*gutter)/atlas->width);
      const size_t previous=separated;
      for(size_t face=0;face<faces;++face)if(!separate[face] && invalid(face,u_scale,u_offset)){separate[face]=1;++separated;}
      separate_overlaps(u_scale,u_offset);
      if(separated==previous || separated<=size_t(columns)*rows){stable=true;break;}
    }
    if(!stable)throw std::length_error("Atlas small-face promotion exceeded its bounded packing passes");
    Mesh repaired;repaired.indices.resize(output.indices.size());
    const size_t predicted=size_t(output.positions.size())+3*separated;
    const size_t reserve=std::min(predicted,maximum_vertices);
    repaired.positions.reserve(reserve);repaired.normals.reserve(reserve);repaired.uv.reserve(reserve);
    std::vector<uint32_t> remap(output.positions.size(),UINT32_MAX);
    std::vector<uint8_t> active_charts(generated->chartCount);
    size_t chart=0;
    for(size_t face=0;face<faces;++face)for(size_t k=0;k<3;++k) {
      const uint32_t old=output.indices[face*3+k];uint32_t id;
      if(separate[face]) {
        if(repaired.positions.size()>=maximum_vertices)throw std::length_error("Atlas small-face charts exceed the native vertex budget");
        id=uint32_t(repaired.positions.size());
        const uint32_t x=atlas->width-band+uint32_t(chart%columns)*cell+gutter,
                       y=uint32_t(chart/columns)*cell+gutter;
        repaired.positions.push_back(output.positions[old]);repaired.normals.push_back(output.normals[old]);
        repaired.uv.push_back({float(double(x+(k==1?side:0))/atlas->width),float(double(y+(k==2?side:0))/atlas->height)});
        if(k==2)++chart;
      } else {
        id=remap[old];
        if(id==UINT32_MAX) {
          if(repaired.positions.size()>=maximum_vertices)throw std::length_error("Atlas small-face charts exceed the native vertex budget");
          id=remap[old]=uint32_t(repaired.positions.size());repaired.positions.push_back(output.positions[old]);
          repaired.normals.push_back(output.normals[old]);auto uv=output.uv[old];uv[0]=u_offset+uv[0]*u_scale;repaired.uv.push_back(uv);
        }
        const auto index=generated->vertexArray[old].chartIndex;
        if(index<0 || size_t(index)>=active_charts.size())throw std::runtime_error("Invalid surviving atlas chart identity");
        active_charts[size_t(index)]=1;
      }
      repaired.indices[face*3+k]=id;
    }
    output=std::move(repaired);
    chart_count=uint32_t(std::count(active_charts.begin(),active_charts.end(),uint8_t(1))+separated);
  }
  double occupied=0;
  for(size_t c=0;c<output.indices.size();c+=3) {
    auto a=output.uv[output.indices[c]],b=output.uv[output.indices[c+1]],d=output.uv[output.indices[c+2]];
    const double area=std::abs(paint_uv_area(a,b,d));
    if(!std::isfinite(area) || area<1e-14)throw std::runtime_error("Atlas produced a nonpaintable UV face after bounded repair");
    occupied+=area*.5;
  }
  output.validate();
  AtlasInfo info{atlas->width, atlas->height, chart_count,
                 separated?float(std::clamp(occupied,0.,1.)):atlas->utilization[0]};
  AtlasDiagnostics report;
  if(diagnostics) {
    report.internal_padding=pack.padding;report.band_width=band;report.u_scale=u_scale;report.u_offset=u_offset;
    report.repaired_faces=separate;report.face_charts.resize(faces);uint32_t next=generated->chartCount;
    for(size_t face=0;face<faces;++face)report.face_charts[face]=separate[face]?next++:
      uint32_t(generated->vertexArray[generated->indexArray[face*3]].chartIndex);
  }
  mesh = std::move(output);
  if(diagnostics)*diagnostics=std::move(report);
  return info;
}
AtlasInfo unwrap(Mesh &mesh,uint32_t resolution,uint32_t padding) {
  return detail::unwrap_atlas(mesh,resolution,padding,nullptr);
}
} // namespace dome
