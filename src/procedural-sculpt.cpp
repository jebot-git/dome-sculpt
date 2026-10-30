// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/procedural-sculpt.hpp"
#include "csg-kernel.hpp"
#include <memory_resource>
#include <unordered_map>
namespace dome {
namespace {
void stop(const ProceduralSculptOptions &o) { csg_detail::checkpoint(o.cancelled); }
void notify(const ProceduralSculptOptions &o,float value) { stop(o);if(o.progress)o.progress(value); }
struct Budget final : std::pmr::memory_resource {
  size_t used=0,limit;
  explicit Budget(size_t bytes):limit(bytes){}
  void *do_allocate(size_t n,size_t alignment) override {
    if(n>limit-used)throw std::length_error("Procedural sculpt workspace budget exceeded");
    auto p=std::pmr::new_delete_resource()->allocate(n,alignment);used+=n;return p;
  }
  void do_deallocate(void *p,size_t n,size_t alignment) override {
    std::pmr::new_delete_resource()->deallocate(p,n,alignment);used-=n;
  }
  bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {return this==&other;}
};
float component(const Vec3 &p,unsigned axis) {return axis==0?p.x:axis==1?p.y:p.z;}
float &component(Vec3 &p,unsigned axis) {return axis==0?p.x:axis==1?p.y:p.z;}
int floordiv16(int n) {return n>=0?n/16:-int((-int64_t(n)+15)/16);}
bool same(const CsgBox &a,const CsgBox &b) {
  return a.minimum.x==b.minimum.x&&a.minimum.y==b.minimum.y&&a.minimum.z==b.minimum.z&&
    a.maximum.x==b.maximum.x&&a.maximum.y==b.maximum.y&&a.maximum.z==b.maximum.z&&
    a.rounding==b.rounding&&a.operation==b.operation;
}
}
void ProceduralSculptOptions::validate() const {
  if(unsigned(pattern)>2||axis>2||!rows||rows>32||!columns||columns>32||
     !std::isfinite(density)||density<0||density>1||!std::isfinite(gap)||gap<0||gap>.9f||
     !std::isfinite(depth)||depth<.00001f||depth>10||!std::isfinite(rounding)||rounding<0||rounding>1||
     !maximum_samples||maximum_samples>4'000'000||!maximum_work||maximum_work>128'000'000||
     maximum_workspace_bytes<1024||maximum_workspace_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid procedural sculpt controls or budgets");
}
ProceduralSculptResult compose_sculpt_boxes(const Volume &source,std::span<const CsgBox> boxes,
                                           const ProceduralSculptOptions &options,std::string name) {
  options.validate();stop(options);
  if(source.in_stroke())throw std::logic_error("Finish sculpt stroke before procedural detail");
  csg_detail::validate({},boxes,source.band(),options.cancelled);
  ProceduralSculptResult result;
  if(boxes.empty() || (source.mask_default()==1 && source.mask_values().empty()))return result;
  Budget memory(options.maximum_workspace_bytes);
  std::pmr::unordered_map<Int3,std::array<uint64_t,64>,IntHash> visited(&memory);
  std::pmr::vector<Int3> keys(&memory);
  auto visit=[&] {
    if(++result.report.work>options.maximum_work)throw std::length_error("Procedural sculpt work budget exceeded");
    if((result.report.work&1023)==0)stop(options);
  };
  const double voxel=source.voxel_size(),band=source.band();
  const int domain=int(101/voxel);
  for(const auto &box:boxes) {
    Int3 low,high;
    for(unsigned a=0;a<3;++a) {
      double lo=std::floor((double(component(box.minimum,a))-band)/voxel)-1;
      double hi=std::ceil((double(component(box.maximum,a))+band)/voxel)+1;
      if(lo < -domain || hi > domain)throw std::length_error("Procedural detail exceeds the sculpt domain");
      (a==0?low.x:a==1?low.y:low.z)=int(lo);(a==0?high.x:a==1?high.y:high.z)=int(hi);
    }
    uint64_t nx=int64_t(high.x)-low.x+1,ny=int64_t(high.y)-low.y+1,nz=int64_t(high.z)-low.z+1;
    size_t remaining=options.maximum_work-result.report.work;
    if(nx>remaining||ny>remaining/nx||nz>remaining/(nx*ny))throw std::length_error("Procedural detail support exceeds the work budget");
    for(int z=low.z;z<=high.z;++z)for(int y=low.y;y<=high.y;++y)for(int x=low.x;x<=high.x;++x) {
      visit();Int3 key{x,y,z},page{floordiv16(x),floordiv16(y),floordiv16(z)};
      unsigned index=unsigned(x-page.x*16)+16*unsigned(y-page.y*16)+256*unsigned(z-page.z*16);
      auto &bits=visited[page];uint64_t bit=uint64_t(1)<<(index%64);
      if(bits[index/64]&bit)continue;
      bits[index/64]|=bit;
      if(keys.size()>=options.maximum_samples)throw std::length_error("Procedural sculpt sample budget exceeded");
      keys.push_back(key);
    }
  }
  if(boxes.size() && keys.size()>(options.maximum_work-result.report.work)/(boxes.size()+1))
    throw std::length_error("Procedural CSG evaluation exceeds the work budget");
  // Stable key order makes layer archives and repeated seeded runs reproducible.
  std::sort(keys.begin(),keys.end());notify(options,.35f);
  std::pmr::vector<Sample> samples(&memory);samples.reserve(keys.size());
  for(auto q:keys) {
    visit();samples.push_back({Vec3{float(q.x),float(q.y),float(q.z)}*float(voxel),source.at(q),0,1-source.mask_at(q)});
  }
  apply_csg(samples,boxes,source.band(),options.cancelled);
  result.report.work+=samples.size()*boxes.size();
  result.report.samples=samples.size();result.report.primitives=boxes.size();notify(options,.75f);
  std::optional<Volume> candidate;
  Volume::GeometryChange dirty;dirty.kind=Volume::GeometryChange::Kind::Local;
  for(size_t n=0;n<keys.size();++n) {
    if((n&1023)==0)stop(options);
    const auto q=keys[n];const auto target=samples[n].distance;
    if(target==source.at(q))continue;
    if(!candidate) {candidate=source.geometry_snapshot();candidate->add_layer(std::move(name));}
    // The underlying layer sum can extend beyond the clamped visible band.
    // Subtracting the visible float here would make later recomposition drift.
    candidate->layers_.back().delta.emplace(q,double(target)-source.sum(q));
    if(target<source.band())candidate->cells_[q]=target;else candidate->cells_.erase(q);
    Volume::geometry_include(dirty,q);++result.report.changed_samples;
  }
  if(candidate) {
    if(candidate->stored_samples()>options.maximum_samples)throw std::length_error("Procedural detail exceeds total layer and mask storage budget");
    auto history=source.prepare_geometry_history(candidate->cells_,dirty);
    candidate->geometry_history_=std::move(history);
    result.report.changed=true;
  }
  notify(options,.98f);result.replacement=std::move(candidate);return result;
}
ProceduralSculptResult procedural_sculpt(const Volume &source,const ProceduralSculptOptions &options) {
  options.validate();stop(options);
  if(source.in_stroke())throw std::logic_error("Finish sculpt stroke before procedural detail");
  if(options.density==0 || (source.mask_default()==1 && source.mask_values().empty()))return {};
  bool found=false;Vec3 low{},high{};size_t scanned=0;
  for(auto [key,value]:source.voxels()) {
    if((++scanned&1023)==0)stop(options);
    if(scanned>options.maximum_work)throw std::length_error("Procedural surface scan exceeds the work budget");
    if(value>=0)continue;
    Vec3 p{key.x*source.voxel_size(),key.y*source.voxel_size(),key.z*source.voxel_size()};
    if(!found){low=high=p;found=true;}else for(unsigned a=0;a<3;++a){component(low,a)=std::min(component(low,a),component(p,a));component(high,a)=std::max(component(high,a),component(p,a));}
  }
  if(!found)return {};
  for(unsigned a=0;a<3;++a){component(low,a)-=source.voxel_size();component(high,a)+=source.voxel_size();}
  const unsigned u=(options.axis+1)%3,v=(options.axis+2)%3;
  float step_u=(component(high,u)-component(low,u))/options.columns;
  float step_v=(component(high,v)-component(low,v))/options.rows;
  std::vector<CsgBox> boxes;
  uint32_t seed=options.seed;auto random=[&] {seed=1664525u*seed+1013904223u;return float(seed>>8)*(1.f/16777216.f);};
  size_t projected=0,projection_work=scanned;
  for(unsigned row=0;row<options.rows;++row)for(unsigned col=0;col<options.columns;++col) {
    stop(options);float chance=random(),jitter_u=random(),jitter_v=random(),width=random(),height=random(),kind=random();
    if(chance>=options.density)continue;
    Vec3 origin{};component(origin,u)=component(low,u)+(float(col)+.5f+(jitter_u-.5f)*.2f)*step_u;
    component(origin,v)=component(low,v)+(float(row)+.5f+(jitter_v-.5f)*.2f)*step_v;
    float clearance=source.band()+source.voxel_size()*2;
    component(origin,options.axis)=options.reverse?component(low,options.axis)-clearance:component(high,options.axis)+clearance;
    Vec3 direction{};component(direction,options.axis)=options.reverse?1:-1;
    float length=component(high,options.axis)-component(low,options.axis)+clearance*2;
    // Every ray independently has a20k candidate-cell ceiling. Charge that
    // whole allowance before dispatch so larger grids cannot bypass the total
    // operation work budget merely by missing the surface.
    if(projection_work>options.maximum_work || options.maximum_work-projection_work<20'000)
      throw std::length_error("Procedural projection exceeds the work budget");
    projection_work+=20'000;
    auto hit=source.raycast(origin,direction,length,options.cancelled);if(!hit)continue;++projected;
    bool recess=options.pattern==ProceduralSculptOptions::Pattern::Recesses ||
      (options.pattern==ProceduralSculptOptions::Pattern::Mixed&&kind<.5f);
    float half_u=step_u*(1-options.gap)*(.35f+.15f*width),half_v=step_v*(1-options.gap)*(.35f+.15f*height);
    CsgBox box;box.minimum=box.maximum=*hit;
    component(box.minimum,u)-=half_u;component(box.maximum,u)+=half_u;
    component(box.minimum,v)-=half_v;component(box.maximum,v)+=half_v;
    float outward=recess?source.voxel_size()*2:options.depth,inward=recess?options.depth:source.voxel_size()*2;
    float center=component(*hit,options.axis);
    component(box.minimum,options.axis)=center-(options.reverse?outward:inward);
    component(box.maximum,options.axis)=center+(options.reverse?inward:outward);
    box.rounding=options.rounding*std::min({half_u,half_v,(outward+inward)*.5f});
    box.operation=recess?CsgOperation::Difference:CsgOperation::Union;
    unsigned symmetry=source.sculpt_settings().symmetry_axes;
    for(unsigned image=0;image<8;++image)if((image&~symmetry)==0) {
      auto mirror=box;
      for(unsigned a=0;a<3;++a)if(image&(1u<<a)) {
        float plane=component(source.sculpt_settings().symmetry_origin,a);
        component(mirror.minimum,a)=2*plane-component(box.maximum,a);
        component(mirror.maximum,a)=2*plane-component(box.minimum,a);
      }
      if(std::none_of(boxes.begin(),boxes.end(),[&](const auto &b){return same(b,mirror);})) {
        if(boxes.size()==csg_detail::maximum_boxes)throw std::length_error("Procedural symmetry exceeds the primitive budget");
        boxes.push_back(mirror);
      }
    }
  }
  auto remaining=options;
  if(projection_work>=remaining.maximum_work && !boxes.empty())throw std::length_error("Procedural projection exhausted the work budget");
  remaining.maximum_work-=std::min(projection_work,remaining.maximum_work-1);
  auto result=compose_sculpt_boxes(source,boxes,remaining,
    std::string(options.pattern==ProceduralSculptOptions::Pattern::Panels?"Panels":options.pattern==ProceduralSculptOptions::Pattern::Recesses?"Recesses":"Mixed detail")+" seed "+std::to_string(options.seed));
  result.report.projected=projected;result.report.work+=projection_work;return result;
}
bool procedural_sculpt_object(Scene &scene,uint64_t object,const ProceduralSculptOptions &options,ProceduralSculptReport *report) {
  auto &o=scene.object(object);
  if(o.mesh_active || o.locked || !o.visible)throw std::logic_error("Select an unlocked visible sculpt source");
  auto result=procedural_sculpt(*o.volume,options);
  if(report)*report=result.report;
  if(!result.replacement)return false;
  stop(options);o.volume=std::make_shared<Volume>(std::move(*result.replacement));return true;
}
} // namespace dome
