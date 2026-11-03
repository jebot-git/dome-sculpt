// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/native-scene-append.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/retopo-authoring.hpp"
#include "dome/source-placement.hpp"
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <ostream>
#include <streambuf>
namespace dome {
namespace {
// Scene::save has an independent256MiB bound. Count its real CBOR wire bytes
// prospectively without allocating a dense encoded buffer. Scene::encode still
// performs its existing bounded JSON/archive staging; this is not zero-work RAM.
class CborSize final:public std::streambuf {
  const std::function<bool()> &cancelled_;
  size_t bytes_=0,next_poll_=0;
  void add(size_t n){
    if(n>256*1024*1024-bytes_)throw std::length_error("Combined native project exceeds256MiB save byte budget");
    if(bytes_>=next_poll_){if(cancelled_&&cancelled_())throw std::runtime_error("Native package append cancelled");next_poll_=bytes_+65536;}
    bytes_+=n;
  }
  std::streamsize xsputn(const char *,std::streamsize n)override{
    if(n<0)throw std::invalid_argument("Negative native CBOR byte count");add(size_t(n));return n;
  }
  int_type overflow(int_type c)override{if(!traits_type::eq_int_type(c,traits_type::eof()))add(1);return traits_type::not_eof(c);}
public:
  explicit CborSize(const std::function<bool()> &c):cancelled_(c){}
};
void require_saveable(const Scene &scene,const std::function<bool()> &cancelled){
  CborSize sink(cancelled);std::ostream output(&sink);output.exceptions(std::ios::badbit|std::ios::failbit);
  nlohmann::json::to_cbor(scene.encode(),output);
}
struct Append {
  const std::function<bool()> &cancelled;
  explicit Append(const std::function<bool()> &c):cancelled(c){}
  std::set<uint64_t> instances,reserved_groups,source_group_ids;
  std::set<const PolygonOrigin *> seen_origins;
  std::set<const PolygonAuthority *> seen_authorities;
  std::map<uint64_t,uint64_t> objects,groups;
  std::set<uint64_t> retopo_sources;
  struct ModelLess {
    bool operator()(const std::shared_ptr<const CreatorSourceModel> &a,
                    const std::shared_ptr<const CreatorSourceModel> &b) const {
      if(a.owner_before(b))return true;
      if(b.owner_before(a))return false;
      return std::less<const CreatorSourceModel *>{}(a.get(),b.get());
    }
  };
  std::map<std::shared_ptr<const CreatorSourceModel>,std::map<uint64_t,uint64_t>,ModelLess> remapped_instances;
  std::map<const SourceNodePlacement *,std::shared_ptr<const SourceNodePlacement>> nodes;
  std::map<const SourcePlacementCapsule *,std::shared_ptr<const SourcePlacementCapsule>> capsules;
  std::map<const PolygonOrigin *,std::shared_ptr<const PolygonOrigin>> origins;
  std::map<const PolygonAuthority *,std::shared_ptr<const PolygonAuthority>> authorities;
  std::map<const RetopoPatch *,std::shared_ptr<const RetopoPatch>> patches;
  void stop() const {if(cancelled&&cancelled())throw std::runtime_error("Native package append cancelled");}
  void collect_group(const SceneGroup &g){reserved_groups.insert(g.id);source_group_ids.insert(g.id);if(g.source_placement)instances.insert(g.source_placement->source_instance);}
  void collect(const std::shared_ptr<const PolygonAuthority> &p){
    stop();if(p&&seen_authorities.insert(p.get()).second)collect(p->origin);
  }
  void collect(const std::shared_ptr<const PolygonOrigin> &p){
    stop();if(!p||!seen_origins.insert(p.get()).second)return;
    if(p->placement)for(const auto &g:p->placement->groups)collect_group(g);
    if(p->retopology)retopo_sources.insert(p->retopology->source);
    collect(p->polygon);collect(p->shape_source);collect(p->rig_source);
  }
  void collect(const Scene &s){
    for(const auto &g:s.groups()){stop();collect_group(g);}
    for(const auto &o:s.objects()){
      stop();if(o.retopology)retopo_sources.insert(o.retopology->source);
      collect(o.polygon);collect(o.shape_source);collect(o.rig_source);
    }
  }
  uint64_t instance(const SourceNodePlacement &p){
    auto &by_id=remapped_instances[p.model];
    if(auto i=by_id.find(p.source_instance);i!=by_id.end())return i->second;
    uint64_t value=1;
    while(instances.contains(value)||reserved_groups.contains(value)){
      if(value==UINT64_MAX)throw std::length_error("Native source instance identity exhausted");
      ++value;
    }
    instances.insert(value);by_id.emplace(p.source_instance,value);return value;
  }
  template<class T>static bool owner(const std::shared_ptr<const T> &a,const std::shared_ptr<const T> &b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
  void witness(const std::vector<SceneGroup> &path,const std::shared_ptr<const CreatorAppearance> &appearance){
    for(auto at=path.rbegin();at!=path.rend();++at){
      const auto &p=at->source_placement;
      if(!p)continue;
      if(appearance&&(!owner(p->model->asset,appearance->asset)||!owner(p->model->original_bytes,appearance->original_bytes)||appearance->source_node<0||p->source_node!=uint32_t(appearance->source_node)))continue;
      const auto root=std::find_if(path.begin(),path.end(),[&](const auto &g){return g.id==p->source_instance;});
      if(root!=path.end()){
        const auto first=std::find_if(path.begin(),path.end(),[&](const auto &g){return g.source_placement&&owner(g.source_placement->model,p->model)&&g.source_placement->source_instance==p->source_instance;});
        if(first!=path.end()&&root>=first)throw std::invalid_argument("Native source-instance root does not precede its authenticated source chain");
        if(!appearance){
          // A deleted/unused mesh may leave only its model and placement path.
          // Authenticate every original node/parent against that exact immutable
          // model; a coincident native ID is not a source-root witness.
          int previous=-1;bool complete=true;
          for(auto g=root+1;g!=at.base();++g){
            const auto &n=g->source_placement;
            if(!n||!owner(n->model,p->model)||n->source_instance!=p->source_instance||n->model->asset->nodes.at(n->source_node).parent!=previous){complete=false;break;}
            previous=int(n->source_node);
          }
          if(!complete)continue;
        }
        auto &by_id=remapped_instances[p->model];const auto value=groups.at(root->id);
        auto [i,inserted]=by_id.emplace(p->source_instance,value);
        if(!inserted&&i->second!=value)throw std::invalid_argument("Conflicting authenticated native source root relation");
        instances.insert(value);
      }
      break;
    }
  }
  std::shared_ptr<const SourceNodePlacement> node(const std::shared_ptr<const SourceNodePlacement> &p){
    stop();if(!p)return {};
    if(auto i=nodes.find(p.get());i!=nodes.end())return i->second;
    auto n=std::make_shared<SourceNodePlacement>(*p);n->source_instance=instance(*p);
    nodes.emplace(p.get(),n);return n;
  }
  std::shared_ptr<const SourcePlacementCapsule> capsule(const std::shared_ptr<const SourcePlacementCapsule> &p){
    stop();if(!p)return {};
    if(auto i=capsules.find(p.get());i!=capsules.end())return i->second;
    auto n=std::make_shared<SourcePlacementCapsule>(*p);
    for(auto &g:n->groups){g.id=groups.at(g.id);g.parent=g.parent?groups.at(g.parent):0;g.source_placement=node(g.source_placement);}
    // Historical native IDs are namespaced with this append; matrices and
    // original source-node IDs stay exact, including the witnessed root link.
    capsules.emplace(p.get(),n);return n;
  }
  std::shared_ptr<const RetopoPatch> patch(const std::shared_ptr<const RetopoPatch> &p){
    stop();if(!p)return {};
    if(auto i=patches.find(p.get());i!=patches.end())return i->second;
    auto n=std::make_shared<RetopoPatch>(*p);n->source=objects.at(p->source);
    patches.emplace(p.get(),n);return n;
  }
  std::shared_ptr<const PolygonAuthority> authority(const std::shared_ptr<const PolygonAuthority> &p){
    stop();if(!p)return {};
    if(auto i=authorities.find(p.get());i!=authorities.end())return i->second;
    auto n=std::make_shared<PolygonAuthority>(*p);n->origin=origin(p->origin);
    authorities.emplace(p.get(),n);return n;
  }
  std::shared_ptr<const PolygonOrigin> origin(const std::shared_ptr<const PolygonOrigin> &p){
    stop();if(!p)return {};
    if(auto i=origins.find(p.get());i!=origins.end())return i->second;
    auto n=std::make_shared<PolygonOrigin>(*p);
    n->placement=capsule(p->placement);n->polygon=authority(p->polygon);
    n->shape_source=origin(p->shape_source);n->rig_source=origin(p->rig_source);
    n->retopology=patch(p->retopology);
    origins.emplace(p.get(),n);return n;
  }
};
}
NativeSceneAppend append_native_scene(const Scene &destination,const Scene &package,
                                     const std::function<bool()> &cancelled){
  Append a{cancelled};a.stop();destination.validate();a.stop();package.validate();a.stop();
  if(package.objects().size()>256-destination.objects().size())
    throw std::length_error("Native package append exceeds Scene object budget");
  if(package.groups().size()>128-destination.groups().size())
    throw std::length_error("Native package append exceeds Scene group budget");
  // Snapshot mutable paint/volume state; all source, appearance and before-state
  // owners remain immutable and retain their exact alias relationships.
  NativeSceneAppend out{destination.snapshot(),{}};auto source=package.snapshot();a.stop();
  a.collect(destination);a.retopo_sources.clear();a.source_group_ids.clear();a.seen_origins.clear();a.seen_authorities.clear();a.collect(source);
  // A deleted retopology source is a persistent nonlive reference. Reserve a
  // fresh tombstone through the Scene allocator rather than target an existing
  // destination object or guess its private next-ID counter.
  for(auto id:a.retopo_sources){
    a.stop();bool live=false;for(const auto &o:source.objects())if(o.id==id){live=true;break;}
    if(!live){if(out.scene.next_==UINT64_MAX)throw std::length_error("Historical native object identity exhausted");a.objects.emplace(id,out.scene.next_++);}
  }
  for(const auto &o:source.objects()){
    a.stop();auto id=out.scene.add(o.name,Volume{});a.objects.emplace(o.id,id);out.objects.push_back(id);
  }
  std::set<uint64_t> live_group_ids;
  for(const auto &g:source.groups())live_group_ids.insert(g.id);
  for(auto old:a.source_group_ids)if(!live_group_ids.contains(old)){
    uint64_t id;
    do {
      a.stop();if(out.scene.next_group_==UINT64_MAX)throw std::length_error("Historical native group identity exhausted");
      id=out.scene.next_group_++;
    }while(a.reserved_groups.contains(id)||a.instances.contains(id));
    a.reserved_groups.insert(id);a.groups.emplace(old,id);
  }
  // Source archives allow arbitrary group ordering. Insert parents first while
  // preserving group metadata and local matrices exactly.
  size_t live_groups=0;
  while(live_groups<source.groups().size()){
    bool progress=false;
    for(const auto &g:source.groups()){
      a.stop();if(a.groups.contains(g.id)||(g.parent&&!a.groups.contains(g.parent)))continue;
      while(a.reserved_groups.contains(out.scene.next_group_)||a.instances.contains(out.scene.next_group_)){
        a.stop();if(out.scene.next_group_==UINT64_MAX)throw std::length_error("Native group identity exhausted");
        ++out.scene.next_group_;
      }
      const auto id=out.scene.add_group(g.name,{},g.parent?a.groups.at(g.parent):0);
      a.reserved_groups.insert(id);
      auto copy=g;copy.id=id;copy.parent=g.parent?a.groups.at(g.parent):0;
      out.scene.group(id)=std::move(copy);
      a.groups.emplace(g.id,id);++live_groups;progress=true;
    }
    if(!progress)throw std::invalid_argument("Native package group hierarchy is incomplete");
  }
  // Historical native paths also need their own namespace: when an original
  // source is restored, its actual instance-root relationship must remain true.
  for(const auto &o:source.objects())a.witness(source.group_path(o.group),o.appearance);
  for(const auto &g:source.groups())if(g.source_placement)a.witness(source.group_path(g.id),{});
  for(const auto *origin:a.seen_origins)if(origin->placement)a.witness(origin->placement->groups,origin->appearance);
  for(const auto &g:source.groups())out.scene.group(a.groups.at(g.id)).source_placement=a.node(g.source_placement);
  for(const auto &o:source.objects()){
    a.stop();auto copy=o;copy.id=a.objects.at(o.id);copy.group=o.group?a.groups.at(o.group):0;
    copy.polygon=a.authority(o.polygon);copy.shape_source=a.origin(o.shape_source);
    copy.rig_source=a.origin(o.rig_source);copy.retopology=a.patch(o.retopology);
    out.scene.object(copy.id)=std::move(copy);
  }
  out.scene.select(a.objects.at(source.active().id));a.stop();out.scene.validate();a.stop();require_saveable(out.scene,cancelled);a.stop();return out;
}
}
