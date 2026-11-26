// SPDX-License-Identifier: GPL-3.0-or-later
#include "creator-vrm.hpp"
#include "dome/material-bindings.hpp"
#include "dome/affine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace dome::detail {
namespace {
[[noreturn]] void fail(const char *s){throw std::invalid_argument(std::string("Creator VRM: ")+s);}
const HumanoidControls &recipe(const std::vector<ObjectSurface> &objects) {
  if(objects.empty()||objects.front().groups.empty()||!objects.front().groups.back().humanoid)
    fail("saved creator settings are missing");
  return *objects.front().groups.back().humanoid;
}
bool same_mesh(const Mesh &a,const Mesh &b) {
  if(a.positions.size()!=b.positions.size()||a.normals.size()!=b.normals.size()||a.indices!=b.indices||a.uv!=b.uv)return false;
  for(size_t i=0;i<a.positions.size();++i)if(length(a.positions[i]-b.positions[i])>2e-6f)return false;
  for(size_t i=0;i<a.normals.size();++i)if(length(a.normals[i]-b.normals[i])>2e-6f)return false;
  return true;
}
bool same_position(Vec3 a,Vec3 b) {
  return a.x==b.x && a.y==b.y && a.z==b.z;
}
bool preserves_source_corners(const ObjectSurface &object,const SourceUvOrigin &origin,
                              const std::function<bool()> &cancel) {
  const auto &current=object.mesh;
  const auto &original=origin.paint->mesh();
  const auto &current_source=object.appearance->source_vertices;
  const auto &original_source=origin.appearance->source_vertices;
  if(!object.material_bindings->sampling_reparameterized ||
     current.indices.size()!=original.indices.size() ||
     current.positions.size()!=current_source.size() ||
     current.normals.size()!=current.positions.size() ||
     current.positions.size()>current.indices.size())
    return false;

  // A UV edit can split a vertex at an island boundary. Each current corner
  // must still address the same rest position, normal and original accessor.
  std::vector<uint32_t> remap(current.positions.size(),UINT32_MAX);
  for(size_t corner=0;corner<current.indices.size();++corner) {
    if((corner%4096)==0 && cancel && cancel())
      throw std::runtime_error("Creator VRM authority cancelled");
    const auto vertex=current.indices[corner],source=original.indices[corner];
    if(vertex>=remap.size()) return false;
    auto &previous=remap[vertex];
    if(previous==source) continue;
    if(previous!=UINT32_MAX || current_source[vertex]!=original_source[source] ||
       !same_position(current.positions[vertex],original.positions[source]) ||
       !same_position(current.normals[vertex],original.normals[source]))
      return false;
    previous=source;
  }
  return std::none_of(remap.begin(),remap.end(),[](auto vertex){return vertex==UINT32_MAX;});
}
Vec3 vector(const nlohmann::json &j){return {j.value("x",0.f),j.value("y",0.f),j.value("z",0.f)};}
nlohmann::json vector(Vec3 v){return {{"x",v.x},{"y",v.y},{"z",v.z}};}
Vec3 unity(Vec3 p){return {p.x,p.y,-p.z};}
Vec3 position(const ImportedAsset &asset,size_t n){auto &m=asset.nodes.at(n).world;return {m[12],m[13],m[14]};}
bool positive_similarity(const AffineTransform &placement) {
  if (placement.orientation() != AffineOrientation::Preserving)
    return false;
  auto x = placement.direction({1, 0, 0});
  auto y = placement.direction({0, 1, 0});
  auto z = placement.direction({0, 0, 1});
  const auto norm = [](const AffineVector &axis) {
    return std::hypot(axis[0], axis[1], axis[2]);
  };
  const double scale = std::max({norm(x), norm(y), norm(z)});
  if (!std::isfinite(scale) || scale == 0)
    return false;
  for (auto *axis : {&x, &y, &z})
    for (auto &component : *axis)
      component /= scale;
  const auto dot = [](const AffineVector &a, const AffineVector &b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  };
  constexpr double tolerance = 128 * std::numeric_limits<double>::epsilon();
  return std::abs(dot(x, x) - 1) <= tolerance &&
         std::abs(dot(y, y) - 1) <= tolerance &&
         std::abs(dot(z, z) - 1) <= tolerance &&
         std::abs(dot(x, y)) <= tolerance &&
         std::abs(dot(x, z)) <= tolerance &&
         std::abs(dot(y, z)) <= tolerance;
}
bool has_spring_geometry(const nlohmann::json &spring) {
  if (spring.contains("boneGroups") &&
      std::ranges::any_of(spring.at("boneGroups"), [](const auto &group) {
        return !group.at("bones").empty();
      }))
    return true;
  return spring.contains("colliderGroups") &&
         std::ranges::any_of(spring.at("colliderGroups"), [](const auto &group) {
           return !group.at("colliders").empty();
         });
}
}
bool has_creator_vrm_origin(const std::vector<ObjectSurface> &objects) {
  for(const auto &o:objects)if(o.material_bindings&&o.material_bindings->uv_origin&&
     !o.material_bindings->uv_origin->creator_kind.empty())return true;
  return false;
}
CreatorVrmAuthority::CreatorVrmAuthority(const std::vector<ObjectSurface> &objects,
    const CreatorAppearance &appearance,const std::function<bool()> &cancel)
    :shape_(recipe(objects),appearance.original_bytes,cancel),nodes_(shape_.joint_nodes()) {
  const auto encoded=recipe(objects).encode();auto reference=shape_.reference(cancel);
  if(objects.size()!=reference.parts.size())fail("complete creator parts including generated groom are required");
  const auto placement = objects.front().world_matrix();
  nonlinear_physics_placement_ =
      !shape_.has_uniform_shape() && !positive_similarity(placement);
  std::set<size_t> used;
  for(const auto &o:objects) {
    if(cancel&&cancel())throw std::runtime_error("Creator VRM authority cancelled");
    if(o.groups.empty()||o.groups.back().id!=objects.front().groups.back().id||!o.groups.back().humanoid||o.groups.back().humanoid->encode()!=encoded)
      fail("creator group or saved recipe differs");
    if(o.world_matrix().matrix()!=placement.matrix())
      fail("creator parts require one shared placement for the avatar rig");
    // No arbitrary bind-space edits can borrow a creator genesis certificate.
    if(!o.deformation||!o.deformation->skeleton||o.deformation->skeleton->encode()!=shape_.skeleton().encode())
      fail("current rest rig differs from the actual creator recipe");
    if(!o.material_bindings||!o.material_bindings->uv_origin)fail("immutable creator genesis is missing");
    o.material_bindings->validate();const auto &origin=*o.material_bindings->uv_origin;
    if(origin.creator_version!=1||HumanoidControls::decode(nlohmann::json::parse(origin.creator_parameters)).encode()!=encoded||!origin.paint||!origin.deformation)
      fail("stale or incomplete creator genesis recipe");
    size_t match=SIZE_MAX;
    for(size_t i=0;i<reference.parts.size();++i) {
      const auto &p=reference.parts[i];
      if(bool(p.appearance)!=bool(o.appearance))continue;
      if(p.appearance&&(p.appearance->source_mesh!=o.appearance->source_mesh||p.appearance->source_primitive!=o.appearance->source_primitive||p.appearance->source_node!=o.appearance->source_node))continue;
      if(!p.appearance&&origin.creator_kind!="vroid-groom")continue;
      match=i;break;
    }
    if(match==SIZE_MAX||!used.insert(match).second)fail("ambiguous creator part identity");
    const auto &expected=reference.parts[match];const auto &mesh=origin.paint->mesh();
    if(!same_mesh(mesh,expected.mesh)||origin.deformation->encode(mesh)!=expected.deformation.encode(expected.mesh))
      fail("creator genesis geometry, UV, skin, morph or rig differs from licensed creation");
    if(!o.deformation->humanoid || o.deformation->humanoid->encode()!=expected.deformation.humanoid->encode())
      fail("current humanoid roles differ from the certified creator rig");
    if(o.appearance) {
      if(origin.creator_kind!="vroid-shape"||!origin.appearance||
         o.appearance->source_joint_nodes!=nodes_||origin.appearance->source_joint_nodes!=nodes_||
         origin.appearance->source_vertices!=expected.appearance->source_vertices ||
         !o.appearance->source_basis_quaternion ||
         o.appearance->source_basis_quaternion->w!=0 || o.appearance->source_basis_quaternion->x!=0 ||
         std::abs(o.appearance->source_basis_quaternion->y)!=1 || o.appearance->source_basis_quaternion->z!=0)
        fail("creator source accessor or joint identity differs");
      if(o.appearance->source_vertices!=origin.appearance->source_vertices &&
         !preserves_source_corners(o,origin,cancel))
        fail("creator UV remap changes the original source corners");
    } else if(origin.appearance||origin.material||o.material_bindings->has_mtoon)
      fail("generated groom cannot impersonate an imported material or appearance");
  }
}
nlohmann::json CreatorVrmAuthority::refit(const nlohmann::json &source,
    const ImportedAsset &asset,const std::function<bool()> &cancel) const {
  // The shared placement is emitted around the rig and mesh. Bone-local
  // offsets stay in the certified creator frame. Nonlinear spring refitting
  // certifies spherical bounds in that frame and needs a positive similarity
  // to carry the bounds through to the consumer's spring solver.
  if (nonlinear_physics_placement_ &&
      has_spring_geometry(source.at("secondaryAnimation")))
    fail("nonlinear creator spring refitting requires positive similarity placement");
  auto result=source;
  std::map<uint32_t,Vec3> current;
  // Native source basis is the creator's proper Y half-turn. Export restores
  // glTF axes; VRM0 bone-local offsets use Unity's left-handed Z convention.
  for(auto n:nodes_)current[n]=Vec3{-point(position(asset,n)).x,point(position(asset,n)).y,-point(position(asset,n)).z};
  auto local=[&](size_t n,Vec3 offset){
    if(!current.contains(uint32_t(n)))fail("metadata node is outside the exact creator rig");
    auto original=position(asset,n)+unity(offset);auto p=point(original);p={-p.x,p.y,-p.z};
    return unity(p-current.at(uint32_t(n)));
  };
  auto &first=result["firstPerson"];
  if(first.contains("firstPersonBoneOffset")) {
    int n=first.value("firstPersonBone",-1);if(n<0)fail("creator first-person offset has no explicit bone");
    first["firstPersonBoneOffset"]=vector(local(size_t(n),vector(first["firstPersonBoneOffset"])));
  }
  auto &spring=result["secondaryAnimation"];
  if(spring.contains("colliderGroups"))for(auto &g:spring["colliderGroups"]) {
    auto n=g.at("node").get<size_t>();
    for(auto &c:g["colliders"]) {
      auto offset=c.contains("offset")?vector(c["offset"]):Vec3{};
      c["offset"]=vector(local(n,offset));
      c["radius"]=shape_.sphere_radius(position(asset,n)+unity(offset),c.value("radius",0.f),cancel);
    }
  }
  if(spring.contains("boneGroups"))for(auto &g:spring["boneGroups"])if(g.contains("hitRadius")) {
    float radius=g["hitRadius"],adapted=0;
    std::set<uint32_t> visited;std::vector<uint32_t> pending=g.at("bones").get<std::vector<uint32_t>>();
    while(!pending.empty()) {
      auto n=pending.back();pending.pop_back();if(!visited.insert(n).second)continue;
      if(!current.contains(n))fail("spring recursion leaves the certified creator rig");
      const auto &node=asset.nodes.at(n);auto center=position(asset,n);
      Vec3 actual_tail{};
      if(node.children.empty()) {
        if(node.parent<0)fail("spring leaf has no authored direction");
        auto direction=center-position(asset,size_t(node.parent));if(length(direction)<1e-6f)fail("spring leaf direction is degenerate");
        center+=normalized(direction)*.07f;
        auto new_direction=current.at(n)-current.at(uint32_t(node.parent));
        if(length(new_direction)<1e-6f)fail("shaped spring leaf direction is degenerate");
        actual_tail=current.at(n)+normalized(new_direction)*.07f;
      } else {
        // UniVRM uses the first actual child and recurses over ALL children.
        // The creator keeps exactly the source hierarchy, with no helper nodes.
        if(!std::is_sorted(node.children.begin(),node.children.end()))fail("spring child order cannot be reproduced by creator rig emission");
        center=position(asset,node.children.front());actual_tail=current.at(node.children.front());
        for(auto child:node.children)pending.push_back(child);
      }
      auto mapped=point(center);mapped={-mapped.x,mapped.y,-mapped.z};
      // Leaves use a fixed consumer 7cm continuation, so include that explicit
      // center displacement instead of pretending the nonlinear tail is kept.
      adapted=std::max(adapted,shape_.sphere_radius(center,radius,cancel)+length(mapped-actual_tail));
    }
    g["hitRadius"]=adapted;
  }
  return result;
}
}
