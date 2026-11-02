// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-rest-metadata.hpp"
#include "vrm-source-graph.hpp"
#include "affine-matrix.hpp"
#include <cmath>
namespace dome::detail {
namespace {
void require(bool value,const char *message){if(!value)throw std::invalid_argument(std::string("VRM rest metadata: ")+message);}
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("VRM rest metadata transport cancelled");}
std::optional<uint32_t> node(const nlohmann::json &value,size_t size,bool absent=false){require(value.is_number_integer()||value.is_number_unsigned(),"node identity must be integral");if(value.is_number_unsigned()){const auto index=value.get<uint64_t>();require(index<size,"node identity is outside the actual original graph");return uint32_t(index);}const auto index=value.get<int64_t>();if(index==-1&&absent)return {};require(index>=0&&uint64_t(index)<size,"node identity is outside the actual original graph");return uint32_t(index);}
}
VrmRestMetadata prepare_vrm0_rest_metadata(const VrmSourceGraph &graph,const nlohmann::json &vrm,const std::function<bool()> &cancelled){
 stop(cancelled);require(graph.model&&graph.model->asset,"missing actual source model");const auto &nodes=graph.model->asset->nodes;const auto count=nodes.size();require(count&&count<=100000&&graph.parents.size()==count&&graph.rest_local.size()==count&&graph.rest_world.size()==count&&graph.current_local.size()==count,"incomplete current source graph");
 AffineTransform prefix;for(const auto &group:graph.prefix){stop(cancelled);prefix=prefix*group_local_matrix(group);}
 VrmRestMetadata result;result.first_person=vrm.at("firstPerson");const auto &spring=vrm.at("secondaryAnimation");std::vector<uint8_t> roots(count),member(count),state(count);size_t work=0;
 auto tick=[&]{stop(cancelled);if(++work>50000000)throw std::length_error("VRM rest metadata work budget exceeded");};
 if(spring.contains("boneGroups")){require(spring.at("boneGroups").is_array()&&spring.at("boneGroups").size()<=100000,"spring group count exceeds metadata bounds");for(const auto &group:spring.at("boneGroups")){tick();if(group.contains("bones")){require(group.at("bones").is_array(),"spring bone roots must be an array");for(const auto &root:group.at("bones")){tick();roots[*node(root,count)]=1;}}}}
 auto visit=[&](auto &&self,size_t n,size_t depth)->void{tick();require(depth<=256,"spring ancestry exceeds256 levels");if(state[n]==2)return;require(state[n]==0,"cycle in current spring ancestry");state[n]=1;const auto parent=graph.parents[n];require(parent>=-1&&parent<int(count),"invalid current spring parent identity");if(parent>=0)self(self,size_t(parent),depth+1);member[n]=roots[n]||(parent>=0&&member[size_t(parent)]);state[n]=2;};for(size_t n=0;n<count;++n)visit(visit,n,1);
 for(size_t n=0;n<count;++n)if(member[n]){tick();
  // UniVRM0 SetupRecursive follows EVERY actual child, including its first-child
  // tail choice. Matrix-factor helpers or rendered-mesh children must not be
  // silently inserted into that simulation hierarchy.
  require(gltf_trs_matrix(graph.current_local[n]),"an affine factor would add a helper inside an auto-recursed spring chain; retain the native project until an exact spring adapter is prepared");
  require(nodes[n].mesh<0,"a source spring node with a mesh needs a topology-preserving mesh-node exporter; retain the native project");
  // Child order is a physical parameter (the first child defines the tail).
  // Reparenting into/out of the chain and changing its rest axes/lengths need
  // an explicit simulation adapter, not merely unchanged JSON parameters.
  require(graph.parents[n]==nodes[n].parent&&graph.rest_local[n]==nodes[n].local,
          "spring-chain rest/parent edits require explicit simulation reauthoring");
  for(auto child:nodes[n].children){tick();require(child<count&&graph.parents[child]==int(n),
          "a spring child left its authored simulation chain");}
 }
 for(size_t n=0;n<count;++n){tick();const auto parent=graph.parents[n];if(parent>=0&&member[size_t(parent)])require(nodes[n].parent==parent,"a new child entered an auto-recursed spring chain");}
 auto frame=[&](uint32_t source,bool sphere)->std::optional<VrmMetadataFrame>{tick();const auto &old=nodes[source].world;if(graph.rest_world[source]==old)return {};require(!member[source],"a collider/center attachment frame would add a child inside an auto-recursed spring chain; reauthor that attachment explicitly");
  bool same_linear=true;for(size_t col=0;col<3;++col)for(size_t row=0;row<3;++row)same_linear&=graph.rest_world[source][col*4+row]==old[col*4+row];
  // Unity SphereCollider uses Transform.lossyScale, not an ellipsoid matrix.
  // A translation-only child has identity rotation/scale and preserves that
  // exact parent scale authority. General SVD/rotated/sheared collider helpers
  // need a separately proven runtime adapter; matching a world matrix alone
  // does not prove Unity's decomposed radius/sign convention.
  require(!sphere||same_linear,"collider linear REST edits need an exact runtime sphere/capsule adapter; retain the native project");
  auto local=(AffineTransform(graph.rest_world[source]).inverse()*AffineTransform(old)).float_matrix();
  if(same_linear)for(size_t col=0;col<3;++col)for(size_t row=0;row<3;++row)local[col*4+row]=float(col==row);
  const auto restored=prefix*AffineTransform(graph.rest_world[source])*AffineTransform(local),wanted=prefix*AffineTransform(old);for(size_t k=0;k<16;++k)require(std::abs(restored.matrix()[k]-wanted.matrix()[k])<=.00002,"attachment-frame coefficient/translation reconstruction bound exceeded (2e-5)");return VrmMetadataFrame{source,local};};
 if(spring.contains("colliderGroups")){const auto &groups=spring.at("colliderGroups");require(groups.is_array()&&groups.size()<=100000,"collider group count exceeds metadata bounds");result.colliders.reserve(groups.size());for(const auto &group:groups)result.colliders.push_back(frame(*node(group.at("node"),count),true));}
 if(spring.contains("boneGroups")){const auto &groups=spring.at("boneGroups");result.centers.reserve(groups.size());for(const auto &group:groups){tick();const auto center=group.contains("center")?node(group.at("center"),count,true):std::nullopt;result.centers.push_back(center?frame(*center,false):std::nullopt);}}
 std::optional<uint32_t> first;if(result.first_person.contains("firstPersonBone"))first=node(result.first_person.at("firstPersonBone"),count,true);
 if(!first){for(const auto &bone:graph.humanoid.at("humanBones")){tick();if(bone.at("bone").get_ref<const std::string&>()=="head")first=node(bone.at("node"),count);}require(first.has_value(),"default first-person bone has no actual current humanoid head");}
 const auto correction=AffineTransform(graph.rest_world[*first]).inverse()*AffineTransform(nodes[*first].world);
 if(graph.rest_world[*first]!=nodes[*first].world){
  // Pinned UniVRM0 Exporter uses inverseAxesZ for glTF nodes but writes these
  // Unity-space offsets unchanged. Transport is H*C*H, H=diag(1,1,-1).
  // FirstPersonBone itself remains the actual bone: it is also the head-erasure
  // root, and substituting an attachment helper would change visible triangles.
  std::array<double,3> old{};if(result.first_person.contains("firstPersonBoneOffset")){const auto &offset=result.first_person.at("firstPersonBoneOffset");old={offset.value("x",0.f),offset.value("y",0.f),offset.value("z",0.f)};}
  const auto mapped=correction.point({old[0],old[1],-old[2]});const std::array<float,3> next{float(mapped[0]),float(mapped[1]),float(-mapped[2])};for(auto v:next)require(std::isfinite(v),"first-person point is outside finite float range");
  const auto before=(prefix*AffineTransform(nodes[*first].world)).point({old[0],old[1],-old[2]}),after=(prefix*AffineTransform(graph.rest_world[*first])).point({next[0],next[1],-next[2]});for(size_t k=0;k<3;++k)require(std::abs(before[k]-after[k])<=.00002,"first-person point reconstruction exceeds20micrometres");
  result.first_person["firstPersonBoneOffset"]={{"x",next[0]},{"y",next[1]},{"z",next[2]}};result.camera_changed=true;
 }
 // Custom angle ranges describe a declared local axis frame. They cannot be
 // silently reused when that actual local linear authority changed.
 for(const auto &bone:graph.humanoid.at("humanBones")){tick();if(bone.contains("useDefaultValues")&&bone.at("useDefaultValues").get<bool>())continue;const auto source=*node(bone.at("node"),count);for(size_t col=0;col<3;++col)for(size_t row=0;row<3;++row)require(graph.rest_local[source][col*4+row]==nodes[source].local[col*4+row],"custom humanoid limits require explicit transport to changed bone-local axes");}
 stop(cancelled);return result;
}
}
