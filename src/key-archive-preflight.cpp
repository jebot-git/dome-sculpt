// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-archive-preflight.hpp"
#include "normal-key-derivation.hpp"
#include "frame-position-domain.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/polygon-scene.hpp"
#include <set>
#include <map>
namespace dome::detail {
namespace {
class Prospect {
  static constexpr size_t frame_limit=128*1024*1024,deform_limit=8'000'000;
  const SculptObject &current_;const KeyArchiveProspect &plan_;
  const std::function<bool()> &cancelled_;
  KeyArchiveUsage usage_;
  std::set<const NormalFrameSet*> frames_;
  std::set<const NormalFrames*> roles_;
  std::set<const FramePositionDomain*> domains_;
  std::set<const Mesh*> meshes_;
  using MaterialKey=std::pair<const SourceMaterialBindings*,bool>;
  std::set<MaterialKey> materials_,material_path_;
  std::map<MaterialKey,size_t> key_heights_;
  using HistoryKey=std::pair<const void*,bool>;
  std::map<HistoryKey,size_t> history_heights_;
  std::set<HistoryKey> history_path_;
  using NormalKey=std::pair<const SourceNormalPreparation*,bool>;
  std::set<NormalKey> normals_,normal_path_;
  std::set<const SourceColorPreparation*> colors_,color_path_;
  std::set<const SourceUvOrigin*> uv_origins_;
  std::set<const SourceKeyDerivation*> keys_,key_path_;
  std::set<const PolygonOrigin*> origins_,origin_path_;
  std::set<const PolygonAuthority*> authorities_,authority_path_;
  void stop()const{if(cancelled_&&cancelled_())throw std::runtime_error("Prospective key archive cancelled");}
  void frame_bytes(size_t n){if(n>frame_limit-usage_.frame_bytes)throw std::length_error("Prospective retained key frames exceed 128 MiB before allocation");usage_.frame_bytes+=n;}
  void entries(size_t n){if(n>deform_limit-usage_.deformation_entries)throw std::length_error("Prospective retained key deformation exceeds native quota before allocation");usage_.deformation_entries+=n;}
  void deform(const std::shared_ptr<const DeformationState>&p){stop();if(p)entries(p->resource_entries());}
  void frame(const std::shared_ptr<const NormalFrameSet>&p){
    stop();if(!p||frames_.contains(p.get()))return;
    if(frames_.size()>=255)throw std::length_error("Prospective key frame registry exceeds native quota");
    frames_.insert(p.get());
    if(p->position_domain)p->position_domain->charge_storage(usage_.frame_bytes,frame_limit,domains_,meshes_);
    for(const auto &f:{p->pbr,p->mtoon,p->canonical})if(f&&roles_.insert(f.get()).second)frame_bytes(f->storage_bytes());
    for(const auto &t:p->targets){stop();if(t.positions.size()>frame_limit/sizeof(Vec3))throw std::length_error("Invalid retained key POSITION size");frame_bytes(t.positions.size()*sizeof(Vec3));}
  }
  void uv(const std::shared_ptr<const SourceUvOrigin>&p){
    stop();if(!p||!uv_origins_.insert(p.get()).second)return;
    if(uv_origins_.size()>256)throw std::length_error("Prospective key UV-origin registry exceeds native quota");deform(p->deformation);
  }
  void color(const std::shared_ptr<const SourceColorPreparation>&p){
    stop();if(!p)return;if(color_path_.contains(p.get()))throw std::length_error("Prospective key color history contains a cycle");if(colors_.contains(p.get()))return;
    color_path_.insert(p.get());struct Pop{std::set<const SourceColorPreparation*>&s;const SourceColorPreparation*p;~Pop(){s.erase(p);}}pop{color_path_,p.get()};colors_.insert(p.get());
    if(colors_.size()>256)throw std::length_error("Prospective key color registry exceeds native quota");
    deform(p->original_deformation);material(p->original_material);
    if(p->uv_reprojection){deform(p->uv_reprojection->before_deformation);material(p->uv_reprojection->before_material);}
    if(p->paint_sampling){deform(p->paint_sampling->before_deformation());material(p->paint_sampling->before_material());}
  }
  void normal(const std::shared_ptr<const SourceNormalPreparation>&p,bool future){
    stop();if(!p)return;const NormalKey id{p.get(),future};if(normal_path_.contains(id))throw std::length_error("Prospective key normal history contains a cycle");if(normals_.contains(id))return;
    normal_path_.insert(id);struct Pop{std::set<NormalKey>&s;NormalKey id;~Pop(){s.erase(id);}}pop{normal_path_,id};normals_.insert(id);
    if(normals_.size()>256)throw std::length_error("Prospective key normal registry exceeds native quota");
    if(!future)frame(p->target_frames);
    deform(p->original_deformation);material(p->original_material);
    if(p->uv_reprojection){deform(p->uv_reprojection->before_deformation);material(p->uv_reprojection->before_material);}
    if(p->sampling_domain){deform(p->sampling_domain->before_deformation);material(p->sampling_domain->before_material);material(p->sampling_domain->sampling_material);}
  }
  void key(const std::shared_ptr<const SourceKeyDerivation>&p){
    stop();if(!p)return;if(key_path_.contains(p.get()))throw std::length_error("Prospective key owner history contains a cycle");if(keys_.contains(p.get()))return;
    if(key_path_.size()>=32)throw std::length_error("Prospective key owner history exceeds bounded depth");key_path_.insert(p.get());struct Pop{std::set<const SourceKeyDerivation*>&s;const SourceKeyDerivation*p;~Pop(){s.erase(p);}}pop{key_path_,p.get()};keys_.insert(p.get());
    if(keys_.size()>255)throw std::length_error("Prospective key owner registry exceeds native quota");
    // The durable key table stores this deformation inline, even if another
    // context has the same immutable value. Frame data is genuinely pooled.
    deform(p->before_deformation());frame(p->frames());material(p->before_material());
  }
  size_t history_height(const std::shared_ptr<const PolygonAuthority>&p){
    stop();if(!p)return 0;const HistoryKey id{p.get(),true};
    if(history_path_.contains(id))throw std::length_error("Prospective polygon history contains a cycle");
    if(auto it=history_heights_.find(id);it!=history_heights_.end())return it->second;
    if(history_path_.size()>=32)throw std::length_error("Prospective polygon history exceeds bounded depth");
    history_path_.insert(id);struct Pop{std::set<HistoryKey>&s;HistoryKey id;~Pop(){s.erase(id);}}pop{history_path_,id};
    const auto height=1+history_height(p->origin);if(height>32)throw std::length_error("Prospective polygon history exceeds bounded depth");history_heights_.emplace(id,height);return height;
  }
  size_t history_height(const std::shared_ptr<const PolygonOrigin>&p){
    stop();if(!p)return 0;const HistoryKey id{p.get(),false};
    if(history_path_.contains(id))throw std::length_error("Prospective source history contains a cycle");
    if(auto it=history_heights_.find(id);it!=history_heights_.end())return it->second;
    if(history_path_.size()>=32)throw std::length_error("Prospective source history exceeds bounded depth");
    history_path_.insert(id);struct Pop{std::set<HistoryKey>&s;HistoryKey id;~Pop(){s.erase(id);}}pop{history_path_,id};
    const auto height=1+std::max({history_height(p->polygon),history_height(p->shape_source),history_height(p->rig_source)});if(height>32)throw std::length_error("Prospective source history exceeds bounded depth");history_heights_.emplace(id,height);return height;
  }
  size_t material_height(const SourceMaterialBindings &p,bool future)const{
    size_t h=0;auto take=[&](const std::shared_ptr<const SourceMaterialBindings>&b,size_t plus=0){if(b)h=std::max(h,plus+key_heights_.at({b.get(),false}));};
    if(p.color_preparation){const auto &c=*p.color_preparation;take(c.original_material);if(c.uv_reprojection)take(c.uv_reprojection->before_material);if(c.paint_sampling)take(c.paint_sampling->before_material());}
    if(p.normal_preparation){const auto &n=*p.normal_preparation;take(n.original_material);if(n.uv_reprojection)take(n.uv_reprojection->before_material);if(n.sampling_domain){take(n.sampling_domain->before_material);take(n.sampling_domain->sampling_material);}}
    if(future)take(plan_.reused_anchor?plan_.reused_anchor->before_material():current_.material_bindings,1);else if(p.key_derivation)take(p.key_derivation->before_material(),1);
    return h;
  }
public:
  Prospect(const SculptObject &c,const KeyArchiveProspect &p,const std::function<bool()> &stop):current_(c),plan_(p),cancelled_(stop){}
  void material(const std::shared_ptr<const SourceMaterialBindings>&p,bool future=false){
    stop();if(!p)return;const MaterialKey id{p.get(),future};
    if(material_path_.contains(id))throw std::length_error("Prospective key material history contains a cycle");
    if(materials_.contains(id))return;
    if(materials_.size()>=256||material_path_.size()>=256)throw std::length_error("Prospective key material registry exceeds native quota");
    material_path_.insert(id);struct Pop{std::set<MaterialKey>&s;MaterialKey id;~Pop(){s.erase(id);}}pop{material_path_,id};
    // Reserve before following edges; the path check still precedes completed
    // hits, so this bounded per-call visitation never certifies a cycle.
    materials_.insert(id);uv(p->uv_origin);color(p->color_preparation);normal(p->normal_preparation,future);
    if(future){
      const auto *anchor=plan_.reused_anchor;
      deform(anchor?anchor->before_deformation():current_.deformation);
      material(anchor?anchor->before_material():current_.material_bindings);
    }else {frame(p->normal_frames);key(p->key_derivation);}
    const auto height=material_height(*p,future);if(height>32)throw std::length_error("Prospective key owner history exceeds bounded depth");key_heights_.emplace(id,height);
  }
  void authority(const std::shared_ptr<const PolygonAuthority>&p){
    stop();if(!p)return;if(authority_path_.contains(p.get()))throw std::length_error("Prospective key polygon history contains a cycle");if(authorities_.contains(p.get()))return;
    if(authorities_.size()>=512||authority_path_.size()>=32)throw std::length_error("Prospective key polygon history exceeds native quota");
    authorities_.insert(p.get());authority_path_.insert(p.get());struct Pop{std::set<const PolygonAuthority*>&s;const PolygonAuthority*p;~Pop(){s.erase(p);}}pop{authority_path_,p.get()};origin(p->origin);
  }
  void origin(const std::shared_ptr<const PolygonOrigin>&p){
    stop();if(!p)return;if(origin_path_.contains(p.get()))throw std::length_error("Prospective key source history contains a cycle");if(origins_.contains(p.get()))return;
    if(origins_.size()>=512||origin_path_.size()>=32)throw std::length_error("Prospective key source history exceeds native quota");
    origins_.insert(p.get());origin_path_.insert(p.get());struct Pop{std::set<const PolygonOrigin*>&s;const PolygonOrigin*p;~Pop(){s.erase(p);}}pop{origin_path_,p.get()};
    deform(p->deformation);material(p->material_bindings);authority(p->polygon);origin(p->shape_source);origin(p->rig_source);
  }
  KeyArchiveUsage run(const Scene &scene){
    if(!plan_.domain||!current_.material_bindings)throw std::invalid_argument("Incomplete prospective key archive plan");
    for(const auto &o:scene.objects()){
      stop();const bool selected=o.id==current_.id;
      const auto height=std::max({history_height(o.polygon),history_height(o.shape_source),history_height(o.rig_source)});
      if(selected&&!plan_.reused_anchor&&height>=32)throw std::length_error("Prospective named source capture exceeds bounded depth");
      if(selected)entries(plan_.deformation_entries);else deform(o.deformation);
      material(o.material_bindings,selected);authority(o.polygon);origin(o.shape_source);origin(o.rig_source);
    }
    if(!plan_.reused_anchor){
      if(origins_.size()>=512)throw std::length_error("Prospective named source registry exceeds native quota");
      // A named operation captures one new complete before-origin. Its old
      // origin/rig/polygon roots above are shared, not copied or erased.
      deform(current_.deformation);material(current_.material_bindings);
    }
    plan_.domain->charge_storage(usage_.frame_bytes,frame_limit,domains_,meshes_);
    frame_bytes(plan_.frame_payload_bytes);stop();return usage_;
  }
};
}
KeyArchiveUsage preflight_key_archive(const Scene &scene,uint64_t object,const KeyArchiveProspect &plan,const std::function<bool()> &cancelled){return Prospect(scene.object(object),plan,cancelled).run(scene);}
}
