// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-authoring.hpp"
#include "dome/mesh-processing.hpp"
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
} // namespace dome
