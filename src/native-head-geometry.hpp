// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "document-authority.hpp"
#include "skin-affine.hpp"
#include "weight-declared-support.hpp"
#include "volume-topology.hpp"
#include <map>
#include <nlohmann/json.hpp>
namespace dome::detail {
// Research geometry analysis only. A matched, co-moving edge is attachment
// evidence, not original topology, artist skin, a weld, or joint authority.
struct NativeHeadGeometryReport {
  struct Part {
    uint64_t object = 0;
    bool primary_witness = false, head_region = false;
    size_t components = 0;
    HeatJointMask geometric_controls{};
  };
  struct Contact { uint64_t first = 0, second = 0; size_t edges = 0; };
  std::vector<Part> parts;
  std::vector<Contact> contacts;
  size_t ambiguous = 0, rejected = 0, work = 0, bytes = 0;
};
inline NativeHeadGeometryReport analyze_native_head_geometry(
    const Scene &scene, uint64_t reference,
    size_t maximum_work = 100'000'000,
    size_t maximum_bytes = 128u * 1024u * 1024u,
    const std::function<bool()> &cancelled = {}) {
  using namespace weight_surface;
  if (!maximum_work || maximum_work > 500'000'000 || maximum_bytes < 4096 ||
      maximum_bytes > 256u * 1024u * 1024u)
    throw std::invalid_argument("Head geometry resource limits");
  const SculptObject ref = scene.object(reference);
  const size_t initial_objects = scene.objects().size();
  if (!ref.deformation || !ref.deformation->skeleton || !ref.deformation->humanoid)
    throw std::invalid_argument("Head geometry needs current native rig and roles");
  const auto &rig = *ref.deformation->skeleton;
  const auto &roles = ref.deformation->humanoid->joints;
  ref.deformation->humanoid->validate(rig, false);
  if (!roles.contains("head") || !roles.contains("leftEye") || !roles.contains("rightEye"))
    throw std::invalid_argument("Head geometry lacks actual head/ocular roles");
  const auto head = roles.at("head");
  if(!rig.bones[head].deform)throw std::invalid_argument("Head region cannot emit a nondeform control");
  HeatJointMask branch{};
  for (auto name : {"neck", "head", "leftEye", "rightEye"}) {
    auto it = roles.find(name);
    if (it != roles.end()) branch[it->second/64] |= uint64_t(1) << (it->second%64);
  }
  struct Captured {
    SculptObject object;
    std::array<double,16> world{};
    uint64_t paint_revision = 0, volume_revision = 0;
    Mesh mesh;
    std::vector<uint32_t> source_targets;
    std::vector<std::vector<Vec3>> deltas;
    uint32_t domain = 0;
  };
  std::vector<Captured> captured;
  size_t planned = 65536, raw = 0, corners = 0;
  // Capture strong owners and placements BEFORE any user cancellation callback.
  // The caller supplies its detached immutable worker scene.
  for (const auto &o : scene.objects()) if (o.mesh_active && o.painted && o.appearance) {
    if (captured.size() == 64) throw std::length_error("Head source part cap");
    const auto &m = o.painted->mesh();
    const size_t targets = o.deformation && o.deformation->morphs ? o.deformation->morphs->targets.size() : 0;
    if (m.positions.size() > 2'000'000 || m.indices.size() > 6'000'000 || targets > 128 ||
        o.appearance->source_vertices.size() != m.positions.size())
      throw std::invalid_argument("Head current source/REST dimensions");
    const size_t cost = m.positions.size()*(160 + targets*16) + m.indices.size()*(512 + targets*32);
    if (cost > maximum_bytes - planned) throw std::length_error("Head boundary-key bytes before copies");
    planned += cost; raw += m.positions.size(); corners += m.indices.size();
    captured.push_back({o,scene.world_matrix(o.id).matrix(),o.painted->revision(),o.volume->revision(),{}, {}, {}, 0});
  }
  if (captured.empty() || raw > 2'000'000 || corners > 6'000'000)
    throw std::length_error("Head aggregate geometry cap");
  Budget work{0,maximum_work,cancelled};
  work.step_items(raw+corners+captured.size(),64);
  const auto source_world = scene.world_matrix(reference);
  auto shift=AffineTransform{}.matrix();for(size_t k=0;k<3;++k)shift[12+k]=-source_world.matrix()[12+k];
  const AffineTransform origin(shift);
  auto profile=declared_weight_profile(rig,work,DeclaredRigGeometry::NativeBoneAxes);
  for(auto&s:profile.segments){auto f=[&](std::array<double,3>a){auto p=skin_affine_point(origin*source_world,{float(a[0]),float(a[1]),float(a[2])});return std::array<double,3>{p.x,p.y,p.z};};s.head=f(s.head);s.tail=f(s.tail);}
  struct Domain {
    std::shared_ptr<const ImportedAsset> asset;
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    uint64_t group; int node,mesh,position;
    std::vector<uint32_t> targets;
  };
  std::vector<Domain> domains;
  auto same=[](const auto&a,const auto&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);};
  NativeHeadGeometryReport result;result.bytes=planned;
  struct Metadata {std::shared_ptr<const ImportedAsset> asset;std::shared_ptr<const std::vector<uint8_t>> bytes;nlohmann::json json;};
  std::vector<Metadata> metadata;
  size_t metadata_bytes=0;
  std::map<std::tuple<uint32_t,int>,uint64_t> primitive_owners;
  for(auto&c:captured){
    work.stop();const auto&o=c.object;const auto&a=*o.appearance;
    if(!a.asset||!a.original_bytes||a.source_node<0||a.source_mesh<0||a.source_primitive<0||
       !a.asset->skins.empty()||!a.source_joint_nodes.empty())
      throw std::invalid_argument("Head analysis requires genuine retained unskinned source");
    for(const auto&p:a.asset->primitives)if(!p.influences.empty())throw std::invalid_argument("Artist influences forbidden");
    size_t model=0;
    for(;model<metadata.size();++model){work.step(8);if(same(a.asset,metadata[model].asset)&&same(a.original_bytes,metadata[model].bytes))break;}
    if(model==metadata.size()){
      const auto&bytes=*a.original_bytes;
      auto u32=[&](size_t at){if(at+4>bytes.size())throw std::invalid_argument("Head GLB header");return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|(uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);};
      const size_t size=u32(12);
      if(u32(0)!=0x46546c67||u32(16)!=0x4e4f534a||size>8u*1024u*1024u||size>bytes.size()-20||size>(maximum_bytes-planned-metadata_bytes)/32)
        throw std::invalid_argument("Head canonical GLB metadata cap");
      metadata_bytes+=size*32;result.bytes=planned+metadata_bytes;
      work.step_items(size,16);
      metadata.push_back({a.asset,a.original_bytes,nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+size)});
    }
    // Top-call-local immutable metadata only; every appearance/owner/node and
    // current target correspondence below is still checked independently.
    const auto&json=metadata[model].json;
    if(json.at("nodes").at(a.source_node).at("mesh")!=a.source_mesh)
      throw std::invalid_argument("Head source node/mesh association");
    const auto&p=json.at("meshes").at(a.source_mesh).at("primitives").at(a.source_primitive);
    const int accessor=p.at("attributes").at("POSITION");
    const auto&raw_primitive=a.asset->primitives.at(a.asset->meshes.at(a.source_mesh).primitives.at(a.source_primitive));
    if(raw_primitive.source_mesh!=a.source_mesh||raw_primitive.source_primitive!=a.source_primitive)
      throw std::invalid_argument("Head strong primitive identity mismatch");
    const auto count=o.deformation&&o.deformation->morphs?o.deformation->morphs->targets.size():0;
    if(a.source_morph_targets.size()!=count)
      throw std::invalid_argument("Head current morph/source identities incomplete");
    // This first source-aware construction requires exact current REST and
    // all current target rows to match the authenticated original accessor.
    // Edited/native-only shapes remain a separate required adapter.
    const auto&rest=o.painted->mesh();
    work.step_items(rest.positions.size(),8*(count+1));
    for(size_t i=0;i<rest.positions.size();++i){
      const auto raw_id=a.source_vertices[i];
      if(raw_id>=raw_primitive.mesh.positions.size())throw std::invalid_argument("Head source vertex outside actual accessor");
      auto equal=[](Vec3 x,Vec3 y){return x.x==y.x&&x.y==y.y&&x.z==y.z;};
      if(!equal(rest.positions[i],raw_primitive.mesh.positions[raw_id]))throw std::invalid_argument("Head current REST lacks exact original correspondence");
      for(size_t t=0;t<count;++t){
        auto target=a.source_morph_targets[t];
        if(target>=raw_primitive.morph_targets.size())throw std::invalid_argument("Head current target outside original accessor");
        const auto&delta=raw_primitive.morph_targets[target].positions;
        Vec3 expected=delta.empty()?Vec3{}:delta.at(raw_id);
        if(!equal(o.deformation->morphs->targets[t].delta.at(i),expected))throw std::invalid_argument("Head current morph lacks exact original correspondence");
      }
    }
    c.source_targets=a.source_morph_targets;
    Domain d{a.asset,a.original_bytes,o.group,a.source_node,a.source_mesh,accessor,c.source_targets};
    size_t domain=0;
    for(;domain<domains.size();++domain){work.step(32+count);const auto&v=domains[domain];if(same(v.asset,d.asset)&&same(v.bytes,d.bytes)&&v.group==d.group&&v.node==d.node&&v.mesh==d.mesh&&v.position==d.position&&v.targets==d.targets)break;}
    if(domain==domains.size())domains.push_back(std::move(d));c.domain=uint32_t(domain);
    if(!primitive_owners.emplace(std::tuple{c.domain,a.source_primitive},o.id).second)
      throw std::invalid_argument("Head duplicate primitive/current instance ambiguity");
    SkinGeometryWork geom(work.maximum-work.used,cancelled);auto remaining=geom.remaining();
    const auto placement=origin*AffineTransform(c.world);c.mesh=skin_world_mesh(o.painted->mesh(),placement,geom);work.step(remaining-geom.remaining());
    if(o.deformation&&o.deformation->morphs){
      o.deformation->morphs->validate(o.painted->mesh());
      for(const auto&t:o.deformation->morphs->targets){std::vector<Vec3> delta;delta.reserve(t.delta.size());for(auto p:t.delta){work.step(32);auto q=placement.direction({p.x,p.y,p.z});delta.push_back({float(q[0]),float(q[1]),float(q[2])});}c.deltas.push_back(std::move(delta));}
    }
    HeatBindingOptions opts;opts.maximum_work=std::min(size_t(500'000'000),work.maximum-work.used);opts.maximum_bytes=maximum_bytes-planned-metadata_bytes;
    auto intent=declared_weight_intent(o,{o.id,{}},profile,opts,work,DeclaredSupportPolicy::UniqueSourceWitnesses,c.mesh,ref.deformation);
    const auto masks=intent.masks(o);HeatJointMask aggregate{};for(auto mask:masks)for(size_t k=0;k<4;++k){work.step();aggregate[k]|=mask[k];}
    bool primary=(aggregate[head/64]&(uint64_t(1)<<(head%64)))!=0;
    for(size_t k=0;k<4;++k)primary&=(aggregate[k]&~branch[k])==0;
    size_t local=0;auto components=heat_intent_components(c.mesh,{},local,work.maximum-work.used,cancelled);work.step(local);size_t component_count=0;for(size_t i=0;i<components.size();++i)component_count+=components[i]==i;
    result.parts.push_back({o.id,primary,primary,component_count,aggregate});
  }
  struct Edge {uint32_t part,a,b,face;};
  struct Key {uint32_t domain;std::vector<uint32_t> first,second;};
  struct Less {Budget*w;bool operator()(const Key&a,const Key&b)const{w->step(4+a.first.size()+a.second.size()+b.first.size()+b.second.size());return std::tie(a.domain,a.first,a.second)<std::tie(b.domain,b.first,b.second);}};
  std::map<Key,std::vector<Edge>,Less> boundary(Less{&work});
  for(uint32_t owner=0;owner<captured.size();++owner){
    const auto&c=captured[owner];const auto&m=c.mesh;std::map<std::array<uint32_t,2>,std::vector<Edge>> edges;
    for(size_t at=0;at<m.indices.size();at+=3)for(size_t k=0;k<3;++k){work.step(16);auto a=m.indices[at+k],b=m.indices[at+(k+1)%3];edges[{std::min(a,b),std::max(a,b)}].push_back({owner,a,b,uint32_t(at/3)});}
    auto key=[&](uint32_t i){std::vector<uint32_t> v;v.reserve(3+3*c.deltas.size());auto add=[&](Vec3 p){for(float f:{p.x,p.y,p.z}){work.step();v.push_back(std::bit_cast<uint32_t>(f==0?0.f:f));}};add(m.positions[i]);for(const auto&t:c.deltas)add(t[i]);return v;};
    for(const auto&[unused,es]:edges)if(es.size()==1){(void)unused;auto e=es[0];auto a=key(e.a),b=key(e.b);if(b<a)std::swap(a,b);boundary[{c.domain,std::move(a),std::move(b)}].push_back(e);}
  }
  std::map<std::array<uint32_t,2>,size_t> contacts;
  for(const auto&[key,pair]:boundary){
    work.step();(void)key;if(pair.size()!=2){result.ambiguous+=pair.size()>2;continue;}
    const auto&a=pair[0],&b=pair[1];if(a.part==b.part)continue;
    const auto&x=captured[a.part];const auto&y=captured[b.part];
    auto p=[](Vec3 v){return std::array<float,3>{v.x,v.y,v.z};};
    auto tri=[&](const Captured&c,uint32_t f){const auto&m=c.mesh;return volume_topology::Triangle{p(m.positions[m.indices[f*3]]),p(m.positions[m.indices[f*3+1]]),p(m.positions[m.indices[f*3+2]])};};
    if(p(x.mesh.positions[a.a])!=p(y.mesh.positions[b.b])||p(x.mesh.positions[a.b])!=p(y.mesh.positions[b.a])||volume_topology::unexpected_contact(tri(x,a.face),tri(y,b.face),work)){++result.rejected;continue;}
    ++contacts[{std::min(a.part,b.part),std::max(a.part,b.part)}];
  }
  for(const auto&[edge,count]:contacts)result.contacts.push_back({captured[edge[0]].object.id,captured[edge[1]].object.id,count});
  // Explicit candidate segmentation: a separate primitive with a primary Head
  // witness and no other body branch seeds the region; exact co-moving boundary
  // evidence propagates that candidate. It grants NO topology/skin authority.
  auto compatible_branch=[&](size_t i){for(size_t k=0;k<4;++k)if(result.parts[i].geometric_controls[k]&~branch[k])return false;return true;};
  for(size_t step=0;step<result.parts.size();++step){
    bool changed=false;
    for(const auto&[e,count]:contacts){
      (void)count;work.step(12);
      for(size_t direction=0;direction<2;++direction){auto from=e[direction],to=e[1-direction];
        if(result.parts[from].head_region&&!result.parts[to].head_region&&compatible_branch(to)){
          result.parts[to].head_region=true;changed=true;
        }
      }
    }
    if(!changed)break;
  }
  if(scene.objects().size()!=initial_objects)throw std::runtime_error("Head geometry membership changed");
  for(const auto&c:captured){work.step(64);const auto&o=scene.object(c.object.id);if(!same_object_authority(c.object,o)||!same(c.object.painted,o.painted)||o.painted->revision()!=c.paint_revision||o.volume->revision()!=c.volume_revision||scene.world_matrix(o.id).matrix()!=c.world)throw std::runtime_error("Head geometry captured input changed");}
  result.work=work.used;return result;
}
} // namespace dome::detail
