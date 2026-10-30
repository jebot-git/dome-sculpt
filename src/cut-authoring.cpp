// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/cut-authoring.hpp"
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if(cancelled && cancelled())throw std::runtime_error("Plane cutting cancelled");
}
Mesh part(const PlaneCutResult &cut, bool cap, std::vector<uint32_t> *triangles = {},
          std::vector<std::array<float,3>> *barycentrics = {}) {
  Mesh mesh;
  for(size_t t=0;t<cut.source_triangles.size();++t){
    if((cut.source_triangles[t]==UINT32_MAX)!=cap)continue;
    if(triangles)triangles->push_back(cut.source_triangles[t]);
    for(int k=0;k<3;++k){
      auto at=t*3+k;auto i=cut.mesh.indices[at];
      mesh.indices.push_back(uint32_t(mesh.positions.size()));
      mesh.positions.push_back(cut.mesh.positions[i]);mesh.normals.push_back(cut.mesh.normals[i]);
      if(!cut.mesh.uv.empty())mesh.uv.push_back(cut.mesh.uv[i]);
      if(barycentrics)barycentrics->push_back(cut.source_corner_barycentrics[at]);
    }
  }
  return mesh;
}
void atlas(Mesh &mesh,uint32_t resolution,const std::function<bool()> &cancelled){
  auto before=mesh;
  unwrap(mesh,resolution,4,cancelled);
  if(mesh.indices.size()!=before.indices.size())throw std::runtime_error("Cut atlas changed face ordering");
  for(size_t k=0;k<mesh.indices.size();++k)
    if(length(mesh.positions[mesh.indices[k]]-before.positions[before.indices[k]])>1e-7f)
      throw std::runtime_error("Cut atlas changed provenance corner ordering");
}
void localize(Mesh &mesh,CanvasTransform world){
  auto inverse=ob::inverse(world);
  for(auto &p:mesh.positions){auto q=ob::transform(inverse,{p.x,p.y,p.z});p={q.x,q.y,q.z};}
  for(auto &n:mesh.normals){auto q=ob::rotate(inverse.orientation,{n.x,n.y,n.z});n={q.x,q.y,q.z};}
}
}
void CutSettings::validate() const {
  if(!std::isfinite(yaw)||std::abs(yaw)>180||!std::isfinite(pitch)||std::abs(pitch)>180||
     !std::isfinite(offset_mm)||std::abs(offset_mm)>100'000)
    throw std::invalid_argument("Invalid plane orientation or document-millimetre offset");
  cap_material.validate();
}
CutPlane cut_plane(const CutSettings &settings,Vec3 center){
  settings.validate();if(!finite(center))throw std::invalid_argument("Invalid plane center");
  constexpr float radians=3.14159265358979323846f/180;
  float yaw=settings.yaw*radians,pitch=settings.pitch*radians;
  Vec3 normal{std::cos(yaw)*std::cos(pitch),std::sin(pitch),std::sin(yaw)*std::cos(pitch)};
  float offset=dot(normal,center)+settings.offset_mm*.001f;
  return settings.flipped?CutPlane{normal*-1,-offset}:CutPlane{normal,offset};
}
std::pair<Vec3,float> cut_bounds(const Mesh &mesh){
  mesh.validate();Vec3 low=mesh.positions.front(),high=low;
  for(auto p:mesh.positions){low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
                           high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
  return {(low+high)*.5f,std::max(.001f,length(high-low)*.5f)};
}
CutPreview preview_cut(const Mesh &source,const CutSettings &settings,const std::function<bool()> &cancelled){
  stop(cancelled);auto [center,radius]=cut_bounds(source);(void)radius;
  CutPreview preview;preview.settings=settings;preview.plane=cut_plane(settings,center);
  PlaneCutOptions options;options.normal=preview.plane.normal;options.offset=preview.plane.offset;
  options.cap=settings.cap;options.cap_material=1;options.cancelled=cancelled;
  auto result=plane_cut(source,options);
  preview.changed=std::any_of(source.positions.begin(),source.positions.end(),[&](Vec3 p){return dot(preview.plane.normal,p)<preview.plane.offset;});
  preview.result=std::make_shared<const PlaneCutResult>(std::move(result));
  preview.cap_mesh=part(*preview.result,true);stop(cancelled);return preview;
}
CutPublishReport publish_cut(Scene &scene,const CutPreview &preview,const BakeSource &source,
                             uint32_t resolution,const std::function<bool()> &cancelled,
                             const std::function<void(float)> &progress){
  stop(cancelled);preview.settings.validate();scene.validate();
  if(!preview.result)throw std::invalid_argument("Missing plane-cut preview");
  if(!preview.changed)return {};
  if(resolution<64||resolution>2048||(resolution&(resolution-1)))throw std::invalid_argument("Invalid cut texture resolution");
  auto original=scene.active();
  if(original.locked||!original.visible)throw std::logic_error("Show and unlock the cut source");
  if(scene.objects().size()>254||scene.groups().size()>=128)throw std::length_error("No room for cut shell/cap objects and group");
  std::vector<uint32_t> triangles;std::vector<std::array<float,3>> barycentrics;
  auto shell=part(*preview.result,false,&triangles,&barycentrics);
  auto cap=part(*preview.result,true);
  if(progress)progress(.05f);
  atlas(shell,resolution,cancelled);stop(cancelled);
  BakeOptions options;options.resolution=resolution;options.ao_samples=0;options.padding=8;
  options.source_triangles=std::move(triangles);options.source_corner_barycentrics=std::move(barycentrics);
  options.cancelled=cancelled;if(progress)options.progress=[&](float p){progress(.15f+p*.65f);};
  auto baked=bake_mesh(shell,source,options);stop(cancelled);
  BakedTextures textures;textures.maps=std::move(baked.maps);textures.maps.normal=std::move(baked.normal);
  for(auto coverage:baked.coverage)textures.coverage.push_back(coverage==1||coverage==2?255:0);
  auto world=scene.world_transform(original.id);localize(shell,world);
  auto shell_paint=std::make_shared<PaintedMesh>(std::move(shell),resolution);
  shell_paint->install_bake(std::move(textures));
  std::shared_ptr<PaintedMesh> cap_paint;
  if(!cap.indices.empty()){atlas(cap,resolution,cancelled);localize(cap,world);cap_paint=std::make_shared<PaintedMesh>(std::move(cap),resolution);}
  stop(cancelled);if(progress)progress(.9f);
  auto next=scene.snapshot();
  auto name=original.name.size()<=252?original.name+" cut":std::string("Cut result");
  auto group=next.add_group(std::move(name),world);
  CutPublishReport report;report.changed=true;report.projected=baked.projected;
  report.shell=next.add("Cut shell",Volume{});auto &side=next.object(report.shell);
  side.group=group;side.material=original.material;side.mesh_active=true;side.painted=std::move(shell_paint);
  if(cap_paint){report.cap=next.add("Cut cap",Volume{});auto &end=next.object(report.cap);
    end.group=group;end.material=preview.settings.cap_material;end.mesh_active=true;end.painted=std::move(cap_paint);}
  next.object(original.id).visible=false;next.select(report.shell);next.validate();stop(cancelled);
  scene=std::move(next);if(progress)progress(1);return report;
}
} // namespace dome
