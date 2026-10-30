// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/sculpt-resolution.hpp"
#include <bit>
#include <map>
#include <limits>
#include <set>
#include <unordered_set>
namespace dome {
namespace {
struct Work {
  const SculptResolutionOptions &options;
  SculptResolutionReport &report;
  void stop() const {if(options.cancelled && options.cancelled())throw std::runtime_error("Sculpt resolution change cancelled");}
  void visit(size_t count=1) {
    if(count>options.maximum_work-report.work)throw std::length_error("Sculpt resolution work budget exceeded");
    report.work+=count;if((report.work&511)<count)stop();
  }
};
int floor_div(int value,int divisor) {return value>=0?value/divisor:-int((-int64_t(value)+divisor-1)/divisor);}
template<class Map> double interpolate(const Map &map,Int3 q,float target,float source,double background,Work &work) {
  double p[]={double(q.x)*target/source,double(q.y)*target/source,double(q.z)*target/source};
  Int3 lo{int(std::floor(p[0])),int(std::floor(p[1])),int(std::floor(p[2]))};
  double f[]={p[0]-lo.x,p[1]-lo.y,p[2]-lo.z},result=background;
  work.visit(8);
  for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
    auto found=map.find(lo+Int3{x,y,z});
    if(found!=map.end())result+=(double(found->second)-background)*(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);
  }
  return result;
}
// Union occupied interpolation cells before projecting supports to target tiles.
// Disconnected objects/mirrors do not enumerate their global empty-space hull.
template<class Map,class Emit> void sample_field(const Map &input,float source,float target,size_t remaining,Work &work,Emit emit) {
  std::unordered_set<Int3,IntHash> cells,tiles;
  for(const auto &[key,value]:input) {
    (void)value;
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
      work.visit();cells.insert(key-Int3{x,y,z});
      if(cells.size()>work.options.maximum_samples)throw std::length_error("Sculpt resolution source support budget exceeded");
    }
  }
  const int domain=int(101/target);
  for(auto cell:cells) {
    auto bound=[&](int a,bool upper) {
      double p=double(int64_t(a)+(upper?1:0))*source/target;
      // Widen by one lattice point for floating boundary rounding. Zero-weight
      // interpolation results are discarded; no displacement is invented.
      return std::clamp(int(upper?std::ceil(p)+1:std::floor(p)-1),-domain,domain);
    };
    Int3 lo{floor_div(bound(cell.x,false),8),floor_div(bound(cell.y,false),8),floor_div(bound(cell.z,false),8)};
    Int3 hi{floor_div(bound(cell.x,true),8),floor_div(bound(cell.y,true),8),floor_div(bound(cell.z,true),8)};
    uint64_t nx=uint64_t(int64_t(hi.x)-lo.x+1),ny=uint64_t(int64_t(hi.y)-lo.y+1),nz=uint64_t(int64_t(hi.z)-lo.z+1);
    auto available=work.options.maximum_work-work.report.work;
    if(nx>available || ny>available/nx || nz>available/(nx*ny))throw std::length_error("Sculpt resolution target support budget exceeded");
    for(int z=lo.z;z<=hi.z;++z)for(int y=lo.y;y<=hi.y;++y)for(int x=lo.x;x<=hi.x;++x) {
      work.visit();tiles.insert({x,y,z});
      if(tiles.size()>(work.options.maximum_work-work.report.work)/4096)
        throw std::length_error("Sculpt resolution target sampling budget exceeded");
    }
  }
  size_t emitted=0;
  for(auto tile:tiles)for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
    Int3 q{tile.x*8+x,tile.y*8+y,tile.z*8+z};
    if(std::max({std::abs(q.x),std::abs(q.y),std::abs(q.z)})>domain)continue;
    if(emit(q)) {if(++emitted>remaining)throw std::length_error("Sculpt resolution layer/mask storage budget exceeded");}
  }
}
size_t crossing_cells(const Volume &volume,Work &work) {
  std::unordered_set<Int3,IntHash> cells;
  for(auto [p,value]:volume.voxels())if(value<0)
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
      Int3 cell=p-Int3{x,y,z};bool outside=false;
      for(int dz=0;dz<2;++dz)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx) {work.visit();outside|=volume.at(cell+Int3{dx,dy,dz})>=0;}
      if(outside) {
        cells.insert(cell);
        if(cells.size()>work.options.maximum_surface_cells)throw std::length_error("Sculpt resolution surface validation budget exceeded");
      }
    }
  return cells.size();
}
struct EulerSignature {int64_t total=0;std::vector<int64_t> components;};
EulerSignature euler(const Mesh &mesh,Work &work) {
  std::map<std::array<uint32_t,3>,uint32_t> points;
  std::vector<uint32_t> remap;
  for(auto p:mesh.positions) {
    work.visit();
    std::array<uint32_t,3> key{p.x==0?0:std::bit_cast<uint32_t>(p.x),p.y==0?0:std::bit_cast<uint32_t>(p.y),p.z==0?0:std::bit_cast<uint32_t>(p.z)};
    auto [it,inserted]=points.emplace(key,uint32_t(points.size()));(void)inserted;remap.push_back(it->second);
  }
  std::vector<uint32_t> parents(points.size());
  for(uint32_t i=0;i<parents.size();++i)parents[i]=i;
  auto root=[&](uint32_t i){while(parents[i]!=i){parents[i]=parents[parents[i]];i=parents[i];}return i;};
  std::set<std::array<uint32_t,2>> edges;
  for(size_t i=0;i<mesh.indices.size();i+=3)for(int k=0;k<3;++k) {
    work.visit();auto a=remap[mesh.indices[i+k]],b=remap[mesh.indices[i+(k+1)%3]];
    edges.insert({std::min(a,b),std::max(a,b)});parents[root(a)]=root(b);
  }
  std::map<uint32_t,int64_t> components;
  for(uint32_t i=0;i<parents.size();++i){work.visit();++components[root(i)];}
  for(auto edge:edges){work.visit();--components[root(edge[0])];}
  for(size_t i=0;i<mesh.indices.size();i+=3){work.visit();++components[root(remap[mesh.indices[i]])];}
  EulerSignature result;
  for(auto [id,value]:components){(void)id;result.total+=value;result.components.push_back(value);}
  std::sort(result.components.begin(),result.components.end());return result;
}
struct Triangle {Vec3 a,b,c;unsigned depth=0;};
void compare_surface(const Volume &field,const Mesh &mesh,const Mesh &target,float tolerance,Work &work) {
  SurfaceQuery query(target,work.options.cancelled);
  SurfaceQueryOptions options;options.cancelled=work.options.cancelled;
  options.triangle_filter=[&](uint32_t) {
    if(++work.report.projection_tests>work.options.maximum_projection_tests)throw std::length_error("Sculpt resolution projection budget exceeded");
    return true;
  };
  std::vector<Triangle> pending;
  for(size_t i=0;i<mesh.indices.size();i+=3) {
    pending.push_back({mesh.positions[mesh.indices[i]],mesh.positions[mesh.indices[i+1]],mesh.positions[mesh.indices[i+2]]});
    while(!pending.empty()) {
      work.stop();auto t=pending.back();pending.pop_back();
      if(++work.report.quality_samples>work.options.maximum_quality_samples)throw std::length_error("Sculpt resolution quality sampling budget exceeded");
      Vec3 center=(t.a+t.b+t.c)/3.f;
      auto hit=query.closest(center,options);
      float alignment=dot(field.gradient(center),hit.normal);
      work.report.minimum_normal_dot=std::min(work.report.minimum_normal_dot,alignment);
      if(!std::isfinite(alignment) || alignment<work.options.minimum_normal_dot)
        throw std::runtime_error("Sculpt resolution cannot retain surface normal direction within the requested bound");
      float radius=std::max({length(t.a-center),length(t.b-center),length(t.c-center)});
      float coordinate=std::max({1.f,std::abs(center.x),std::abs(center.y),std::abs(center.z)});
      float epsilon=coordinate*std::numeric_limits<float>::epsilon()*32;
      float bound=hit.distance+radius+epsilon;
      if(hit.distance>tolerance)throw std::runtime_error("Sculpt resolution loses surface detail beyond the requested distance bound");
      if(bound<=tolerance) {work.report.maximum_surface_bound=std::max(work.report.maximum_surface_bound,bound);continue;}
      if(t.depth>=12 || epsilon>=tolerance)throw std::runtime_error("Sculpt resolution quality bound cannot be certified at this coordinate precision");
      Vec3 ab=t.b-t.a,bc=t.c-t.b,ca=t.a-t.c;
      if(dot(ab,ab)>=dot(bc,bc) && dot(ab,ab)>=dot(ca,ca)) {
        auto middle=t.a+(t.b-t.a)*.5f;pending.push_back({t.a,middle,t.c,t.depth+1});pending.push_back({middle,t.b,t.c,t.depth+1});
      } else if(dot(bc,bc)>=dot(ca,ca)) {
        auto middle=t.b+(t.c-t.b)*.5f;pending.push_back({t.a,t.b,middle,t.depth+1});pending.push_back({t.a,middle,t.c,t.depth+1});
      } else {
        auto middle=t.c+(t.a-t.c)*.5f;pending.push_back({t.a,t.b,middle,t.depth+1});pending.push_back({middle,t.b,t.c,t.depth+1});
      }
    }
  }
}
void quality(const Volume &source,const Volume &target,Work &work) {
  const auto source_cells=crossing_cells(source,work),target_cells=crossing_cells(target,work);
  if(!source_cells && !target_cells)return;
  if(!source_cells || !target_cells)throw std::runtime_error("Sculpt resolution removes or invents an existing surface");
  auto original=surface_mesh(source,work.options.cancelled),candidate=surface_mesh(target,work.options.cancelled);
  for(const auto *mesh:{&original,&candidate})
    if(mesh->positions.size()>work.options.maximum_vertices || mesh->indices.size()/3>work.options.maximum_triangles)
      throw std::length_error("Sculpt resolution surface mesh budget exceeded");
  work.report.source=inspect_mesh(original,work.options.cancelled);work.report.target=inspect_mesh(candidate,work.options.cancelled);
  auto source_euler=euler(original,work),target_euler=euler(candidate,work);
  work.report.source_euler=source_euler.total;work.report.target_euler=target_euler.total;
  auto manifold_components=[](const EulerSignature &signature,const TopologyReport &report) {
    return signature.components.size()==report.components &&
      std::none_of(signature.components.begin(),signature.components.end(),[](auto chi){return chi>2 || chi%2!=0;});
  };
  if(!manifold_components(source_euler,work.report.source) || !manifold_components(target_euler,work.report.target) ||
      !work.report.source.closed_manifold() || !work.report.target.closed_manifold() ||
      work.report.source.components!=work.report.target.components || source_euler.components!=target_euler.components)
    throw std::runtime_error("Sculpt resolution cannot certify closed topology (source boundary/nonmanifold/inconsistent="+
        std::to_string(work.report.source.boundary_edges)+"/"+std::to_string(work.report.source.nonmanifold_edges)+"/"+std::to_string(work.report.source.inconsistent_edges)+
        ", source degenerate/duplicate/components/euler="+std::to_string(work.report.source.degenerate_triangles)+"/"+std::to_string(work.report.source.duplicate_triangles)+"/"+std::to_string(work.report.source.components)+"/"+std::to_string(work.report.source_euler)+
        ", target="+std::to_string(work.report.target.boundary_edges)+"/"+std::to_string(work.report.target.nonmanifold_edges)+"/"+std::to_string(work.report.target.inconsistent_edges)+
        ", target degenerate/duplicate/components/euler="+std::to_string(work.report.target.degenerate_triangles)+"/"+std::to_string(work.report.target.duplicate_triangles)+"/"+std::to_string(work.report.target.components)+"/"+std::to_string(work.report.target_euler)+")");
  float tolerance=work.options.maximum_surface_error>0?work.options.maximum_surface_error:std::max(source.voxel_size(),target.voxel_size())*.5f;
  compare_surface(source,original,candidate,tolerance,work);
  compare_surface(target,candidate,original,tolerance,work);
}
void validate(const SculptResolutionOptions &o) {
  if(!std::isfinite(o.voxel_size) || o.voxel_size<.00001f || o.voxel_size>1 ||
      !std::isfinite(o.maximum_surface_error) || o.maximum_surface_error<0 || o.maximum_surface_error>1 ||
      !std::isfinite(o.maximum_layer_error) || o.maximum_layer_error<0 || o.maximum_layer_error>1e6 ||
      !std::isfinite(o.maximum_mask_error) || o.maximum_mask_error<0 || o.maximum_mask_error>1 ||
      !std::isfinite(o.minimum_normal_dot) || o.minimum_normal_dot<0 || o.minimum_normal_dot>1 ||
      !o.maximum_samples || o.maximum_samples>4'000'000 || !o.maximum_work || o.maximum_work>1'000'000'000 ||
      !o.maximum_surface_cells || o.maximum_surface_cells>500'000 || !o.maximum_vertices || o.maximum_vertices>4'000'000 ||
      !o.maximum_triangles || o.maximum_triangles>8'000'000 || !o.maximum_quality_samples || o.maximum_quality_samples>4'000'000 ||
      !o.maximum_projection_tests || o.maximum_projection_tests>100'000'000)
    throw std::invalid_argument("Invalid sculpt resolution or bounded validation settings");
}
} // namespace
SculptResolutionResult resample_sculpt(const Volume &source,const SculptResolutionOptions &options) {
  validate(options);SculptResolutionResult result;Work work{options,result.report};work.stop();
  if(source.in_stroke())throw std::logic_error("Finish the current sculpt stroke before changing resolution");
  if(options.voxel_size==source.voxel_size())return result;
  Volume next(options.voxel_size,source.history_limit_);next.band_=source.band();
  next.layers_.clear();next.active_=source.active_;next.next_layer_=source.next_layer_;next.settings_=source.settings_;
  for(const auto &layer:source.layers_) {
    Volume::Layer output=layer;output.delta=decltype(output.delta){};
    sample_field(layer.delta,source.voxel_size(),next.voxel_size(),options.maximum_samples-result.report.layer_samples,work,[&](Int3 q) {
      auto value=interpolate(layer.delta,q,next.voxel_size(),source.voxel_size(),0,work);
      if(value==0)return false;
      if(!std::isfinite(value) || std::abs(value)>1e6)throw std::runtime_error("Sculpt resolution produced an invalid layer displacement");
      output.delta.emplace(q,value);return true;
    });
    bool meaningful=false;
    const double tolerance=options.maximum_layer_error>0?options.maximum_layer_error:std::max(source.voxel_size(),next.voxel_size());
    for(auto [q,value]:layer.delta) {
      meaningful|=value!=0;
      auto error=std::abs(interpolate(output.delta,q,source.voxel_size(),next.voxel_size(),0,work)-value);
      result.report.maximum_layer_error=std::max(result.report.maximum_layer_error,float(error));
      if(error>tolerance)throw std::runtime_error("Sculpt resolution loses independent layer detail beyond the requested metre bound");
    }
    if(meaningful && !output.delta.size())throw std::runtime_error("Sculpt resolution removes an existing displacement layer");
    result.report.layer_samples+=output.delta.size();next.layers_.push_back(std::move(output));
  }
  next.mask_.background=source.mask_.background;
  sample_field(source.mask_.values,source.voxel_size(),next.voxel_size(),options.maximum_samples-result.report.layer_samples,work,[&](Int3 q) {
    float value=float(std::clamp(interpolate(source.mask_.values,q,next.voxel_size(),source.voxel_size(),source.mask_.background,work),0.,1.));
    if(value==source.mask_.background)return false;
    next.mask_.values.emplace(q,value);return true;
  });
  result.report.mask_samples=next.mask_.values.size();
  for(auto [q,value]:source.mask_.values) {
    auto error=std::abs(interpolate(next.mask_.values,q,source.voxel_size(),next.voxel_size(),next.mask_.background,work)-value);
    result.report.maximum_mask_error=std::max(result.report.maximum_mask_error,float(error));
    if(error>options.maximum_mask_error)throw std::runtime_error("Sculpt resolution loses protection detail beyond the requested bound");
  }
  if(source.mask_.values.size() && !next.mask_.values.size())throw std::runtime_error("Sculpt resolution removes existing protection detail");
  work.stop();next.recompose();next.geometry_history_.reset();quality(source,next,work);work.stop();
  result.report.changed=true;result.replacement=std::move(next);return result;
}
bool resample_sculpt_object(Scene &scene,uint64_t object,const SculptResolutionOptions &options,SculptResolutionReport *report) {
  const auto &source=scene.object(object);
  if(source.locked || !source.visible || source.mesh_active)throw std::logic_error("Select an unlocked visible sculpt source to change resolution");
  if(options.voxel_size==source.volume->voxel_size()) {
    auto result=resample_sculpt(*source.volume,options);if(report)*report=result.report;return false;
  }
  size_t used=0;
  for(const auto &other:scene.objects())if(other.id!=object) {
    auto count=std::max(other.volume->voxels().size(),other.volume->stored_samples());
    if(count>4'000'000-used)throw std::length_error("Scene sculpt resolution storage budget exceeded");
    used+=count;
  }
  const auto available=size_t(4'000'000)-used;
  if(!available && source.volume->stored_samples())throw std::length_error("Scene sculpt resolution storage budget exceeded");
  auto bounded=options;bounded.maximum_samples=std::min(bounded.maximum_samples,std::max(size_t(1),available));
  auto result=resample_sculpt(*source.volume,bounded);
  if(!result.replacement) {if(report)*report=result.report;return false;}
  auto volume=std::make_shared<Volume>(std::move(*result.replacement));
  Scene proposed=scene;proposed.object(object).volume=std::move(volume);proposed.validate(false);
  if(options.cancelled && options.cancelled())throw std::runtime_error("Sculpt resolution change cancelled");
  if(report)*report=result.report;
  scene=std::move(proposed);return true;
}
} // namespace dome
