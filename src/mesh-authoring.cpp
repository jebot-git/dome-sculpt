// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-authoring.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/remesh.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-conversion.hpp"
#include "text-utf8.hpp"
#include <cmath>
namespace dome {
size_t condition_sculpt_normals(Mesh &mesh,const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Sculpt normal preparation cancelled");};
  stop();mesh.validate();
  if(!mesh.uv.empty()||!mesh.uv_pins.empty()||!mesh.uv_seams.empty())
    throw std::logic_error("Sculpt normals must be prepared before authored UVs and painting");
  if(mesh.positions.size()>2'000'000||mesh.indices.size()>6'000'000)
    throw std::length_error("Sculpt normal preparation exceeds the surface budget");
  std::vector<std::pair<size_t,Vec3>> repairs;
  for(size_t face=0;face<mesh.indices.size();face+=3) {
    if((face&1023)==0)stop();
    auto a=mesh.positions[mesh.indices[face]],b=mesh.positions[mesh.indices[face+1]],c=mesh.positions[mesh.indices[face+2]];
    double x=double(b.x)-a.x,y=double(b.y)-a.y,z=double(b.z)-a.z;
    double u=double(c.x)-a.x,v=double(c.y)-a.y,w=double(c.z)-a.z;
    double nx=y*w-z*v,ny=z*u-x*w,nz=x*v-y*u,length=std::hypot(nx,ny,nz);
    Vec3 normal{float(nx/length),float(ny/length),float(nz/length)};
    bool unsafe=false;
    for(int k=0;k<3;++k) {
      auto n=mesh.normals[mesh.indices[face+k]];
      unsafe|=dome::length(n)<1e-8f || dot(normalized(n),normal)<1e-4f;
    }
    if(unsafe) {
      if(repairs.size()>=(2'000'000-mesh.positions.size())/3)
        throw std::length_error("Sculpt normal corner splits exceed the surface budget");
      repairs.emplace_back(face,normal);
    }
  }
  if(repairs.empty())return 0;
  Mesh prepared=mesh;prepared.positions.reserve(mesh.positions.size()+repairs.size()*3);
  prepared.normals.reserve(mesh.normals.size()+repairs.size()*3);
  for(auto [face,normal]:repairs) {
    stop();for(int k=0;k<3;++k) {
      auto old=mesh.indices[face+k];prepared.indices[face+k]=uint32_t(prepared.positions.size());
      prepared.positions.push_back(mesh.positions[old]);prepared.normals.push_back(normal);
    }
  }
  stop();mesh=std::move(prepared);return repairs.size();
}

uint64_t create_voxel_sculpt(Scene &scene, float world_voxel_mm,
                             const std::function<bool()> &cancelled) {
  if (!std::isfinite(world_voxel_mm) || world_voxel_mm < .01f ||
      world_voxel_mm > 1000)
    throw std::invalid_argument("Voxel size must be between 0.01 and 1000 mm");
  scene.validate();
  auto source = scene.active();
  auto placement = scene.world_transform(source.id);
  if (!source.visible || source.locked)
    throw std::logic_error("Show and unlock the source before voxel remeshing");
  if (scene.objects().size() >= 256)
    throw std::length_error("The scene object limit is reached");
  size_t used = 0;
  for (auto &object : scene.objects())
    used += object.volume->stored_samples();
  if (used >= 4'000'000)
    throw std::length_error("No sculpt sample budget remains for a new object");
  Mesh mesh;
  if (source.mesh_active) {
    if (!source.painted)
      throw std::logic_error("The source has no prepared mesh");
    mesh = source.deformation ? source.deformation->preview(source.painted->mesh())
                              : source.painted->mesh();
  } else
    mesh = surface_mesh(*source.volume, cancelled);
  if (mesh.indices.empty())
    throw std::runtime_error("The source has no surface to remesh");
  VoxelizeOptions options;
  options.voxel_size = world_voxel_mm * .001f / placement.scale;
  options.max_grid_points = 4'000'000 - used;
  options.cancelled = cancelled;
  auto volume = mesh_to_volume(mesh, options);
  if (std::none_of(volume.voxels().begin(), volume.voxels().end(),
                   [](const auto &sample) { return sample.second < 0; }))
    throw std::runtime_error("Voxel size is too coarse for this shape; choose a finer size");
  if (cancelled && cancelled())
    throw std::runtime_error("Voxel remeshing cancelled");
  auto name = source.name.size() <= 250 ? source.name + " voxel" : "Voxel sculpt";
  auto id = scene.add(std::move(name), std::move(volume), placement);
  scene.object(id).material = source.material;
  scene.object(source.id).visible = false;
  return id;
}
uint64_t create_triangle_remesh(Scene &scene,float world_edge_mm,
                               const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Triangle remeshing cancelled");};
  stop();
  if(!std::isfinite(world_edge_mm)||world_edge_mm<.01f||world_edge_mm>1000)
    throw std::invalid_argument("Edge length must be between 0.01 and 1000 mm");
  scene.validate();
  const auto source=scene.active();
  if(!source.visible||source.locked)
    throw std::logic_error("Show and unlock the source before remeshing");
  if(scene.objects().size()>=256)throw std::length_error("The scene object limit is reached");
  Mesh mesh;
  if(source.mesh_active){
    if(!source.painted)throw std::logic_error("The source has no prepared mesh");
    mesh=source.deformation?source.deformation->preview(source.painted->mesh()):source.painted->mesh();
  }else mesh=surface_mesh(*source.volume,cancelled);
  if(mesh.indices.empty())throw std::runtime_error("The source has no surface to remesh");
  RemeshOptions options;options.target_length=world_edge_mm*.001f;
  options.maximum_projection_distance=options.target_length*.6f;options.cancelled=cancelled;
  if(mesh.positions.size()>options.max_vertices||mesh.indices.size()/3>options.max_triangles)
    throw std::length_error("Remesh input exceeds its vertex/triangle budget");
  // Projection and edge lengths use the full affine world frame. No TRS fit or
  // uniform-scale approximation changes a sheared/reflected source shape.
  const auto world=scene.world_matrix(source.id);
  auto point=[](AffineVector p){Vec3 out{float(p[0]),float(p[1]),float(p[2])};
    if(!std::isfinite(out.x)||!std::isfinite(out.y)||!std::isfinite(out.z)||
       (p[0]!=0&&out.x==0)||(p[1]!=0&&out.y==0)||(p[2]!=0&&out.z==0))
      throw std::invalid_argument("Remesh world coordinates exceed mesh precision");
    return out;};
  for(auto &p:mesh.positions){stop();p=point(world.point({p.x,p.y,p.z}));}
  for(auto &n:mesh.normals){stop();const auto v=world.normal({n.x,n.y,n.z});
    const auto length=std::hypot(v[0],v[1],v[2]);
    if(!(length>0))throw std::invalid_argument("Remesh source normal is degenerate");
    n={float(v[0]/length),float(v[1]/length),float(v[2]/length)};
  }
  if(world.orientation()==AffineOrientation::Reversing)
    for(size_t i=0;i<mesh.indices.size();i+=3){if((i&1023)==0)stop();std::swap(mesh.indices[i+1],mesh.indices[i+2]);
      if(!mesh.uv_seams.empty())std::swap(mesh.uv_seams[i],mesh.uv_seams[i+2]);}
  auto result=remesh_triangles(mesh,options);stop();
  // Keep the source's real placement on the new object. Restore intentionally
  // preserves current placement; a world-baked identity object would restore
  // the original local REST at the wrong location. Geometry is solved in world
  // units, then returned to that exact original local frame without a TRS fit.
  const auto inverse=world.inverse();
  for(auto &p:result.mesh.positions){stop();const auto reference=p;
    p=point(inverse.point({p.x,p.y,p.z}));const auto roundtrip=world.point({p.x,p.y,p.z});
    if(std::hypot(roundtrip[0]-reference.x,roundtrip[1]-reference.y,roundtrip[2]-reference.z)>options.target_length/128.)
      throw std::invalid_argument("Remesh precision is insufficient at this placement; choose a coarser edge length");
  }
  for(auto &n:result.mesh.normals){stop();const auto v=inverse.normal({n.x,n.y,n.z});const auto length=std::hypot(v[0],v[1],v[2]);
    if(!(length>0))throw std::invalid_argument("Remesh result normal is degenerate");n={float(v[0]/length),float(v[1]/length),float(v[2]/length)};
  }
  if(world.orientation()==AffineOrientation::Reversing)
    for(size_t i=0;i<result.mesh.indices.size();i+=3){if((i&1023)==0)stop();std::swap(result.mesh.indices[i+1],result.mesh.indices[i+2]);
      if(!result.mesh.uv_seams.empty())std::swap(result.mesh.uv_seams[i],result.mesh.uv_seams[i+2]);}
  PolygonLimits limits;limits.cancelled=cancelled;
  auto topology=prepare_unpainted_polygon_atlas(PolygonMesh::from_triangles(result.mesh,limits),512,cancelled);
  auto authority=std::make_shared<PolygonAuthority>();authority->topology=std::move(topology);
  authority->origin=capture_polygon_origin(scene,source.id);
  auto derivative=authority->topology.triangulate(limits).mesh;
  PaintSurfaceOptions paint_options;paint_options.cancelled=cancelled;
  auto paint=PaintedMesh::from_mesh(std::move(derivative),512,paint_options);
  // Construct every heavy value before mutating even a detached caller. Keep
  // the original source and its complete pose/paint/material/placement intact.
  auto prepared=scene.snapshot();
  const auto id=prepared.add(text::utf8_prefix(source.name,235)+" remesh",Volume{},source.transform);
  auto &target=prepared.object(id);target.material=source.material;
  target.group=source.group;target.affine_edit=source.affine_edit;
  target.painted=std::move(paint);target.mesh_active=true;target.polygon=std::move(authority);
  prepared.object(source.id).visible=false;prepared.validate();stop();scene=std::move(prepared);
  return id;
}
} // namespace dome
